/**
 * @file keyboard.c
 * @brief PS/2 Keyboard driver
 */

#include "../../include/kernel.h"
#include <stdint.h>
#include <stdbool.h>

// Keyboard controller ports
#define KB_DATA_PORT     0x60  // Data port (read/write)
#define KB_STATUS_PORT   0x64  // Status register port (read)
#define KB_COMMAND_PORT  0x64  // Command register port (write)

// Keyboard controller commands
#define KB_CMD_READ_CONFIG      0x20  // Read configuration byte
#define KB_CMD_WRITE_CONFIG     0x60  // Write configuration byte
#define KB_CMD_DISABLE_FIRST    0xAD  // Disable first PS/2 port
#define KB_CMD_ENABLE_FIRST     0xAE  // Enable first PS/2 port
#define KB_CMD_DISABLE_SECOND   0xA7  // Disable second PS/2 port
#define KB_CMD_ENABLE_SECOND    0xA8  // Enable second PS/2 port
#define KB_CMD_TEST_FIRST       0xAB  // Test first PS/2 port
#define KB_CMD_TEST_SECOND      0xA9  // Test second PS/2 port
#define KB_CMD_TEST_CONTROLLER  0xAA  // Test keyboard controller
#define KB_CMD_SYSTEM_RESET     0xFE  // System reset

// Keyboard commands
#define KB_CMD_RESET            0xFF  // Reset keyboard
#define KB_CMD_ENABLE_SCANNING  0xF4  // Enable scanning (keyboard starts sending data)
#define KB_CMD_DISABLE_SCANNING 0xF5  // Disable scanning
#define KB_CMD_SET_DEFAULTS     0xF6  // Set default parameters
#define KB_CMD_SET_TYPEMATIC    0xF3  // Set typematic rate/delay

// Controller status bits
#define KB_STATUS_OUTPUT_FULL   0x01  // Output buffer status (0 = empty, 1 = full)
#define KB_STATUS_INPUT_FULL    0x02  // Input buffer status (0 = empty, 1 = full)
#define KB_STATUS_SYSTEM_FLAG   0x04  // System flag (0 = power-on reset, 1 = self-test success)
#define KB_STATUS_COMMAND_DATA  0x08  // Command/data (0 = data written to input buffer is data, 1 = command)
#define KB_STATUS_TIMEOUT       0x40  // Timeout error
#define KB_STATUS_PARITY_ERROR  0x80  // Parity error

// Controller configuration bits
#define KB_CONFIG_FIRST_INT     0x01  // First PS/2 port interrupt (IRQ1)
#define KB_CONFIG_SECOND_INT    0x02  // Second PS/2 port interrupt (IRQ12)
#define KB_CONFIG_SYSTEM_FLAG   0x04  // System flag
#define KB_CONFIG_FIRST_CLOCK   0x10  // First PS/2 port clock (0 = disabled, 1 = enabled)
#define KB_CONFIG_SECOND_CLOCK  0x20  // Second PS/2 port clock (0 = disabled, 1 = enabled)
#define KB_CONFIG_FIRST_TRANS   0x40  // First PS/2 port translation (0 = disabled, 1 = enabled)

// Keyboard responses
#define KB_RESPONSE_ACK         0xFA  // Command acknowledged
#define KB_RESPONSE_RESEND      0xFE  // Resend last command (error)
#define KB_RESPONSE_ERROR       0xFC  // Error (self-test failed)

// IRQ line for the keyboard
#define KEYBOARD_IRQ            1

// Key states
#define KEY_RELEASED            0x00
#define KEY_PRESSED             0x01
#define KEY_REPEATED            0x02

// Maximum size of keyboard buffer
#define KB_BUFFER_SIZE          256

// Keyboard buffer (circular)
static uint8_t kb_buffer[KB_BUFFER_SIZE];
static uint32_t kb_buffer_head = 0;
static uint32_t kb_buffer_tail = 0;

// Keyboard state
static bool kb_initialized = false;
static bool num_lock = false;
static bool caps_lock = false;
static bool scroll_lock = false;
static bool extended_key = false;
static bool shift_pressed = false;
static bool alt_pressed = false;
static bool ctrl_pressed = false;

// Key states (true = pressed, false = released)
static bool key_states[256] = {0};

