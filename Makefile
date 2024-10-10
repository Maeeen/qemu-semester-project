
CC=cc
ASM=nasm
LD=ld

CFLAGS= -Wall -g -O -fPIC
RM=rm -f
DBG=gdb

ROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

TARGET_BIN=hello-asm
TARGET=targets/$(TARGET_BIN)
TARGET_FULLPATH:=$(ROOT_DIR)/$(TARGET)

FS_RUN=targets/fs-c

QEMUPATH=$(ROOT_DIR)/qemu
CFLAGS += -I$(QEMUPATH)/include/qemu
CFLAGS += $(shell pkg-config --cflags glib-2.0)
CFLAGS += $(shell pkg-config --cflags capstone)

plugin.so: CFLAGS += -Werror

.SECONDARY:
.PHONY: all clean

all: plugin.so $(TARGET) $(FS_RUN)

# plugin so
# then no dots
clean:
	find ./ -type f -name "*.o" -delete
	find ./ -type f -name "*.so" -delete
	# Delete all executables
	find ./targets -type f ! -name "*.*" -delete
	# Delete all object files
	find ./targets -type f -name "*.o" -delete
	rm cmplog-bootstrapper

# Compile plugin
plugin.so: plugin.o afl.o fs.o
	$(LINK.c) -shared $^ -o $@

cmplog.so: cmplog.o afl.o fs.o afl-cmplog.o disas.o
	$(LINK.c) -shared $^ -o $@
cmplog.o: afl.o fs.o disas.o afl-cmplog.o
	$(CC) -DCMPLOG='1' $(CFLAGS) plugin.c $^ -c -o $@
cmplog-bootstrapper: cmplog-bootstrapper.c cmplog.so
	$(CC) -g -O3 -DQEMU='"$(QEMUPATH)/build/qemu-x86_64"' -DQEMU_PLUGIN='"$(ROOT_DIR)/cmplog.so"' -DTARGET='"$(TARGET_FULLPATH)"' -o $@ cmplog-bootstrapper.c

targets/coverage:
	$(CC) -DTARGET='"$(TARGET_FULLPATH)"' -O3 -g -nostdlib -o $@ targets/coverage.c -fno-stack-protector

targets/dead:
	$(CC) -DTARGET='"$(TARGET_FULLPATH)"' -O3 -g -nostdlib -o $@ targets/dead.c

# Targets
# $(ASM) -f elf64 -DTARGET='"./$(TARGET)"' -o $@.o targets/$*.asm;
targets/%:
	@if [ -f targets/$*.c ]; then \
		$(CC) $(CFLAGS) -DTARGET='"$(TARGET_FULLPATH)"' -O0 -o $@ targets/$*.c; \
	elif [ -f targets/$*.asm ]; then \
		echo "Target is $(TARGET_FULLPATH)"; \
		$(ASM) -f elf64 -DTARGET='"$(TARGET_FULLPATH)"' -o $@.o targets/$*.asm; \
		$(LD) -o $@ $@.o; \
	else \
		echo "No source file found for $*"; \
		exit 1; \
	fi

run: $(TARGET) plugin.so
	$(QEMUPATH)/build/qemu-x86_64 -plugin ./plugin.so ./$(TARGET)

run-cmplog: $(TARGET) cmplog.so
	$(QEMUPATH)/build/qemu-x86_64 -plugin ./cmplog.so ./$(TARGET)

run-fs: $(TARGET) $(FS_RUN) plugin.so
	$(QEMUPATH)/build/qemu-x86_64 -plugin ./plugin.so ./$(FS_RUN)

run-fs-nnqem: $(TARGET) $(FS_RUN)
	./$(FS_RUN)

debug: $(TARGET) plugin.so
	$(DBG) --args $(QEMUPATH)/build/qemu-x86_64 -plugin ./plugin.so ./$(TARGET)

debug-fs: $(TARGET) $(FS_RUN) plugin.so
	$(DBG) --args $(QEMUPATH)/build/qemu-x86_64 -plugin ./plugin.so ./$(FS_RUN)