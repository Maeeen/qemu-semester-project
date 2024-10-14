#include "disas.h"
#include <glib.h>
#include <fcntl.h>
#include <capstone.h>

int fd[2] = { 0 };

#define MAX_LENGTH_INSTRUCTION 16

void print_array(uint8_t* array, size_t size) {
  for(size_t i = 0; i < size; i++) {
    pp("%02x ", array[i]);
  }
  pp("\n");
}

guint uint8_array_hash(gconstpointer key) {
  const uint8_t *array = key;
  guint hash = 0;

  for (gint i = 0; i < MAX_LENGTH_INSTRUCTION; i++) {
      hash = hash * 31 + array[i];  // Example: simple hash
  }

  return hash;
}

// Custom equality function for uint8_t arrays
gboolean uint8_array_equal(gconstpointer a, gconstpointer b) {
  const uint8_t *array_a = a;
  const uint8_t *array_b = b;

  return memcmp(array_a, array_b, MAX_LENGTH_INSTRUCTION) == 0;
}

GHashTable* register_map = NULL;
GHashTable* disas_map = NULL;

u64 get_register_idx(const char* name) {
  if (register_map != NULL) {
    gpointer idx = g_hash_table_lookup(register_map, name);
    if (idx != NULL) {
      return GPOINTER_TO_INT(idx) - 1;
    }
  }
  return (u64) -1;
}

void disas_declare_reg(const char* name, u64 idx) {
  if (register_map == NULL) {
    // TODO: free it
    register_map = g_hash_table_new(g_str_hash, g_str_equal);
  }
  g_hash_table_insert(register_map, g_strdup(name), GINT_TO_POINTER(idx + 1)); // nasty +1 trick to check if register is actually here
}

csh handle;

void add_operand(struct disas_insn_operands* op, struct disas_op_source* src) {
  if (op->reg0.tpe == UNKNOWN) {
    op->reg0 = *src;
    if (src->tpe == MEMORY) {
      op->mem_accesses++;
    }
  } else if (op->reg1.tpe == UNKNOWN) {
    op->reg1 = *src;
    if (src->tpe == MEMORY) {
      op->mem_accesses++;
    }
  }
  op->nb_effective = MAX(src->size, op->nb_effective);
}

struct disas_insn_operands get_operands(void* iaddr, size_t isz) {
  char insn_padded[MAX_LENGTH_INSTRUCTION] = { 0 };
  memcpy(insn_padded, iaddr, isz); // TODO: isz should be less than 16
  if (disas_map && g_hash_table_contains(disas_map, g_memdup(insn_padded, MAX_LENGTH_INSTRUCTION))) {
    return *(struct disas_insn_operands*) g_hash_table_lookup(disas_map, insn_padded);
  }

  cs_insn* insns;
  size_t count = cs_disasm(handle, (const uint8_t*) iaddr, isz, 0x1000, 0, &insns);
  struct disas_insn_operands op = { 0 };

  // Signal to parent that we have disassembled this instruction
  write(fd[1], &isz, sizeof(size_t));
  write(fd[1], iaddr, isz);

  // Let's actually query which register it read
  if (count <= 0) {
    // Could not disassemble the instruction
    return op;
  }

  if (count > 1) {
    pf("Warning: capstone disassembled more than one instruction.\n");
    return op;
  }
  
  cs_insn insn = insns[0];

  memcpy(op.mnemonic, insn.mnemonic, 16);

  if (!strstr(insn.mnemonic, "cmp") && !strstr(insn.mnemonic, "test")) {
    return op;
  }

  op.should_instrument = true;
  // pf("disas %s\n", insn.mnemonic);

  for(size_t i = 0; i < 8; i++) {
    if (insn.detail->x86.operands[i].type == X86_OP_REG) {
      struct disas_op_source src = { 0 };
      src.tpe = REGISTER;
      src.size = insn.detail->x86.operands[i].size * 8;
      // pf("Register: %s\n", cs_reg_name(handle, insn.detail->x86.operands[i].reg));
      src.reg.idx = get_register_idx(cs_reg_name(handle, insn.detail->x86.operands[i].reg));
      strncpy(src.reg.name, cs_reg_name(handle, insn.detail->x86.operands[i].reg), 16);
      add_operand(&op, &src);
    } else if (insn.detail->x86.operands[i].type == X86_OP_IMM) {
      struct disas_op_source src = { 0 };
      src.tpe = CONSTANT;
      src.size = insn.detail->x86.operands[i].size * 8;
      src.c.value = insn.detail->x86.operands[i].imm;
      add_operand(&op, &src);
      // pf("Immediate: %lu\n", insn.detail->x86.operands[i].imm);
    } else if (insn.detail->x86.operands[i].type == X86_OP_MEM) {
      struct disas_op_source src = { 0 };
      src.tpe = MEMORY;
      src.size = insn.detail->x86.operands[i].size * 8;
      add_operand(&op, &src);
      // pf("Memory\n");
    }
  }
  return op;
}

void disas_setup_pipe() {
  disas_map = g_hash_table_new(uint8_array_hash, uint8_array_equal);
  if (pipe(fd)) {
    pf("Error creating disassembly pipe.\n");
    return;
  }
  if (fcntl(fd[0], F_SETFL, O_NONBLOCK)) {
    pf("Error setting non-blocking mode on disassembly pipe.\n");
    return;
  }
  if (fcntl(fd[1], F_SETFL, O_NONBLOCK)) {
    pf("Error setting non-blocking mode on disassembly pipe.\n");
    return;
  }
}

int disas_init(const char* arch) {
  disas_setup_pipe();

  if (strcmp(arch, "x86_64") == 0) {
    if (cs_open(CS_ARCH_X86, CS_MODE_64, &handle) != CS_ERR_OK) {
      pf("Could not initialize capstone. Aborting.");
      exit(-1);
    }
    cs_option(handle, CS_OPT_DETAIL, CS_OPT_ON);
  } else {
    pf("Unsupported architecture: %s\n", arch);
    exit(-1); // Fatal error…
    return -1;
  }
}

void disas_handle_pending() {
  pf("Handling pending instructions.\n");
  size_t i = 0;
  while (!disas_handle_pending_one()) {
    i++;
  }
  pf("Handled %zu instructions. Size of hash is %zu\n", i, g_hash_table_size(disas_map));
}

int disas_handle_pending_one() {
  // read from fd and disassemble
  size_t isz = 0;
  if (read(fd[0], &isz, sizeof(size_t))== -1) {
    if (errno == EAGAIN) {
      return 1;
    }
  }
  uint8_t iaddr[isz];
  read(fd[0], iaddr, isz);
  
  // TODO: free
  struct disas_insn_operands* ops_ptr = g_malloc(sizeof(struct disas_insn_operands));

  char insn_padded[MAX_LENGTH_INSTRUCTION] = { 0 };
  memcpy(insn_padded, iaddr, isz);

  *ops_ptr = get_operands(iaddr, isz);
  g_hash_table_insert(disas_map, g_memdup(insn_padded, MAX_LENGTH_INSTRUCTION), ops_ptr);

  return 0;
}