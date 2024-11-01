#include <qemu-plugin.h>
#include "afl.h"
#include "fs.h"

#ifdef CMPLOG
#include <glib.h>
#include "afl-cmplog.h"
#include "disas.h"
#endif

/// To fork only once.
static bool is_forked = 0;
static int has_started = 0;

/// Setups callbacks to be called whenever the plugin resets after a successful
/// fork.
void setup_callbacks(qemu_plugin_id_t id);

/// Setups callbacks to be called when the plugin is loaded.
void setup_initial_callbacks(qemu_plugin_id_t id);


void fork_start(qemu_plugin_id_t id) {
  if (!is_forked && has_started) {
    is_forked = 1;
    // qemu_plugin_reset(id, test);

    #ifdef CMPLOG
      // declare available registers for cmplog, to be able to find them back easily.
      g_autoptr(GArray) reg_list = qemu_plugin_get_registers();
      if (reg_list == NULL) {
        pf("Error getting register list.\n");
        exit(-1);
      }
      for (int i = 0; i < reg_list->len; i++) {
        qemu_plugin_reg_descriptor* reg = &g_array_index(reg_list, qemu_plugin_reg_descriptor, i);
        disas_declare_reg(reg->name, i);
      }

      if (unlikely(cmplog_init())) {
        pf("Error initializing cmplog.\n");
        exit(-1);
      }
    #endif

    if (unlikely(fs_init())) {
      pf("Error initializing the fork server.");
      exit(-1);
    }
    if (likely(afl_is_here())) {
      if (unlikely(fs_handshake())) {
        pf("Error handshake.");
        exit(-1);
      }
    }

    if (likely(afl_is_here())) {
      while(1) {
        #ifdef CMPLOG
        // between very iteration, disassemble the pending instructions
        if (fs_loop(disas_handle_pending)) {
          break;
        }
        #else
        if (fs_loop(NULL)) {
          break;
        }
        #endif

      }
    } else {
      // Mainly for debugging.
      while(1) {
        #include <sys/types.h>
        #include <sys/wait.h>
        #ifdef CMPLOG
        disas_handle_pending();
        #endif
        pf("Press a key to launch a new instance.\n");
        getchar();
        if (fork() == 0) break;
        waitpid(-1, NULL, 0);
        pf("New instance finished.\n");
      }
    }
  }
}

void syscall_cb(qemu_plugin_id_t id, unsigned int vcpu_index,
  int64_t num, uint64_t a1, uint64_t a2,
  uint64_t a3, uint64_t a4, uint64_t a5,
  uint64_t a6, uint64_t a7, uint64_t a8) {
    fork_start(id);
}

/// On vCPU init, fork.
void vcpu_init(qemu_plugin_id_t id, unsigned int cpu_index) {
  // fork_start();
}

/// Register the execution of a new address.
void tb_exec(uint32_t vcpu_index, void* data) {
  fs_register_exec((size_t) data);
}

#ifdef CMPLOG
void completed_cmp_exec(struct cmplog_cb_data* cb_data) {
  if (unlikely(!cb_data)) {
    pf("Error: no data in completed_cmp_exec.\n");
    return;
  }
  u64 v0 = 0, v1 = 0;

  // Registers
  GArray* arr = qemu_plugin_get_registers();

  // v0
  if (cb_data->ops.reg0.tpe == CONSTANT) {
    v0 = cb_data->ops.reg0.c.value;
  } else if (cb_data->ops.reg0.tpe == REGISTER) {
    // read register
    if (unlikely(cb_data->ops.reg0.reg.idx >= arr->len)) {
      // pf("Error: invalid register index.\n");
      return;
    }
    qemu_plugin_reg_descriptor* reg0 = &g_array_index(arr, qemu_plugin_reg_descriptor, cb_data->ops.reg0.reg.idx);
    GByteArray* a = g_byte_array_new();
    size_t s = qemu_plugin_read_register(reg0->handle, a);
    if (s != a->len) {
      // pf("Error reading register.\n");
      return;
    }

    size_t req_len = MIN(a->len, cb_data->ops.reg0.size >> 3);

    for (size_t i = 0; i < req_len; i++) {
      v0 |= (u64) a->data[i] << (i * 8);   
    }
  } else if (cb_data->ops.reg0.tpe == MEMORY) {
    v0 = cb_data->v0_mem;
  }

  if (cb_data->ops.reg1.tpe == CONSTANT) {
    v1 = cb_data->ops.reg1.c.value;
  } else if (cb_data->ops.reg1.tpe == REGISTER) {
    if (unlikely(cb_data->ops.reg1.reg.idx >= arr->len)) {
      // pf("Error: invalid register index.\n");
      return;
    }
    // read register
    qemu_plugin_reg_descriptor* reg1 = &g_array_index(arr, qemu_plugin_reg_descriptor, cb_data->ops.reg1.reg.idx);
    GByteArray* a = g_byte_array_new();
    size_t s = qemu_plugin_read_register(reg1->handle, a);
    if (s != a->len) {
      // pf("Error reading register.\n");
      return;
    }

    size_t req_len = MIN(a->len, cb_data->ops.reg1.size >> 3);

    for (size_t i = 0; i < req_len; i++) {
      v1 |= (u64) a->data[i] << (i * 8);
    }
  } else if (cb_data->ops.reg1.tpe == MEMORY) {
    v1 = cb_data->v1_mem;
  }
  cmplog_log(cb_data->location, v0, v1, cb_data->ops.nb_effective);
}

// This gets executed if an instruction requires memory accesses to be executed.
void insn_mem(unsigned int vcpu_index, qemu_plugin_meminfo_t info, uint64_t vaddr, void *userdata) {
  struct cmplog_cb_data* cb_data = (struct cmplog_cb_data*) userdata;
  if (unlikely(!cb_data)) {
    // pf("Error: no data in insn_mem.\n");
    return;
  }
  if (cb_data->ops.mem_accesses == 0) {
    // pf("Error: unexpected memory access.\n");
    return;
  }
  // Identifying which memory access we are doing
  // The implementation is simpler and does not handle the case where
  // an instruction requires more than one memory accesses.
  // TODO: add support for that.
  u64* where_to_write;
  if (cb_data->ops.reg0.tpe == MEMORY && cb_data->mem_accesses == 0) {
    where_to_write = &cb_data->v0_mem;
  } else if (cb_data->ops.reg1.tpe == MEMORY && cb_data->mem_accesses == 0 && cb_data->ops.reg0.tpe != MEMORY) {
    where_to_write = &cb_data->v1_mem;
  } else {
    // pf("Error: invalid memory access done=mem_accesses=%x, insn=mem_accesses=%x.\n", cb_data->mem_accesses, cb_data->ops.mem_accesses);
    return;
  }

  // read memory
  qemu_plugin_mem_value v = qemu_plugin_mem_get_value(info);
  switch (v.type) {
    case QEMU_PLUGIN_MEM_VALUE_U8:
      *where_to_write = v.data.u8;
      break;
    case QEMU_PLUGIN_MEM_VALUE_U16:
      *where_to_write = v.data.u16;
      break;
    case QEMU_PLUGIN_MEM_VALUE_U32:
      *where_to_write = v.data.u32;
      break;
    case QEMU_PLUGIN_MEM_VALUE_U64:
      *where_to_write = v.data.u64;
      break;
    default:
      // pf("Error: unexpected memory access size.\n");
      return;
  }

  cb_data->mem_accesses++;
  // if we have done all of the memory accesses required, we can log the comparison.
  // and reset the counter.
  if (cb_data->ops.mem_accesses == cb_data->mem_accesses) {
    completed_cmp_exec(cb_data);
    cb_data->v0_mem = 0;
    cb_data->v1_mem = 0;
    cb_data->mem_accesses = 0;
  }
}

// This gets executed if an instruction does not require memory accesses to be executed.
void insn_exec(unsigned int vcpu_index, void* data) {
  struct cmplog_cb_data* cb_data = (struct cmplog_cb_data*) data;
  // pf("Running insn_exec %s\n", cb_data->ops.mnemonic);
  if (unlikely(!cb_data)) {
    pf("Error: no data in insn_exec.\n");
    return;
  }
  completed_cmp_exec(cb_data);
}
#endif

void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb) {
  if (unlikely(has_started == 0)) {
    if (qemu_plugin_start_code() == qemu_plugin_tb_vaddr(tb)) {
      has_started = 1;
    } else {
      return;
    }
  }
  if (unlikely(!is_forked)) {
    return;
  }
  if (unlikely(qemu_plugin_tb_n_insns(tb) == 0)) return;
  // if the void* pointer is not enough to store the hardware address
  // give up.
  _Static_assert(sizeof(void*) >= sizeof(size_t), "Invalid architecture.");
  size_t haddr = (size_t) qemu_plugin_insn_haddr(qemu_plugin_tb_get_insn(tb, 0));
  qemu_plugin_register_vcpu_tb_exec_cb(tb, tb_exec, QEMU_PLUGIN_CB_NO_REGS, (void*) haddr);

  #ifdef CMPLOG
  size_t n_insn = qemu_plugin_tb_n_insns(tb);
  for(size_t i = 0; i < n_insn; i++) {
    struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);

    // Getting the binary instruction
    unsigned char data[16];
    size_t data_sz = qemu_plugin_insn_data(insn, data, 16);


    struct disas_insn_operands ops = get_operands(data, data_sz);
    if (ops.should_instrument) {
      struct cmplog_cb_data *cb_data = calloc(1, sizeof(struct cmplog_cb_data));
      cb_data->location = qemu_plugin_insn_vaddr(insn);
      cb_data->ops = ops;
      if (ops.mem_accesses) {
        qemu_plugin_register_vcpu_mem_cb(insn, insn_mem, QEMU_PLUGIN_CB_R_REGS, QEMU_PLUGIN_MEM_R, (void*) cb_data);
      } else {
        qemu_plugin_register_vcpu_insn_exec_cb(insn, insn_exec, QEMU_PLUGIN_CB_R_REGS, (void*) cb_data);
      }
    }
  }
  #endif
}

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

/// Setups callbacks to be called whenever the plugin resets after a successful
/// fork.
void setup_callbacks(qemu_plugin_id_t id) {
  qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
}

/// Setups callbacks to be called when the plugin is loaded.
void setup_initial_callbacks(qemu_plugin_id_t id) {
  setup_callbacks(id);
  qemu_plugin_register_vcpu_syscall_cb(id, syscall_cb);
  // qemu_plugin_register_vcpu_init_cb(id, vcpu_init);
}

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info, int argc, char **argv) {
  #ifdef CMPLOG
  if (disas_init(info->target_name)) {
    pf("Error initializing the disassembler.");
    exit(-1);
  }
  #endif
  setup_initial_callbacks(id);
  // qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
  // qemu_plugin_register_vcpu_syscall_cb(id, syscall_cb);
  return 0;
}