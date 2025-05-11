/**
 * @file keyboard.c
 * @brief Keyboard controller driver
 */

#include "../../include/kernel.h"
#include <stdint.h>
#include <stdbool.h>

// Keyboard controller ports
#define KEYBOARD_DATA_PORT      0x60
#define KEYBOARD_STATUS_PORT    0x64
#define KEYBOARD_COMMAND_PORT   0x64

// Keyboard controller commands
#define KEYBOARD_CMD_READ_CONFIG    0x20
#define KEYBOARD_CMD_WRITE_CONFIG   0x60
#define KEYBOARD_CMD_SELF_TEST      0xAA
#define KEYBOARD_CMD_INTERFACE_TEST 0xAB
#define KEYBOARD_CMD_ENABLE         0xAE
#define KEYBOARD_CMD_DISABLE        0xAD
#define KEYBOARD_CMD_READ_INPUT     0xC0

// Keyboard controller status register bits
#define KEYBOARD_STATUS_OUTPUT_FULL 0x01
#define KEYBOARD_STATUS_INPUT_FULL  0x02
#define KEYBOARD_STATUS_SYSTEM_FLAG 0x04
#define KEYBOARD_STATUS_COMMAND     0x08
#define KEYBOARD_STATUS_UNLOCKED    0x10
#define KEYBOARD_STATUS_AUX_OUTPUT  0x20
#define KEYBOARD_STATUS_TIMEOUT     0x40
#define KEYBOARD_STATUS_PARITY      0x80

// Keyboard controller config register bits
#define KEYBOARD_CONFIG_INT         0x01
#define KEYBOARD_CONFIG_AUX_INT     0x02
#define KEYBOARD_CONFIG_DISABLE     0x10
#define KEYBOARD_CONFIG_DISABLE_AUX 0x20
#define KEYBOARD_CONFIG_SELF_TEST   0x40
#define KEYBOARD_CONFIG_AUX_TEST    0x80

// Keyboard IRQ
#define KEYBOARD_IRQ               1

// Keyboard keys
#define KEY_ESCAPE                 0x01
#define KEY_BACKSPACE              0x0E
#define KEY_TAB                    0x0F
#define KEY_ENTER                  0x1C
#define KEY_LEFT_CONTROL           0x1D
#define KEY_LEFT_SHIFT             0x2A
#define KEY_RIGHT_SHIFT            0x36
#define KEY_LEFT_ALT               0x38
#define KEY_CAPS_LOCK              0x3A
#define KEY_F1                     0x3B
#define KEY_F2                     0x3C
#define KEY_F3                     0x3D
#define KEY_F4                     0x3E
#define KEY_F5                     0x3F
#define KEY_F6                     0x40
#define KEY_F7                     0x41
#define KEY_F8                     0x42
#define KEY_F9                     0x43
#define KEY_F10                    0x44
#define KEY_F11                    0x57
#define KEY_F12                    0x58
#define KEY_NUM_LOCK               0x45
#define KEY_SCROLL_LOCK            0x46
#define KEY_RIGHT_CONTROL          0x61
#define KEY_RIGHT_ALT              0x64

// Keyboard modifiers
#define KEY_MOD_CONTROL            0x01
#define KEY_MOD_SHIFT              0x02
#define KEY_MOD_ALT                0x04
#define KEY_MOD_CAPS_LOCK          0x08
#define KEY_MOD_NUM_LOCK           0x10
#define KEY_MOD_SCROLL_LOCK        0x20

// Key state
#define KEY_PRESSED                0x00
#define KEY_RELEASED               0x80

