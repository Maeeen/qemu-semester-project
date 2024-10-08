global _start

section .text

_start:
  mov rdi, 0 ; 1 data transfer
loop_start:
  cmp rdi, 100 ; 101 cmp
  jge loop_end ; 1 
  inc rdi ; 100 arithmetic
  jmp loop_start
loop_end:
  ; exit 0
  mov rax, 60
  syscall
