#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include "../types.h"

#define SS 500

#ifndef TARGET
#define TARGET "hello-asm"
#endif

int main(void) {
    pf("[FS] Waiting for signal to fork. Launching: " TARGET "\n");
    syscall(SS, 0);

    while(1) {
        syscall(SS, 1); // ready
        pid_t child = fork();
        if (child == 0) {
            pf("[FS] Children running.");
            execv(TARGET, NULL);
            pf("[FS] Failed to exec\n");
            return -1;
        } else {
            syscall(SS, 2, child); // child
            int status;
            pf("[FS] Waiting for child to finish\n");
            syscall(SYS_wait4, -1, &status, NULL, NULL);
            pf("[FS] Child finished with status %d\n", status);
            syscall(SS, 3, status); // status
        }
    }
}