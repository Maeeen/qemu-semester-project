global _start

%macro print 2
  mov rax, 1
  mov rdi, 1
  mov rsi, %1
  mov rdx, %2
  syscall
%endmacro

; Syscall interactions (rax, rdi, rsi)
; rax: 500: talk to plugin
; rdi:
;  0 -- hello
;  1 -- ready
;  2 -- pid (rsi has pid)
;  3 -- status (rsi has status)

%macro cust_syscall 2
  mov rax, 500
  mov rdi, %1
  mov rsi, %2
  syscall
%endmacro

%macro send_hello 0
  cust_syscall 0, 0
%endmacro

%macro send_alive 0
  cust_syscall 1, 0
%endmacro

%macro send_pid 1
  cust_syscall 2, %1
%endmacro

%macro send_status 1
  cust_syscall 3, %1
%endmacro

section .text

_start:
  print info, infolen

fork_loop:
  print cssyscall, cssyscalllen
  send_alive ; signal alive

  ; fork the process
  mov rax, 57 ; fork
  syscall

  ; if we are the child process, exec the target binary
  cmp rax, 0
  jne parent

  ; exec the target binary
  mov rax, 59 ; execve
  mov rdi, target ; char* pathname
  lea rsi, [rsp + 8] ; char* const argv[]
  ; write the first argv to target
  ; mov qword [rsp + 8], target
  mov rdx, [rsp + 16]
  syscall

  print failed, failedlen
  ; exit 500
  mov rax, 60
  mov rdi, 500
  syscall

parent:
  ; send pid to plugin
  send_pid rax
  ; wait for the child process to finish
  mov rax, 61 ; wait4
  mov rdi, -1 ; pid_t pid
  lea rsi, [rsp + 8] ; int* status
  mov rdx, 0 ; int options
  mov r10, 0 ; struct rusage* rusage
  syscall
  ; send the status to the parent
  ; read status
  mov rsi, [rsp + 8]
  send_status rsi

  jmp fork_loop

fork_quit:
  print failedloop, failedlooplen

  ; exit 0
  mov rax, 60
  mov rdi, 0
  syscall

section .rodata
  target: db TARGET, 0
  targetlen: equ $ - target
  info: db "[FS] Targetting ", TARGET, 10
  infolen: equ $ - info
  cssyscall: db "[FS] Waiting for signal to fork.", 10
  cssyscalllen: equ $ - cssyscall
  failed: db "[FS] Failed to exec target.", 10
  failedlen: equ $ - failed
  failedloop: db "[FS] Failed to exec target in loop.", 10
  failedlooplen: equ $ - failedloop