#include <qemu-plugin.h>
#include <stdio.h>

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;


void insn_exec(unsigned int vcpu_index, void *userdata) {
  printf("{\"type\": \"exec\", \"n_insn\": %zu},", (size_t) userdata);
}

void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb) {
  size_t n_insn = qemu_plugin_tb_n_insns(tb);
  printf("{\"type\": \"trans\", \"n_insn\": %zu},", n_insn);
  qemu_plugin_register_vcpu_tb_exec_cb(tb, insn_exec, QEMU_PLUGIN_CB_NO_REGS, (void*) n_insn);
}

void when_exit(qemu_plugin_id_t id, void *userdata) {
  printf("{}]\n");
  fflush(stdout);
}

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info, int argc, char **argv) {
  printf("[");
  qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
  qemu_plugin_register_atexit_cb(id, when_exit, NULL);
  return 0;
}

