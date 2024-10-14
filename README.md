# Implementing coverage on QEMU with plugins

For convenience, AFLplusplus and QEMU are submodules. If not used as submodules,
the initialization of submodules can be ignored.

## Project layout

- `AFLplusplus/`: AFLplusplus submodule
- `qemu/`: QEMU submodule
- `Makefile`: self-explanatory
- `targets/`: target programs
- `afl.c/afl.h/afl.o`: used for communication with AFL
- `fs.c/fs.h/fs.o`: used for spinning up the forkserver
- `plugin.c/plugin.so`: the main plugin

About *cmp*-log:

- `afl-cmplog.c/afl-cmplog.h/afl-cmplog.o`: AFL protocol for cmplog
- `disas.c/disas.h/disas.o`: disassembler for cmplog

## Implementation

### Fork server

The fork server is initialized at the initialization of a CPU. The fork server
then waits for a message from AFL to start the target program. Why the fork
server is started at the initialization of a CPU you might ask: it is because
it is the only way to `fork` a process in QEMU from a plugin.

### Coverage

The plugin is implemented in `plugin.c`. For every basic block executed (a.k.a.
translation blocks in QEMU's terminology), the plugin will mark the block as
executed and send the coverage information to AFL.

### `cmplog`

This is a bit more complicated. The plugin will disassemble the basic block and
check what registers, memory and constants are used as operands. On memory access 
or execution, the plugin will log the operands and the result of the operation.

For speed, the plugin will also cache disassemblies of instructions.

Currently, the link with AFL has not been tested, but the plugin is able to
log the operands and the result of the operation.

`cmplog.so` is built with the `plugin.c` file with the `CMPLOG` preprocessor 
variable defined.

## Building

Requirements:
- `clang` or `gcc`
- `make`
- `nasm`
- `ld`
- `glib-2.0`
- `libcapstone` (for `cmplog`)

Most of those are requirements from QEMU itself.

Once built, the plugin can be loaded with the `-plugin` option of QEMU. With AFL++:

```
make plugin.so
AFL_SKIP_BIN_CHECK=1 afl-fuzz -i afl-in -o afl-out -- qemu-x86_64 -plugin ./plugin.so <target>
```

As well, you can use the `cmplog` feature:

```
make plugin.so cmplog.so
make cmplog-bootstrapper QEMUPATH=<qemu-path> TARGET_FULLPATH=<target>
AFL_SKIP_BIN_CHECK=1 afl-fuzz -i afl-in -o afl-out -c ./cmplog-bootstrapper -- qemu-x86_64 -plugin ./plugin.so <target>
```

Note the need for a bootstrapper for `cmplog`. This is because `-c` flag for
AFL++ takes only a single argument, and the bootstrapper is used to pass the
plugin path and target path to QEMU.

### Building arguments/targets

- `DEBUG=0`: disable debug messages
- `TARGET_BIN=<target>`: specify the target binary in `targets/`

- `cmplog.so`: build the `cmplog` plugin
- `cmplog-bootstrapper`: build the bootstrapper for `cmplog`
- `plugin.so`: build the main plugin
- `clean`: clean the build directory

More for debugging purposes:
- `run`: run the target binary with the plugin
- `run-cmplog`: run the target binary with the `cmplog` plugin
- `dbg`: run the target binary with the plugin in `gdb`
- `dbg-cmplog`: run the target binary with the `cmplog` plugin in `gdb`