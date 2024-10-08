#include "afl.h"
#include <sys/types.h>
#include <sys/shm.h>

int afl_is_here() {
    return getenv(SHM_ENV_VAR) != NULL
      && fcntl(FORKSRV_FD_IN, F_GETFD) != -1
      && fcntl(FORKSRV_FD_OUT, F_GETFD) != -1;
}

int afl_read(u32 *out) {
  if (unlikely(!afl_is_here())) {
    return 0;
  }

  ssize_t res = read(FORKSRV_FD_IN, out, sizeof(u32));

  return res != 4;
}

int afl_write(u32 value) {
  if (unlikely(!afl_is_here())) {
    return 0;
  }

  ssize_t res = write(FORKSRV_FD_OUT, &value, sizeof(u32));

  if (unlikely(res == -1 && errno == EINTR)) {
    fprintf(stderr, "Interrupted while sending message to AFL.\n");
  }
  return res != 4;
}

int afl_senderr(u32 error) {
  return afl_write(error | FS_NEW_ERROR);
}

int afl_setup_shmem(void** mem, size_t* size) {
  if (unlikely(!afl_is_here())) {
    pf("AFL is not detected. Using a dummy shared map.\n");
    *mem = malloc(DEFAULT_MAP_SIZE);
    *size = DEFAULT_MAP_SIZE;
    return 0;
  }

  const char* shm_id_str = getenv(SHM_ENV_VAR);
  int shm_id = atoi(shm_id_str);
  if (shm_id <= 0) {
    pf("Invalid shared memory ID.\n");
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

  pf("Shared memory loaded.\n");
  return 0;
}