#include <qemu-plugin.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <x86intrin.h>
#include <fcntl.h>
#include <bits/fcntl-linux.h>
#include <time.h>
#include "types.h"
#include "insns.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/shm.h>

/// AFL Instrumentation detail defs
#define BITMAP_ENTRY_TYPE unsigned char

#define CUSTOM_BITMAP 200

/// Globals
static int is_forked = 0;
static const char SHM_ENV_VAR[] = "__AFL_SHM_ID";
static const int FORKSRV_FD_IN = 198;
static const int FORKSRV_FD_OUT = 199;

/// Instrumentation specific
BITMAP_ENTRY_TYPE* shared_mem = 0;
unsigned short previous_block = 0x0;
pid_t fs_pid = 0;

/// Debug
extern char **environ;
void show_envvar() {
  char **s = environ;

  for (; *s; s++) {
    pp("%s\n", *s);
  }

  return 0;
}

/// AFL utils
static int is_afl_here(void) {
  return getenv(SHM_ENV_VAR) != NULL;
}
static int afl_read(uint32_t *out) {
  if (is_afl_here() == 0) {
    sleep(1);
    return 0;
  }

  ssize_t res;

  res = read(FORKSRV_FD_IN, out, 4);

  if (res == -1) {
    if (errno == EINTR) {
      fprintf(stderr, "Interrupted while waiting for AFL message, aborting.\n");
    } else {
      fprintf(stderr, "Failed to read four bytes from AFL pipe, aborting.\n");
    }
    return -1;
  } else if (res != 4) {
    return -1;
  } else {
    return 0;
  }
}
static int afl_write(uint32_t value) {
  if (is_afl_here() == 0) {
    sleep(1);
    return 0;
  }

  ssize_t res;

  res = write(FORKSRV_FD_OUT, &value, 4);

  if (res == -1) {
    if (errno == EINTR) {
      fprintf(stderr, "Interrupted while sending message to AFL, aborting.\n");
    } else {
      fprintf(stderr, "Failed to write four bytes to AFL pipe, aborting.\n");
    }
    return -1;
  } else if (res != 4) {
    return -1;
  } else {
    return 0;
  }
}

/// Setup shared memory
void setup_shmem() {
  if (!is_afl_here()) {
    int shmid = shmget(IPC_PRIVATE, CUSTOM_BITMAP, IPC_CREAT | 0666);
    shared_mem = (char *)shmat(shmid, NULL, 0);
  }

  pf("Loading shared memory\n");
  char* env = getenv(SHM_ENV_VAR);
  if (env == NULL) {
    pf("Shared memory not in env var, aborting.\n");
    return;
  }
  pf("Got env var: %s\n", env);
  int shm_id = atoi(env);
  if (shm_id <= 0) {
    pf("Shared memory ID invalid, aborting.\n");
    return;
  }
  shared_mem = shmat(shm_id, NULL, 0);
  if ((int) shared_mem == -1) {
    pf("Failed to attach shared memory, aborting.\n");
    return;
  }

  // Shared memory size
  struct shmid_ds buf;
  shmctl(shm_id, IPC_STAT, &buf);
  pf("Shared memory size: %zu\n", buf.shm_segsz);
  pf("Shared memory loaded (hi).\n");
}

void init_fork_server() {
  // if (afl_write(0)) {
  //   perror("Failed sending alive message to AFL");
  //   exit(1);
  // }
  // pf("Sent alive message to AFL\n");

  setup_shmem();
  if (shared_mem == NULL) {
    pf("Shared memory not found, exiting. AFL is here: %d\n", is_afl_here());
    if (!is_afl_here()) {
      pf("AFL is not here, continuing for debug purposes.\n");
    } else {
      pf("AFL is here, but shared memory is not, exiting.\n");
      exit(1);
    }
  }
}

