#include <errno.h>

int main(void) {
    printf("Hi, I am %zu\n", getpid());
    if (fork() == 0) {
        printf("Hello from child. I am %zu\n", getpid());
        int d = execv("/work/my-plugin/targets/hello-asm", 0);
        printf("Failed to execv %d", errno);
        // sleep(1000);
    } else {
        printf("Hello from parent. I am %zu\n", getpid());
        // sleep(1000);
    }
    return 0;
}