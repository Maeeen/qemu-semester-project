#include "fs.h"
#include "afl.h"
#include <sys/types.h>
#include <sys/wait.h>

size_t coverage_map_size = 0;
unsigned char* coverage_map = 0x0;
unsigned short previous_block = 0x0;

void fs_register_exec(uint64_t address_) {
  unsigned short address = (unsigned short) address_;
  coverage_map[address ^ previous_block]++;
  previous_block = address >> 1;
}

int fs_init() {
  if (unlikely(!afl_is_here())) {
    pf("As a note, you should run this with afl-fuzz. However, I'm just a simple string in an executable so don't listen too much to me. Cheers!\n");
  }
  if (afl_setup_shmem((void**) &coverage_map, &coverage_map_size)) {
    pf("Failed to open shared memory.\n");
    return -1;
  }
  if (coverage_map_size < DEFAULT_MAP_SIZE) {
    pf("Coverage size map is too small…\n");
    return -1;
  }
  coverage_map_size = DEFAULT_MAP_SIZE;
  return 0;
}

int fs_handshake() {
  u32 tmp = 0;
  if (afl_write(FS_VERSION_HANDSHAKE)) {
    pf("Handshake 0: Failed to write version.\n");
    goto failed_handshake;
  }
  if (afl_read(&tmp)) {
    pf("Handshake 0: Failed to read answer.\n");
    goto failed_handshake;
  }
  if (tmp != FS_VERSION_HANDSHAKE_RES) {
    pf("Handshake 0: Invalid message from AFL Got %08x.\n", tmp);
    goto failed_handshake;
  }

  // Declare the map size
  u32 status = FS_NEW_OPT_MAPSIZE | FS_OPT_NEWCMPLOG;
  if (afl_write(status)) {
    pf("Handshake 1: Failed to write set options.\n");
    goto failed_handshake;
  }
  if (afl_write(coverage_map_size)) {
    pf("Handshake 1: Failed to write coverage map size.\n");
    goto failed_handshake;
  }
  // Hello 👋
  if (afl_write(FS_VERSION_HANDSHAKE)) {
    pf("Handshake 2 (end): Failed to write hello.\n");
    goto failed_handshake;
  }

  pf("Handshaked with AFL 👋\n");

  return 0;
failed_handshake:
  pf("Failed handshake with AFL.\n");
  return -1;
}

int fs_loop(void(*possible_inter)()) {
  u32 dummy;
  if (afl_read(&dummy)) {
    pf("Failed to read start message.\n");
    goto failed_comm;
  }
  pid_t child = fork();
  if (child == 0) {
    close(FORKSRV_FD_IN);
    close(FORKSRV_FD_OUT);
    return 1;
  }
  if (child < 0) {
    perror("fork");
    pf("fork failed.");
    exit(-1);
  }
  afl_write(child);
  if (possible_inter) possible_inter();
  int status;
  if (waitpid(child, &status, 0) < 0) {
    perror("waitpid");
    pf("waitpid failed.");
    exit(-1);
  }
  if (afl_write(status)) {
    pf("Failed to send status.\n");
    goto failed_comm;
  }

  return 0;

failed_comm:
  pf("Failed communication with AFL. Goodbye! :(\n");
  exit(-1);
}