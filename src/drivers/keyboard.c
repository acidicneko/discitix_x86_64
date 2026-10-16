#include <arch/x86_64/irq.h>
#include <arch/ports.h>
#include <drivers/keyboard.h>
#include <drivers/tty/tty.h>
#include <libk/stdio.h>
#include <libk/utils.h>
#include <libk/errno.h>
#include <fs/devfs.h>

// keyboard buffer handling
char buf_char;
uint8_t last_scancode;

// modifier key flags
uint8_t isShift = 0;
uint8_t isCtrl = 0;
uint8_t isAlt = 0;
uint8_t caps = 0;
uint8_t isExtended = 0;  // Flag for extended scancodes (0xE0 prefix)

/*keep tracks if key was pressed or not*/
int irq_done = 0;

/*English US QWERTY layout. non-shifted*/
const char keyMap_normal[58] = {
  0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0','-', '=', '\b',
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
  'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ',
};

/*same but shifted*/
const char keyMap_shift[58] = {
  0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')','_', '+', '\b',
  '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~', 0, '|',                                                                                                   
  'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ',                                                                                                      
};

// /dev/kbd: raw scancodes (with 0xE0 prefixes and releases) while it's open
#define KBD_BUF_SIZE 256
static uint8_t kbd_buf[KBD_BUF_SIZE];
static volatile uint32_t kbd_head, kbd_tail;
static int kbd_users;

static void kbd_push(uint8_t scancode) {
  uint32_t next = (kbd_head + 1) % KBD_BUF_SIZE;
  if (next == kbd_tail) return;   // full: drop
  kbd_buf[kbd_head] = scancode;
  kbd_head = next;
}

// Keep modifier state right while keys bypass the TTY
static void track_modifiers(uint8_t scancode) {
  switch (scancode) {
    case LEFT_SHIFT_PRESSED:  case RIGHT_SHIFT_PRESSED:  isShift = 1; break;
    case LEFT_SHIFT_RELEASED: case RIGHT_SHIFT_RELEASED: isShift = 0; break;
    case LEFT_CTRL_PRESSED:  isCtrl = 1; break;
    case LEFT_CTRL_RELEASED: isCtrl = 0; break;
    case LEFT_ALT_PRESSED:   isAlt = 1; break;
    case LEFT_ALT_RELEASED:  isAlt = 0; break;
  }
  isExtended = 0;
}

/*keyboard IRQ handler*/
void keyboard_handler(register_t* regs){
  (void)regs;
  uint8_t scancode = inb(0x60); /*read from keyboard data port*/
  if (kbd_users) kbd_push(scancode);
  if (tty_graphics_mode) {
    track_modifiers(scancode);
    return;
  }
  handleKey_normal(scancode);
}

static int kbd_open(inode_t *inode, uint32_t flags) {
  (void)inode; (void)flags;
  if (kbd_users++ == 0) kbd_tail = kbd_head;
  return 0;
}

static int kbd_close(inode_t *inode) {
  (void)inode;
  if (kbd_users > 0) kbd_users--;
  return 0;
}

