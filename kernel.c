#include <stdint.h>
#include <stdbool.h>

// ==================== CONFIG ====================
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define CMD_BUFFER_SIZE 128
#define MAX_CMD_HISTORY 10

// ==================== VGA ====================
static uint16_t* const VGA_MEMORY = (uint16_t*)0xB8000;
static uint32_t cursor_x = 0;
static uint32_t cursor_y = 0;
static uint8_t vga_color = 0x0F;

// ==================== TERMINAL ====================
static char cmd_buffer[CMD_BUFFER_SIZE];
static uint32_t cmd_pos = 0;
static char cmd_history[MAX_CMD_HISTORY][CMD_BUFFER_SIZE];
static uint32_t history_count = 0;
static uint32_t history_pos = 0;

// ==================== I/O PORTS ====================
static inline void outb(uint16_t port, uint8_t value) {
    asm volatile ("outb %0, %1" :: "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// ==================== VGA FUNCTIONS ====================
static void vga_set_color(uint8_t fg, uint8_t bg) {
    vga_color = (bg << 4) | (fg & 0x0F);
}

static void vga_putc(char c) {
    if (c == '\n') {
        cursor_x = 0;
        if (++cursor_y >= VGA_HEIGHT) {
            cursor_y = VGA_HEIGHT - 1;
            // Scroll screen
            for (uint32_t i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
                VGA_MEMORY[i] = VGA_MEMORY[i + VGA_WIDTH];
            }
            // Clear last line
            for (uint32_t i = 0; i < VGA_WIDTH; i++) {
                VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + i] = vga_color << 8 | ' ';
            }
        }
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
        } else if (cursor_y > 0) {
            cursor_y--;
            cursor_x = VGA_WIDTH - 1;
        }
        VGA_MEMORY[cursor_y * VGA_WIDTH + cursor_x] = vga_color << 8 | ' ';
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\t') {
        cursor_x = (cursor_x + 4) & ~3;
    } else {
        VGA_MEMORY[cursor_y * VGA_WIDTH + cursor_x] = vga_color << 8 | c;
        cursor_x++;
    }
    
    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }
}

static void vga_puts(const char* str) {
    while (*str) vga_putc(*str++);
}

static void vga_clear(void) {
    for (uint32_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_MEMORY[i] = vga_color << 8 | ' ';
    }
    cursor_x = 0;
    cursor_y = 0;
}

