# Create the application framework.

all: create build install check

create:
	# Create the application directory.
	mkdir -p demo
	cp hello.c demo

	# Copy source files to the dist directory.
	cp Makefile.app ../dist
	cp app.xml ../dist
	cp appwrapper.c ../dist

# Build vxMain.o and app.lds
build:
	make -C demo -f ../../dist/Makefile.app POS=$(POS) vxMain.o app.lds

install:
	cp demo/vxMain.o demo/app.lds ../dist

# Build the application (Test)
check:
	make -C demo -f ../../dist/Makefile.app APP=hello DIR=../../dist APP_OBJ=hello.o

clean:
	rm -rf demo

