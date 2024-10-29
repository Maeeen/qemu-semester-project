#include "types.h"
#include "afl-cmplog.h"
#include <stdlib.h>
#include <stddef.h>
#include <fcntl.h>
#include <sys/shm.h>

struct cmp_map *compare_map = NULL;

/// Initializes the cmplog shared memory.
int cmplog_init() {
  if (unlikely(!afl_is_here()) || !getenv(CMPLOG_ENV)) {
    pf("AFL is not detected. Continuing without.\n");
  }
  size_t cmpmap_sz;
  if (cmplog_shmem((void**) &compare_map, &cmpmap_sz)) {
    pf("Failed to initialize cmplog.\n");
    return -1;
  }
}

/// Logs a comparison.
void cmplog_log(size_t location_, u64 v0, u64 v1, u64 effective_bits) {
  unsigned short location = (unsigned short) location_;
  // printf("Tried to compare %llx with %llx bits %zu at %lu (%hu)\n", v0, v1, effective_bits, location_, location);
  if (likely(compare_map)) {
    u32 hits = 0;
    if (compare_map->headers[location].type != CMP_TYPE_INS)
      compare_map->headers[location].type = CMP_TYPE_INS;
    
    if (compare_map->headers[location].hits == 0) {
      if (effective_bits == 8)
        compare_map->headers[location].shape = 0;
      else if (effective_bits == 16)
        compare_map->headers[location].shape = 1;
      else if (effective_bits == 32)
        compare_map->headers[location].shape = 3;
      else if (effective_bits == 64)
        compare_map->headers[location].shape = 7;
      else if (effective_bits == 128)
        compare_map->headers[location].shape = 15;
      else
        pf("Invalid effective bits: %lu\n", effective_bits);
      compare_map->headers[location].type = CMP_TYPE_INS;
    } else {
      hits = compare_map->headers[location].hits;
    }

    compare_map->headers[location].hits = hits + 1;
    hits &= CMP_MAP_H - 1,
    compare_map->log[location][hits].v0 = v0;
    compare_map->log[location][hits].v1 = v1;
  }
}

/// Initializes the shared memory for cmplog.
int cmplog_shmem(void** mem, size_t* size) {
  if (unlikely(!afl_is_here()) || !getenv(CMPLOG_ENV)) {
    pf("AFL is not detected. Using a dummy shared for complog map.\n");
    *size = sizeof(struct cmp_map);
    *mem = malloc(*size);
    return 0;
  }

  const char* shm_id_str = getenv(CMPLOG_SHM_ENV);
  int shm_id = atoi(shm_id_str);
  if (shm_id <= 0) {
    pf("Invalid shared memory ID for complog.\n");
    return -1;
  }

  *mem = shmat(shm_id, NULL, 0);
  if (!*mem || *mem == (void*) -1) {
    pf("Failed to attach shared memory, aborting.\n");
    afl_senderr(FS_ERROR_SHM_OPEN);
    return -1;
  }
  struct shmid_ds buf;
  shmctl(shm_id, IPC_STAT, &buf);
  *size = buf.shm_segsz;

  if (*size < sizeof(struct cmp_map)) {
    pf("Invalid shared memory size. AFL gave us %zu but we need %zu.\n", size, sizeof(struct cmp_map));
    return -1;
  }

  pf("Shared memory loaded.\n");
  return 0;
}