static void vga_set_cursor(void) {
    uint16_t pos = cursor_y * VGA_WIDTH + cursor_x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

// ==================== STRING FUNCTIONS ====================
static size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

static int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

static void strcpy(char* dest, const char* src) {
    while ((*dest++ = *src++));
}

static void memset(void* dest, int value, size_t n) {
    unsigned char* d = dest;
    while (n--) *d++ = (unsigned char)value;
}

// ==================== TERMINAL FUNCTIONS ====================
static void show_prompt(void) {
    vga_set_color(2, 0);  // Green
    vga_puts("root~bloodos:~ ");
    vga_set_color(7, 0);  // White
    cmd_pos = 0;
}

static void add_to_history(const char* cmd) {
    if (history_count < MAX_CMD_HISTORY) {
        strcpy(cmd_history[history_count], cmd);
        history_count++;
    } else {
        // Shift history up
        for (uint32_t i = 0; i < MAX_CMD_HISTORY - 1; i++) {
            strcpy(cmd_history[i], cmd_history[i + 1]);
        }
        strcpy(cmd_history[MAX_CMD_HISTORY - 1], cmd);
    }
    history_pos = history_count;
}

// ==================== COMMAND EXECUTION ====================
static void execute_command(const char* cmd) {
    add_to_history(cmd);
    
    // Extract command and arguments
    char command[32];
    char args[96];
    int i = 0, j = 0;
    
    // Skip leading spaces
    while (cmd[i] == ' ') i++;
    
    // Get command
    while (cmd[i] && cmd[i] != ' ' && j < 31) {
        command[j++] = cmd[i++];
    }
    command[j] = '\0';
    
    // Get arguments
    j = 0;
    while (cmd[i] == ' ') i++;
    while (cmd[i] && j < 95) {
        args[j++] = cmd[i++];
    }
    args[j] = '\0';
    
    // Execute
    if (strcmp(command, "help") == 0) {
        vga_puts("\nAvailable commands:\n");
        vga_puts("  clear     - Clear screen\n");
        vga_puts("  echo      - Display message\n");
        vga_puts("  reboot    - Restart system\n");
        vga_puts("  shutdown  - Power off\n");
        vga_puts("  ver       - Show version\n");
        vga_puts("  color     - Change color\n");
        vga_puts("  ls        - List files\n");
        vga_puts("  time      - Show time\n");
        vga_puts("  date      - Show date\n");
        vga_puts("  calc      - Calculator\n");
        vga_puts("  mem       - Memory info\n");
        vga_puts("  cls       - Clear screen\n");
        vga_puts("  exit      - Exit shell\n");
    }
    else if (strcmp(command, "clear") == 0 || strcmp(command, "cls") == 0) {
        vga_clear();
    }
    else if (strcmp(command, "echo") == 0) {
        vga_puts("\n");
        vga_puts(args);
    }
    else if (strcmp(command, "reboot") == 0) {
        vga_puts("\nRebooting...");
        outb(0x64, 0xFE);
        while(1);
    }
    else if (strcmp(command, "shutdown") == 0) {
        vga_puts("\nShutting down...");
        // ACPI shutdown
        outb(0xF4, 0x00);
        outb(0x604, 0x2000);
        while(1);
    }
    else if (strcmp(command, "ver") == 0) {
        vga_puts("\nBloodOS v2.0 - Terminal Edition");
    }
    else if (strcmp(command, "color") == 0) {
        if (args[0] >= '0' && args[0] <= '9') {
            int color = args[0] - '0';
            vga_set_color(color, 0);
            vga_puts("\nColor changed");
        }
    }
    else if (strcmp(command, "ls") == 0) {
        vga_puts("\nbin/    dev/    etc/    home/");
        vga_puts("\nlib/    proc/   root/   tmp/");
        vga_puts("\nusr/    var/    boot/   sys/");
    }
    else if (strcmp(command, "time") == 0) {
        vga_puts("\n00:00:00 UTC");
    }
    else if (strcmp(command, "date") == 0) {
        vga_puts("\n2024-01-01");
    }
    else if (strcmp(command, "calc") == 0) {
        vga_puts("\nCalculator: Enter expression");
    }
    else if (strcmp(command, "mem") == 0) {
        vga_puts("\nMemory: 64MB total, 32MB free");
    }
    else if (strcmp(command, "exit") == 0) {
        vga_puts("\nLogging out...");
        vga_clear();
        show_prompt();
        return;
    }
    else if (command[0] != '\0') {
        vga_puts("\nCommand not found: ");
        vga_puts(command);
        vga_puts("\nType 'help' for available commands");
    }
    
    show_prompt();
}

// ==================== KEYBOARD HANDLER ====================
static char get_ascii(uint8_t scancode) {
    static const char* qwerty = "??1234567890-=??qwertyuiop[]?asdfghjkl;'`?\\zxcvbnm,./";
    
    if (scancode >= sizeof(qwerty)) return 0;
    char c = qwerty[scancode];
    return (c != '?') ? c : 0;
}

static void handle_keyboard(void) {
    uint8_t scancode = inb(0x60);
    
    // Key press (bit 7 clear)
    if (!(scancode & 0x80)) {
        if (scancode == 0x1C) { // Enter
            cmd_buffer[cmd_pos] = '\0';
            vga_putc('\n');
            if (cmd_pos > 0) {
                execute_command(cmd_buffer);
                memset(cmd_buffer, 0, CMD_BUFFER_SIZE);
                cmd_pos = 0;
            } else {
                show_prompt();
            }
        }
        else if (scancode == 0x0E) { // Backspace
            if (cmd_pos > 0) {
                cmd_pos--;
                vga_putc('\b');
            }
        }
        else {
            char c = get_ascii(scancode);
            if (c && cmd_pos < CMD_BUFFER_SIZE - 1) {
                cmd_buffer[cmd_pos++] = c;
                vga_putc(c);
            }
        }
    }
    
    // Acknowledge interrupt
    outb(0x20, 0x20);
}

// ==================== SYSTEM INITIALIZATION ====================
static void init_pic(void) {
    // Remap PIC
    outb(0x20, 0x11);  // ICW1
    outb(0xA0, 0x11);
    outb(0x21, 0x20);  // ICW2: IRQ0-7 -> INT 0x20-0x27
    outb(0xA1, 0x28);  // ICW2: IRQ8-15 -> INT 0x28-0x2F
    outb(0x21, 0x04);  // ICW3
    outb(0xA1, 0x02);
    outb(0x21, 0x01);  // ICW4
    outb(0xA1, 0x01);
    
    // Enable keyboard interrupt only
    outb(0x21, 0xFD);  // Enable IRQ1 (keyboard)
    outb(0xA1, 0xFF);  // Disable all slave IRQs
}

static void init_idt(void) {
    // Minimal IDT setup
    // In a real OS, this would set up interrupt gates
    // For now, we just enable interrupts
}

// ==================== BLOODOS ASCII ART ====================
static void show_banner(void) {
    vga_set_color(4, 0);  // Red
    
    vga_puts("\n");
    vga_puts("╔══════════════════════════════════════════════════════════╗\n");
    vga_puts("║                                                          ║\n");
    vga_puts("║   ██████╗ ██╗      ██████╗ ██████╗ ██████╗ ███████╗      ║\n");
    vga_puts("║   ██╔══██╗██║     ██╔═══██╗██╔══██╗██╔══██╗██╔════╝      ║\n");
    vga_puts("║   ██████╔╝██║     ██║   ██║██║  ██║██║  ██║███████╗      ║\n");
    vga_puts("║   ██╔══██╗██║     ██║   ██║██║  ██║██║  ██║╚════██║      ║\n");
    vga_puts("║   ██████╔╝███████╗╚██████╔╝██████╔╝██████╔╝███████║      ║\n");
    vga_puts("║   ╚═════╝ ╚══════╝ ╚═════╝ ╚═════╝ ╚═════╝ ╚══════╝      ║\n");
    vga_puts("║                                                          ║\n");
    vga_puts("║                    ██████╗ ███████╗                      ║\n");
    vga_puts("║                   ██╔═══██╗██╔════╝                      ║\n");
    vga_puts("║                   ██║   ██║███████╗                      ║\n");
    vga_puts("║                   ██║   ██║╚════██║                      ║\n");
    vga_puts("║                   ╚██████╔╝███████║                      ║\n");
    vga_puts("║                    ╚═════╝ ╚══════╝                      ║\n");
    vga_puts("║                                                          ║\n");
    vga_puts("╚══════════════════════════════════════════════════════════╝\n");
    
    vga_set_color(7, 0);
    vga_puts("\n                    Terminal Ready\n");
    vga_puts("            Type 'help' for available commands\n\n");
}

// ==================== MAIN KERNEL ====================
void kernel_main(void) {
    // Initialize
    vga_clear();
    show_banner();
    
    // Initialize system
    init_idt();
    init_pic();
    
    // Enable interrupts
    asm volatile("sti");
    
    // Show prompt
    show_prompt();
    
    // Main loop
    while (1) {
        // Handle keyboard in interrupt
        asm volatile("hlt");
    }
}