// US QWERTY layout character mapping
static const char kb_layout_lower[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-',
    0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const char kb_layout_upper[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-',
    0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// Scancode to key name mapping
static const char* key_names[128] = {
    "UNKNOWN", "ESC", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "MINUS", "EQUAL", "BACKSPACE",
    "TAB", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "LBRACKET", "RBRACKET", "ENTER",
    "LCTRL", "A", "S", "D", "F", "G", "H", "J", "K", "L", "SEMICOLON", "APOSTROPHE", "BACKTICK",
    "LSHIFT", "BACKSLASH", "Z", "X", "C", "V", "B", "N", "M", "COMMA", "PERIOD", "SLASH", "RSHIFT",
    "KP_MULTIPLY", "LALT", "SPACE", "CAPSLOCK", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10",
    "NUMLOCK", "SCROLLLOCK", "KP_7", "KP_8", "KP_9", "KP_MINUS", "KP_4", "KP_5", "KP_6", "KP_PLUS",
    "KP_1", "KP_2", "KP_3", "KP_0", "KP_DECIMAL", "UNKNOWN", "UNKNOWN", "UNKNOWN", "F11", "F12"
};

// Extended key scancode to key name mapping
static const char* ext_key_names[128] = {
    "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN",
    "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN",
    "PREV_TRACK", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN",
    "UNKNOWN", "NEXT_TRACK", "UNKNOWN", "UNKNOWN", "KP_ENTER", "RCTRL", "UNKNOWN", "UNKNOWN",
    "MUTE", "CALCULATOR", "PLAY", "UNKNOWN", "STOP", "UNKNOWN", "UNKNOWN", "UNKNOWN",
    "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "VOL_DOWN", "UNKNOWN",
    "VOL_UP", "UNKNOWN", "WWW_HOME", "UNKNOWN", "UNKNOWN", "KP_DIVIDE", "UNKNOWN", "PRTSCR",
    "RALT", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN",
    "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "PAUSE", "UNKNOWN", "HOME",
    "UP", "PGUP", "UNKNOWN", "LEFT", "UNKNOWN", "RIGHT", "UNKNOWN", "END",
    "DOWN", "PGDN", "INSERT", "DELETE", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN"
};

// Forward declarations
static bool kb_wait_for_output(void);
static bool kb_wait_for_input(void);
static bool kb_send_command(uint8_t command);
static bool kb_send_data(uint8_t data);
static uint8_t kb_read_data(void);
static void kb_handle_scancode(uint8_t scancode);
static void kb_update_leds(void);
static bool kb_buffer_is_empty(void);
static bool kb_buffer_is_full(void);
static void kb_buffer_push(uint8_t scancode);
static uint8_t kb_buffer_pop(void);

/**
 * @brief Wait for the keyboard controller output buffer to be full
 * 
 * @return true if buffer became full within timeout
 */
static bool kb_wait_for_output(void) {
    // Try for a reasonable amount of time
    for (int i = 0; i < 0x10000; i++) {
        if (inb(KB_STATUS_PORT) & KB_STATUS_OUTPUT_FULL) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Wait for the keyboard controller input buffer to be empty
 * 
 * @return true if buffer became empty within timeout
 */
static bool kb_wait_for_input(void) {
    // Try for a reasonable amount of time
    for (int i = 0; i < 0x10000; i++) {
        if (!(inb(KB_STATUS_PORT) & KB_STATUS_INPUT_FULL)) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Send a command to the keyboard controller
 * 
 * @param command Command to send
 * @return true if command was sent successfully
 */
static bool kb_send_command(uint8_t command) {
    if (!kb_wait_for_input()) {
        return false;
    }
    outb(KB_COMMAND_PORT, command);
    return true;
}

/**
 * @brief Send data to the keyboard
 * 
 * @param data Data to send
 * @return true if data was sent successfully
 */
static bool kb_send_data(uint8_t data) {
    if (!kb_wait_for_input()) {
        return false;
    }
    outb(KB_DATA_PORT, data);
    return true;
}

/**
 * @brief Read data from the keyboard
 * 
 * @return Data read from the keyboard, or 0 if no data available
 */
static uint8_t kb_read_data(void) {
    if (!kb_wait_for_output()) {
        return 0;
    }
    return inb(KB_DATA_PORT);
}

/**
 * @brief Check if the keyboard buffer is empty
 * 
 * @return true if buffer is empty
 */
static bool kb_buffer_is_empty(void) {
    return kb_buffer_head == kb_buffer_tail;
}

/**
 * @brief Check if the keyboard buffer is full
 * 
 * @return true if buffer is full
 */
static bool kb_buffer_is_full(void) {
    return ((kb_buffer_head + 1) % KB_BUFFER_SIZE) == kb_buffer_tail;
}

/**
 * @brief Push a scancode into the keyboard buffer
 * 
 * @param scancode Scancode to push
 */
static void kb_buffer_push(uint8_t scancode) {
    if (kb_buffer_is_full()) {
        return;  // Drop the key if buffer is full
    }
    
    kb_buffer[kb_buffer_head] = scancode;
    kb_buffer_head = (kb_buffer_head + 1) % KB_BUFFER_SIZE;
}

/**
 * @brief Pop a scancode from the keyboard buffer
 * 
 * @return Scancode, or 0 if buffer is empty
 */
static uint8_t kb_buffer_pop(void) {
    if (kb_buffer_is_empty()) {
        return 0;
    }
    
    uint8_t scancode = kb_buffer[kb_buffer_tail];
    kb_buffer_tail = (kb_buffer_tail + 1) % KB_BUFFER_SIZE;
    return scancode;
}

/**
 * @brief Update keyboard LEDs based on lock states
 */
static void kb_update_leds(void) {
    // Calculate LED byte: bit 0 = scroll lock, bit 1 = num lock, bit 2 = caps lock
    uint8_t leds = 0;
    if (scroll_lock) leds |= 1;
    if (num_lock) leds |= 2;
    if (caps_lock) leds |= 4;
    
    // Send LED update command
    if (!kb_send_data(KB_CMD_SET_TYPEMATIC)) {
        return;
    }
    
    // Wait for ACK
    if (kb_read_data() != KB_RESPONSE_ACK) {
        return;
    }
    
    // Send LED state
    if (!kb_send_data(leds)) {
        return;
    }
    
    // Wait for ACK (ignore if not received)
    kb_read_data();
}

/**
 * @brief Handle a keyboard scancode
 * 
 * @param scancode Scancode to handle
 */
static void kb_handle_scancode(uint8_t scancode) {
    // Check for extended key prefix (0xE0)
    if (scancode == 0xE0) {
        extended_key = true;
        return;
    }
    
    // Key release has bit 7 set
    bool is_release = (scancode & 0x80) != 0;
    uint8_t key = scancode & 0x7F;  // Key code without release bit
    
    // Track key states
    if (extended_key) {
        key |= 0x80;  // Set high bit for extended keys
        extended_key = false;
    }
    
    // Update key state
    if (is_release) {
        key_states[key] = false;
        
        // Update modifier key states
        if (key == 0x2A || key == 0x36) {  // Left or right shift
            shift_pressed = false;
        } else if (key == 0x1D || (key & 0x7F) == 0x1D) {  // Left or right ctrl
            ctrl_pressed = false;
        } else if (key == 0x38 || (key & 0x7F) == 0x38) {  // Left or right alt
            alt_pressed = false;
        }
    } else {
        key_states[key] = true;
        
        // Update modifier key states
        if (key == 0x2A || key == 0x36) {  // Left or right shift
            shift_pressed = true;
        } else if (key == 0x1D || (key & 0x7F) == 0x1D) {  // Left or right ctrl
            ctrl_pressed = true;
        } else if (key == 0x38 || (key & 0x7F) == 0x38) {  // Left or right alt
            alt_pressed = true;
        }
        
        // Check for toggle keys
        if (key == 0x3A && !is_release) {  // Caps Lock
            caps_lock = !caps_lock;
            kb_update_leds();
        } else if (key == 0x45 && !is_release) {  // Num Lock
            num_lock = !num_lock;
            kb_update_leds();
        } else if (key == 0x46 && !is_release) {  // Scroll Lock
            scroll_lock = !scroll_lock;
            kb_update_leds();
        }
    }
}

/**
 * @brief Keyboard interrupt handler
 */
static void kb_interrupt_handler(void) {
    // Read scancode from keyboard
    uint8_t scancode = inb(KB_DATA_PORT);
    
    // Push to buffer
    kb_buffer_push(scancode);
    
    // Handle the key
    kb_handle_scancode(scancode);
    
    // Send EOI to PIC
    pic_send_eoi(KEYBOARD_IRQ);
}

/**
 * @brief Initialize the keyboard controller
 */
void kb_init(void) {
    // Disable both ports during initialization
    kb_send_command(KB_CMD_DISABLE_FIRST);
    kb_send_command(KB_CMD_DISABLE_SECOND);
    
    // Flush the output buffer
    while (inb(KB_STATUS_PORT) & KB_STATUS_OUTPUT_FULL) {
        inb(KB_DATA_PORT);
    }
    
    // Read current configuration
    kb_send_command(KB_CMD_READ_CONFIG);
    uint8_t config = kb_read_data();
    
    // Update configuration:
    // - Enable first port interrupt (IRQ1)
    // - Enable first port clock
    // - Disable second port clock
    // - Disable translation (we'll handle scancodes directly)
    config |= KB_CONFIG_FIRST_INT | KB_CONFIG_FIRST_CLOCK;
    config &= ~(KB_CONFIG_SECOND_CLOCK | KB_CONFIG_FIRST_TRANS);
    
    // Write updated configuration
    kb_send_command(KB_CMD_WRITE_CONFIG);
    kb_send_data(config);
    
    // Perform self-test
    kb_send_command(KB_CMD_TEST_CONTROLLER);
    if (kb_read_data() != 0x55) {
        kprintf("Keyboard: Controller self-test failed\n");
        return;
    }
    
    // Test first port
    kb_send_command(KB_CMD_TEST_FIRST);
    if (kb_read_data() != 0x00) {
        kprintf("Keyboard: First port test failed\n");
        return;
    }
    
    // Enable first port
    kb_send_command(KB_CMD_ENABLE_FIRST);
    
    // Reset the keyboard
    kb_send_data(KB_CMD_RESET);
    if (kb_read_data() != KB_RESPONSE_ACK) {
        kprintf("Keyboard: Reset command failed (ACK not received)\n");
        return;
    }
    
    // Wait for reset to complete
    uint8_t reset_response = kb_read_data();
    if (reset_response != 0xAA) {
        kprintf("Keyboard: Reset failed (0x%02X)\n", reset_response);
        return;
    }
    
    // Enable keyboard scanning
    kb_send_data(KB_CMD_ENABLE_SCANNING);
    if (kb_read_data() != KB_RESPONSE_ACK) {
        kprintf("Keyboard: Enable scanning failed\n");
        return;
    }
    
    // Initialize key state
    num_lock = false;
    caps_lock = false;
    scroll_lock = false;
    extended_key = false;
    shift_pressed = false;
    alt_pressed = false;
    ctrl_pressed = false;
    
    // Update LEDs
    kb_update_leds();
    
    // Register interrupt handler
    register_interrupt_handler(KEYBOARD_IRQ + 32, kb_interrupt_handler);
    
    // Unmask the keyboard IRQ
    pic_unmask_irq(KEYBOARD_IRQ);
    
    // Mark as initialized
    kb_initialized = true;
    kbd_ready = true;
    
    kprintf("Keyboard: Initialized\n");
}

/**
 * @brief Get a character from the keyboard
 * 
 * @return Character code, or 0 if no character available
 */
char kb_get_char(void) {
    if (!kb_initialized || kb_buffer_is_empty()) {
        return 0;
    }
    
    // Process scancodes until we get a key press (not release)
    while (!kb_buffer_is_empty()) {
        uint8_t scancode = kb_buffer_pop();
        
        // Skip extended key marker
        if (scancode == 0xE0) {
            continue;
        }
        
        // Skip key releases
        if (scancode & 0x80) {
            continue;
        }
        
        // Convert to character
        if (scancode < 128) {
            bool use_uppercase = (shift_pressed != caps_lock);
            return use_uppercase ? kb_layout_upper[scancode] : kb_layout_lower[scancode];
        }
    }
    
    return 0;
}

/**
 * @brief Get a scancode from the keyboard
 * 
 * @return Scancode, or 0 if no scancode available
 */
uint8_t kb_get_scancode(void) {
    if (!kb_initialized || kb_buffer_is_empty()) {
        return 0;
    }
    
    return kb_buffer_pop();
}

/**
 * @brief Check if a key is currently pressed
 * 
 * @param scancode Scancode to check
 * @return true if key is pressed
 */
bool kb_is_key_pressed(uint8_t scancode) {
    if (!kb_initialized) {
        return false;
    }
    
    return key_states[scancode & 0x7F];
}

/**
 * @brief Get the name of a key from its scancode
 * 
 * @param scancode Scancode to get name for
 * @return Key name
 */
const char* kb_get_key_name(uint8_t scancode) {
    bool is_extended = (scancode & 0x80) != 0;
    uint8_t key = scancode & 0x7F;
    
    if (key >= 128) {
        return "UNKNOWN";
    }
    
    return is_extended ? ext_key_names[key] : key_names[key];
}

/**
 * @brief Check if Shift is pressed
 * 
 * @return true if Shift is pressed
 */
bool kb_is_shift_pressed(void) {
    return shift_pressed;
}

/**
 * @brief Check if Alt is pressed
 * 
 * @return true if Alt is pressed
 */
bool kb_is_alt_pressed(void) {
    return alt_pressed;
}

/**
 * @brief Check if Ctrl is pressed
 * 
 * @return true if Ctrl is pressed
 */
bool kb_is_ctrl_pressed(void) {
    return ctrl_pressed;
}

/**
 * @brief Check if Caps Lock is active
 * 
 * @return true if Caps Lock is active
 */
bool kb_is_caps_lock_active(void) {
    return caps_lock;
}

/**
 * @brief Check if Num Lock is active
 * 
 * @return true if Num Lock is active
 */
bool kb_is_num_lock_active(void) {
    return num_lock;
}

/**
 * @brief Check if Scroll Lock is active
 * 
 * @return true if Scroll Lock is active
 */
bool kb_is_scroll_lock_active(void) {
    return scroll_lock;
}
