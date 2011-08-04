#
# Copyright (c) 2007 Wind River Systems, Inc.
#
# The right to copy, distribute, modify or otherwise make use
# of this software may be licensed only pursuant to the terms
# of an applicable Wind River license agreement.
#

all: create build

create:
# Create the integration project
	mkdir -p demo-$(POS)
	cp Makefile.int demo-$(POS)/Makefile
ifdef DEBUG
	cat module-head.xml hm-debug.xml module-tail.xml > demo-$(POS)/hello-module.xml
else
	cat module-head.xml hm-nodebug.xml module-tail.xml > demo-$(POS)/hello-module.xml
endif
	cp hello-part.xml demo-$(POS)
	cp ../mos/demo/bsp.xml demo-$(POS)
	cp ../pos-$(POS)/demo/hello-pos.xml demo-$(POS)
	cp ../dist/app.xml demo-$(POS)

build:
# Integrate
	make -C demo-$(POS) POS=$(POS)

install:
	make -C demo-$(POS) POS=$(POS) install

clean:
	-rm -rf demo*

