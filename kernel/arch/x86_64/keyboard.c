/**
 * @file keyboard.c
 * @brief PS/2 Keyboard controller implementation
 */

#include "../../include/kernel.h"
#include <stdint.h>
#include <stdbool.h>

// PS/2 Keyboard I/O ports
#define KB_DATA_PORT     0x60    // Data port
#define KB_STATUS_PORT   0x64    // Status register
#define KB_COMMAND_PORT  0x64    // Command register

// PS/2 Controller commands
#define KB_CMD_READ_CONFIG   0x20    // Read controller configuration
#define KB_CMD_WRITE_CONFIG  0x60    // Write controller configuration
#define KB_CMD_DISABLE_PORT2 0xA7    // Disable second PS/2 port
#define KB_CMD_ENABLE_PORT2  0xA8    // Enable second PS/2 port
#define KB_CMD_TEST_PORT2    0xA9    // Test second PS/2 port
#define KB_CMD_TEST_CTRL     0xAA    // Test PS/2 controller
#define KB_CMD_TEST_PORT1    0xAB    // Test first PS/2 port
#define KB_CMD_DISABLE_PORT1 0xAD    // Disable first PS/2 port
#define KB_CMD_ENABLE_PORT1  0xAE    // Enable first PS/2 port

// Keyboard commands
#define KB_RESET           0xFF    // Reset keyboard
#define KB_ENABLE_SCANNING 0xF4    // Enable scanning
#define KB_DISABLE_SCANNING 0xF5   // Disable scanning
#define KB_SET_DEFAULTS    0xF6    // Set default parameters
#define KB_SET_TYPEMATIC   0xF3    // Set typematic rate/delay

// PS/2 Controller status register bits
#define KB_STATUS_OUTPUT_FULL  0x01    // Output buffer full (data available)
#define KB_STATUS_INPUT_FULL   0x02    // Input buffer full (don't write)
#define KB_STATUS_SYSTEM_FLAG  0x04    // System flag
#define KB_STATUS_COMMAND_DATA 0x08    // 0 = Data write, 1 = Command write
#define KB_STATUS_TIMEOUT      0x40    // Timeout error
#define KB_STATUS_PARITY_ERR   0x80    // Parity error

// PS/2 Controller configuration bits
#define KB_CONFIG_PORT1_INT    0x01    // Enable interrupt for first PS/2 port
#define KB_CONFIG_PORT2_INT    0x02    // Enable interrupt for second PS/2 port
#define KB_CONFIG_SYSTEM_FLAG  0x04    // System flag
#define KB_CONFIG_PORT1_CLOCK  0x10    // First PS/2 port clock disabled
#define KB_CONFIG_PORT2_CLOCK  0x20    // Second PS/2 port clock disabled
#define KB_CONFIG_PORT1_TRANS  0x40    // First PS/2 port translation enabled

// Scan code set 1 - Special keys
#define SC_ESCAPE       0x01
#define SC_BACKSPACE    0x0E
#define SC_TAB          0x0F
#define SC_ENTER        0x1C
#define SC_LCTRL        0x1D
#define SC_LSHIFT       0x2A
#define SC_RSHIFT       0x36
#define SC_LALT         0x38
#define SC_CAPSLOCK     0x3A
#define SC_F1           0x3B
#define SC_F2           0x3C
#define SC_F3           0x3D
#define SC_F4           0x3E
#define SC_F5           0x3F
#define SC_F6           0x40
#define SC_F7           0x41
#define SC_F8           0x42
#define SC_F9           0x43
#define SC_F10          0x44
#define SC_F11          0x57
#define SC_F12          0x58
#define SC_NUMLOCK      0x45
#define SC_SCROLLLOCK   0x46
#define SC_RELEASED     0x80

// Keyboard state
static bool shift_pressed = false;
static bool ctrl_pressed = false;
static bool alt_pressed = false;
static bool caps_lock = false;
static bool num_lock = false;
static bool scroll_lock = false;
static bool extended_key = false;

