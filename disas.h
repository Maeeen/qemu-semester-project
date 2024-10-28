#pragma once

#include "types.h"

struct disas_op_reg {
  char name[16];
  char idx;
};


struct disas_op_memory {

};

struct disas_op_const {
  u64 value;
};

enum disas_op_source_case {
  UNKNOWN = 0,
  REGISTER,
  MEMORY,
  CONSTANT
};

struct disas_op_source {
  enum disas_op_source_case tpe;
  size_t size; // size in bits of the operand, if unknown, set to zero.
  union {
    struct disas_op_reg reg;
    struct disas_op_memory mem;
    struct disas_op_const c;
  };
};

struct disas_insn_operands {
  char mnemonic[16];
  char mem_accesses; // number of memory accesses required for that instruction
  char should_instrument; // is 1 if the instruction is a cmp or test
  size_t nb_effective; // number of effective bits compared
  struct disas_op_source reg0;
  struct disas_op_source reg1;
};

// Initializes the disassembler
int disas_init(const char* arch);
// Declares a register that will be used in QEMU. The name should match
// with capstone register names.
void disas_declare_reg(const char* name, u64 idx);
// Returns operands
struct disas_insn_operands get_operands(void* iaddr, size_t isz);
/// Returns 0 if successfuly handled one pending instructions. 1 otherwise.
int disas_handle_pending_one();
/// Handles all pending instructions
void disas_handle_pending();