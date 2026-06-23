#include <fs/procfs.h>
#include <stdint.h>
#include <kernel/vfs/vfs.h>
#include <libk/stdio.h>
#include <libk/string.h>
#include <mm/pmm.h>
#include <libk/utils.h>
#include <mm/liballoc.h>

long mem_read(file_t* file, void* buf, size_t len, uint64_t off){
  (void)file;
  char tmp[128];

    int total = sprintf(
        tmp,
        "%ul/%ul\n",
        get_free_physical_memory(),
        get_total_physical_memory()
    );

    if (off >= (uint64_t)total)
        return 0;

    int remaining = total - off;

    if ((int)len > remaining)
        len = remaining;

    memcpy(buf, (uint8_t*)(tmp + off), len);

    return len;
}

struct file_operations procfs_file_ops = {
    .open = NULL,
    .close = NULL,
    .read = mem_read,
    .write = NULL
};

void init_procfs(){
    vfs_mkdir("/proc");
    inode_t* proc_inode = NULL;
    superblock_t* root_sb = vfs_get_root_superblock();
    if(!root_sb){
        dbgln("Procfs: Failed to get root superblock!\n\r");
        return;
    }
    proc_inode = (inode_t*)kmalloc(sizeof(inode_t));
    memset(proc_inode, 0, sizeof(inode_t));
    proc_inode->atime = 0;
    proc_inode->ctime = 0;
    proc_inode->is_directory = 0;
    proc_inode->type = FT_REG;
    proc_inode->size = 0;
    proc_inode->ino = 10000;
    proc_inode->f_ops = &procfs_file_ops;
    proc_inode->mode = 2;

    vfs_register_device("/proc/meminfo", proc_inode);
    dbgln("Procfs: Registered /proc/meminfo\n\r");
}