// US QWERTY layout
static const char kbd_us[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// US QWERTY layout with shift
static const char kbd_us_shift[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// Keyboard callback function pointer
typedef void (*keyboard_callback_t)(uint8_t scancode, char ascii);
static keyboard_callback_t keyboard_callback = NULL;

/**
 * @brief Wait for the keyboard controller to be ready for commands
 * 
 * @return true if ready for commands, false if timeout
 */
static bool kb_wait_for_controller(void) {
    // Try up to 1000 times before giving up
    for (int timeout = 1000; timeout > 0; timeout--) {
        // Check if input buffer is empty (ready for commands)
        if ((inb(KB_STATUS_PORT) & KB_STATUS_INPUT_FULL) == 0) {
            return true;
        }
        io_wait();
    }
    return false;
}

/**
 * @brief Wait for data to be available from the keyboard
 * 
 * @return true if data is available, false if timeout
 */
static bool kb_wait_for_data(void) {
    // Try up to 1000 times before giving up
    for (int timeout = 1000; timeout > 0; timeout--) {
        // Check if output buffer has data
        if (inb(KB_STATUS_PORT) & KB_STATUS_OUTPUT_FULL) {
            return true;
        }
        io_wait();
    }
    return false;
}

/**
 * @brief Send a command to the keyboard controller
 * 
 * @param command Command to send
 * @return true if command was sent successfully, false otherwise
 */
static bool kb_send_command(uint8_t command) {
    if (kb_wait_for_controller()) {
        outb(KB_COMMAND_PORT, command);
        return true;
    }
    return false;
}

/**
 * @brief Send a command to the keyboard itself
 * 
 * @param command Command to send
 * @return true if command was sent successfully, false otherwise
 */
static bool kb_send_keyboard_command(uint8_t command) {
    if (kb_wait_for_controller()) {
        outb(KB_DATA_PORT, command);
        return true;
    }
    return false;
}

/**
 * @brief Read data from the keyboard
 * 
 * @return Data read from the keyboard, or 0 if timeout
 */
static uint8_t kb_read_data(void) {
    if (kb_wait_for_data()) {
        return inb(KB_DATA_PORT);
    }
    return 0;
}

/**
 * @brief Register a keyboard callback function
 * 
 * @param callback Function to call when a key is pressed
 */
void kbd_register_callback(keyboard_callback_t callback) {
    keyboard_callback = callback;
}

/**
 * @brief Convert a scan code to ASCII
 * 
 * @param scancode Scan code to convert
 * @return ASCII character, or 0 if not a printable character
 */
static char scancode_to_ascii(uint8_t scancode) {
    // Ignore key releases and extended scancodes
    if (scancode & SC_RELEASED || extended_key) {
        return 0;
    }
    
    // Convert scan code to ASCII
    char ascii = 0;
    
    // Check if we should use the shifted layout
    bool use_shift = shift_pressed ^ caps_lock;
    
    // Get the ASCII value from the appropriate layout
    if (scancode < 128) {
        ascii = use_shift ? kbd_us_shift[scancode] : kbd_us[scancode];
    }
    
    return ascii;
}

/**
 * @brief Process a keyboard scancode
 * 
 * @param scancode Scan code to process
 */
static void process_scancode(uint8_t scancode) {
    // Check for extended key sequence (e.g., arrow keys)
    if (scancode == 0xE0) {
        extended_key = true;
        return;
    }
    
    // Check if this is a key release (bit 7 set)
    bool released = (scancode & SC_RELEASED) != 0;
    uint8_t key = scancode & ~SC_RELEASED;
    
    // Handle key states
    if (extended_key) {
        // Handle extended keys
        extended_key = false;
    } else {
        // Handle normal keys
        switch (key) {
            case SC_LSHIFT:
            case SC_RSHIFT:
                shift_pressed = !released;
                break;
                
            case SC_LCTRL:
                ctrl_pressed = !released;
                break;
                
            case SC_LALT:
                alt_pressed = !released;
                break;
                
            case SC_CAPSLOCK:
                if (!released) {
                    caps_lock = !caps_lock;
                }
                break;
                
            case SC_NUMLOCK:
                if (!released) {
                    num_lock = !num_lock;
                }
                break;
                
            case SC_SCROLLLOCK:
                if (!released) {
                    scroll_lock = !scroll_lock;
                }
                break;
                
            default:
                // Only process key presses, not releases
                if (!released) {
                    char ascii = scancode_to_ascii(scancode);
                    
                    // Call the registered callback if available
                    if (keyboard_callback && ascii) {
                        keyboard_callback(scancode, ascii);
                    }
                }
                break;
        }
    }
}

/**
 * @brief Keyboard interrupt handler
 */
static void keyboard_handler(void) {
    // Read scan code from the keyboard
    uint8_t scancode = inb(KB_DATA_PORT);
    
    // Process the scan code
    process_scancode(scancode);
}

/**
 * @brief Initialize the keyboard controller
 */
void kbd_init(void) {
    // Disable both PS/2 ports
    kb_send_command(KB_CMD_DISABLE_PORT1);
    kb_send_command(KB_CMD_DISABLE_PORT2);
    
    // Flush the output buffer
    inb(KB_DATA_PORT);
    
    // Read the current configuration
    kb_send_command(KB_CMD_READ_CONFIG);
    uint8_t config = kb_read_data();
    
    // Set the configuration: enable port 1 interrupts, disable port 2
    config |= KB_CONFIG_PORT1_INT;
    config &= ~KB_CONFIG_PORT2_INT;
    kb_send_command(KB_CMD_WRITE_CONFIG);
    outb(KB_DATA_PORT, config);
    
    // Enable the first PS/2 port
    kb_send_command(KB_CMD_ENABLE_PORT1);
    
    // Reset the keyboard
    kb_send_keyboard_command(KB_RESET);
    
    // Wait for the keyboard to acknowledge
    uint8_t response = kb_read_data();
    if (response != 0xFA) {
        kprintf("Keyboard: Reset failed, response = 0x%02x\n", response);
    } else {
        // Wait for self-test completion
        response = kb_read_data();
        if (response != 0xAA) {
            kprintf("Keyboard: Self-test failed, response = 0x%02x\n", response);
        }
    }
    
    // Set default parameters
    kb_send_keyboard_command(KB_SET_DEFAULTS);
    
    // Enable scanning
    kb_send_keyboard_command(KB_ENABLE_SCANNING);
    
    // Register the keyboard interrupt handler
    register_interrupt_handler(33, (interrupt_handler_t)keyboard_handler);
    
    // Unmask (enable) IRQ1 in the PIC
    extern void pic_unmask_irq(uint8_t);
    pic_unmask_irq(1);
    
    kprintf("Keyboard: Initialized\n");
    
    // Keyboard is now ready
    extern bool kbd_ready;
    kbd_ready = true;
}