// US QWERTY keymap
static const char keymap_us[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-',
    0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// US QWERTY keymap with shift
static const char keymap_us_shift[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-',
    0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// Keyboard state
static uint8_t keyboard_modifiers = 0;
static volatile bool keyboard_ready = false;

// Keyboard callback
typedef void (*keyboard_callback_t)(uint8_t scancode, char character);
static keyboard_callback_t keyboard_callback = NULL;

/**
 * @brief Wait for the keyboard controller input buffer to be empty
 */
static void keyboard_wait_input(void) {
    uint32_t timeout = 100000;
    while (timeout-- && (inb(KEYBOARD_STATUS_PORT) & KEYBOARD_STATUS_INPUT_FULL)) {
        io_wait();
    }
}

/**
 * @brief Wait for the keyboard controller output buffer to be full
 */
static void keyboard_wait_output(void) {
    uint32_t timeout = 100000;
    while (timeout-- && !(inb(KEYBOARD_STATUS_PORT) & KEYBOARD_STATUS_OUTPUT_FULL)) {
        io_wait();
    }
}

/**
 * @brief Send a command to the keyboard controller
 * 
 * @param command Command byte
 */
static void keyboard_send_command(uint8_t command) {
    keyboard_wait_input();
    outb(KEYBOARD_COMMAND_PORT, command);
}

/**
 * @brief Send data to the keyboard controller
 * 
 * @param data Data byte
 */
static void keyboard_send_data(uint8_t data) {
    keyboard_wait_input();
    outb(KEYBOARD_DATA_PORT, data);
}

/**
 * @brief Read data from the keyboard controller
 * 
 * @return Data byte read
 */
static uint8_t keyboard_read_data(void) {
    keyboard_wait_output();
    return inb(KEYBOARD_DATA_PORT);
}

/**
 * @brief Read the keyboard controller configuration
 * 
 * @return Configuration byte
 */
static uint8_t keyboard_read_config(void) {
    keyboard_send_command(KEYBOARD_CMD_READ_CONFIG);
    return keyboard_read_data();
}

/**
 * @brief Write the keyboard controller configuration
 * 
 * @param config Configuration byte
 */
static void keyboard_write_config(uint8_t config) {
    keyboard_send_command(KEYBOARD_CMD_WRITE_CONFIG);
    keyboard_send_data(config);
}

/**
 * @brief Update the LED states based on the modifiers
 */
static void keyboard_update_leds(void) {
    uint8_t leds = 0;
    
    if (keyboard_modifiers & KEY_MOD_SCROLL_LOCK) {
        leds |= 1;
    }
    
    if (keyboard_modifiers & KEY_MOD_NUM_LOCK) {
        leds |= 2;
    }
    
    if (keyboard_modifiers & KEY_MOD_CAPS_LOCK) {
        leds |= 4;
    }
    
    // Send the SET_LEDS command
    keyboard_send_data(0xED);
    keyboard_send_data(leds);
}

/**
 * @brief Process a keyboard scancode
 * 
 * @param scancode Scancode received from the keyboard
 */
static void keyboard_process_scancode(uint8_t scancode) {
    // Check if key was released (bit 7 set)
    bool released = (scancode & KEY_RELEASED) != 0;
    uint8_t key = scancode & ~KEY_RELEASED;
    char character = 0;
    
    // Handle modifier keys
    switch (key) {
        case KEY_LEFT_CONTROL:
        case KEY_RIGHT_CONTROL:
            if (released) {
                keyboard_modifiers &= ~KEY_MOD_CONTROL;
            } else {
                keyboard_modifiers |= KEY_MOD_CONTROL;
            }
            break;
            
        case KEY_LEFT_SHIFT:
        case KEY_RIGHT_SHIFT:
            if (released) {
                keyboard_modifiers &= ~KEY_MOD_SHIFT;
            } else {
                keyboard_modifiers |= KEY_MOD_SHIFT;
            }
            break;
            
        case KEY_LEFT_ALT:
        case KEY_RIGHT_ALT:
            if (released) {
                keyboard_modifiers &= ~KEY_MOD_ALT;
            } else {
                keyboard_modifiers |= KEY_MOD_ALT;
            }
            break;
            
        case KEY_CAPS_LOCK:
            if (!released) {
                // Toggle Caps Lock
                keyboard_modifiers ^= KEY_MOD_CAPS_LOCK;
                keyboard_update_leds();
            }
            break;
            
        case KEY_NUM_LOCK:
            if (!released) {
                // Toggle Num Lock
                keyboard_modifiers ^= KEY_MOD_NUM_LOCK;
                keyboard_update_leds();
            }
            break;
            
        case KEY_SCROLL_LOCK:
            if (!released) {
                // Toggle Scroll Lock
                keyboard_modifiers ^= KEY_MOD_SCROLL_LOCK;
                keyboard_update_leds();
            }
            break;
            
        default:
            // Regular key
            if (!released && key < 128) {
                if (keyboard_modifiers & KEY_MOD_SHIFT) {
                    character = keymap_us_shift[key];
                } else {
                    character = keymap_us[key];
                }
                
                // Apply Caps Lock if active
                if (keyboard_modifiers & KEY_MOD_CAPS_LOCK) {
                    if (character >= 'a' && character <= 'z') {
                        character = character - 'a' + 'A';
                    } else if (character >= 'A' && character <= 'Z') {
                        character = character - 'A' + 'a';
                    }
                }
            }
            break;
    }
    
    // Call the keyboard callback if registered
    if (keyboard_callback) {
        keyboard_callback(scancode, character);
    }
}

/**
 * @brief Keyboard interrupt handler
 */
static void keyboard_handler(void) {
    // Read the scancode
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);
    
    // Process the scancode
    keyboard_process_scancode(scancode);
    
    // Send EOI
    pic_send_eoi(KEYBOARD_IRQ);
}

/**
 * @brief Initialize the keyboard controller
 */
void keyboard_init(void) {
    // Disable the keyboard and mouse interfaces
    keyboard_send_command(KEYBOARD_CMD_DISABLE);
    
    // Flush the output buffer
    if (inb(KEYBOARD_STATUS_PORT) & KEYBOARD_STATUS_OUTPUT_FULL) {
        inb(KEYBOARD_DATA_PORT);
    }
    
    // Perform controller self-test
    keyboard_send_command(KEYBOARD_CMD_SELF_TEST);
    if (keyboard_read_data() != 0x55) {
        kprintf("KEYBOARD: Controller self-test failed\n");
        return;
    }
    
    // Get the current configuration
    uint8_t config = keyboard_read_config();
    
    // Enable the keyboard interface, disable the mouse interface
    config |= KEYBOARD_CONFIG_INT;          // Enable keyboard interrupts
    config &= ~KEYBOARD_CONFIG_DISABLE;     // Enable keyboard
    config |= KEYBOARD_CONFIG_DISABLE_AUX;  // Disable mouse
    keyboard_write_config(config);
    
    // Enable the keyboard
    keyboard_send_command(KEYBOARD_CMD_ENABLE);
    
    // Reset keyboard modifiers
    keyboard_modifiers = 0;
    
    // Register the keyboard interrupt handler
    register_interrupt_handler(KEYBOARD_IRQ + 0x20, keyboard_handler);
    
    // Unmask the keyboard IRQ
    pic_set_mask(KEYBOARD_IRQ, true);
    
    // Mark keyboard as ready
    keyboard_ready = true;
    kbd_ready = true;
    
    kprintf("KEYBOARD: Initialized\n");
}

/**
 * @brief Register a keyboard callback function
 * 
 * @param callback Function to call for each keyboard event
 * @return Previous callback function, or NULL if none
 */
keyboard_callback_t keyboard_register_callback(keyboard_callback_t callback) {
    keyboard_callback_t old_callback = keyboard_callback;
    keyboard_callback = callback;
    return old_callback;
}

/**
 * @brief Check if a modifier key is pressed
 * 
 * @param modifier Modifier to check (KEY_MOD_*)
 * @return true if the modifier is active, false otherwise
 */
bool keyboard_is_modifier_active(uint8_t modifier) {
    return (keyboard_modifiers & modifier) != 0;
}

/**
 * @brief Get the keyboard modifier state
 * 
 * @return Current modifier state
 */
uint8_t keyboard_get_modifiers(void) {
    return keyboard_modifiers;
}
