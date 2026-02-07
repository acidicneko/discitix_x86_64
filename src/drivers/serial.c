#include <arch/ports.h>
#include <drivers/serial.h>
#include <libk/stdio.h>
#include <libk/utils.h>
#include <stdbool.h>
#include <kernel/vfs/vfs.h>
#include <mm/liballoc.h>
#include <libk/string.h>

uint16_t def_port = 0;
bool initialized = false;

struct file_operations serial_file_ops = {
    .open = NULL,
    .close = NULL,
    .read = NULL,
    .write = serial_write
};

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
        return 1;
    }
    
    outb(port + 4, 0x0F);
    def_port = port;
    initialized = true;
    return 0;
}

bool is_serial_initialized(){
    return initialized;
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


long serial_write(file_t* file, const void* buf, size_t count, uint64_t off){
    (void)file;
    (void)off;
    const char* cbuf = (const char*)buf;
    for(size_t i = 0; i < count; i++){
        serial_putchar(cbuf[i]);
    }
    return (long)count;
}


void init_serial_device(){
    inode_t* serial_inode = NULL;
    superblock_t* root_sb = vfs_get_root_superblock();
    if(!root_sb){
        dbgln("Serial: Failed to get root superblock!\n\r");
        return;
    }
    serial_inode = (inode_t*)kmalloc(sizeof(inode_t));
    memset(serial_inode, 0, sizeof(inode_t));
    serial_inode->atime = 0;
    serial_inode->ctime = 0;
    serial_inode->is_directory = 0;
    serial_inode->type = FT_CHR;
    serial_inode->size = 0;
    serial_inode->ino = 5000;
    serial_inode->f_ops = &serial_file_ops;
    serial_inode->mode = 2;
    dentry_t *d = (dentry_t *)kmalloc(sizeof(dentry_t));
    memset(d, 0, sizeof(dentry_t));
    strncpy(d->name, "sr0", 4);
    d->name[NAME_MAX - 1] = '\0';
    d->inode = serial_inode;
    d->parent = root_sb->root;
    d->next = root_sb->root->next;
    root_sb->root->next = d;
}