section .multiboot
align 8
MULTIBOOT2_HEADER_MAGIC equ 0xE85250D6
MULTIBOOT2_HEADER_ARCH  equ 0
MULTIBOOT2_HEADER_LEN   equ (header_end - header_start)
MULTIBOOT2_HEADER_CSUM  equ -(MULTIBOOT2_HEADER_MAGIC + MULTIBOOT2_HEADER_ARCH + MULTIBOOT2_HEADER_LEN)

header_start:
dd MULTIBOOT2_HEADER_MAGIC
dd MULTIBOOT2_HEADER_ARCH
dd MULTIBOOT2_HEADER_LEN
dd MULTIBOOT2_HEADER_CSUM
dw 0
dw 0
dd 8
header_end:

section .text
global _start
global mboot_info         ; export for C
extern kernel_main

_start:
  cli
  mov esp, stack_top
  push ebx            
  call kernel_main
  
.hang:
  hlt
  jmp .hang

section .bss
align 4
mboot_info: resd 1        ; uint32_t mboot_info

align 16
resb 16384
stack_top:

