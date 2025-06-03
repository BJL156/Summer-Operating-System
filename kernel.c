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

const char us_keyboard[] = {
  0,  27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
  '\t', 'q','w','e','r','t','y','u','i','o','p','[',']','\n',
  0, 'a','s','d','f','g','h','j','k','l',';','\'','`',
  0, '\\','z','x','c','v','b','n','m',',','.','/', 0,
  '*', 0,  ' ',
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

  terminal_put_string(&terminal, "That was a lot of lines.\nIs that correct grammar?");

  while (true) {
    if (inb(0x64) & 1) {
      uint8_t scancode = inb(0x60);

      char key = us_keyboard[scancode];
      if (key) {
        terminal_put_char(&terminal, key);
      }
    }
  }
}
