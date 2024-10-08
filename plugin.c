#include <qemu-plugin.h>
#include "afl.h"
#include "fs.h"

static bool is_forked = 0;
void vcpu_init(qemu_plugin_id_t id, unsigned int cpu_index) {
  if (!is_forked) {
    is_forked = 1;
    if (unlikely(fs_init())) {
      pf("Error initializing the fork server.");
      exit(-1);
    }
    if (likely(afl_is_here())) {
      if (unlikely(fs_handshake())) {
        pf("Error handshake.");
        exit(-1);
      }
      fs_loop();
    }
  }
}

void tb_exec(uint32_t vcpu_index, void* data) {
  fs_register_exec((size_t) data);
}

void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb) {
  if (unlikely(qemu_plugin_tb_n_insns(tb) == 0)) return;
  // if the void* pointer is not enough to store the hardware address
  // give up.
  _Static_assert(sizeof(void*) >= sizeof(size_t), "Invalid architecture.");
  size_t haddr = (size_t) qemu_plugin_insn_haddr(qemu_plugin_tb_get_insn(tb, 0));
  qemu_plugin_register_vcpu_tb_exec_cb(tb, tb_exec, QEMU_PLUGIN_CB_NO_REGS, (void*) haddr);
}

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info, int argc, char **argv) {
  qemu_plugin_register_vcpu_init_cb(id, vcpu_init);
  qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
  return 0;
}