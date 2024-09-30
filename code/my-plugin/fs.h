#pragma once

#include "types.h"

void fs_register_exec(size_t address);

int fs_init();
int fs_handshake();
void fs_loop();