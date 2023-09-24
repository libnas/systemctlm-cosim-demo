/* Modified by libnas.  */

/*
 * Copyright (C) 2022, Advanced Micro Devices, Inc.
 * Written by Fred Konrad
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#define SC_INCLUDE_DYNAMIC_PROCESSES

#include "soc/pci/xilinx/xdma_dut.h"
#include "tlm-modules/pcie-controller.h"
#include "tests/test-modules/signals-xdma.h"

using namespace sc_core;
using namespace sc_dt;
using namespace std;

#define PCI_VENDOR_ID_XILINX		(0x10ee)
#define PCI_SUBSYSTEM_ID_XILINX_TEST	(0x000A)

#define PCI_CLASS_BASE_NETWORK_CONTROLLER     (0x02)

#define KiB (1024)
#define RAM_SIZE (4 * KiB)

#define NR_MMIO_BAR 6
#define NR_IRQ 15

sc_clock clk("clk", sc_time(20, SC_US));

template<typename XDMA_t>
class pcie_versal : public pci_device_base
{
private:

	// BARs towards the XDMA
	tlm_utils::simple_initiator_socket<pcie_versal> user_bar_init_socket;
	tlm_utils::simple_initiator_socket<pcie_versal> cfg_init_socket;

	// XDMA towards PCIe interface (host)
	tlm_utils::simple_target_socket<pcie_versal> brdg_dma_tgt_socket;

	// MSI-X propagation
	// sc_vector<sc_signal<bool> > signals_irq;

	//
	// Nothing to attach to the XDMA yet, just add a dummy memory.
	// With that the testcase will be able to check what has been
	// written in the memory..  Add an interconnect, so we can map
	// it anywhere.
	//
	iconnect<1, 1> bus;
	memory sbi_dummy;

	void bar_b_transport(int bar_nr, tlm::tlm_generic_payload &trans,
				sc_time &delay)
	{
		switch (bar_nr) {
			case 0:
				user_bar_init_socket->b_transport(trans, delay);
				break;
			case 1:
				cfg_init_socket->b_transport(trans, delay);
				break;
			default:
				SC_REPORT_ERROR("pcie_versal",
					"writing to an unimplemented bar");
				trans.set_response_status(
					tlm::TLM_GENERIC_ERROR_RESPONSE);
				break;
		}
	}

	//
	// Forward DMA requests received from the XDMA
	//
	void fwd_dma_b_transport(tlm::tlm_generic_payload& trans,
					sc_time& delay)
	{
		dma->b_transport(trans, delay);
	}

public:
	XDMA_t xdma;
	SC_HAS_PROCESS(pcie_versal);

	pcie_versal(sc_core::sc_module_name name) :

		pci_device_base(name, NR_MMIO_BAR, NR_IRQ),

		xdma("xdma"),

		user_bar_init_socket("user_bar_init_socket"),
		cfg_init_socket("cfg_init_socket"),
		brdg_dma_tgt_socket("brdg-dma-tgt-socket"),

		// signals_irq("signals_irq", NR_IRQ),

		bus("bus"),
		sbi_dummy("sbi_dummy", sc_time(0, SC_NS), RAM_SIZE)
	{
		//
		// XDMA connections
		//
		// user_bar_init_socket.bind(xdma.user_bar);
		cfg_init_socket.bind(xdma.config_bar);

		// Setup DMA forwarding path (xdma.dma -> upstream to host)
		xdma.dma.bind(brdg_dma_tgt_socket);
		brdg_dma_tgt_socket.register_b_transport(
			this, &pcie_versal::fwd_dma_b_transport);

		/* Connect the SBI dummy RAM
		bus.memmap(0x102100000ULL, 0x1000 - 1,
			   ADDRMODE_RELATIVE, -1, sbi_dummy.socket);
		xdma.card_bus.bind((*bus.t_sk[0])); // define the card_bus in the xdma.h
		*/
	}

	
	void resetn(sc_signal<bool>& rst)
	{
		xdma.resetn(rst);
	}
	
};

