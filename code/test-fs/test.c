#include <time.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <x86intrin.h>
#include <fcntl.h>
#include <bits/fcntl-linux.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>

static const char SHM_ENV_VAR[] = "__AFL_SHM_ID";
static const int FORKSRV_FD_IN = 198;
static const int FORKSRV_FD_OUT = 199;

static int is_afl_here() {
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

int main(void) {
    while(1) {
        afl_write(0); // ready to hear
        uint32_t msg;
        afl_read(&msg);
        if (msg == 0) {
            pid_t child = fork();
            if (child == 0) {
                execve("/work/my-plugin/targets/coverage", NULL, NULL);
                fprintf(stderr, "Failed to exec\n");
                return -1;
            } else {
                afl_write(child);
                int status;
                waitpid(child, &status, 0);
                afl_write(status);
            }
        }
    }
}