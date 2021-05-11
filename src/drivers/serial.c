#include "arch/ports.h"
#include "drivers/serial.h"
#include "libk/stdio.h"
#include "libk/utils.h"

uint16_t def_port = 0;

int serial_init(uint16_t port){
    outb(port + 1, 0x00);
    outb(port + 3, 0x80);
    outb(port + 0, 0x01);
    outb(port + 1, 0x00);
    outb(port + 3, 0x03);
    outb(port + 2, 0xC7);
    outb(port + 4, 0x0B);
    outb(port + 4, 0x1E);
    outb(port + 0, 0xAE);

    if(inb(port) != 0xAE){
        log(ERROR, "Serial initliasation failed!\n");
        return 1;
    }
    
    outb(port + 4, 0x0F);
    def_port = port;
    log(INFO, "Serial initliased\n");
    return 0;
}

void serial_putchar(char c){
    while (!(inb(def_port + 5) & 0x20));
    outb(def_port, (uint8_t)c);
}

void serial_puts(const char* str){
    while(*str != 0){
        serial_putchar(*str);
        str++;
    }
}

void serial_printf(char* fmt, ...){
    va_list args;
    va_start(args, fmt);
    __vsprintf__(fmt, args, serial_putchar, serial_puts);
    va_end(args);
}