#include <stddef.h>
#include <unistd.h>

#ifndef TARGET
    #error "TARGET not defined"
#endif
#ifndef QEMU
    #error "QEMU not defined"
#endif
#ifndef QEMU_PLUGIN
    #error "QEMU_PLUGIN not defined"
#endif

int main(void) {
    execve(QEMU, (char *[]) {QEMU, "-plugin", QEMU_PLUGIN, TARGET, NULL}, (char *[]) {NULL});
}