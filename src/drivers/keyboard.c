#include "arch/x86_64/irq.h"
#include "arch/ports.h"
#include "drivers/keyboard.h"
#include "libk/stdio.h"
#include "libk/utils.h"

// keyboard buffer handling
char buf_char;
uint8_t last_scancode;

// shift and caps flags
uint8_t isShift = 0;
uint8_t caps = 0;

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

/*keyboard IRQ handler*/                                                                                                                                                 
void keyboard_handler(register_t* regs){
  (void)regs;                                                                                                                                                         
  uint8_t scancode;                                                                                                                                                      
  scancode = inb(0x60); /*read from keyboard data port*/
  handleKey_normal(scancode);
}

/*translate a scancode to an ascii character*/
char translate(uint8_t scancode){
  if(scancode > 58)
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
    case CAPS_LOCK:
      if(caps == 0)
        caps = 1;
      else if(caps == 1)
        caps = 0;
      return;
  }

  char ascii = translate(scancode); /*translate the scancode*/
  if(ascii > 0){                    /*if it's a printable character*/
    buf_char = ascii;               /*store it in the buffer character*/
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
  log(INFO, "Keyboard Installed\n");        /*notify that keyboard has been installed*/
}