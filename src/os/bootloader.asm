[BITS 16]
[ORG 0x7C00]

PML4    equ 0x90000
PDPT    equ 0x91000
PD0     equ 0x92000
PD1     equ 0x93000
PD2     equ 0x94000
PD3     equ 0x95000

start:
    xor ax, ax
    mov ds, ax
    mov es, ax

    mov si, msg
    call BIOS_print

    in  al, 0x92
    or  al, 2
    out 0x92, al

    mov ax, 0
    mov es, ax
    mov bx, 0x1000
    mov ah, 0x02
    mov al, SECTORS
    mov ch, 0
    mov cl, 9
    mov dh, 0
    mov dl, 0x80
    int 0x13
    jc  disk_error
    cmp al, SECTORS
    jne disk_error
    jmp read_ok

disk_error:
    mov si, err_msg
    call BIOS_print
    jmp $

read_ok:
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x08:protected_mode

;* --DATA--

data:
msg:
    db "Hallo", 0
err_msg:
    db "DISK ERR", 0

gdt_start:

gdt_null:
    dq 0

; 0x08 — 32-bit protected-mode code
gdt_code32:
    dq 0x00CF9A000000FFFF

; 0x10 — 64-bit long-mode code
gdt_code64:
    dq 0x00AF9A000000FFFF

; 0x18 — data
gdt_data:
    dq 0x00CF92000000FFFF

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

;* --FUNCTIONS--

BIOS_print:
    mov ah, 0x0E
    mov bh, 0x00

.next:
    mov al, [si]
    cmp al, 0
    je .done

    int 0x10
    inc si
    jmp .next

.done:
    ret

[BITS 32]
protected_mode:
    mov ax, 0x18

    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov esp, 0x70000

    call enable_SSE2

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

enable_SSE2:
    ; Check for CPUID first
    pushfd
    pop eax
    mov ecx, eax
    xor eax, 1 << 21
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

    ; Enable SSE
    mov eax, cr0
    and eax, ~(1 << 2)    ; CR0.EM = 0
    or  eax, 1 << 1       ; CR0.MP = 1
    mov cr0, eax

    ; Enable OS support for FXSAVE/FXRSTOR and SSE
    mov eax, cr4
    or  eax, (1 << 9) | (1 << 10)   ; OSFXSR | OSXMMEXCPT
    mov cr4, eax

    ; SSE2 is now usable
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

;* --MAGIC--

times 510-($-$$) db 0
dw 0xAA55
