# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, can be overridden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-gcc

# binary name
OBJDIR = build
LIB = $(OBJDIR)/libegel.so
EXE = $(OBJDIR)/stub

# all sources 
SRCS := libegel.c master.c dpdk.c dpdkflow.c dpdknic.c flow.c igmp.c ipaddr.c print.c q.c
OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)

CC = gcc
CFLAGS = -fPIC -O3 -g -Wfatal-errors -D__GCC__
LDFLAGS = -shared 
RTE_LDFLAGS = -Wl,--no-undefined -L$(RTE_SDK)/$(RTE_TARGET)/lib -lrte_eal -lrte_ethdev -lrte_mbuf -lrte_mempool -lrte_net -lrte_timer
LDLIBS = -pthread 

RTE_CFLAGS = -m64 -pthread -march=native -DRTE_MACHINE_CPUFLAG_SSE -DRTE_MACHINE_CPUFLAG_SSE2 -DRTE_MACHINE_CPUFLAG_SSE3 -DRTE_MACHINE_CPUFLAG_SSSE3 -DRTE_MACHINE_CPUFLAG_SSE4_1 -DRTE_MACHINE_CPUFLAG_SSE4_2 -DRTE_MACHINE_CPUFLAG_AES -DRTE_MACHINE_CPUFLAG_PCLMULQDQ -DRTE_MACHINE_CPUFLAG_AVX -DRTE_MACHINE_CPUFLAG_RDRAND -DRTE_MACHINE_CPUFLAG_FSGSBASE -DRTE_MACHINE_CPUFLAG_F16C -DRTE_MACHINE_CPUFLAG_AVX2  
RTE_CFLAGS += -I$(RTE_SDK)/$(RTE_TARGET)/include -include $(RTE_SDK)/$(RTE_TARGET)/include/rte_config.h

all: prebuild ${LIB} ${EXE} postbuild

$(OBJDIR)/%.o: %.c
	$(CC) $(RTE_CFLAGS) $(CFLAGS) -c -o $@ $<
	
$(LIB): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(RTE_LDFLAGS) $(LDLIBS) 

#include $(RTE_SDK)/mk/rte.vars.mk

${EXE}: ${LIB}
	$(CC) main.c $(CFLAGS) -c -o $(OBJDIR)/main.o
	$(CC) $(OBJDIR)/main.o -o ${EXE} -L$(OBJDIR) -Wl,-rpath,. -legel $(RTE_LDFLAGS) 
prebuild:
	mkdir -p build
	
postbuild: prebuild ${LIB} ${EXE}
	mkdir -p ../lib
	cp $(OBJDIR)/libegel.so ../lib
	mkdir -p ../include
	cp libegel.h ../include
	
clean:
	rm -rf $(OBJDIR)/*
