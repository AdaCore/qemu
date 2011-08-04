# 
# Copyright (c) 2007 Wind River Systems, Inc.
#
# The right to copy, distribute, modify or otherwise make use
# of this software may be licensed only pursuant to the terms
# of an applicable Wind River license agreement.
#

#CPU = PPC604

all: pos-full.sm pos-full-stubs.o install

include $(WIND_BASE)/target/vThreads/config/make/Makefile.vars

vpath %.c   $(WIND_BASE)/target/vThreads/config/comps/src
vpath %.o   $(WIND_BASE)/target/vThreads/lib/obj$(CPU)gnuvx

pos-full.sm: sslMain.o vThreadsComponent.o apexComponent.o pos-full-ept.o pos-full.lds
	$(LD) $(LDFLAGS) -T pos-full.lds -o $@ $(filter %.o,$^)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

pos-full-ept.c: pos-full-api.xml
	xmlgen --linkage --output-entrypoints $@ $<

pos-full-stubs.c: pos-full-api.xml
	xmlgen --linkage --arch $(TOOLARCH) --output-stubs $@ $<

pos-full.lds: hello-pos.xml
	xmlgen --ldScript --arch $(TOOLARCH) -o $@ $<

install: pos-full-stubs.o
	cp pos-full-stubs.o ../../dist
