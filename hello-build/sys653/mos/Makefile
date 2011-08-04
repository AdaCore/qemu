# Makefile to configure and build the 653 kernel (core OS)

# CPU = PPC604
BSP=wrSbc8560

include $(WIND_BASE)/target/config/make/Makefile.vars

all: config compile

config:
	# Create the project.
	prjCreate -domtype kernel -prj demo/ -bsp $(BSP) -name coreOS

	# Additional components.
ifdef DEBUG
	prj domComponentBundleAdd -prj demo/ $(WIND_BASE)/target/config/comps/vxWorks/sysTemplates/vxKernel/targetTools.ddf
	# A protocol is needed to load the symbol table.
#	prj domComponentAdd -prj demo/ INCLUDE_MAL_HTTP
endif

	# Create the linker script.
	cp $(WIND_BASE)/target/config/$(BSP)/wrSbc8560.xml demo/bsp.xml
ifeq ($(notdir $(WIND_BASE)),vxworks653-2.3)
	prj projBuildTagValueSet -prj demo/ BLACKBOX_XML_FILE ../bsp.xml
else
	xmlgen --ldScript --arch $(TOOLARCH) -o demo/coreOS.lds demo/bsp.xml
endif

	# Finish the configuration (add dependencies).
	make -C demo ADD_NEEDED

# Build the core OS
compile:
	make -C demo
	make -C demo payloadObjs

clean:
	rm -rf demo
