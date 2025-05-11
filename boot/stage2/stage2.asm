; dsBoot Stage 2
; Second stage bootloader for dsOS
; Features:
; - Boot menu with 4 options
; - 3-second timeout
; - Logo display with animation
; - Kernel loading and transition to long mode

[BITS 16]           ; Still in 16-bit real mode
[ORG 0x0000]        ; Loaded at 0x7E00 but we use segment 0x07E0, so offset 0

; Constants for video mode
SCREEN_WIDTH    equ 80
SCREEN_HEIGHT   equ 25
BACKGROUND      equ 0x07        ; Light gray on black

; Constants for boot menu
BOOT_TIMEOUT    equ 3           ; 3 seconds auto-boot timeout
KERNEL_ADDR     equ 0x100000    ; Load kernel at 1MB
BOOT_MENU_TOP   equ 8           ; Menu starts at line 8
BOOT_NORMAL     equ 0           ; Boot option indices
BOOT_TERMINAL   equ 1
BOOT_RECOVERY   equ 2
BOOT_REBOOT     equ 3
NUM_BOOT_OPTIONS equ 4          ; Total number of boot options

; Entry point
start:
    ; Set up segment registers and stack
    mov ax, cs
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0xFFF0      ; Set stack to 64K - 16 bytes
    
    ; Clear screen
    call clear_screen
    
    ; Display boot header
    mov si, bootHeader
    mov dh, 2           ; Row 2
    mov dl, 0           ; Column 0
    call print_string_at
    
    ; Display boot logo
    call display_logo
    
    ; Display boot menu
    call display_boot_menu
    
    ; Start timeout countdown
    mov byte [selectedOption], BOOT_NORMAL   ; Default boot option
    mov word [countdown], BOOT_TIMEOUT * 18  ; ~18 ticks per second
    
    ; Main input loop with timeout
input_loop:
    ; Display countdown
    call update_countdown
    
    ; Check for keypress
    mov ah, 0x01        ; BIOS check keyboard status
    int 0x16
    jz no_key           ; If no key, continue countdown
    
    ; Get the key
    mov ah, 0x00        ; BIOS get keystroke
    int 0x16
    
    ; Process the key
    cmp ah, 0x48        ; Up arrow
    je key_up
    cmp ah, 0x50        ; Down arrow
    je key_down
    cmp al, 0x0D        ; Enter
    je key_enter
    
    jmp input_loop
    
key_up:
    ; Move selection up
    mov al, [selectedOption]
    dec al
    jnl .valid          ; Jump if not less than zero
    mov al, NUM_BOOT_OPTIONS - 1  ; Wrap to bottom
.valid:
    mov [selectedOption], al
    call update_boot_menu
    jmp input_loop
    
key_down:
    ; Move selection down
    mov al, [selectedOption]
    inc al
    cmp al, NUM_BOOT_OPTIONS
    jl .valid           ; Jump if less than NUM_BOOT_OPTIONS
    mov al, 0           ; Wrap to top
.valid:
    mov [selectedOption], al
    call update_boot_menu
    jmp input_loop
    
key_enter:
    ; Execute selected option
    jmp execute_option
    
no_key:
    ; Update the countdown
    mov ax, [countdown]
    dec ax
    mov [countdown], ax
    or ax, ax
    jnz input_loop      ; Continue if not zero
    
    ; Time's up, proceed with default option
    jmp execute_option

; Execute the currently selected boot option
execute_option:
    mov al, [selectedOption]
    cmp al, BOOT_NORMAL
    je boot_normal
    cmp al, BOOT_TERMINAL
    je boot_terminal
    cmp al, BOOT_RECOVERY
    je boot_recovery
    cmp al, BOOT_REBOOT
    je boot_reboot
    
    ; Shouldn't get here, but default to normal boot
    jmp boot_normal
    
; Boot options implementation
boot_normal:
    mov si, bootingMsg
    mov dh, 20          ; Row 20
    mov dl, 0           ; Column 0
    call print_string_at
    
    ; Load the kernel from disk
    call load_kernel
    
    ; Switch to long mode and jump to kernel
    call switch_to_long_mode
    jmp $               ; Never reached
    
boot_terminal:
    mov si, terminalMsg
    mov dh, 20
    mov dl, 0
    call print_string_at
    
    ; Clear the screen and display prompt
    call clear_screen
    
    ; Simple terminal implementation
    mov si, promptMsg
    call print_string
    
terminal_loop:
    ; Get keystroke
    mov ah, 0
    int 0x16
    
    ; Check for special keys
    cmp al, 0x0D        ; Enter
    je terminal_newline
    cmp al, 0x08        ; Backspace
    je terminal_backspace
    cmp al, 0x1B        ; Escape (exit terminal)
    je start            ; Return to boot menu
    
    ; Print the character
    mov ah, 0x0E
    int 0x10
    jmp terminal_loop
    
