[ORG 0x7C00]
[BITS 16]

E820_MAP       equ 0x8F04
E820_MAX_ENTRIES equ 64
E820_ENTRY_SIZE equ 24

start:
    xor ax, ax
    mov ds, ax
    mov es, ax

    mov si, msg
    call BIOS_print

    call get_memory_map
    jc memory_map_error

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

    mov ax, 0
    mov es, ax
    mov bx, 0x7E00
    mov ah, 0x02
    mov al, 1
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, 0x80
    int 0x13
    jc  disk_error
    cmp al, 1
    jne disk_error
    ;jmp read_ok

read_ok:
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x08:0x7E00

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

disk_error:
    mov si, err_msg_disk
    call BIOS_print
    jmp $

memory_map_error:
    mov si, err_msg_memory_map
    call BIOS_print
    jmp $

get_memory_map:
    push es
    push di
    push bp

    xor ax, ax
    mov es, ax

    mov di, E820_MAP
    mov word [es:di], 0
    add di, 2

    xor ebx, ebx
    xor bp, bp

.next:
    cmp bp, E820_MAX_ENTRIES
    jae .failed

    ; Initialize descriptor.
    mov dword [es:di], 0
    mov dword [es:di + 4], 0
    mov dword [es:di + 8], 0
    mov dword [es:di + 12], 0
    mov dword [es:di + 16], 0
    mov dword [es:di + 20], 1

    mov eax, 0xE820
    mov edx, 0x534D4150
    mov ecx, E820_ENTRY_SIZE

    int 0x15
    jc .failed

    cmp eax, 0x534D4150
    jne .failed

    cmp ecx, 20
    jb .failed

    ; Ignore zero-length regions.
    mov eax, [es:di + 8]
    or eax, [es:di + 12]
    jz .skip

    inc bp
    mov [es:E820_MAP], bp

    add di, E820_ENTRY_SIZE

.skip:
    test ebx, ebx
    jnz .next

    clc
    jmp .done

.failed:
    stc

.done:
    pop bp
    pop di
    pop es
    ret

;* --DATA--
data:
msg:
    db "Hallo", 0
err_msg_disk:
    db "DISK ERR", 0
err_msg_memory_map:
    db "MEMORY MAP ERR", 0

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

;* --MAGIC--

; 446 bytes of boot code before partition table
times 0x1BE - ($ - $$) db 0

; Partition 1
db 0x00                    ; not bootable

; starting CHS
db 0xFE
db 0xFF
db 0xFF

db 0x07                    ; exFAT / NTFS

; ending CHS
db 0xFE
db 0xFF
db 0xFF

dd 8 + SECTORS            ; partition start LBA
dd 524288 - (8 + SECTORS) ; partition size in sectors

; partitions 2-4 unused
times 3 * 16 db 0

dw 0xAA55
