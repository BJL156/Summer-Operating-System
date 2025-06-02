#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define VGA_ADDRESS 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

enum VGAColor {
  BLACK = 0,
  LIGHT_GRAY = 7,
};

uint8_t vga_color_entry(uint8_t fg, uint8_t bg) {
  return fg | bg << 4;
}

uint16_t vga_entry(char c, uint8_t color) {
  return (uint16_t)c | (uint16_t)color << 8;
}

struct Terminal {
  size_t row;
  size_t col;
  uint8_t color;
  uint16_t *buffer;
};

void terminal_clear(struct Terminal *terminal) {
  for (size_t y = 0; y < VGA_HEIGHT; y++) {
    for (size_t x = 0; x < VGA_WIDTH; x++) {
      const size_t index = y * VGA_WIDTH + x;
      terminal->buffer[index] = vga_entry(' ', terminal->color);
    }
  }
}

void terminal_initialize(struct Terminal *terminal) {
  terminal->row = 0;
  terminal->col = 0;
  terminal->color = vga_color_entry(LIGHT_GRAY, BLACK);
  terminal->buffer = (uint16_t *)VGA_ADDRESS;
}

static inline uint8_t inb(uint16_t port) {
  uint8_t ret;
  __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
  __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

void terminal_update_cursor(struct Terminal *terminal) {
  uint16_t index = terminal->row * VGA_WIDTH + terminal->col;

  outb(0x3D4, 0x0E);
  outb(0x3D5, (index >> 8) & 0xFF);

  outb(0x3D4, 0x0F);
  outb(0x3D5, index & 0xFF);
}

void terminal_scroll(struct Terminal *terminal) {
  for (size_t y = 1; y < VGA_HEIGHT; y++) {
    for (size_t x = 0; x < VGA_WIDTH; x++) {
      size_t last_line_index = (y - 1) * VGA_WIDTH + x;
      size_t current_line_index = y * VGA_WIDTH + x;

      terminal->buffer[last_line_index] = terminal->buffer[current_line_index];
    }
  }

  for (size_t x = 0; x < VGA_WIDTH; x++) {
    size_t index = (VGA_HEIGHT - 1) * VGA_WIDTH + x;
    terminal->buffer[index] = vga_entry(' ', terminal->color);
  }

  terminal->row = VGA_HEIGHT - 1;

  terminal_update_cursor(terminal);
}

void terminal_bounds_check(struct Terminal *terminal) {
  if (terminal->col >= VGA_WIDTH) {
    terminal->col = 0;
    terminal->row++;

    if (terminal->row >= VGA_HEIGHT) {
      terminal_scroll(terminal);
    }
  }
}

bool handle_escape_char(struct Terminal *terminal, char c) {
  switch (c) {
    case '\n':
      terminal->col = 0;
      terminal->row++;

      if (terminal->row >= VGA_HEIGHT) {
        terminal_scroll(terminal);
      }

      return true;
    case '\r':
      terminal->col = 0;

      return true;
    default:
      return false;
  }
}

void terminal_put_char(struct Terminal *terminal, char c) {
  if (handle_escape_char(terminal, c)) {
    return;
  }

  const size_t index = terminal->row * VGA_WIDTH + terminal->col;
  terminal->buffer[index] = vga_entry(c, terminal->color);

  terminal->col++;
  terminal_bounds_check(terminal);

  terminal_update_cursor(terminal);
}

void terminal_put_string(struct Terminal *terminal, const char *str) {
  for (size_t i = 0; str[i] != '\0'; i++) {
    terminal_put_char(terminal, str[i]);
  }
}

void itoa(int val, char *buffer) {
  if (val == 0) {
    buffer[0] = '0';
    buffer[1] = '\0';
    return;
  }

  bool is_neg = false;
  if (val < 0) {
    is_neg = true;
    val = -val;
  }

  char temp[12];

  size_t i = 0;
  while (val > 0) {
    temp[i++] = '0' + (val % 10);
    val /= 10;
  }

  if (is_neg) {
    temp[i++] = '-';
  }

  size_t j = 0;
  while (i > 0) {
    buffer[j++] = temp[--i];
  }

  buffer[j] = '\0';
}


#define KEYBOARD_STATUS 0x64
#define KEYBOARD_EA 0x60

uint8_t last_scancode = 0;

static void send_command(uint8_t command) {
  while ((inb(KEYBOARD_STATUS) & 2));

  outb(KEYBOARD_EA, command);
}

uint32_t get_scancode() {
 static unsigned e0_code = 0;
  static unsigned e1_code = 0;
  static uint16_t e1_prev = 0;
  uint8_t scancode = 0;
  if (inb(KEYBOARD_STATUS) & 1) {
    // a scancode is available in the buffer
    scancode = inb(KEYBOARD_EA);
    if (e0_code == 1) {
      // scancode is an e0 code
      e0_code = 0;
      return (0xe000 | scancode);
    } else if (e1_code == 1) {
      // scancode is first byte of e1 code
      e1_prev = scancode;
      e1_code = 2;
    } else  if (e1_code == 2) {
      // scancode is second byte of e1 code (first is in e1_prev)
      e1_code = 0;
      return (0xe10000 | e1_prev << 8 | scancode);
    } else if (scancode == 0xe0) {
      e0_code = 1;
      scancode = 0;
    } else if (scancode == 0xe1) {
      e1_code = 1;
      scancode = 0;
    }
  }
  return scancode;
}

void initialize() {
  while (inb(KEYBOARD_STATUS) & 1) {
    inb(KEYBOARD_EA);
  }

  // activate keyboard
  send_command(0xF4);
  while (inb(KEYBOARD_STATUS) & 1) {
    inb(KEYBOARD_EA); // read (and drop) what's left in the keyboard buffer
  }

  // self-test (should answer with 0xEE)
  send_command(0xEE);
}

void loop() {
  uint8_t scancode = 0;
  while ((scancode = get_scancode()) == 0)  { // loop until we get a keypress
    // delay();
  }
  last_scancode = scancode;
}

char kbd_US [128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
  '\t', /* <-- Tab */
  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, /* <-- control key */
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',  0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0,
  '*',
    0,  /* Alt */
  ' ',  /* Space bar */
    0,  /* Caps lock */
    0,  /* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,  /* < ... F10 */
    0,  /* 69 - Num lock*/
    0,  /* Scroll Lock */
    0,  /* Home key */
    0,  /* Up Arrow */
    0,  /* Page Up */
  '-',
    0,  /* Left Arrow */
    0,
    0,  /* Right Arrow */
  '+',
    0,  /* 79 - End key*/
    0,  /* Down Arrow */
    0,  /* Page Down */
    0,  /* Insert Key */
    0,  /* Delete Key */
    0,   0,   0,
    0,  /* F11 Key */
    0,  /* F12 Key */
    0,  /* All other keys are undefined */
};

void kernel_main() {
  struct Terminal terminal;
  terminal_initialize(&terminal);

  for (size_t i = 0; i < 50; i++) {
    char line_number[12];
    itoa(i, line_number);

    terminal_put_string(&terminal, line_number);
    terminal_put_char(&terminal, '\n');
  }

  terminal_put_string(&terminal, "That was a lot of lines.\n");

  initialize();
  while (true) {
    loop();

    char c = kbd_US[last_scancode];
    if (c != 0) {
      terminal_put_char(&terminal, c);
    }
  }
}
