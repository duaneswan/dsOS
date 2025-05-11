/**
 * @file keyboard.c
 * @brief PS/2 Keyboard driver
 */

#include "../../include/kernel.h"
#include <stdint.h>
#include <stdbool.h>

// IRQ lines (from pic.c)
#define IRQ_TIMER         0           // Timer IRQ
#define IRQ_KEYBOARD      1           // Keyboard IRQ
#define IRQ_CASCADE       2           // Cascade IRQ (used internally by the PICs)
#define IRQ_COM2          3           // COM2 IRQ
#define IRQ_COM1          4           // COM1 IRQ
#define IRQ_LPT2          5           // LPT2 IRQ
#define IRQ_FLOPPY        6           // Floppy disk IRQ
#define IRQ_LPT1          7           // LPT1 IRQ (spurious)
#define IRQ_RTC           8           // Real-time clock IRQ
#define IRQ_ACPI          9           // ACPI IRQ
#define IRQ_AVAILABLE1    10          // Available IRQ
#define IRQ_AVAILABLE2    11          // Available IRQ
#define IRQ_PS2_MOUSE     12          // PS/2 mouse IRQ
#define IRQ_FPU           13          // FPU IRQ
#define IRQ_ATA_PRIMARY   14          // Primary ATA IRQ
#define IRQ_ATA_SECONDARY 15          // Secondary ATA IRQ

// PIC interrupt offsets (from pic.c)
#define PIC1_OFFSET       0x20        // Master PIC base interrupt number
#define PIC2_OFFSET       0x28        // Slave PIC base interrupt number

// PS/2 controller ports
#define PS2_DATA_PORT         0x60    // Data port (read/write)
#define PS2_STATUS_PORT       0x64    // Status register (read only)
#define PS2_COMMAND_PORT      0x64    // Command register (write only)

// PS/2 controller status register bits
#define PS2_STATUS_OUTPUT     0x01    // Output buffer status (0 = empty, 1 = full)
#define PS2_STATUS_INPUT      0x02    // Input buffer status (0 = empty, 1 = full)
#define PS2_STATUS_SYSTEM     0x04    // System flag (0 = power on reset, 1 = diagnostic passed)
#define PS2_STATUS_COMMAND    0x08    // Command/data (0 = data written to input was for PS/2 device, 1 = command written to input was for PS/2 controller)
#define PS2_STATUS_TIMEOUT    0x40    // Timeout error
#define PS2_STATUS_PARITY     0x80    // Parity error

// PS/2 controller commands
#define PS2_CMD_READ_CONFIG   0x20    // Read controller configuration byte
#define PS2_CMD_WRITE_CONFIG  0x60    // Write controller configuration byte
#define PS2_CMD_DISABLE_P2    0xA7    // Disable second PS/2 port
#define PS2_CMD_ENABLE_P2     0xA8    // Enable second PS/2 port
#define PS2_CMD_TEST_P2       0xA9    // Test second PS/2 port
#define PS2_CMD_TEST_CTRL     0xAA    // Test PS/2 controller
#define PS2_CMD_TEST_P1       0xAB    // Test first PS/2 port
#define PS2_CMD_DISABLE_P1    0xAD    // Disable first PS/2 port
#define PS2_CMD_ENABLE_P1     0xAE    // Enable first PS/2 port

// PS/2 device commands
#define PS2_DEV_RESET         0xFF    // Reset device
#define PS2_DEV_DISABLE_SCAN  0xF5    // Disable scanning
#define PS2_DEV_ENABLE_SCAN   0xF4    // Enable scanning
#define PS2_DEV_SET_DEFAULTS  0xF6    // Set default parameters
#define PS2_DEV_SET_LEDS      0xED    // Set keyboard LEDs

// Controller configuration byte bits
#define PS2_CONFIG_P1_INT     0x01    // Enable interrupt for first PS/2 port
#define PS2_CONFIG_P2_INT     0x02    // Enable interrupt for second PS/2 port
#define PS2_CONFIG_SYSTEM     0x04    // System flag
#define PS2_CONFIG_P1_CLOCK   0x10    // First PS/2 port clock (0 = enable, 1 = disable)
#define PS2_CONFIG_P2_CLOCK   0x20    // Second PS/2 port clock (0 = enable, 1 = disable)
#define PS2_CONFIG_P1_TRANS   0x40    // First PS/2 port translation (0 = disable, 1 = enable)

// Keyboard LED bits
#define KB_LED_SCROLL_LOCK    0x01    // Scroll Lock LED
#define KB_LED_NUM_LOCK       0x02    // Num Lock LED
#define KB_LED_CAPS_LOCK      0x04    // Caps Lock LED

