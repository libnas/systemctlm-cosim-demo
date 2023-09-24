#
# Cosim Makefiles
#
# Copyright (c) 2016 Xilinx Inc.
# Written by Edgar E. Iglesias
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

-include .config.mk

INSTALL ?= install

SYSTEMC ?= /usr/local/systemc-2.3.2/
SYSTEMC_INCLUDE ?=$(SYSTEMC)/include/
SYSTEMC_LIBDIR ?= $(SYSTEMC)/lib-linux64
# In case your TLM-2.0 installation is not bundled with
# with the SystemC one.
# TLM2 ?= /opt/systemc/TLM-2009-07-15

SCML ?= /usr/local/scml-2.3/
SCML_INCLUDE ?= $(SCML)/include/
SCML_LIBDIR ?= $(SCML)/lib-linux64/

CFLAGS += -Wall -O2 -g
CXXFLAGS += -Wall -O2 -g

ifneq "$(SYSTEMC_INCLUDE)" ""
CPPFLAGS += -I $(SYSTEMC_INCLUDE)
endif
ifneq "$(TLM2)" ""
CPPFLAGS += -I $(TLM2)/include/tlm
endif

CPPFLAGS += -I .
LDFLAGS  += -L $(SYSTEMC_LIBDIR)
#LDLIBS += -pthread -Wl,-Bstatic -lsystemc -Wl,-Bdynamic
LDLIBS   += -pthread -lsystemc

PCIE_MODEL_O = pcie-model/tlm-modules/pcie-controller.o
PCIE_MODEL_O += pcie-model/tlm-modules/libpcie-callbacks.o
PCIE_MODEL_CPPFLAGS += -I pcie-model/libpcie/src -I pcie-model/

PCIE_XDMA_DEMO_C = pcie/versal/xdma-demo.cc
PCIE_XDMA_DEMO_O = $(PCIE_XDMA_DEMO_C:.cc=.o)
PCIE_XDMA_DEMO_OBJS += $(PCIE_XDMA_DEMO_O) $(PCIE_MODEL_O)

# Uncomment to enable use of scml2
# CPPFLAGS += -I $(SCML_INCLUDE)
# LDFLAGS += -L $(SCML_LIBDIR)
# LDLIBS += -lscml2 -lscml2_logging

SC_OBJS += trace.o
SC_OBJS += debugdev.o
SC_OBJS += demo-dma.o
SC_OBJS += xilinx-axidma.o

LIBSOC_PATH=libsystemctlm-soc
CPPFLAGS += -I $(LIBSOC_PATH)

CPPFLAGS += -I $(LIBSOC_PATH)/tests/test-modules/
CPPFLAGS += -I $(LIBSOC_PATH)/tlm-bridges/
CPPFLAGS += -I $(LIBSOC_PATH)/tlm-extensions/
CPPFLAGS += -I $(LIBSOC_PATH)/soc/pci/core/
SC_OBJS += $(LIBSOC_PATH)/tests/test-modules/memory.o

LIBRP_PATH=$(LIBSOC_PATH)/libremote-port
C_OBJS += $(LIBRP_PATH)/safeio.o
C_OBJS += $(LIBRP_PATH)/remote-port-proto.o
C_OBJS += $(LIBRP_PATH)/remote-port-sk.o
SC_OBJS += $(LIBRP_PATH)/remote-port-tlm.o
SC_OBJS += $(LIBRP_PATH)/remote-port-tlm-memory-master.o
SC_OBJS += $(LIBRP_PATH)/remote-port-tlm-memory-slave.o
SC_OBJS += $(LIBRP_PATH)/remote-port-tlm-wires.o
SC_OBJS += $(LIBRP_PATH)/remote-port-tlm-ats.o
SC_OBJS += $(LIBRP_PATH)/remote-port-tlm-pci-ep.o
SC_OBJS += $(LIBSOC_PATH)/soc/pci/core/pci-device-base.o
SC_OBJS += $(LIBSOC_PATH)/soc/dma/xilinx/mcdma/mcdma.o
SC_OBJS += $(LIBSOC_PATH)/soc/net/ethernet/xilinx/mrmac/mrmac.o
CPPFLAGS += -I $(LIBRP_PATH)

OBJS = $(C_OBJS) $(SC_OBJS)

PCIE_XDMA_DEMO_OBJS += $(OBJS)

TARGET_PCIE_XDMA_DEMO = pcie/versal/xdma-demo

PCIE_MODEL_DIR=pcie-model/tlm-modules
ifneq ($(wildcard $(PCIE_MODEL_DIR)/.),)
TARGETS += $(TARGET_PCIE_XDMA_DEMO)
endif


-include $(PCIE_XDMA_DEMO_OBJS:.o=.d)

CFLAGS += -MMD
CXXFLAGS += -MMD

$(TARGET_PCIE_XDMA_DEMO): $(PCIE_XDMA_DEMO_OBJS)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

## libpcie ##
-include pcie-model/libpcie/libpcie.mk

$(TARGET_PCIE_XDMA_DEMO): CPPFLAGS += $(PCIE_MODEL_CPPFLAGS)
$(TARGET_PCIE_XDMA_DEMO): LDLIBS += libpcie.a
$(TARGET_PCIE_XDMA_DEMO): $(PCIE_XDMA_DEMO_OBJS) libpcie.a
	$(CXX) $(LDFLAGS) -o $@ $(PCIE_XDMA_DEMO_OBJS) $(LDLIBS)

TARGETS += $(TARGET_PCIE_XDMA_DEMO)

all: $(TARGETS)

clean:
	$(RM) $(OBJS) $(OBJS:.o=.d) $(TARGETS)
	$(RM) $(PCIE_XDMA_DEMO_OBJS) $(PCIE_XDMA_DEMO_OBJS:.o=.d)
	$(RM) -r libpcie libpcie.a