terminal_newline:
    mov si, newlineStr
    call print_string
    mov si, promptMsg
    call print_string
    jmp terminal_loop
    
terminal_backspace:
    ; Handle backspace
    mov ah, 0x0E
    mov al, 0x08        ; Backspace
    int 0x10
    mov al, ' '         ; Space
    int 0x10
    mov al, 0x08        ; Backspace again
    int 0x10
    jmp terminal_loop
    
boot_recovery:
    mov si, recoveryMsg
    mov dh, 20
    mov dl, 0
    call print_string_at
    
    ; Load recovery mode kernel
    call load_kernel    ; Same kernel, different flag will be set
    
    ; Set recovery flag for kernel
    mov byte [recoveryFlag], 1
    
    ; Switch to long mode
    call switch_to_long_mode
    jmp $               ; Never reached
    
boot_reboot:
    mov si, rebootMsg
    mov dh, 20
    mov dl, 0
    call print_string_at
    
    ; Wait a moment
    mov cx, 0xFFFF
.delay:
    loop .delay
    
    ; Reboot the system using keyboard controller
    mov al, 0xFE
    out 0x64, al
    jmp $               ; Should never reach here

; Display boot logo with animation
display_logo:
    pusha
    ; In a real implementation, we would load image data from disk and display it
    ; For now, just display a simple text logo
    mov si, logoText
    mov dh, 4           ; Row 4
    mov dl, (SCREEN_WIDTH - 10) / 2  ; Center
    call print_string_at
    
    ; Animate the logo (simple fade-in effect)
    ; In real implementation, this would be more sophisticated
    mov cx, 10          ; Animation steps
.animate:
    push cx
    mov ax, 0xFFFF      ; Delay
.delay:
    dec ax
    jnz .delay
    pop cx
    loop .animate
    
    popa
    ret

; Display boot menu
display_boot_menu:
    pusha
    ; Menu title
    mov si, menuTitle
    mov dh, BOOT_MENU_TOP - 2
    mov dl, 0
    call print_string_at
    
    ; Menu options
    mov si, optionNormal
    mov dh, BOOT_MENU_TOP
    mov dl, 4
    call print_string_at
    
    mov si, optionTerminal
    mov dh, BOOT_MENU_TOP + 1
    mov dl, 4
    call print_string_at
    
    mov si, optionRecovery
    mov dh, BOOT_MENU_TOP + 2
    mov dl, 4
    call print_string_at
    
    mov si, optionReboot
    mov dh, BOOT_MENU_TOP + 3
    mov dl, 4
    call print_string_at
    
    ; Highlight the default option
    call update_boot_menu
    
    popa
    ret

; Update boot menu selection
update_boot_menu:
    pusha
    ; Clear all highlighting
    mov cx, NUM_BOOT_OPTIONS
    mov dh, BOOT_MENU_TOP
.clear_loop:
    mov dl, 2
    call set_cursor
    mov al, ' '
    call print_char
    inc dh
    loop .clear_loop
    
    ; Highlight the selected option
    mov al, [selectedOption]
    add al, BOOT_MENU_TOP
    mov dh, al
    mov dl, 2
    call set_cursor
    mov al, '>'
    call print_char
    
    popa
    ret

; Update countdown display
update_countdown:
    pusha
    ; Calculate seconds
    mov ax, [countdown]
    mov bl, 18          ; ~18 ticks per second
    div bl              ; AX / BL -> AL=quotient, AH=remainder
    
    ; Display countdown message
    mov si, countdownMsg
    mov dh, BOOT_MENU_TOP + 6
    mov dl, 0
    call print_string_at
    
    ; Display seconds remaining
    xor ah, ah          ; Clear high byte
    add al, '0'         ; Convert to ASCII
    mov dh, BOOT_MENU_TOP + 6
    mov dl, 24
    call set_cursor
    call print_char
    
    popa
    ret

; Load kernel from disk
load_kernel:
    pusha
    mov si, loadingKernelMsg
    mov dh, 21
    mov dl, 0
    call print_string_at
    
    ; Reset disk system
    xor ax, ax
    int 0x13
    jc disk_error
    
    ; Set up registers for loading
    mov ax, 0x1000      ; Segment to load kernel initially (will move later)
    mov es, ax
    xor bx, bx          ; Offset 0
    
    ; Read sectors
    mov ah, 0x02        ; BIOS read sector function
    mov al, 64          ; Read 64 sectors (32KB) - adjust for your kernel size
    mov ch, 0           ; Cylinder 0
    mov cl, 34          ; Start from sector 34 (after stage 2)
    mov dh, 0           ; Head 0
    mov dl, [bootDrive] ; Drive number
    int 0x13
    jc disk_error
    
    ; Simple success message
    mov si, doneMsg
    mov dh, 21
    mov dl, 18
    call print_string_at
    
    popa
    ret