// Keyboard special keys and modifiers
#define KB_KEY_ESCAPE         0x01
#define KB_KEY_BACKSPACE      0x0E
#define KB_KEY_TAB            0x0F
#define KB_KEY_ENTER          0x1C
#define KB_KEY_CTRL           0x1D
#define KB_KEY_LEFT_SHIFT     0x2A
#define KB_KEY_RIGHT_SHIFT    0x36
#define KB_KEY_ALT            0x38
#define KB_KEY_CAPS_LOCK      0x3A
#define KB_KEY_F1             0x3B
#define KB_KEY_F2             0x3C
#define KB_KEY_F3             0x3D
#define KB_KEY_F4             0x3E
#define KB_KEY_F5             0x3F
#define KB_KEY_F6             0x40
#define KB_KEY_F7             0x41
#define KB_KEY_F8             0x42
#define KB_KEY_F9             0x43
#define KB_KEY_F10            0x44
#define KB_KEY_F11            0x57
#define KB_KEY_F12            0x58
#define KB_KEY_NUM_LOCK       0x45
#define KB_KEY_SCROLL_LOCK    0x46

// Extended key flag
#define KB_EXTENDED_FLAG      0xE0

// Keyboard state flags
#define KB_STATE_SHIFT        0x01
#define KB_STATE_CTRL         0x02
#define KB_STATE_ALT          0x04
#define KB_STATE_CAPS_LOCK    0x08
#define KB_STATE_NUM_LOCK     0x10
#define KB_STATE_SCROLL_LOCK  0x20
#define KB_STATE_EXTENDED     0x40

// Maximum number of keys that can be pressed simultaneously
#define KB_MAX_PRESSED_KEYS   6

// Keyboard state
static uint8_t kb_state = 0;
static uint8_t kb_leds = 0;
static uint8_t kb_pressed_keys[KB_MAX_PRESSED_KEYS] = {0};
static uint8_t kb_pressed_count = 0;

