#pragma once

#include "types.h"

/// Registers an execution at a given address.
void fs_register_exec(size_t address);

/// Initializes the shared memory for the coverage map.
int fs_init();
/// Performs the handshake with AFL.
int fs_handshake();
/// Main loop for the fuzzer.
int fs_loop();