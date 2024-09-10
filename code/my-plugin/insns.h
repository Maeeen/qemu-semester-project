#pragma once

#include "types.h"

typedef struct {
  u64 opcode;
  u64 mask;
  u16 class_id;
  char *class;
  char *description;
} InsnDetails;

static const char* insn_group[] = {
  "Invalid", "Data Transfer", "Stack Operation", "Arithmetic", "Control Flow", "Miscellaneous", "FPU Operations", "Extended", "Extended control flow", "Unknown"
};

#define NB_CLASSES 10


// Thanks ChatGPT
// But it's shit tho…
static const InsnDetails lookupTable[] = {
    // Data Transfer Instructions (Group ID: 1)
    {0xB8, 0xFF, 1, "Data Transfer", "MOV r64, imm (MOV with immediate to 64-bit register)"},
    {0xA0, 0xFF, 1, "Data Transfer", "MOV AL, [address]"},
    {0xA1, 0xFF, 1, "Data Transfer", "MOV AX, [address]"},
    {0xA2, 0xFF, 1, "Data Transfer", "MOV [address], AL"},
    {0xA3, 0xFF, 1, "Data Transfer", "MOV [address], AX"},
    {0x48A1, 0xFFFF, 1, "Data Transfer", "MOV r64, [address] (Move memory to 64-bit register)"},
    {0x4889, 0xFFFF, 1, "Data Transfer", "MOV r/m64, r64 (Move 64-bit register to memory/register)"},
    {0x488B, 0xFFFF, 1, "Data Transfer", "MOV r64, r/m64 (Move memory/register to 64-bit register)"},

    // Stack Operations (Group ID: 2)
    {0x50, 0xFF, 2, "Stack Operation", "PUSH r64 (Push 64-bit register)"},
    {0x58, 0xFF, 2, "Stack Operation", "POP r64 (Pop 64-bit register)"},
    {0x9C, 0xFF, 2, "Stack Operation", "PUSHFQ (Push RFLAGS register)"},
    {0x9D, 0xFF, 2, "Stack Operation", "POPFQ (Pop RFLAGS register)"},

    // Arithmetic Instructions (Group ID: 3)
    {0x48FF, 0xFFFF, 3, "Arithmetic", "INCQ r64 (Increment 64-bit register)"},
    {0x48F7, 0xFFFF, 3, "Arithmetic", "IDIV r/m64 (Signed divide 64-bit)"},
    {0x4883C0, 0xFFFFFF, 3, "Arithmetic", "ADD r64, imm8 (Add immediate value to 64-bit register)"},
    {0x48F7E3, 0xFFFFFFFF, 3, "Arithmetic", "MUL r/m64 (Unsigned multiply 64-bit)"},
    {0x4883FF, 0xFFFFFF, 3, "Arithmetic", "CMP r64, imm8 (Compare 64-bit register with immediate value)"},
    {0x4829, 0xFFFF, 3, "Arithmetic", "SUB r/m64, r64 (Subtract 64-bit register from memory/register)"},
    {0x4839, 0xFFFF, 3, "Arithmetic", "CMPQ r/m64, r64 (Compare 64-bit register/memory with 64-bit register)"},
    {0x4881FF, 0xFFFFFF, 3, "Arithmetic", "CMPQ r64, imm32 (Compare 64-bit register with immediate value)"},

    // Logical Instructions (Group ID: 4)
    {0x4885, 0xFFFF, 4, "Logical", "TEST r/m64, r64 (Logical AND test for 64-bit registers)"},
    {0x4881E0, 0xFFFFFF, 4, "Logical", "AND r64, imm32 (Logical AND with immediate)"},
    {0x4881C8, 0xFFFFFF, 4, "Logical", "OR r64, imm32 (Logical OR with immediate)"},
    {0x4831, 0xFFFF, 4, "Logical", "XOR r64, r/m64 (Exclusive OR of 64-bit registers)"},

    // Control Flow Instructions (Group ID: 5)
    {0xEB, 0xFF, 5, "Control Flow", "JMP rel8 (Short jump)"},
    {0xE9, 0xFF, 5, "Control Flow", "JMP rel32 (Near jump relative)"},
    {0xFF, 0xFF, 5, "Control Flow", "JMP r/m64 (Jump to memory/register)"},
    {0x0F8D, 0xFFFF, 5, "Control Flow", "JGE rel32 (Jump if Greater or Equal)"},
    {0x0F05, 0xFFFF, 5, "Control Flow", "SYSCALL (Invoke system call)"},

    // Miscellaneous Instructions (Group ID: 6)
    {0xF4, 0xFF, 6, "Miscellaneous", "HLT (Halt the CPU)"},
    {0x90, 0xFF, 6, "Miscellaneous", "NOP (No operation)"},
    {0xF5, 0xFF, 6, "Miscellaneous", "CMC (Complement Carry Flag)"},

    // FPU Operations (Group ID: 7)
    {0xD8, 0xFF, 7, "FPU", "FADD (Add floating-point values)"},  // Base for FPU operations
    {0xD9, 0xFF, 7, "FPU", "FLD (Load floating-point value)"},
    {0xDB, 0xFF, 7, "FPU", "FSTP (Store and pop floating-point value)"},

    // SIMD Instructions (Group ID: 8)
    {0x0F10, 0xFFFF, 8, "SIMD", "MOVUPS (Move unaligned packed single-precision floating-point values)"},
    {0x0F28, 0xFFFF, 8, "SIMD", "MOVAPS (Move aligned packed single-precision floating-point values)"},
    {0x660F7E, 0xFFFFFF, 8, "SIMD", "MOVD (Move 32-bit integer to/from XMM register)"},

    // Extended Control Flow (Group ID: 9)
    {0x0F01, 0xFFFF, 9, "Extended Control Flow", "LGDT (Load Global Descriptor Table)"},
    {0x0F20, 0xFFFF, 9, "Extended Control Flow", "MOV CR0, r64 (Move to/from control register)"},

    {0, 0, 0, "Unknown", "Unknown"}
};

const InsnDetails* lookup(void* opcode, size_t size);
const char* get_insn_class_str(u16 class_id);