// Never blocks: returns 0 when no scancodes are waiting
static long kbd_read(file_t *file, void *buf, size_t len, uint64_t off) {
  (void)file; (void)off;
  uint8_t *dest = (uint8_t *)buf;
  size_t n = 0;
  while (n < len && kbd_tail != kbd_head) {
    dest[n++] = kbd_buf[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUF_SIZE;
  }
  return (long)n;
}

static file_operations_t kbd_ops = {
  .open = kbd_open,
  .close = kbd_close,
  .read = kbd_read,
};

void init_kbd_device(void) {
  if (devfs_register_device("kbd", &kbd_ops, FT_CHR, NULL) == 0)
    log("KBD", INFO, "Registered /dev/kbd\n\r");
}

/*translate a scancode to an ascii character*/
char translate(uint8_t scancode){
  if(scancode >= 58)
    return 0;

  if(isShift == 1)
    return keyMap_shift[scancode];

  if(caps == 1){
    if((int)keyMap_normal[scancode] >= 97 && (int)keyMap_normal[scancode] <= 122){
      return (int)keyMap_normal[scancode] - 32;
    }
  }
  return keyMap_normal[scancode];
}

/*handles a normal key*/
void handleKey_normal(uint8_t scancode){
  // Handle extended scancode prefix
  if (scancode == EXTENDED_SCANCODE) {
    isExtended = 1;
    return;
  }
  
  // Handle extended scancodes (arrow keys, etc.)
  if (isExtended) {
    isExtended = 0;
    tty_t* tty = get_current_tty();
    
    // Handle key releases (high bit set)
    if (scancode & 0x80) {
      return;  // Ignore extended key releases for now
    }
    
    // Send ANSI escape sequences for arrow keys
    if (tty) {
      switch(scancode) {
        case KEY_UP:
          tty_input_char(tty, '\033');
          tty_input_char(tty, '[');
          tty_input_char(tty, 'A');
          return;
        case KEY_DOWN:
          tty_input_char(tty, '\033');
          tty_input_char(tty, '[');
          tty_input_char(tty, 'B');
          return;
        case KEY_RIGHT:
          tty_input_char(tty, '\033');
          tty_input_char(tty, '[');
          tty_input_char(tty, 'C');
          return;
        case KEY_LEFT:
          tty_input_char(tty, '\033');
          tty_input_char(tty, '[');
          tty_input_char(tty, 'D');
          return;
        case KEY_HOME:
          tty_input_char(tty, '\033');
          tty_input_char(tty, '[');
          tty_input_char(tty, 'H');
          return;
        case KEY_END:
          tty_input_char(tty, '\033');
          tty_input_char(tty, '[');
          tty_input_char(tty, 'F');
          return;
        case KEY_DELETE:
          tty_input_char(tty, '\033');
          tty_input_char(tty, '[');
          tty_input_char(tty, '3');
          tty_input_char(tty, '~');
          return;
      }
    }
    return;
  }
  
  switch(scancode){
    case LEFT_SHIFT_PRESSED:
      isShift = 1;
      return;
    case LEFT_SHIFT_RELEASED:
      isShift = 0;
      return;
    case RIGHT_SHIFT_PRESSED:
      isShift = 1;
      return;
    case RIGHT_SHIFT_RELEASED:
      isShift = 0;
      return;
    case LEFT_CTRL_PRESSED:
      isCtrl = 1;
      return;
    case LEFT_CTRL_RELEASED:
      isCtrl = 0;
      return;
    case LEFT_ALT_PRESSED:
      isAlt = 1;
      return;
    case LEFT_ALT_RELEASED:
      isAlt = 0;
      return;
    case CAPS_LOCK:
      if(caps == 0)
        caps = 1;
      else if(caps == 1)
        caps = 0;
      return;
  }

  if (isCtrl) {
    switch(scancode) {
      case KEY_1: tty_switch(0); return;
      case KEY_2: tty_switch(1); return;
      case KEY_3: tty_switch(2); return;
      case KEY_4: tty_switch(3); return;
    }

    char base = keyMap_normal[scancode < 58 ? scancode : 0];
    if (base >= 'a' && base <= 'z') {
        uint8_t ctrl_code = (uint8_t)(base - 'a' + 1);
        tty_t* tty = get_current_tty();
        if (tty) tty_input_char(tty, ctrl_code);
        return;
    }
    return;
  }  

  char ascii = translate(scancode); /*translate the scancode*/
  if(ascii > 0){                    /*if it's a printable character*/
    // Route input to the current TTY's line discipline
    tty_t* tty = get_current_tty();
    if (tty) {
      tty_input_char(tty, ascii);
    }
    // Also keep legacy behavior for keyboard_read()
    buf_char = ascii;
    irq_done = 1;                   /*notify that key has been handled*/
  }
  last_scancode = scancode;         /*this is the last scancode now*/
}

char keyboard_read(){               /*reads a single character from keyboard*/
  while(irq_done!=1){               /*while key hasn't been pressed, wait*/
    asm volatile("sti;hlt;cli");
  }
  irq_done = 0;                     /*key has been read, make itready for next read*/
  return buf_char;                  /*return the buffer character*/
}

void keyboard_install(void){        /*keyboard installer function*/
  irq_install_handler(1, keyboard_handler); /*register the handler for IRQ1, keyboard IRQ*/
  log("KBD",INFO,"Keyboard initialised\n\r");
}