// Default US QWERTY scancode to ASCII mapping (set 1)
static const char kb_us_layout[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// Shifted US layout
static const char kb_us_shift_layout[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// Numpad keys (when Num Lock is on)
static const char kb_numpad_layout[16] = {
    '7', '8', '9', '-',
    '4', '5', '6', '+',
    '1', '2', '3', '0',
    '.', 0, 0, 0
};

// Keyboard callback function type
typedef void (*kb_callback_t)(uint8_t scancode, char ascii);

// Keyboard callbacks
static kb_callback_t kb_callback = NULL;

/**
 * @brief Wait for the PS/2 controller to be ready for input
 * 
 * @return true if ready, false if timeout
 */
static bool ps2_wait_input(void) {
    for (int i = 0; i < 1000; i++) {
        if ((inb(PS2_STATUS_PORT) & PS2_STATUS_INPUT) == 0) {
            return true;
        }
        io_wait();
    }
    return false;
}

/**
 * @brief Wait for the PS/2 controller to have output data available
 * 
 * @return true if data available, false if timeout
 */
static bool ps2_wait_output(void) {
    for (int i = 0; i < 1000; i++) {
        if (inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT) {
            return true;
        }
        io_wait();
    }
    return false;
}

/**
 * @brief Send a command to the PS/2 controller
 * 
 * @param command Command byte
 * @return true if successful, false if failed
 */
static bool ps2_send_command(uint8_t command) {
    if (!ps2_wait_input()) {
        return false;
    }
    outb(PS2_COMMAND_PORT, command);
    return true;
}

/**
 * @brief Send a command with data to the PS/2 controller
 * 
 * @param command Command byte
 * @param data Data byte
 * @return true if successful, false if failed
 */
static bool ps2_send_command_data(uint8_t command, uint8_t data) {
    if (!ps2_send_command(command)) {
        return false;
    }
    if (!ps2_wait_input()) {
        return false;
    }
    outb(PS2_DATA_PORT, data);
    return true;
}

/**
 * @brief Send a byte to the PS/2 device (keyboard)
 * 
 * @param data Data byte
 * @return true if successful, false if failed
 */
static bool ps2_write_device(uint8_t data) {
    if (!ps2_wait_input()) {
        return false;
    }
    outb(PS2_DATA_PORT, data);
    if (!ps2_wait_output()) {
        return false;
    }
    return (inb(PS2_DATA_PORT) == 0xFA); // ACK
}

/**
 * @brief Update keyboard LEDs based on lock key states
 */
static void kb_update_leds(void) {
    uint8_t leds = 0;
    
    if (kb_state & KB_STATE_SCROLL_LOCK) {
        leds |= KB_LED_SCROLL_LOCK;
    }
    if (kb_state & KB_STATE_NUM_LOCK) {
        leds |= KB_LED_NUM_LOCK;
    }
    if (kb_state & KB_STATE_CAPS_LOCK) {
        leds |= KB_LED_CAPS_LOCK;
    }
    
    if (kb_leds != leds) {
        kb_leds = leds;
        ps2_write_device(PS2_DEV_SET_LEDS);
        ps2_write_device(kb_leds);
    }
}

/**
 * @brief Add a key to the pressed keys array
 * 
 * @param scancode Key scancode
 */
static void kb_add_pressed_key(uint8_t scancode) {
    // Don't add modifiers or extended keys
    if (scancode == KB_KEY_CTRL || scancode == KB_KEY_LEFT_SHIFT || 
        scancode == KB_KEY_RIGHT_SHIFT || scancode == KB_KEY_ALT || 
        scancode == KB_EXTENDED_FLAG) {
        return;
    }
    
    // Check if key is already in the pressed keys array
    for (int i = 0; i < kb_pressed_count; i++) {
        if (kb_pressed_keys[i] == scancode) {
            return;
        }
    }
    
    // Add key if there's room
    if (kb_pressed_count < KB_MAX_PRESSED_KEYS) {
        kb_pressed_keys[kb_pressed_count++] = scancode;
    }
}

/**
 * @brief Remove a key from the pressed keys array
 * 
 * @param scancode Key scancode
 */
static void kb_remove_pressed_key(uint8_t scancode) {
    // Find key in pressed keys array
    for (int i = 0; i < kb_pressed_count; i++) {
        if (kb_pressed_keys[i] == scancode) {
            // Remove key by shifting remaining keys
            for (int j = i; j < kb_pressed_count - 1; j++) {
                kb_pressed_keys[j] = kb_pressed_keys[j + 1];
            }
            kb_pressed_count--;
            break;
        }
    }
}

/**
 * @brief Convert a scancode to ASCII based on current keyboard state
 * 
 * @param scancode Key scancode
 * @return ASCII character or 0 if no ASCII equivalent
 */
static char kb_scancode_to_ascii(uint8_t scancode) {
    char ascii = 0;
    bool extended = (kb_state & KB_STATE_EXTENDED) != 0;
    bool shift = (kb_state & KB_STATE_SHIFT) != 0;
    bool caps = (kb_state & KB_STATE_CAPS_LOCK) != 0;
    bool numlock = (kb_state & KB_STATE_NUM_LOCK) != 0;
    
    // Clear extended flag
    kb_state &= ~KB_STATE_EXTENDED;
    
    // Handle regular keys
    if (!extended && scancode < 128) {
        if (shift) {
            ascii = kb_us_shift_layout[scancode];
        } else {
            ascii = kb_us_layout[scancode];
        }
        
        // Apply Caps Lock to letters
        if (caps && ascii >= 'a' && ascii <= 'z') {
            ascii = kb_us_shift_layout[scancode];
        } else if (caps && ascii >= 'A' && ascii <= 'Z') {
            ascii = kb_us_layout[scancode];
        }
    }
    
    // Handle extended keys
    if (extended) {
        // Numpad keys
        if (numlock && scancode >= 0x47 && scancode <= 0x53) {
            int index = scancode - 0x47;
            if (index < 16) {
                ascii = kb_numpad_layout[index];
            }
        }
    }
    
    return ascii;
}

/**
 * @brief Process a keyboard scancode
 * 
 * @param scancode Scancode received from keyboard
 */
static void kb_process_scancode(uint8_t scancode) {
    // Handle extended key prefix
    if (scancode == KB_EXTENDED_FLAG) {
        kb_state |= KB_STATE_EXTENDED;
        return;
    }
    
    // Check if it's a key release (bit 7 set)
    bool release = (scancode & 0x80) != 0;
    if (release) {
        scancode &= 0x7F;  // Clear release bit
        
        // Handle modifier key releases
        switch (scancode) {
            case KB_KEY_LEFT_SHIFT:
            case KB_KEY_RIGHT_SHIFT:
                kb_state &= ~KB_STATE_SHIFT;
                break;
                
            case KB_KEY_CTRL:
                kb_state &= ~KB_STATE_CTRL;
                break;
                
            case KB_KEY_ALT:
                kb_state &= ~KB_STATE_ALT;
                break;
        }
        
        // Remove from pressed keys
        kb_remove_pressed_key(scancode);
    } else {
        // Handle key press
        // Handle modifier key presses
        switch (scancode) {
            case KB_KEY_LEFT_SHIFT:
            case KB_KEY_RIGHT_SHIFT:
                kb_state |= KB_STATE_SHIFT;
                break;
                
            case KB_KEY_CTRL:
                kb_state |= KB_STATE_CTRL;
                break;
                
            case KB_KEY_ALT:
                kb_state |= KB_STATE_ALT;
                break;
                
            case KB_KEY_CAPS_LOCK:
                kb_state ^= KB_STATE_CAPS_LOCK;
                kb_update_leds();
                break;
                
            case KB_KEY_NUM_LOCK:
                kb_state ^= KB_STATE_NUM_LOCK;
                kb_update_leds();
                break;
                
            case KB_KEY_SCROLL_LOCK:
                kb_state ^= KB_STATE_SCROLL_LOCK;
                kb_update_leds();
                break;
                
            default:
                // Add to pressed keys
                kb_add_pressed_key(scancode);
                break;
        }
        
        // Convert to ASCII and call callback
        char ascii = kb_scancode_to_ascii(scancode);
        if (kb_callback != NULL && ascii != 0) {
            kb_callback(scancode, ascii);
        }
    }
}

/**
 * @brief Keyboard interrupt handler
 */
static void kb_handler(void) {
    // Read scancode from keyboard data port
    uint8_t scancode = inb(PS2_DATA_PORT);
    
    // Process the scancode
    kb_process_scancode(scancode);
    
    // Send EOI to PIC
    pic_send_eoi(IRQ_KEYBOARD);
}

/**
 * @brief Register a keyboard callback function
 * 
 * @param callback Function to call when a key is pressed
 */
void kb_register_callback(kb_callback_t callback) {
    kb_callback = callback;
}

/**
 * @brief Default keyboard callback that outputs to console
 * 
 * @param scancode Key scancode
 * @param ascii ASCII character
 */
static void kb_default_callback(uint8_t scancode, char ascii) {
    // Output the character to the VGA console
    vga_putchar(ascii);
}

/**
 * @brief Get the current keyboard state
 * 
 * @return Keyboard state flags
 */
uint8_t kb_get_state(void) {
    return kb_state;
}

/**
 * @brief Get an array of currently pressed keys
 * 
 * @param keys Array to fill with scancodes
 * @param max_keys Maximum number of keys to return
 * @return Number of keys copied
 */
uint8_t kb_get_pressed_keys(uint8_t* keys, uint8_t max_keys) {
    uint8_t count = (kb_pressed_count < max_keys) ? kb_pressed_count : max_keys;
    for (uint8_t i = 0; i < count; i++) {
        keys[i] = kb_pressed_keys[i];
    }
    return count;
}

/**
 * @brief Check if a specific key is currently pressed
 * 
 * @param scancode Key scancode to check
 * @return true if key is pressed, false otherwise
 */
bool kb_is_key_pressed(uint8_t scancode) {
    for (int i = 0; i < kb_pressed_count; i++) {
        if (kb_pressed_keys[i] == scancode) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Initialize the keyboard
 */
void kb_init(void) {
    // Reset the keyboard controller
    ps2_send_command(PS2_CMD_TEST_CTRL);
    if (!ps2_wait_output() || inb(PS2_DATA_PORT) != 0x55) {
        kprintf("KEYBOARD: Controller self-test failed\n");
        return;
    }
    
    // Disable ports during initialization
    ps2_send_command(PS2_CMD_DISABLE_P1);
    ps2_send_command(PS2_CMD_DISABLE_P2);
    
    // Flush output buffer
    while (inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT) {
        inb(PS2_DATA_PORT);
    }
    
    // Set the controller configuration
    ps2_send_command(PS2_CMD_READ_CONFIG);
    if (!ps2_wait_output()) {
        kprintf("KEYBOARD: Failed to read controller config\n");
        return;
    }
    
    uint8_t config = inb(PS2_DATA_PORT);
    
    // Disable translation, enable keyboard interrupts
    config |= PS2_CONFIG_P1_INT;    // Enable keyboard interrupts
    config &= ~PS2_CONFIG_P1_TRANS; // Disable translation
    
    ps2_send_command_data(PS2_CMD_WRITE_CONFIG, config);
    
    // Re-enable first port (keyboard)
    ps2_send_command(PS2_CMD_ENABLE_P1);
    
    // Reset the keyboard
    if (!ps2_write_device(PS2_DEV_RESET)) {
        kprintf("KEYBOARD: Reset failed\n");
        return;
    }
    
    // Wait for self-test completion
    if (!ps2_wait_output() || inb(PS2_DATA_PORT) != 0xAA) {
        kprintf("KEYBOARD: Self-test failed\n");
        return;
    }
    
    // Set default parameters
    ps2_write_device(PS2_DEV_SET_DEFAULTS);
    
    // Enable scanning
    ps2_write_device(PS2_DEV_ENABLE_SCAN);
    
    // Set initial LED state
    kb_state = KB_STATE_NUM_LOCK;  // Num Lock on by default
    kb_update_leds();
    
    // Register the keyboard interrupt handler
    register_interrupt_handler(IRQ_KEYBOARD + PIC1_OFFSET, kb_handler);
    
    // Unmask the keyboard IRQ
    pic_unmask_irq(IRQ_KEYBOARD);
    
    // Register default callback
    kb_register_callback(kb_default_callback);
    
    kprintf("KEYBOARD: Initialized\n");
}