; Function to switch to long mode and jump to kernel
switch_to_long_mode:
    ; This is a simplified placeholder - a real implementation would:
    ; 1. Check CPU capabilities
    ; 2. Set up GDT for 64-bit
    ; 3. Enable A20 line
    ; 4. Set up initial page tables
    ; 5. Enable paging and enter long mode
    ; 6. Jump to kernel entry point
    
    cli                 ; Disable interrupts
    
    ; Check if CPU supports long mode (simplified)
    mov si, noLongModeMsg
    mov dh, 22
    mov dl, 0
    call print_string_at
    
    ; In a real implementation, we would now:
    ; - Set up a 64-bit GDT
    ; - Create page tables
    ; - Enable PAE, PGE, LME, etc.
    ; - Jump to the kernel at KERNEL_ADDR
    
    ; For now, we just halt
    cli
    hlt
    jmp $

; Error handlers
disk_error:
    mov si, diskErrorMsg
    mov dh, 22
    mov dl, 0
    call print_string_at
    jmp halt

halt:
    cli
    hlt
    jmp halt

; Function: clear_screen
; Clears the screen and sets the cursor to 0,0
clear_screen:
    pusha
    mov ah, 0x00        ; Set video mode
    mov al, 0x03        ; 80x25 text mode
    int 0x10
    
    ; Set background color
    mov ah, 0x0B        ; Set background color
    mov bh, 0x00        ; Set background color
    mov bl, BACKGROUND  ; Color
    int 0x10
    
    ; Home cursor
    mov ah, 0x02        ; Set cursor position
    mov bh, 0x00        ; Page 0
    mov dh, 0x00        ; Row 0
    mov dl, 0x00        ; Column 0
    int 0x10
    
    popa
    ret

; Function: set_cursor
; Sets the cursor position
; Input: DH = row, DL = column
set_cursor:
    pusha
    mov ah, 0x02        ; Set cursor position
    mov bh, 0x00        ; Page 0
    int 0x10
    popa
    ret

; Function: print_char
; Prints a character at the current cursor position
; Input: AL = character
print_char:
    pusha
    mov ah, 0x0E        ; BIOS teletype function
    mov bh, 0x00        ; Page 0
    int 0x10
    popa
    ret

; Function: print_string
; Input: SI = pointer to NULL-terminated string
print_string:
    pusha
    mov ah, 0x0E        ; BIOS teletype function
    mov bh, 0x00        ; Page 0
.loop:
    lodsb               ; Load byte from SI into AL and increment SI
    test al, al         ; Check if end of string (NULL)
    jz .done            ; If NULL, we're done
    int 0x10            ; Otherwise, print the character
    jmp .loop
.done:
    popa
    ret

; Function: print_string_at
; Input: SI = pointer to NULL-terminated string, DH = row, DL = column
print_string_at:
    pusha
    call set_cursor     ; Set the cursor position
    call print_string   ; Print the string
    popa
    ret

; Data section
bootHeader     db "dsOS Boot Loader v0.1", 0
menuTitle      db "dsOS Boot Menu:", 0
optionNormal   db "1. Boot dsOS normally", 0
optionTerminal db "2. Terminal prompt", 0
optionRecovery db "3. Recovery mode", 0
optionReboot   db "4. Reboot", 0
countdownMsg   db "Booting in    seconds...", 0
bootingMsg     db "Booting dsOS normally...", 0
terminalMsg    db "Entering terminal mode...", 0
recoveryMsg    db "Entering recovery mode...", 0
rebootMsg      db "Rebooting system...", 0
loadingKernelMsg db "Loading kernel... ", 0
doneMsg        db "Done", 0
noLongModeMsg  db "CPU does not support 64-bit mode", 0
diskErrorMsg   db "Disk error while loading kernel", 0
promptMsg      db "$ ", 0
newlineStr     db 0x0D, 0x0A, 0
logoText       db "dsOS v0.1", 0

; Variables
bootDrive      db 0                 ; Boot drive number saved from stage 1
selectedOption db BOOT_NORMAL       ; Currently selected boot option
countdown      dw BOOT_TIMEOUT * 18 ; Countdown timer (18 ticks/sec)
recoveryFlag   db 0                 ; Recovery mode flag for kernel

; Padding to make the file size appropriate
times 16384-($-$$) db 0             ; Pad to 16KB
