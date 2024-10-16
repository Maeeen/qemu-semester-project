from utils import exec

def indent(s):
    return " " * 4 + s.replace('\n', '\n' + " " * 4)

def assemble(assembly, out):
    boilerplate_start = """
global _start

section .text

_start:
"""
    boilerplate_end = """
mov rax, 60
mov rdi, 0
syscall
"""
    
    code = boilerplate_start + indent(assembly) + indent(boilerplate_end)
    # write to file
    with open(out + '.asm', 'w') as f:
        f.write(code)
    # assemble
    exec('nasm', ['-f', 'elf64', out + '.asm', '-o', out + '.o'])
    # link
    exec('ld', ['-o', out + '.out', out + '.o'])
    return out + '.out'