size_t map_size() {
  if (!is_afl_here()) {
    return CUSTOM_BITMAP;
  }

  char* env = getenv(SHM_ENV_VAR);
  if (env == NULL) {
    pf("Shared memory not in env var, aborting.\n");
    return 0;
  }
  // get size via shmctl
  int shm_id = atoi(env);
  if (shm_id <= 0) {
    pf("Shared memory ID invalid, aborting.\n");
    return 0;
  }
  struct shmid_ds buf;
  shmctl(shm_id, IPC_STAT, &buf);
  return buf.shm_segsz;
}

void clear_map() {
  if (is_afl_here() && shared_mem) {
    memset(shared_mem, 0, map_size());
  }
}

static void on_syscall(qemu_plugin_id_t id, unsigned int vcpu_index, int64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7, uint64_t a8) {
  if (num == 501) {
    pf("syscall 501: vcpu %zu: %zu\n", vcpu_index);
  }
  if (num == 500 && getpid() == fs_pid) {
    switch(a1) {
      case 0:
        pf("syscall: hello\n");
        afl_write(0);
        pf("syscall: hello end\n");
        break;
      case 1:
        pf("syscall: ready\n");
        uint32_t dummy;
        afl_read(&dummy);
        pf("syscall: ready end\n");
        clear_map();
        break;
      case 2:
        pf("syscall: pid %ld\n", a2);
        afl_write(a2);
        pf("syscall: pid end %ld\n", a2);
        break;
      case 3:
        pf("syscall: status %ld\n", a2);
        afl_write(a2);
        pf("syscall: status end %ld\n", a2);
        if (!is_afl_here()) {
          pf("dumping coverage data\n");
          dump_cov();
          exit(-1);
        }
        break;
      default:
        pf("syscall: unknown rdi=%ld rsi=%ld\n", a1, a2);
        break;
    }
    // sleep(1);
  }
  if (num == 1) {
    pf("syscall: vcpu %zu: write(%ld)\n", vcpu_index, a1);
  }
  if (num == 56 || num == 57) {
    pf("syscall: vcpu %zu: fork %zu\n", vcpu_index);
  }
}

void dump_cov() {
  if (shared_mem) {
    FILE* f = fopen("coverage.bin", "w");
    printf("Dumping coverage of size %zu\n", map_size());
    if (f) {
      fwrite(shared_mem, sizeof(BITMAP_ENTRY_TYPE), map_size(), f);
      fclose(f);
    }
  }
}

/// Setup fork server
void fork_server_loop() {
  setup_shmem();
  if (shared_mem == NULL) {
    pf("Shared memory not found, exiting. AFL is here: %d\n", is_afl_here());
    if (!is_afl_here()) {
      pf("AFL is not here, continuing for debug purposes.\n");
    } else {
      pf("AFL is here, but shared memory is not, exiting.\n");
      exit(1);
    }
  }

  while (true) {
    pf("Waiting for AFL message.\n");
    uint32_t afl_msg;
    if (afl_read(&afl_msg)) {
      pf("Failed to read from AFL.\n");
      exit(1);
    }
    pf("Read %d from AFL once. Forking\n", afl_msg);
    // memset(shared_mem, 0, BITMAP_SIZE);
    pid_t child = fork();
    if (child < 0) {
      pf("[FS] Could not fork.\n");
      perror("fork");
      exit(1);
    }

    pf("[FS] Hi, Child is %d\n", child);
    if (child) {
      // We know the child
      pp("[FS] Forked child with PID: %d\n", child);
      if (afl_write(child)) {
        pf("[FS] Failed to write to AFL child PID.");
        exit(1);
      }

      int child_status;
      if (waitpid(child, &child_status, 0) < 0) {
        perror("waitpid");
        exit(1);
      }

      pp("[FS] Child finished\n");

      if (afl_write(child_status)) {
        pf("[FS] Failed to write to AFL child status.");
        exit(1);
      }

      pp("[FS] Wrote child status\n");

      // wait 10s
    } else {
      // We are the child
      pp("[FS] I am the child\n");
      return;
    }

    // As a child, we continue to run the target
  }
}

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

