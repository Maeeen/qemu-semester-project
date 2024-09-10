int main(void) {
    if (fork() == 0) {
        printf("Hello from child\n");
    } else {
        printf("Hello from parent\n");
    }
    return 0;
}