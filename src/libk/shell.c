#include "mm/liballoc.h"
#include <drivers/tty/tty.h>
#include <fs/stripFS.h>
#include <kernel/vfs/vfs.h>
#include <libk/shell.h>
#include <libk/stdio.h>
#include <libk/string.h>
#include <libk/utils.h>
#include <mm/pmm.h>
#include <stdint.h>

// Shell's TTY file handle
static file_t* shell_tty = NULL;

// Write string to shell's TTY
static void shell_write(const char* str) {
  if (shell_tty) {
    vfs_write(shell_tty, str, strlen(str));
  }
}

// Write single char to shell's TTY
static void shell_putchar(char c) {
  if (shell_tty) {
    vfs_write(shell_tty, &c, 1);
  }
}

// Read a line from shell's TTY (uses line discipline)
static int shell_gets(char* buf, size_t size) {
  if (!shell_tty || size == 0) return -1;
  long bytes = vfs_read(shell_tty, buf, size - 1);
  if (bytes > 0) {
    // Remove trailing newline if present
    if (buf[bytes - 1] == '\n') {
      buf[bytes - 1] = '\0';
      bytes--;
    } else {
      buf[bytes] = '\0';
    }
  }
  return (int)bytes;
}

int execute(char **argv, int argc) {
  if (!strcmp(argv[0], "uname")) {
    shell_write("Discitix Kernel x86_64; Build: ");
    shell_write(__DATE__);
    shell_write("\n\n");
  } else if (!strcmp(argv[0], "sysfetch")) {
    sysfetch();
  } else if (!strcmp(argv[0], "about")) {
    shell_write("ayu.sh shell; the so called shell\n\n");
  } else if (!strcmp(argv[0], "dbgln")) {
    for (uint8_t i = 1; i < argc; i++) {
      dbgln("%s ", argv[i]);
    }
    dbgln("\n\r");
  } else if (!strcmp(argv[0], "echo")) {
    for (uint8_t i = 1; i < argc; i++) {
      shell_write(argv[i]);
      shell_write(" ");
    }
    shell_write("\n");
  } else if (!strcmp(argv[0], "clear")) {
    tty_clear();
  } else if (!strcmp(argv[0], "free")) {
    char num_buf[32];
    shell_write("\t\ttotal\t\tused\t\tfree\n");
    shell_write("Mem:\t\t");
    itoa(get_total_physical_memory() / 1024, num_buf, 10);
    shell_write(num_buf);
    shell_write("\t\t");
    itoa((get_total_physical_memory() - get_free_physical_memory()) / 1024, num_buf, 10);
    shell_write(num_buf);
    shell_write("\t\t");
    itoa(get_free_physical_memory() / 1024, num_buf, 10);
    shell_write(num_buf);
    shell_write("\n");
  } else if (!strcmp(argv[0], "ls")) {
    superblock_t *sb = vfs_get_root_superblock();
    if (!sb || !sb->root || !sb->root->inode || !sb->root->inode->private) {
      shell_write("No filesystem mounted.\n\n");
      return -1;
    }
    dentry_t *d = sb->root;
    d = d->next; // skip root itself
    while (d) {
      uint32_t mode = d->inode->mode;
      if((mode & (1 << 2))){
        shell_write("r");
      } else {
        shell_write("-");
      }
      if((mode & (1 << 1))){
        shell_write("w");
      } else {
        shell_write("-");
      }
      if(mode & (1 << 0)){
        shell_write("x");
      } else {
        shell_write("-");
      }
      shell_write("  ");
      shell_write(d->name);
      shell_write("\n");
      d = d->next;
    }
    shell_write("\n");
  } else if(!strcmp(argv[0], "cat")) {
    if (argc < 2) {
      shell_write("Usage: cat <filename>\n\n");
      return -1;
    }
    superblock_t *sb = vfs_get_root_superblock();
    if (!sb || !sb->root || !sb->root->inode) {
      shell_write("No filesystem mounted.\n\n");
      return -1;
    }
    sb->root->inode->is_directory = 1; // ensure root is marked as directory
    inode_t *file_inode = NULL;
    if (vfs_lookup(sb->root->inode, argv[1], &file_inode) != 0 || !file_inode) {
      shell_write("ayu.sh: cat: ");
      shell_write(argv[1]);
      shell_write(": No such file\n\n");
      return -1;
    }
    file_t *file = NULL;
    if (vfs_open(&file, file_inode, 0) != 0 || !file) {
      shell_write("ayu.sh: cat: ");
      shell_write(argv[1]);
      shell_write(": Failed to open file\n\n");
      return -1;
    }
    char buffer[1024];
    long bytes_read = vfs_read(file, buffer, sizeof(buffer) - 1);
    if (bytes_read < 0) {
      shell_write("ayu.sh: cat: ");
      shell_write(argv[1]);
      shell_write(": Failed to read file\n\n");
      return -1;
    }
    buffer[bytes_read] = '\0';
    shell_write(buffer);
    vfs_close(file);
  } else if(!strcmp(argv[0], "exit")) {
    return 1;
  } else {
    shell_write("ayu.sh: ");
    shell_write(argv[0]);
    shell_write(": unknown command\n\n");
  }
  return 0;
}

void init_shell() {
  // Open the current TTY for the shell
  tty_t* current = get_current_tty();
  if (!current) {
    dbgln("Shell: No current TTY!\n\r");
    return;
  }
  
  // Build path like "/tty0", "/tty1", etc.
  char tty_path[8] = "/tty";
  char id_str[2];
  itoa(current->id, id_str, 10);
  strcat(tty_path, id_str);
  
  inode_t* tty_inode = NULL;
  vfs_lookup_path(tty_path, &tty_inode);
  if (!tty_inode) {
    dbgln("Shell: Failed to find %s\n\r", tty_path);
    return;
  }
  
  vfs_open(&shell_tty, tty_inode, 0);
  if (!shell_tty) {
    dbgln("Shell: Failed to open %s\n\r", tty_path);
    return;
  }
  
  char *argv[100];
  char input[1024];
  int argc = 0;
  
  while (1) {
    shell_write(">> ");
    if (shell_gets(input, sizeof(input)) < 0) {
      continue;
    }
    if (input[0] == '\0') {
      continue;  // Empty line
    }
    argv[argc] = strtok(input, " \t");
    while (argv[argc]) {
      argc++;
      argv[argc] = strtok(0, " \t");
    }
    if (argc > 0 && execute(argv, argc) == 1) {
      break;
    }
    argc = 0;
  }
  
  vfs_close(shell_tty);
  shell_tty = NULL;
}
