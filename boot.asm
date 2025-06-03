%define ALIGN       (1 << 0)
%define MEMINFO     (1 << 1)
%define FLAGS       (ALIGN | MEMINFO)
%define MAGIC       0x1BADB002
%define CHECKSUM    -(MAGIC + FLAGS)

section .text
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

global _start
extern kernel_main

_start:
    mov esp, stack_top

    call kernel_main

    cli
.hang:
    hlt
    jmp .hang

; Define stack in BSS section
section .bss
align 16
stack_bottom:
    resb 16384     ; Reserve 16 KiB
stack_top:

