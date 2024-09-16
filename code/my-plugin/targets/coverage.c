// Define syscall numbers for x86_64 architecture
#define SYS_read 0     // System call number for read
#define SYS_exit 60    // System call number for exit

// Declare a function to make system calls
long syscall(long number, long arg1, long arg2, long arg3);

void _start() {
    char buffer[16];

    syscall(1, 1, "Enter your key: ", sizeof("Enter your key: "));
    
    // Perform the read system call (file descriptor 0 = stdin)
    long bytes_read = syscall(SYS_read, 0, (long)buffer, 15);

    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';  // Null-terminate the string

        // Manually compare the input string
        if (buffer[0] == 'f' && buffer[1] == 'u' &&
            buffer[2] == 'z' && buffer[3] == 'z') {
            *(int*)0x0 = 0x0; // Cause a segmentation fault by dereferencing null
        }
    }

    // Exit the program with status 0
    syscall(SYS_exit, 0, 0, 0);
}

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