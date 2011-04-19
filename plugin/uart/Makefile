
MODE?=gnatbus

DEVICE_NAME=simple_uart

SRC=uart.c
QEMU_INCLUDE_DIR=../../
LD_FLAGS=-lpthread
C_FLAGS=-Wall

ifeq ($(MODE), plugin)
TARGET=plugin_uart.so
else
TARGET=gnatbus_uart.exe
SRC+=init.c
endif


#####
HELPER_DIR=../$(MODE)/helper
C_FLAGS+= -I$(QEMU_INCLUDE_DIR)

include $(HELPER_DIR)/Makefile.rules

clean:
	rm -f $(OBJS) $(DEPS)

distclean: clean
	rm -f $(TARGET)

-include $(DEPS)
