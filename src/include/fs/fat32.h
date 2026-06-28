#ifndef __FAT32__
#define __FAT32__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <kernel/vfs/vfs.h>

typedef struct {
    char     name[11];            // 8.3 format (8 chars name, 3 chars extension, space padded)
    uint8_t  attributes;
    uint8_t  reserved;
    uint8_t  creation_time_tenths;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t cluster_high;        // High 16 bits of the file's first cluster
    uint16_t last_mod_time;
    uint16_t last_mod_date;
    uint16_t cluster_low;         // Low 16 bits of the file's first cluster
    uint32_t file_size;           // Size of the file in bytes
} __attribute__((packed)) fat32_dir_t;

// BIOS Parameter Block (BPB) for FAT32
typedef struct {
    uint8_t  jmp[3];
    char     oem[8];
    uint16_t bytes_per_sector;      
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;      
    uint8_t  fat_count;             
    uint16_t dir_entries;           
    uint16_t total_sectors_16;      
    uint8_t  media_descriptor;
    uint16_t fat_size_16;           
    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;        
    uint32_t total_sectors_32;

    // FAT32 Extended Boot Record
    uint32_t fat_size_32;           
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;          
    uint16_t fs_info;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;        // We check this for 0x28 or 0x29
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type[8];            
} __attribute__((packed)) fat32_bpb_t;

typedef struct {
    uint32_t dir_entry_lba;     // The exact sector on the disk
    uint32_t dir_entry_offset;  // The byte offset inside that sector (0 to 480)
} fat32_node_info_t;

#define FAT_ATTR_READ_ONLY 0x01
#define FAT_ATTR_HIDDEN    0x02
#define FAT_ATTR_SYSTEM    0x04
#define FAT_ATTR_VOLUME_ID 0x08
#define FAT_ATTR_DIRECTORY 0x10
#define FAT_ATTR_ARCHIVE   0x20
#define FAT_ATTR_LFN       (FAT_ATTR_READ_ONLY | FAT_ATTR_HIDDEN | FAT_ATTR_SYSTEM | FAT_ATTR_VOLUME_ID)

// End of Cluster Chain markers
#define FAT32_EOC_MARKER   0x0FFFFFF8 

bool fat32_init(uint32_t partition_lba);
bool fat32_read_file(const char *filename_83, uint8_t *output_buffer);
void fat32_list_root_dir(void);


bool fat32_init(uint32_t partition_lba);
void mount_filesystem(void);

long fat32_vfs_read(file_t *file, void *buf, size_t len, uint64_t offset);
inode_t* fat32_vfs_lookup(inode_t *parent, const char *name);
int fat32_mount_root(uint32_t partition_lba);
long fat32_dir_getdents(inode_t *inode, uint64_t *offset, void *buf_ptr, uint32_t count); 
long fat32_vfs_write(file_t *file, const void *buf, size_t len, uint64_t offset);
int fat32_vfs_create(inode_t *parent, const char *name, uint32_t mode);
int fat32_vfs_mkdir(inode_t *parent, const char *name);
int fat32_vfs_unlink(inode_t *parent, const char *name);
#endif // __FAT32__
