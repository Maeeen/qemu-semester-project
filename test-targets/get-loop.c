// Define syscall numbers for x86_64 architecture
#define SYS_read 0     // System call number for read
#define SYS_exit 60    // System call number for exit

// Declare a function to make system calls
long syscall(long number, long arg1, long arg2, long arg3);

void _start() {
    char buffer[16];
    
    while (1) {
        long bytes_read = syscall(SYS_read, 0, (long)buffer, 15);
        if (buffer[0] == 'q') {
            break;
        }
    }
    // Exit the program with status 0
    syscall(SYS_exit, 0, 0, 0);
}

inline long syscall(long number, long arg1, long arg2, long arg3) {
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
