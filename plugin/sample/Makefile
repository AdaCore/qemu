
PLUGIN_NAME=dummy_plugin
PLUGIN=dummy_plugin.so
SRC=dummy_plugin.c
QEMU_INCLUDE_DIR=../../
LD_FLAGS=
C_FLAGS=-Wall

#####

C_FLAGS+=-Ihelper -I$(QEMU_INCLUDE_DIR) -DPLUGIN_NAME=\"$(PLUGIN_NAME)\"
SRC+=helper/init.c helper/emulator.c
OBJS=$(SRC:.c=.o)
DEPS=$(SRC:.c=.d)
all: $(PLUGIN)

$(PLUGIN): $(OBJS)
	$(CC) -shared $(LD_FLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) -fPIC -MD $(C_FLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(DEPS)

distclean: clean
	rm -f $(PLUGIN)

-include $(DEPS)
