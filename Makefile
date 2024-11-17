
CC=cc
ASM=nasm
LD=ld

CFLAGS= -Wall -g -O3 -fPIC
RM=rm -f
DBG=gdb

QEMU_VERSION=v9.1.1

DEBUG ?= 1

ifeq ($(DEBUG), 1)
    CFLAGS += -DDEBUG='1'
endif

ifdef API_VER
CFLAGS += -DCOMPATIBILITY_VERSION=$(API_VER)
endif

ROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

TARGET_BIN=coverage
TARGET=test-targets/$(TARGET_BIN)
TARGET_FULLPATH:=$(ROOT_DIR)/$(TARGET)

FS_RUN=test-targets/fs-c

QEMUPATH=$(ROOT_DIR)/qemu
AFLPATH=$(ROOT_DIR)/AFLplusplus
CFLAGS += -I$(QEMUPATH)/include/qemu
CFLAGS += $(shell pkg-config --cflags glib-2.0)
CFLAGS += $(shell pkg-config --cflags capstone)

plugin.so: CFLAGS += -Werror

.SECONDARY:
.PHONY: clean

# plugin so
# then no dots
clean:
	find ./ -type f -name "*.o" -delete
	find ./ -type f -name "*.so" -delete
	# Delete all executables
	find ./test-targets -type f ! -name "*.*" -delete
	# Delete all object files
	find ./test-targets -type f -name "*.o" -delete
	rm cmplog-bootstrapper || true
	rm -rf fuzz || true

# Compile plugin
plugin.so: plugin.o afl.o fs.o
	$(LINK.c) $(CFLAGS) -shared $^ -o $@

cmplog.so: cmplog.o afl.o fs.o afl-cmplog.o disas.o
	$(LINK.c) -shared $^ -o $@
cmplog.o: afl.o fs.o disas.o afl-cmplog.o
	$(CC) -DCMPLOG='1' $(CFLAGS) plugin.c $^ -c -o $@
cmplog-bootstrapper: cmplog-bootstrapper.c cmplog.so
	$(CC) -g -O3 -DQEMU='"$(QEMUPATH)/build/qemu-x86_64"' -DQEMU_PLUGIN='"$(ROOT_DIR)/cmplog.so"' -DTARGET='"$(TARGET_FULLPATH)"' -o $@ cmplog-bootstrapper.c

test-targets/coverage:
	$(CC) -DTARGET='"$(TARGET_FULLPATH)"' -O3 -g -nostdlib -o $@ test-targets/coverage.c -fno-stack-protector

test-targets/coverage-long:
	$(CC) -DTARGET='"$(TARGET_FULLPATH)"' -g -nostdlib -o $@ test-targets/coverage-long.c -fno-stack-protector

test-targets/dead:
	$(CC) -DTARGET='"$(TARGET_FULLPATH)"' -O3 -g -nostdlib -o $@ test-targets/dead.c

# test-targets
# $(ASM) -f elf64 -DTARGET='"./$(TARGET)"' -o $@.o test-targets/$*.asm;
test-targets/%:
	@if [ -f test-targets/$*.c ]; then \
		$(CC) $(CFLAGS) -DTARGET='"$(TARGET_FULLPATH)"' -O0 -o $@ test-targets/$*.c; \
	elif [ -f test-targets/$*.asm ]; then \
		echo "Target is $(TARGET_FULLPATH)"; \
		$(ASM) -f elf64 -DTARGET='"$(TARGET_FULLPATH)"' -o $@.o test-targets/$*.asm; \
		$(LD) -o $@ $@.o; \
	else \
		echo "No source file found for $*"; \
		exit 1; \
	fi



build-qemu:
	@echo "Building QEMU $(QEMU_VERSION)"
	@cd $(QEMUPATH) && (rm -rf build || true) && (git reset --hard || true) && (git clean -Xdf || true) && (git checkout $(QEMU_VERSION) || true) && mkdir build && cd build && ../configure --target-list=x86_64-linux-user && make -j32

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

dbg-cmplog: $(TARGET) cmplog.so
	$(DBG) --args $(QEMUPATH)/build/qemu-x86_64 -plugin ./cmplog.so ./$(TARGET)

fuzz: clean $(TARGET) plugin.so
	@rm -rf fuzz || true
	@mkdir fuzz && cd fuzz; \
	mkdir afl-in afl-out; \
	echo prout > $(ROOT_DIR)/fuzz/afl-in/seed; \
	AFL_SKIP_BIN_CHECK=1 $(AFLPATH)/afl-fuzz -i ./afl-in -o ./afl-out -- ../qemu/build/qemu-x86_64 -plugin ../plugin.so $(TARGET_FULLPATH)

fuzz-cmplog: clean $(TARGET) cmplog.so cmplog-bootstrapper
	@rm -rf fuzz || true
	@mkdir fuzz && cd fuzz; \
	mkdir afl-in afl-out; \
	echo prout > $(ROOT_DIR)/fuzz/afl-in/seed; \
	AFL_SKIP_BIN_CHECK=1 $(AFLPATH)/afl-fuzz -i ./afl-in -o ./afl-out -c ../cmplog-bootstrapper -- ../qemu/build/qemu-x86_64 -plugin ../plugin.so $(TARGET_FULLPATH)