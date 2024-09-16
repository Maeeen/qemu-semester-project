section .bss
    s resb 16          ; Reserve 16 bytes for the string

section .rodata
    msg: db "Hi, enter your key: ", 10
    msglen: equ $ - msg
    prompt: db "%15s", 0 ; Format string for scanf

section .text
    global _start

_start:

    ; Print prompt
    mov rax, 1        ; write(
    mov rdi, 1        ;   STDOUT_FILENO,
    mov rsi, msg      ;   "Hello, world!\n",
    mov rdx, msglen   ;   sizeof("Hello, world!\n")
    syscall           ; );

    ; Call scanf
    mov eax, 3         ; Syscall number for read (stdin)
    mov ebx, 0         ; File descriptor 0 (stdin)
    lea ecx, [s]       ; Load address of string buffer
    mov edx, 16        ; Max number of bytes to read
    int 0x80           ; Interrupt for system call

    ; Check for "fuzz"
    mov al, byte [s]
    cmp al, 'f'
    jne end_program     ; If first char isn't 'f', jump to end

    mov al, byte [s + 1]
    cmp al, 'u'
    jne end_program     ; If second char isn't 'u', jump to end

    mov al, byte [s + 2]
    cmp al, 'z'
    jne end_program     ; If third char isn't 'z', jump to end

    mov al, byte [s + 3]
    cmp al, 'z'
    jne end_program     ; If fourth char isn't 'z', jump to end

    ; If "fuzz" is matched, trigger a segmentation fault
    mov dword [0x0], 0 ; Writing 0 to address 0x0 causes a segfault

end_program:
    ; Exit the program
    mov eax, 1         ; Syscall number for exit
    xor ebx, ebx       ; Exit status 0
    int 0x80           ; Interrupt for system call