; dsBoot Stage 1 (MBR)
; Main bootloader that fits in the Master Boot Record (512 bytes)
; Loads Stage 2 from disk and transfers control

[BITS 16]       ; 16-bit Real Mode
[ORG 0x7C00]    ; BIOS loads the MBR at 0x7C00

; Jump over BIOS Parameter Block area
jmp short start
nop

; BIOS Parameter Block (to be safe with some BIOSes)
bpb_oem_id:                  db "dsBoot  "          ; 8 bytes
bpb_bytes_per_sector:        dw 512
bpb_sectors_per_cluster:     db 1
bpb_reserved_sectors:        dw 32                   ; Stage 2 starts at sector 1
bpb_num_fats:                db 2
bpb_root_dir_entries:        dw 224
bpb_total_sectors:           dw 2880                 ; 1.44 MB floppy
bpb_media_descriptor:        db 0xF0                 ; 3.5" floppy
bpb_sectors_per_fat:         dw 9
bpb_sectors_per_track:       dw 18
bpb_num_heads:               dw 2
bpb_hidden_sectors:          dd 0
bpb_total_sectors_big:       dd 0
bpb_drive_number:            db 0
bpb_reserved:                db 0
bpb_boot_signature:          db 0x29
bpb_volume_id:               dd 0x12345678
bpb_volume_label:            db "dsOS BOOT  "        ; 11 bytes
bpb_filesystem_type:         db "FAT12   "           ; 8 bytes

start:
    cli                 ; Disable interrupts
    xor ax, ax          ; Zero AX register
    mov ds, ax          ; Set DS=0
    mov es, ax          ; Set ES=0
    mov ss, ax          ; Set SS=0
    mov sp, 0x7C00      ; Set stack just below the MBR
    sti                 ; Enable interrupts
    
    ; Save boot drive number
    mov [bootDrive], dl
    
    ; Print boot message
    mov si, bootMsg
    call print_string
    
    ; Load stage 2 bootloader from disk
    mov si, loadingMsg
    call print_string
    
    ; Reset disk system
    xor ax, ax
    int 0x13
    jc disk_error       ; If carry flag set, disk error
    
    ; Read Stage 2 into memory at 0x7E00 (right after MBR)
    mov ax, 0x07E0      ; 0x7E00 / 16 (segment value)
    mov es, ax          ; Set ES to segment
    xor bx, bx          ; Offset 0
    
    ; Read sectors
    mov ah, 0x02        ; BIOS read sector function
    mov al, 32          ; Read 32 sectors (16KB)
    mov ch, 0           ; Cylinder 0
    mov cl, 2           ; Start from sector 2 (1-indexed, 1 is MBR)
    mov dh, 0           ; Head 0
    mov dl, [bootDrive] ; Drive number
    int 0x13
    jc disk_error       ; If carry flag set, disk error
    
    ; Check if we read the correct number of sectors
    cmp al, 32
    jne disk_error
    
    ; Print success message
    mov si, doneMsg
    call print_string
    
    ; Jump to stage 2
    mov si, startingMsg
    call print_string
    
    ; Jump to Stage 2
    jmp 0x07E0:0x0000

disk_error:
    mov si, diskErrorMsg
    call print_string
    jmp halt

halt:
    cli
    hlt
    jmp halt

; Function: print_string
; Input: SI = pointer to NULL-terminated string
print_string:
    push ax
    push bx
    mov ah, 0x0E        ; BIOS teletype function
    mov bh, 0           ; Page number
    mov bl, 0x07        ; Light gray on black
.loop:
    lodsb               ; Load byte from SI into AL and increment SI
    test al, al         ; Check if end of string (NULL)
    jz .done            ; If NULL, we're done
    int 0x10            ; Otherwise, print the character
    jmp .loop
.done:
    pop bx
    pop ax
    ret

; Data
bootMsg         db "dsBoot Stage 1", 0x0D, 0x0A, 0
loadingMsg      db "Loading Stage 2... ", 0
doneMsg         db "Done", 0x0D, 0x0A, 0
startingMsg     db "Starting Stage 2...", 0x0D, 0x0A, 0
diskErrorMsg    db "Disk error", 0x0D, 0x0A, 0
bootDrive       db 0

; Boot signature
times 510-($-$$) db 0   ; Fill with zeros up to 510 bytes
dw 0xAA55               ; Boot signature
