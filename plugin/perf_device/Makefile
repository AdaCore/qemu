
MODE?=gnatbus

DEVICE_NAME=perf_device

SRC=perf_device.c
QEMU_INCLUDE_DIR=../../
LD_FLAGS=-lpthread
C_FLAGS=-Wall

ifeq ($(MODE), plugin)
TARGET=perf_device.so
else
TARGET=perf_device.exe
SRC+=init.c
endif

#####
HELPER_DIR=../$(MODE)/helper
C_FLAGS+= -I$(QEMU_INCLUDE_DIR) -DDEVICE_NAME=\"$(DEVICE_NAME)\"

include $(HELPER_DIR)/Makefile.rules

clean:
	rm -f $(OBJS) $(DEPS)

distclean: clean
	rm -f $(TARGET)

-include $(DEPS)
