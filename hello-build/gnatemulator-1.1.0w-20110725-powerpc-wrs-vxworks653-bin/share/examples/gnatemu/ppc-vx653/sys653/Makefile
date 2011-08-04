# By default, use full
POS=full
# POS=cert

# By default, we don't use debug version of health monitor.
# DEBUG=1

export WIND_BASE:=$(subst \,/,$(WIND_BASE))

all: all-base all-app

all-base:
	make -C mos DEBUG=$(DEBUG)
	make -C pos-full
	make -C pos-cert

all-app:
	make -C app POS=$(POS)
	make -C int DEBUG=$(DEBUG) POS=$(POS)

# Create a partial image from the demo image, that can be reused to quickly
# build a system from an application.
# This is not officially supported.
install:
	make -C int DEBUG=$(DEBUG) POS=$(POS) install

clean-base:
	make -C mos clean
	make -C pos-full clean
	make -C pos-cert clean

clean-app:
	make -C app clean
	make -C int clean

clean: clean-base clean-app
	-rm -rf dist/*

run:
	qemu-system-ppc -M prep -nographic -cpu 750gx -L int/demo-$(POS) -bios hello.flash -boot n -no-reboot -redir udp:17185:10.0.2.1:17185

.PHONY: all all-base all-app clean run
