#include "insns.h"
#include <capstone.h>

const char* get_insn_class_str(u16 class_id) {
  return insn_group[class_id];
}

const InsnDetails* lookup(void* opcode, size_t size) {
  for(size_t i = 0; i < sizeof(lookupTable) / sizeof(InsnDetails); i++) {
    u64 opcode_ = 0;
    for(size_t j = 0; j < size; j++) {
      opcode_ |= ((u64)((u8*)opcode)[j]) << (j * 8);
    }
    if ((opcode_ & lookupTable[i].mask) == lookupTable[i].opcode) {
      return lookupTable + i;
    }
  }
  return NULL;
}