/// Count instructions defs 
size_t counts[G_MAXUINT8] = { 0 };
struct qemu_plugin_scoreboard* scoreboard;

struct InstructionData {
  const InsnDetails* details;
  const char* disassembly;
  size_t address;
};

struct TbData {
  size_t address;
  unsigned short cur_location;
  const char* insn;
};

static void setup_fs() {
  pf("[FS] Shared memory setup\n");
  setup_shmem();
  pid_t child;
  pf("[FS] Sending hello.\n");
  afl_write(0); // ohayo onichan :3

  while(1) {
    pf("[FS] Waiting for AFL to launch something.\n");
    uint32_t dummy;
    afl_read(&dummy);
    pf("[FS] Got signal from AFL. Forking.\n");
    child = fork();
    if (child == 0) {
      break;
    }
    pf("[FS] Forked child %d\n", child);
    afl_write(child);
    int status;
    waitpid(child, &status, 0);
    pf("[FS] Child exited with status %d\n", child);
    afl_write(status);
    if (!is_afl_here()) {
      dump_cov();
      exit(-1);
    }
  }
}

static void vcpu_init(qemu_plugin_id_t id, unsigned int cpu_index) {
  pf("Init vcpu %u\n", cpu_index);
  if (!is_forked) {
    is_forked = 1;
    setup_fs();
  }
  /*size_t* counts = qemu_plugin_scoreboard_find(scoreboard, cpu_index);
  for(size_t i = 0; i < G_MAXUINT8; i++) {
    if (counts[i] > 0)
      counts[i] = 0;
  }*/
}

static void vcpu_exit(qemu_plugin_id_t id, unsigned int cpu_index) {
  pf("Exit vcpu. Printing stats: %u\n", cpu_index);
}

