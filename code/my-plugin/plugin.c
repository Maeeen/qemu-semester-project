#include <qemu-plugin.h>
#include <stdio.h>
#include <stdlib.h>
#include <x86intrin.h>
#include "types.h"
#include "insns.h"

#define DEBUG

#ifdef DEBUG
#define pf(fmt, ...) printf("[Plugin] " fmt, ##__VA_ARGS__)
#define pp(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define pf(fmt, ...)
#define pp(fmt, ...)
#endif

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

u8 hash(size_t address) {
  return (u8) (address % 256);
}

struct InstructionData {
  const InsnDetails* details;
  const char* disassembly;
  size_t address;
};

size_t counts[G_MAXUINT8] = { 0 };
struct qemu_plugin_scoreboard* scoreboard;

static void vcpu_init(qemu_plugin_id_t id, unsigned int cpu_index) {
  pf("Init vcpu %u\n", cpu_index);
  size_t* counts = qemu_plugin_scoreboard_find(scoreboard, cpu_index);
  for(size_t i = 0; i < G_MAXUINT8; i++) {
    if (counts[i] > 0)
      counts[i] = 0;
  }
}

static void vcpu_exit(qemu_plugin_id_t id, unsigned int cpu_index) {
  pf("Exit vcpu. Printing stats: %u\n", cpu_index);
}

static void plugin_atexit(qemu_plugin_id_t id, void *p) {
  printf("Plugin exited. Showing counts:\n");

  printf("Scoreboard:");

  if (scoreboard) {
    size_t* counts = qemu_plugin_scoreboard_find(scoreboard, 0);
    for(size_t i = 0; i < G_MAXUINT8; i++) {
      if (counts[i] > 0)
        printf("%zx: %lu\n", i, counts[i]);
    }

    qemu_plugin_scoreboard_free(scoreboard);
    scoreboard = NULL;
  }

  printf("Shared mem");
  for(size_t i = 0; i < G_MAXUINT8; i++) {
    if (counts[i] > 0)
      printf("%zx: %lu\n", i, counts[i]);
  }
}

static void insn_exec_cb(u32 vcpu_index, void* data) {
  struct InstructionData* cb_data = (struct InstructionData*)data;
  pf("ex: vcpu %u: %s", vcpu_index, cb_data->disassembly);
  if (cb_data->details) {
    pp(" (%s, %s)", cb_data->details->class, cb_data->details->description);
  }

  if (cb_data->details)
    counts[hash(cb_data->address)]++;
  else
    pp("Unknown instruction");
  
  pp("\n");
}

static void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb) {
  size_t nb_insn = qemu_plugin_tb_n_insns(tb);
  // size_t address = qemu_plugin_tb_vaddr(tb);
  for(size_t i = 0; i < nb_insn; i++) {
    struct qemu_plugin_insn* insn = qemu_plugin_tb_get_insn(tb, i);
    const char* disas = qemu_plugin_insn_disas(insn);
  }

  /*for(size_t i = 0; i < nb_insn; i++) {
    struct qemu_plugin_insn* insn = qemu_plugin_tb_get_insn(tb, i);
    size_t size = qemu_plugin_insn_size(insn);
    size_t address = qemu_plugin_insn_vaddr(insn);
    unsigned char opcode[size];
    size = qemu_plugin_insn_data(insn, opcode, size);

    struct InstructionData *cb_data = malloc(sizeof(struct InstructionData));
    cb_data->details = lookup((void*) opcode, size);
    cb_data->disassembly = qemu_plugin_insn_disas(insn);
    cb_data->address = address;

    pf("Translating: Address: %08x, size %zu, disas: %s, insn: ", address, size, cb_data->disassembly);
    for(size_t j = 0; j < size; j++) {
      pp("%02x ", opcode[j]);
    }
    pp("\n");
    qemu_plugin_register_vcpu_insn_exec_cb(insn, insn_exec_cb, QEMU_PLUGIN_CB_NO_REGS, cb_data);
 
    qemu_plugin_register_vcpu_insn_exec_inline_per_vcpu(insn, QEMU_PLUGIN_INLINE_ADD_U64, (qemu_plugin_u64) {scoreboard, hash(address) * 8}, 1);
    // address += size;
  }*/
}

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info, int argc, char **argv) {
  printf("Loaded plugin: Id %" PRIu64 ", running arch %s\n", id, info->target_name);
  scoreboard = qemu_plugin_scoreboard_new(sizeof(u8) * sizeof(size_t));
  qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
  qemu_plugin_register_vcpu_init_cb(id, vcpu_init);
  qemu_plugin_register_vcpu_exit_cb(id, vcpu_exit);
  qemu_plugin_register_atexit_cb(id, plugin_atexit, NULL);
  return 0;
}