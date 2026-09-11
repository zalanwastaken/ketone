[ORG 0x7D00]
[BITS 32]

PML4    equ 0x90000
PDPT    equ 0x91000
PD0     equ 0x92000
PD1     equ 0x93000
PD2     equ 0x94000
PD3     equ 0x95000

protected_mode:
    mov ax, 0x18

    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov esp, 0x70000

    call enable_ISA_extentions

    mov esi, 0x1000
    mov edi, 0x120000
    mov ecx, SECTORS
    shl ecx, 7          ; ecx = number of dwords = SECTORS*128
    xor eax, eax         ; eax = index counter

.copy_loop:
    cmp eax, ecx
    jge .copy_done

    mov edx, [esi + eax*4]
    mov [edi + eax*4], edx

    inc eax
    jmp .copy_loop

.copy_done:
    mov [0x7E00], SECTORS

    ; ------------------------------------------------
    ; PML4[0] -> PDPT
    ; ------------------------------------------------

    mov dword [PML4], PDPT | 0x03
    mov dword [PML4 + 4], 0


    ; ------------------------------------------------
    ; PDPT[0..3] -> four page directories
    ; ------------------------------------------------

    mov dword [PDPT],      PD0 | 0x03
    mov dword [PDPT + 4],  0

    mov dword [PDPT + 8],  PD1 | 0x03
    mov dword [PDPT + 12], 0

    mov dword [PDPT + 16], PD2 | 0x03
    mov dword [PDPT + 20], 0

    mov dword [PDPT + 24], PD3 | 0x03
    mov dword [PDPT + 28], 0


    ; ------------------------------------------------
    ; Generate 2048 × 2 MiB pages
    ;
    ; 2048 * 2 MiB = 4 GiB
    ; ------------------------------------------------

    mov edi, PD0
    xor eax, eax
    mov ecx, 2048

.fill_pages:

    ; Low 32 bits of PDE
    mov edx, eax
    or edx, 0x83
    mov [edi], edx

    ; High 32 bits of PDE
    mov dword [edi + 4], 0

    ; Next 2 MiB
    add eax, 0x200000

    ; Next 8-byte PDE
    add edi, 8

    loop .fill_pages


    ; ------------------------------------------------
    ; Load PML4 into CR3
    ; ------------------------------------------------

    mov eax, PML4
    mov cr3, eax


    ; ------------------------------------------------
    ; Enable PAE
    ; CR4.PAE = bit 5
    ; ------------------------------------------------

    mov eax, cr4
    or eax, (1 << 5)
    mov cr4, eax


    ; ------------------------------------------------
    ; Enable Long Mode
    ; IA32_EFER = 0xC0000080
    ; EFER.LME = bit 8
    ; ------------------------------------------------

    mov ecx, 0xC0000080
    rdmsr

    or eax, (1 << 8)

    wrmsr


    ; ------------------------------------------------
    ; Enable paging
    ; CR0.PG = bit 31
    ; ------------------------------------------------

    mov eax, cr0
    or eax, (1 << 31)
    mov cr0, eax


    ; ------------------------------------------------
    ; FAR JUMP INTO 64-BIT CODE
    ;
    ; 0x10 = 64-bit code segment
    ; ------------------------------------------------

    jmp 0x10:long_mode
enable_ISA_extentions:
    ; Check for CPUID first
    pushfd
    pop eax

    mov ecx, eax
    xor eax, 1 << 21       ; Toggle ID bit
    push eax
    popfd

    pushfd
    pop eax
    xor eax, ecx
    jz .no_cpuid

    ; CPUID leaf 1
    mov eax, 1
    cpuid

    ; EDX bit 26 = SSE2
    test edx, 1 << 26
    jz .no_sse2

    ; --------------------------------
    ; Enable x87/MMX/SSE/SSE2
    ; --------------------------------

    mov eax, cr0

    and eax, ~(1 << 2)     ; CR0.EM = 0
    or  eax,  (1 << 1)     ; CR0.MP = 1

    mov cr0, eax

    mov eax, cr4

    or eax, (1 << 9)       ; CR4.OSFXSR
    or eax, (1 << 10)      ; CR4.OSXMMEXCPT

    mov cr4, eax

    ret

.no_cpuid:
    jmp $

.no_sse2:
    jmp $

[BITS 64]

long_mode:

    ; 0x18 = data segment
    mov ax, 0x18

    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    ; Set up a 64-bit stack
    mov rsp, 0x80000

    ; WE ARE NOW IN 64-BIT LAND :3

    mov rax, 0x120000
    jmp rax

.hang:
    hlt
    jmp .hang

times 512-($-$$) db 0
