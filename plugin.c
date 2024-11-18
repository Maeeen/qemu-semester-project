#include <qemu-plugin.h>
#include "afl.h"
#include "fs.h"

#ifdef CMPLOG
#include <glib.h>
#include "afl-cmplog.h"
#include "disas.h"
#endif

#define ENABLE_TSR
#define INLINE_COVERAGE

#ifndef COMPATIBILITY_VERSION
  #define COMPATIBILITY_VERSION QEMU_PLUGIN_VERSION
#endif
_Static_assert(COMPATIBILITY_VERSION >= 1 && COMPATIBILITY_VERSION <= 4, "Invalid COMPATIBILITY_VERSION");

#ifdef CMPLOG
  #if COMPATIBILITY_VERSION <= 2
  #error "CMPLOG requires COMPATIBILITY_VERSION >= 3, 4 is recommended."
  #endif
#endif

QEMU_PLUGIN_EXPORT int qemu_plugin_version = COMPATIBILITY_VERSION;

/// To fork only once.
static bool is_forked = 0;
static int has_started = 0;

// Enable this to enable the slower fork.
// #define FORK_AT_VCPU_INIT
// Enable this to instrument only after the first instruction of the program
// (works only without FORK_AT_VCPU_INIT)
#define INSTRUMENT_AFTER_START
// To disable the cache of disassemblies
// #define DISABLE_CMPLOG_CACHE


/// Setups callbacks to be called whenever the plugin resets after a successful
/// fork.
void setup_callbacks(qemu_plugin_id_t id);

/// Setups callbacks to be called when the plugin is loaded.
void setup_initial_callbacks(qemu_plugin_id_t id);

// ---- Inline callback ----
#ifdef INLINE_COVERAGE
#ifndef QEMU_PLUGIN_MAE_EXTENSIONS
#error "This plugin requires QEMU to be compiled with the patches provided in `qemu-proposal-patches`"
#endif
#endif

// ---- TSR related ----
#ifdef ENABLE_TSR

#ifndef QEMU_PLUGIN_MAE_EXTENSIONS
  #error "This plugin requires QEMU to be compiled with the patches provided in `qemu-proposal-patches`"
#endif

/// Translation requests
int TSR[2] = { 0 };
/// Handle TSR
void handle_tsr(qemu_plugin_id_t id) {
  uint64_t vaddr;
  while (1) {
    if (read(TSR[0], &vaddr, sizeof(uint64_t)) == -1) {
      if (errno == EAGAIN) {
        return;
      }
    }
    if (qemu_plugin_prefetch(id, vaddr)) {
      pf("Successful prefetch\n");
    }
  }
}
#endif

void inter_fork(qemu_plugin_id_t id) {
  #ifdef ENABLE_TSR
    handle_tsr(id);
  #endif
  #ifdef CMPLOG
  #ifndef DISABLE_CMPLOG_CACHE
    disas_handle_pending();
  #endif
  #endif
}

/// Starts the fork server
void plugin_fork_start(qemu_plugin_id_t id) {
  // Two conditions should be met: we are starting user code and we have not forked yet.
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
        if (fs_loop(id, inter_fork)) {
          break;
        }
      }
    } else {
      // Mainly for debugging.
      while(1) {
        #include <sys/types.h>
        #include <sys/wait.h>
        #ifdef CMPLOG
          disas_handle_pending();
        #endif
        #ifdef ENABLE_TSR
          handle_tsr(id);
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

/// on syscall, fork.
void syscall_cb(qemu_plugin_id_t id, unsigned int vcpu_index,
  int64_t num, uint64_t a1, uint64_t a2,
  uint64_t a3, uint64_t a4, uint64_t a5,
  uint64_t a6, uint64_t a7, uint64_t a8) {
  #ifndef FORK_AT_VCPU_INIT
    if (unlikely(!is_forked)) {
      plugin_fork_start(id);
    }
  #endif
}

/// On vCPU init, fork.
void vcpu_init(qemu_plugin_id_t id, unsigned int cpu_index) {
  #ifdef FORK_AT_VCPU_INIT
  has_started = 1;
  plugin_fork_start(id);
  #endif
}

/// Register the execution of a new address.
void tb_exec(uint32_t vcpu_index, void* data) {
  fs_register_exec((uint64_t) data);
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

    // Read the register
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

// static const char* hi = "hi world!\n";

// QEMU_INLINE_CALLBACK(some_callback,
//   __asm__(
//     "mov $1234567, %%rax \n\t"        // syscall: write
//     "mov $1, %%rdi \n\t"        // file descriptor: stdout
//     "sub $8, %%rsp \n\t"        // allocate space on the stack
//     "movb $'h', (%%rsp) \n\t"   // write 'h' to the stack
//     "movb $'i', 1(%%rsp) \n\t"  // write 'i' to the stack
//     "mov %%rsp, %%rsi \n\t"     // address of the string
//     "mov $2, %%rdx \n\t"        // length of the string
//     "syscall \n\t"              // invoke syscall
//     "add $8, %%rsp \n\t"        // restore stack pointer
//     "int $0x03"
//     :
//     : // No input operands
//     : "rax", "rdi", "rsi", "rdx", "memory" // Clobbered registers
//   );
// )


unsigned char callback_asm[39];

void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb) {
  #ifdef INSTRUMENT_AFTER_START
  if (unlikely(has_started == 0)) {
    if (qemu_plugin_start_code() == qemu_plugin_tb_vaddr(tb)) {
      has_started = 1;
    } else {
      return;
    }
  }
  #else
  has_started = 1;
  #endif

  // If we did not fork, there is no point to instrument the code.
  if (unlikely(!is_forked)) {
    return;
  }

  // If there is no instructions, abort
  if (unlikely(qemu_plugin_tb_n_insns(tb) == 0)) return;

  // if the void* pointer is not enough to store the hardware address
  // give up.
  _Static_assert(sizeof(void*) >= sizeof(size_t), "Invalid architecture.");

  // Virtual address of the first instruction
  uint64_t vaddr = qemu_plugin_insn_vaddr(qemu_plugin_tb_get_insn(tb, 0));
  // Register callback

  #ifdef INLINE_COVERAGE
  generate_assembly_bytes(callback_asm, vaddr);
  struct qemu_plugin_inlined_callback cb = ((struct qemu_plugin_inlined_callback) {
    .start = callback_asm,
    .end = &((char*) callback_asm)[39]
  });
  qemu_plugin_register_vcpu_tb_exec_cb_inlined(tb,cb);
  return;
  #else
  qemu_plugin_register_vcpu_tb_exec_cb(tb, tb_exec, QEMU_PLUGIN_CB_NO_REGS, (void*) vaddr);
  #endif

  #ifdef ENABLE_TSR
    // Forward to parent the request
    write(TSR[1], &vaddr, sizeof(uint64_t));
  #endif

  // In case of cmplog
  #ifdef CMPLOG
    size_t n_insn = qemu_plugin_tb_n_insns(tb);
    for(size_t i = 0; i < n_insn; i++) {
      struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);

      // Getting the binary instruction
      #if COMPATIBILITY_VERSION >= 3
      // Version ≥3, the data is copied to the buffer.
      unsigned char data[16];
      size_t data_sz = qemu_plugin_insn_data(insn, data, 16);
      #else
      // Version ≤2, a pointer is given.
      unsigned char data = qemu_plugin_insn_data(insn);
      size_t data_sz = qemu_plugin_insn_size(insn);
      #endif


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

/// Setups callbacks to be called whenever the plugin resets after a successful
/// fork.
void setup_callbacks(qemu_plugin_id_t id) {
  qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
}

/// Setups callbacks to be called when the plugin is loaded.
void setup_initial_callbacks(qemu_plugin_id_t id) {
  setup_callbacks(id);
  #ifndef FORK_AT_VCPU_INIT
  qemu_plugin_register_vcpu_syscall_cb(id, syscall_cb);
  #else
  qemu_plugin_register_vcpu_init_cb(id, vcpu_init);
  #endif
}

#ifdef ENABLE_TSR
void tsr_setup() {
  if (pipe(TSR)) {
    pf("Error creating disassembly pipe.\n");
    return;
  }
  if (fcntl(TSR[0], F_SETFL, O_NONBLOCK)) {
    pf("Error setting non-blocking mode on disassembly pipe.\n");
    return;
  }
  if (fcntl(TSR[1], F_SETFL, O_NONBLOCK)) {
    pf("Error setting non-blocking mode on disassembly pipe.\n");
    return;
  }
}
#endif

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info, int argc, char **argv) {
  #ifdef CMPLOG
    if (disas_init(info->target_name)) {
      pf("Error initializing the disassembler.");
      exit(-1);
    }
  #endif

  #ifdef ENABLE_TSR
    tsr_setup();
  #endif

  setup_initial_callbacks(id);
  return 0;
}