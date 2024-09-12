
CC=cc
ASM=nasm
LD=ld

CFLAGS= -Wall -g -O -fPIC
RM=rm -f
DBG=gdb

TARGET_BIN=hello-asm
TARGET=targets/$(TARGET_BIN)

QEMUPATH=/home/ubuntu/qemu
CFLAGS += -I$(QEMUPATH)/include/qemu
CFLAGS += $(shell pkg-config --cflags glib-2.0)
CFLAGS += $(shell pkg-config --cflags capstone)

.SECONDARY:
.PHONY: all clean

all: plugin.so $(TARGET)

# plugin so
# then no dots
clean:
	find ./ -type f -name "*.o" -delete
	find ./ -type f -name "*.so" -delete
	# Delete all executables
	find ./targets -type f ! -name "*.*" -delete
	# Delete all object files
	find ./targets -type f -name "*.o" -delete

# Compile plugin
plugin.so: plugin.o insns.o
	$(LINK.c) -shared $^ -o $@

# Targets
targets/%:
	@if [ -f targets/$*.c ]; then \
		$(CC) $(CFLAGS) -O0 -o $@ targets/$*.c; \
	elif [ -f targets/$*.asm ]; then \
		$(ASM) -f elf64 -o $@.o targets/$*.asm; \
		$(LD) -o $@ $@.o; \
	else \
		echo "No source file found for $*"; \
		exit 1; \
	fi

run: $(TARGET) plugin.so
	$(QEMUPATH)/build/qemu-x86_64 -plugin ./plugin.so ./$(TARGET)

debug: $(TARGET) plugin.so
	$(DBG) --args $(QEMUPATH)/build/qemu-x86_64 -plugin ./plugin.so ./$(TARGET)