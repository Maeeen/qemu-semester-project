#include "fs.h"
#include "afl.h"
#include <sys/types.h>
#include <sys/wait.h>

size_t coverage_map_size = 0;
static unsigned char* coverage_map = 0x0;
static unsigned short previous_block = 0x0;

void fs_register_exec(uint64_t address_) {
  unsigned short address = (unsigned short) address_;
  coverage_map[address ^ previous_block]++;
  previous_block = address >> 1;
}

unsigned char* generate_assembly_bytes(unsigned char* buf, uint64_t address_) {
  unsigned short address = (unsigned short)address_;
  unsigned char* assembly_bytes = buf;
  if (unlikely(!assembly_bytes)) {
    return NULL;
  }

  // 1. int3 for debugging
  assembly_bytes[0] = 0x90; // nop // 0xCC; // int3

  // Handwritten assembly, chatgpt is too bad…
  // To recap, address is 16 bit (so 2 bytes, so e.g. 0xabcd)
  // Pointers are 64 bit, so 8 bytes (e.g. 0x7fff7fff7fff7fff)
  // 2. mov $<previous_block, 64 bits>, %rdx
  assembly_bytes[1] = 0x48;
  assembly_bytes[2] = 0xBA;
  //    immediate (which is the previous block addr)
  assembly_bytes[3] = (unsigned char) ((uint64_t) (&previous_block) & 0xFF);
  assembly_bytes[4] = (unsigned char) (((uint64_t) (&previous_block) >> 8) & 0xFF);
  assembly_bytes[5] = (unsigned char) (((uint64_t) (&previous_block) >> 16) & 0xFF);
  assembly_bytes[6] = (unsigned char) (((uint64_t) (&previous_block) >> 24) & 0xFF);
  assembly_bytes[7] = (unsigned char) (((uint64_t) (&previous_block) >> 32) & 0xFF);
  assembly_bytes[8] = (unsigned char) (((uint64_t) (&previous_block) >> 40) & 0xFF);
  assembly_bytes[9] = (unsigned char) (((uint64_t) (&previous_block) >> 48) & 0xFF);
  assembly_bytes[10] = (unsigned char) (((uint64_t) (&previous_block) >> 56) & 0xFF);
  // 3. read value and put it into %eax
  //    movzwl (%rdx), %eax
  assembly_bytes[11] = 0x0F;
  assembly_bytes[12] = 0xB7;
  assembly_bytes[13] = 0x02;
  // 4. xor address with previous_block
  //    xor <address short>, %ax
  assembly_bytes[14] = 0x66;
  assembly_bytes[15] = 0x35;
  assembly_bytes[16] = (unsigned char) (address & 0xFF);
  assembly_bytes[17] = (unsigned char) ((address >> 8) & 0xFF);
  // Here, we have %ax = previous_block ^ address
  // 5. movzwl (zero word extended long) %ax, %eax
  assembly_bytes[18] = 0x0F;
  assembly_bytes[19] = 0xB7;
  assembly_bytes[20] = 0xC0;
  // 6. prepare %rcx for address of coverage map
  //    mov $<coverage_map>, %rcx
  assembly_bytes[21] = 0x48;
  assembly_bytes[22] = 0xB9;
  //    immediate (which is the coverage map addr)
  assembly_bytes[23] = (unsigned char) ((uint64_t) (coverage_map) & 0xFF);
  assembly_bytes[24] = (unsigned char) (((uint64_t) (coverage_map) >> 8) & 0xFF);
  assembly_bytes[25] = (unsigned char) (((uint64_t) (coverage_map) >> 16) & 0xFF);
  assembly_bytes[26] = (unsigned char) (((uint64_t) (coverage_map) >> 24) & 0xFF);
  assembly_bytes[27] = (unsigned char) (((uint64_t) (coverage_map) >> 32) & 0xFF);
  assembly_bytes[28] = (unsigned char) (((uint64_t) (coverage_map) >> 40) & 0xFF);
  assembly_bytes[29] = (unsigned char) (((uint64_t) (coverage_map) >> 48) & 0xFF);
  assembly_bytes[30] = (unsigned char) (((uint64_t) (coverage_map) >> 56) & 0xFF);
  // okay now we have %rcx = coverage_map, %ax = previous_block ^ address (idx), %rdx = previous_block
  // just increment the value at the index
  // 7. addb $1, (%rax, %rcx, 1)
  assembly_bytes[31] = 0x80;
  assembly_bytes[32] = 0x04;
  assembly_bytes[33] = 0x08;
  assembly_bytes[34] = 0x01;
  // 8. change previous block to address >> 1
  //    mov $<address >> 1, %dx
  assembly_bytes[35] = 0x66;
  assembly_bytes[36] = 0xBA;
  assembly_bytes[37] = (unsigned char) ((address >> 1) & 0xFF);
  assembly_bytes[38] = (unsigned char) ((address >> 9) & 0xFF);


  return assembly_bytes;
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

int fs_loop(qemu_plugin_id_t id, void(*possible_inter)(qemu_plugin_id_t id)) {
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
  if (possible_inter) possible_inter(id);
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