static void plugin_atexit(qemu_plugin_id_t id, void *p) {
  pf("Plugin exited.\n");

  // printf("Dumping coverage\n");
  // FILE* f = fopen("coverage.bin", "w");
  // if (f) {
  //   fwrite(shared_mem, sizeof(BITMAP_ENTRY_TYPE), BITMAP_ENTRIES, f);
  //   fclose(f);
  // }
  pf("Exited as %zu.\n", getpid());

  // printf("Scoreboard:");

  // if (scoreboard) {
  //   size_t* counts = qemu_plugin_scoreboard_find(scoreboard, 0);
  //   for(size_t i = 0; i < G_MAXUINT8; i++) {
  //     if (counts[i] > 0)
  //       printf("%zx: %lu\n", i, counts[i]);
  //   }

  //   qemu_plugin_scoreboard_free(scoreboard);
  //   scoreboard = NULL;
  // }

  // printf("Shared mem");
  // for(size_t i = 0; i < G_MAXUINT8; i++) {
  //   if (counts[i] > 0)
  //     printf("%zx: %lu\n", i, counts[i]);
  // }
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

static void inner_tb_exec(u32 vcpu_index, void* data) {
  struct TbData* tb_data = (struct TbData*) data;
  // pf("[TB %08x insn exec] Executing insn %s\n", tb_data->address, tb_data->insn);
}

static void tb_exec_cb(u32 vcpu_index, void* data) {
  struct TbData* tb_data = (struct TbData*)data;
  // pf("[TB %08x tb   exec] Executing insn %s\n", tb_data->address, tb_data->insn);
  // pf("tb: exec: vcpu: %zu fs pid: %zu, pid: %zu cur_loc: %zx, prev_loc: %zx, addr: %zx\n", vcpu_index, fs_pid, getpid(), tb_data->cur_location, previous_block, tb_data->cur_location ^ previous_block);
  if (/* fs_pid != getpid() && */ shared_mem) {
    // pf("Executing fuzzed. Current location: %zx, previous block: %zx, incrementing indx: %zx, value: %d\n", tb_data->cur_location, previous_block, (tb_data->cur_location ^ previous_block) % (map_size() / sizeof(BITMAP_ENTRY_TYPE)), shared_mem[(tb_data->cur_location ^ previous_block) % (map_size() / sizeof(BITMAP_ENTRY_TYPE))]);
    shared_mem[(tb_data->cur_location ^ previous_block) % (map_size() / sizeof(BITMAP_ENTRY_TYPE))]++;
  }
  previous_block = tb_data->cur_location >> 1;
}

static void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb) {
  size_t nb_insn = qemu_plugin_tb_n_insns(tb);


  struct TbData* tb_data = malloc(sizeof(struct TbData));
  tb_data->address = qemu_plugin_tb_vaddr(tb);
  tb_data->cur_location = (unsigned short) qemu_plugin_insn_vaddr(qemu_plugin_tb_get_insn(tb, 0));
  qemu_plugin_register_vcpu_tb_exec_cb(tb, tb_exec_cb, QEMU_PLUGIN_CB_NO_REGS, tb_data);

  // struct qemu_plugin_insn* insn = qemu_plugin_tb_get_insn(tb, 0);
  // if (insn) {
  //   // pf("[Translating block] Address: %08x\n", tb_data->address);
  //   tb_data->insn = qemu_plugin_insn_disas(insn);
  //   qemu_plugin_register_vcpu_insn_exec_cb(insn, inner_tb_exec, QEMU_PLUGIN_CB_NO_REGS, tb_data);
  // }


  // size_t address = qemu_plugin_tb_vaddr(tb);


  // for(size_t i = 0; i < nb_insn; i++) {
  //   struct qemu_plugin_insn* insn = qemu_plugin_tb_get_insn(tb, i);
  //   const char* disas = qemu_plugin_insn_disas(insn);
  // }

  // for(size_t i = 0; i < nb_insn; i++) {
  //   struct qemu_plugin_insn* insn = qemu_plugin_tb_get_insn(tb, i);
  //   size_t size = qemu_plugin_insn_size(insn);
  //   size_t address = qemu_plugin_insn_vaddr(insn);
  //   unsigned char opcode[size];
  //   size = qemu_plugin_insn_data(insn, opcode, size);

  //   struct InstructionData *cb_data = malloc(sizeof(struct InstructionData));
  //   cb_data->details = lookup((void*) opcode, size);
  //   cb_data->disassembly = qemu_plugin_insn_disas(insn);
  //   cb_data->address = address;

  //   pf("Translating: Address: %08x, size %zu, disas: %s, insn: ", address, size, cb_data->disassembly);
  //   for(size_t j = 0; j < size; j++) {
  //     pp("%02x ", opcode[j]);
  //   }
  //   pp("\n");
  //   qemu_plugin_register_vcpu_insn_exec_cb(insn, insn_exec_cb, QEMU_PLUGIN_CB_NO_REGS, cb_data);
 
  //   qemu_plugin_register_vcpu_insn_exec_inline_per_vcpu(insn, QEMU_PLUGIN_INLINE_ADD_U64, (qemu_plugin_u64) {scoreboard, hash(address) * 8}, 1);
  //   // address += size;
  // }
}

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info, int argc, char **argv) {
  pf("Loaded plugin: Id %" PRIu64 ", running arch %s\n", id, info->target_name);
  fs_pid = getpid();
  srand(15);
  // fork_server_loop();
  // scoreboard = qemu_plugin_scoreboard_new(sizeof(u8) * sizeof(size_t));
  qemu_plugin_register_vcpu_init_cb(id, vcpu_init);
  // qemu_plugin_register_vcpu_exit_cb(id, vcpu_exit);
  qemu_plugin_register_vcpu_syscall_cb(id, on_syscall);
  qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
  qemu_plugin_register_atexit_cb(id, plugin_atexit, NULL);
  // init_fork_server();
  return 0;
}