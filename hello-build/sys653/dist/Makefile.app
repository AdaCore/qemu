# This is a generic -*- Makefile -*- to build a vx653 user partition.
#
# APP must be set to the name of the partition.  APP is also the user entry
# point (initial function)
#
# DIR is the path to the dist directory
#
# APP_OBJ is the object files list for the application.
#
# POS is the partition OS name (full or cert)

# PARTADDR defines the base address of application partition.
CPU = PPC604
PARTADDR = 0x40000000
POS=full

all: $(APP).bin

include $(WIND_BASE)/target/vThreads/config/make/Makefile.vars

# List of objects to build the user partition.
# vxMain is the vxWorks internal entry point (call constructors)
# $(APP)-wrap is the wrapper to call user entry point.
# pos-stubs.o is the stubs list to the partition-OS.
ALL_APP_OBJS=$(DIR)/vxMain.o $(APP)-wrap.o $(APP_OBJ) $(DIR)/pos-$(POS)-stubs.o

$(APP).sm: $(ALL_APP_OBJS) $(DIR)/app.lds
	$(LD) $(LDFLAGS) -T $(DIR)/app.lds -o $@ $(ALL_APP_OBJS)

APP_BIN_FLAGS=--only-section .text --change-section-address .text=0xffe00000 --only-section .rodata --change-section-address .rodata=0xffec0000 --only-section .data --change-section-address .data=0xffee0000 --remove-section .bss --pad-to 0xfff00000

# Create the binary image.
$(APP).bin: $(APP).sm
	objcopyppc -O binary $(APP_BIN_FLAGS) $< $@

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

vxMain.o: $(WIND_BASE)/target/vThreads/config/comps/src/vxMain.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(APP)-wrap.o: $(DIR)/appwrapper.c
	$(CC) $(CFLAGS) -c -DENTRY_POINT=$(APP) -o $@ $<

app.lds: ../app.xml
	xmlgen --ldScript --arch $(TOOLARCH) --address $(PARTADDR) -o $@ $<
