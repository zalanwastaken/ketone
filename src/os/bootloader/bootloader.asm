[ORG 0x7C00]
[BITS 16]

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

    mov ax, 0
    mov es, ax
    mov bx, 0x7D00
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

    jmp 0x08:0x7D00

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
