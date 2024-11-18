#pragma once

#include <qemu-plugin.h>
#include "types.h"

/// Registers an execution at a given address.
void fs_register_exec(uint64_t address);

/// Initializes the shared memory for the coverage map.
int fs_init();
/// Performs the handshake with AFL.
int fs_handshake();
/// Main loop for the fuzzer.
int fs_loop(qemu_plugin_id_t id, void(*possible_inter)(qemu_plugin_id_t));