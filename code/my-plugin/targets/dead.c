#include "../types.h"

long syscall(long number, long arg1, long arg2, long arg3) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a" (ret)               // Return value in RAX
        : "a" (number),            // System call number in RAX
          "D" (arg1),              // First argument in RDI
          "S" (arg2),              // Second argument in RSI
          "d" (arg3)               // Third argument in RDX
        : "rcx", "r11", "memory"   // Clobbers
    );
    return ret;
}

int main(void) {
    #ifdef DEBUG
        syscall(1, 1, "Suicide.\n", sizeof("Suicide.\n"));
    #endif
    syscall(501, 0, 0, 0);
    *(int*) (0x0) = 0x0;
}