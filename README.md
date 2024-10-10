# Implementing coverage on QEMU with plugins

For convenience, AFLplusplus and QEMU are submodules. If not used as submodules,
the initialization of submodules can be ignored.

## Project layout

- `AFLplusplus/`: AFLplusplus submodule
- `qemu/`: QEMU submodule
- `Makefile`: self-explanatory
- `afl.c/afl.h/afl.o`: used for communication with AFL
- `fs.c/fs.h/fs.o`: used for spinning up the forkserver
- `plugin.c/plugin.so`: the main plugin

About *comp*-log