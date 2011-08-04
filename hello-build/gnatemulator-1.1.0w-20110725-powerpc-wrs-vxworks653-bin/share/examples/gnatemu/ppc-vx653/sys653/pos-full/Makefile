#
# Copyright (c) 2007 Wind River Systems, Inc.
#
# The right to copy, distribute, modify or otherwise make use
# of this software may be licensed only pursuant to the terms
# of an applicable Wind River license agreement.
#

all:

# Create the partition OS project
	mkdir -p demo
	cp Makefile.pos demo/Makefile
	cp hello-pos.xml demo
	cp pos-full-api.xml demo

# Build the partition OS
	make -C demo

clean:
	rm -rf demo

