DIR=../dist
GNATMAKE=powerpc-vxworksae-gnatmake

hello.flash: hello.bin $(DIR)/sys.bin
	cat $^ > $@

hello.bin: hello.out
	$(MAKE) -f $(DIR)/Makefile.app APP=hello DIR=$(DIR) APP_OBJ=$<


hello.out: force
	$(GNATMAKE) hello

force:

clean:
	$(RM) -f *.o *.ali *.out *.flash *.sm *.bin
