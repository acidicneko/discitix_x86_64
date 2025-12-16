#include <drivers/tty/tty.h>
#include <fs/stripFS.h>
#include <kernel/vfs/vfs.h>
#include <libk/shell.h>
#include <libk/stdio.h>
#include <libk/string.h>
#include <libk/utils.h>
#include <mm/pmm.h>

void execute(char **argv, int argc) {
  if (!strcmp(argv[0], "uname")) {
    printf("Discitix Kernel x86_64; Build: %s\n\n", __DATE__);
  } else if (!strcmp(argv[0], "sysfetch")) {
    sysfetch();
  } else if (!strcmp(argv[0], "about")) {
    printf("ayu.sh shell; the so called shell\n\n");
  } else if (!strcmp(argv[0], "dbgln")) {
    for (uint8_t i = 1; i < argc; i++) {
      dbgln("%s ", argv[i]);
    }
    dbgln("\n\r");
  } else if (!strcmp(argv[0], "read")) {
    read_initrd_stripFS();
  } else if (!strcmp(argv[0], "echo")) {
    for (uint8_t i = 1; i < argc; i++) {
      printf("%s ", argv[i]);
    }
    printf("\n");
  } else if (!strcmp(argv[0], "clear")) {
    tty_clear();
  } else if (!strcmp(argv[0], "free")) {
    printf("\t\ttotal\t\tused\t\tfree\n");
    printf("Mem:\t\t%ul\t\t%ul\t\t%ul\n", get_total_physical_memory() / 1024,
           (get_total_physical_memory() - get_free_physical_memory()) / 1024,
           get_free_physical_memory() / 1024);
  } else if (!strcmp(argv[0], "ls")) {
    superblock_t *sb = vfs_get_root_superblock();
    if (!sb || !sb->root || !sb->root->inode || !sb->root->inode->private) {
      printf("No filesystem mounted.\n\n");
      return;
    }
    dentry_t *d = sb->root;
    d = d->next; // skip root itself
    while (d) {
      printf("%s\n", d->name);
      d = d->next;
    }
    printf("\n");
  } else if(!strcmp(argv[0], "cat")) {
    if (argc < 2) {
      printf("Usage: cat <filename>\n\n");
      return;
    }
    superblock_t *sb = vfs_get_root_superblock();
    if (!sb || !sb->root || !sb->root->inode) {
      printf("No filesystem mounted.\n\n");
      return;
    }
    sb->root->inode->is_directory = 1; // ensure root is marked as directory
    inode_t *file_inode = NULL;
    if (vfs_lookup(sb->root->inode, argv[1], &file_inode) != 0 || !file_inode) {
      printf("ayu.sh: cat: %s: No such file\n\n", argv[1]);
      return;
    }
    file_t *file = NULL;
    if (vfs_open(&file, file_inode, 0) != 0 || !file) {
      printf("ayu.sh: cat: %s: Failed to open file\n\n", argv[1]);
      return;
    }
    char buffer[1024];
    long bytes_read = vfs_read(file, buffer, sizeof(buffer) - 1);
    if (bytes_read < 0) {
      printf("ayu.sh: cat: %s: Failed to read file\n\n", argv[1]);
      return;
    }
    buffer[bytes_read] = '\0'; // null-terminate the buffer
    printf("%s\n", buffer);
    vfs_close(file);
  } else {
    printf("ayu.sh: %s: unknown command\n\n", argv[0]);
  }
}

void init_shell() {
  char *argv[100];
  char input[1024];
  int argc = 0;
  while (1) {
    printf(">> ");
    gets(&input[0]);
    argv[argc] = strtok(input, " \t");
    while (argv[argc]) {
      argc++;
      argv[argc] = strtok(0, " \t");
    }
    execute(argv, argc);
    argc = 0;
  }
}