PhysFuncConfig getPhysFuncConfig()
{
	PhysFuncConfig cfg;
	PMCapability pmCap;
	PCIExpressCapability pcieCap;
	MSIXCapability msixCap;
	uint32_t bar_flags = PCI_BASE_ADDRESS_MEM_TYPE_64;
	// uint32_t msixTableSz = NR_IRQ;
	// uint32_t tableOffset = 0x100 | 4; // Table offset: 0, BIR: 4
	// uint32_t pba = 0x140000 | 4; // BIR: 4
	uint32_t maxLinkWidth;

	cfg.SetPCIVendorID(PCI_VENDOR_ID_XILINX);
	// XDMA
	cfg.SetPCIDeviceID(0x903F);

	cfg.SetPCIClassProgIF(0);
	cfg.SetPCIClassDevice(0);
	cfg.SetPCIClassBase(PCI_CLASS_BASE_NETWORK_CONTROLLER);

	cfg.SetPCIBAR0(256 * KiB, bar_flags); // user_bar_pos
	cfg.SetPCIBAR1(256 * KiB, bar_flags); // config_bar_pos

	cfg.SetPCISubsystemVendorID(PCI_VENDOR_ID_XILINX);
	cfg.SetPCISubsystemID(PCI_SUBSYSTEM_ID_XILINX_TEST);
	cfg.SetPCIExpansionROMBAR(0, 0);

	cfg.AddPCICapability(pmCap);

	maxLinkWidth = 1 << 4;
	pcieCap.SetDeviceCapabilities(PCI_EXP_DEVCAP_RBER);
	pcieCap.SetLinkCapabilities(PCI_EXP_LNKCAP_SLS_2_5GB | maxLinkWidth
				    | PCI_EXP_LNKCAP_ASPM_L0S);
	pcieCap.SetLinkStatus(PCI_EXP_LNKSTA_CLS_2_5GB | PCI_EXP_LNKSTA_NLW_X1);
	cfg.AddPCICapability(pcieCap);

	// msixCap.SetMessageControl(msixTableSz-1);
	// msixCap.SetTableOffsetBIR(tableOffset);
	// msixCap.SetPendingBitArray(pba);
	// cfg.AddPCICapability(msixCap);

	return cfg;
}

// Host / PCIe RC
//
// This pcie_host uses Remote-port to connect to a QEMU PCIe RC.
// If you'd like to connect this demo to something else, you need
// to replace this implementation with the host model you've got.
//
SC_MODULE(pcie_host)
{
private:
	remoteport_tlm_pci_ep rp_pci_ep;

public:
	pcie_root_port rootport;
	sc_in<bool> rst;

	pcie_host(sc_module_name name, const char *sk_descr) :
		sc_module(name),
		rp_pci_ep("rp-pci-ep", 0, 1, 0, sk_descr),
		rootport("rootport"),
		rst("rst")
	{
		rp_pci_ep.rst(rst);
		rp_pci_ep.bind(rootport);
	}
};

SC_MODULE(Top)
{
public:
	SC_HAS_PROCESS(Top);

	pcie_host host;

	PCIeController pcie_ctlr;
	pcie_versal<xdma_def> xdma;

	//
	// Reset signal.
	//
	sc_signal<bool> rst;

	Top(sc_module_name name, const char *sk_descr, sc_time quantum) :
		sc_module(name),
		host("host", sk_descr),
		pcie_ctlr("pcie-ctlr", getPhysFuncConfig()),
		xdma("pcie-xdma"),
		rst("rst")
	{
		m_qk.set_global_quantum(quantum);

		XDMASignals signals("signals");
		xdma_dut dut("dut");

		// Setup TLP sockets (host.rootport <-> pcie-ctlr)
		host.rootport.init_socket.bind(pcie_ctlr.tgt_socket);
		pcie_ctlr.init_socket.bind(host.rootport.tgt_socket);

		//
		// PCIeController <-> XDMA connections
		//
		pcie_ctlr.bind(xdma);

		// Reset signal
		rst.write(false);
		host.rst(rst);
		xdma.resetn(rst);

		signals.connect(xdma.xdma);
		signals.connect(dut);

		SC_THREAD(pull_reset);
	}

	void pull_reset(void) {
		/* Pull the reset signal.  */
		rst.write(true);
		wait(1, SC_US);
		rst.write(false);
	}

private:
	tlm_utils::tlm_quantumkeeper m_qk;
};

void usage(void)
{
	cout << "tlm socket-path sync-quantum-ns" << endl;
}

int sc_main(int argc, char* argv[])
{
	/*
	Top *top;
	uint64_t sync_quantum;
	sc_trace_file *trace_fp = NULL;

	if (argc < 3) {
		sync_quantum = 10000;
	} else {
		sync_quantum = strtoull(argv[2], NULL, 10);
	}

	sc_set_time_resolution(1, SC_PS);

	top = new Top("top", argv[1], sc_time((double) sync_quantum, SC_NS));

	if (argc < 3) {
		sc_start(1, SC_PS);
		sc_stop();
		usage();
		exit(EXIT_FAILURE);
	}

	trace_fp = sc_create_vcd_trace_file("trace");
	if (trace_fp) {
		trace(trace_fp, *top, top->name());
	}

	sc_start();
	if (trace_fp) {
		sc_close_vcd_trace_file(trace_fp);
	}
	return 0;
	*/
	sc_start();
	return 0;
}
