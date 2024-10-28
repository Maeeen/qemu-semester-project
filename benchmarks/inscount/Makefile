
CC=cc

CFLAGS= -Wall -g -O3 -fPIC

ROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

BIN=test.out

QEMUPATH=$(ROOT_DIR)/../../qemu
CFLAGS += -I$(QEMUPATH)/include/qemu
CFLAGS += $(shell pkg-config --cflags glib-2.0)

# Compile plugin
inscount.so: inscount.o
	$(LINK.c) $(CFLAGS) -shared $^ -o $@

tblog.so: tblog.o
	$(LINK.c) $(CFLAGS) -shared $^ -o $@

run: inscount.so
	$(QEMUPATH)/build/qemu-x86_64 -plugin ./inscount.so ./$(BIN)

dbg: inscount.so
	gdb --args $(QEMUPATH)/build/qemu-x86_64 -plugin ./inscount.so ./$(BIN)

clean:
	rm -f inscount.so
	rm -f inscount.o
	rm -f tblog.o
	rm -f tblog.so