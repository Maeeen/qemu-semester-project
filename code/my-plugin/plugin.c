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

#define DEBUG

#ifdef DEBUG
#define pf(fmt, ...) printf("[Plugin %zu] " fmt, getpid(), ##__VA_ARGS__)
#define pp(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define pf(fmt, ...)
#define pp(fmt, ...)
#endif

/// AFL Instrumentation detail defs
// 64kb bitmap
#define BITMAP_SIZE 65536
#define BITMAP_ENTRY_TYPE unsigned char
#define BITMAP_ENTRIES (BITMAP_SIZE / sizeof(BITMAP_ENTRY_TYPE))

/// Globals
static const char SHM_ENV_VAR[] = "__AFL_SHM_ID";
static const int FORKSRV_FD_IN = 198;
static const int FORKSRV_FD_OUT = 199;

/// Instrumentation specific
BITMAP_ENTRY_TYPE* shared_mem = 0;
unsigned short previous_block = 0x0;

/// Debug
extern char **environ;
void show_envvar() {
  char **s = environ;

  for (; *s; s++) {
    pp("%s\n", *s);
  }

  return 0;
}

/// Setup shared memory
void setup_shmem() {
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
  if (shared_mem == (void*)-1) {
    pf("Failed to attach shared memory, aborting.\n");
    return;
  }
  pf("Shared memory loaded.\n");
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

/// Setup fork server
void fork_server_loop() {
  if (afl_write(0)) {
    perror("Failed sending alive message to AFL");
    exit(1);
  }
  pf("Sent alive message to AFL\n");

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
    printf("Waiting for AFL message.\n");
    sleep(1);
    uint32_t afl_msg;
    if (afl_read(&afl_msg)) {
      pf("Failed to read from AFL.\n");
      exit(1);
    }
    printf("Read %d from AFL once. Forking\n", afl_msg);
    // memset(shared_mem, 0, BITMAP_SIZE);
    pid_t child = fork();
    if (child < 0) {
      printf("[FS] Could not fork.\n");
      perror("fork");
      exit(1);
    }

    printf("[FS] Hi, Child is %d\n", child);
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
      close(FORKSRV_FD_IN);
      close(FORKSRV_FD_OUT);
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
};

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


static void tb_exec_cb(u32 vcpu_index, void* data) {
  struct TbData* tb_data = (struct TbData*)data;
  // pf("tb: exec: cur_loc: %zx, prev_loc: %zx, addr: %zx\n", tb_data->cur_location, previous_block, tb_data->cur_location ^ previous_block);
  if (shared_mem)
    shared_mem[(tb_data->cur_location ^ previous_block) % BITMAP_ENTRIES]++;
  previous_block = tb_data->cur_location >> 1;
}

static void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb) {
  size_t nb_insn = qemu_plugin_tb_n_insns(tb);

  struct TbData* tb_data = malloc(sizeof(struct TbData));
  tb_data->address = qemu_plugin_tb_vaddr(tb);
  tb_data->cur_location = rand();

  qemu_plugin_register_vcpu_tb_exec_cb(tb, tb_exec_cb, QEMU_PLUGIN_CB_NO_REGS, tb_data);

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
  srand(15);
  pid_t f;
  if (f = fork()) {
    pf("Forking, child is %zu. I am %zu\n", f, getpid());
    sleep(100);
    exit(1);
  } else {
    pf("Forked, (child, %zu). I am %zu\n", f, getpid());
  }
  // fork_server_loop();
  scoreboard = qemu_plugin_scoreboard_new(sizeof(u8) * sizeof(size_t));
  qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
  // qemu_plugin_register_vcpu_init_cb(id, vcpu_init);
  // qemu_plugin_register_vcpu_exit_cb(id, vcpu_exit);
  qemu_plugin_register_atexit_cb(id, plugin_atexit, NULL);
  return 0;
}