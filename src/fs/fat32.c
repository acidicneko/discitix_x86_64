#include "kernel/vfs/vfs.h"
#include <fs/fat32.h>
#include <drivers/ata.h>
#include <libk/string.h>
#include <mm/liballoc.h>
#include <libk/stdio.h>

static uint32_t fat_start_lba;
static uint32_t data_start_lba;
static uint8_t  sectors_per_cluster;
static uint32_t root_dir_cluster;

static uint32_t cluster_to_lba(uint32_t cluster) {
    return data_start_lba + ((cluster - 2) * sectors_per_cluster);
}

static uint32_t get_next_cluster(uint32_t current_cluster) {
    uint32_t fat_sector = fat_start_lba + ((current_cluster * 4) / 512);
    uint32_t fat_offset = (current_cluster * 4) % 512;

    uint8_t sector_buffer[512];
    if (!ata_read_sector(fat_sector, sector_buffer)) {
        return FAT32_EOC_MARKER; // Return End Of Chain on read error
    }

    uint32_t next_cluster = *((uint32_t*)&sector_buffer[fat_offset]);
    return next_cluster & 0x0FFFFFFF;
}

static bool compare_name_83(const char *entry_name, const char *search_name) {
    for (int i = 0; i < 11; i++) {
        if (entry_name[i] != search_name[i]) return false;
    }
    return true;
}

bool fat32_read_file(const char *filename_83, uint8_t *output_buffer) {
    uint32_t current_cluster = root_dir_cluster;
    uint8_t  sector_buf[512];
    
    uint32_t file_start_cluster = 0;
    uint32_t file_size = 0;
    bool     file_found = false;

    // --- PHASE 1: Search the Root Directory ---
    while (current_cluster < FAT32_EOC_MARKER && !file_found) {
        uint32_t cluster_lba = cluster_to_lba(current_cluster);

        // Read all sectors in this directory cluster
        for (int i = 0; i < sectors_per_cluster; i++) {
            ata_read_sector(cluster_lba + i, sector_buf);
            fat32_dir_t *entries = (fat32_dir_t *)sector_buf;

            // 16 entries of 32-bytes per 512-byte sector
            for (int e = 0; e < 16; e++) {
                if (entries[e].name[0] == 0x00) {
                    // 0x00 means end of directory. File doesn't exist.
                    return false; 
                }
                if ((unsigned char)entries[e].name[0] == 0xE5) {
                    // 0xE5 means deleted file, skip it
                    continue; 
                }
                if (entries[e].attributes == FAT_ATTR_LFN) {
                    // Long File Name entry, skip it for our basic driver
                    continue; 
                }

                // Check if this is the file we want
                if (compare_name_83(entries[e].name, filename_83)) {
                    // Combine high and low 16-bit words to get the 32-bit cluster
                    file_start_cluster = ((uint32_t)entries[e].cluster_high << 16) | entries[e].cluster_low;
                    file_size = entries[e].file_size;
                    file_found = true;
                    break;
                }
            }
            if (file_found) break;
        }

        if (!file_found) {
            // Move to the next cluster of the directory
            current_cluster = get_next_cluster(current_cluster);
        }
    }

    if (!file_found) return false;

    // --- PHASE 2: Read the File Data ---
    uint32_t bytes_read = 0;
    current_cluster = file_start_cluster;
    uint8_t *write_ptr = output_buffer;

    while (current_cluster < FAT32_EOC_MARKER && bytes_read < file_size) {
        uint32_t cluster_lba = cluster_to_lba(current_cluster);

        for (int i = 0; i < sectors_per_cluster; i++) {
            ata_read_sector(cluster_lba + i, sector_buf);

            // Determine how many bytes to copy from this sector
            uint32_t bytes_to_copy = 512;
            if ((file_size - bytes_read) < 512) {
                bytes_to_copy = file_size - bytes_read;
            }

            // Copy to output buffer
            for (uint32_t b = 0; b < bytes_to_copy; b++) {
                *write_ptr++ = sector_buf[b];
            }

            bytes_read += bytes_to_copy;
            if (bytes_read >= file_size) break;
        }

        // Get the next cluster in the chain from the FAT
        current_cluster = get_next_cluster(current_cluster);
    }

    return true; // File successfully read into memory!
}

bool fat32_init(uint32_t partition_lba) {
    uint8_t boot_sector[512];

    // Read the very first sector of the FAT32 partition
    if (!ata_read_sector(partition_lba, boot_sector)) {
        return false; // Disk read error
    }

    fat32_bpb_t *bpb = (fat32_bpb_t *)boot_sector;

    // Verify it's actually a FAT32 volume
    // 0x28 or 0x29 are standard DOS/Windows FAT32 signatures
    if (bpb->boot_signature != 0x28 && bpb->boot_signature != 0x29) {
        return false; 
    }

    // Populate our global variables so the rest of the driver works
    sectors_per_cluster = bpb->sectors_per_cluster;
    root_dir_cluster    = bpb->root_cluster;

    // Calculate exactly where the FAT and the Data regions start on the disk
    fat_start_lba  = partition_lba + bpb->reserved_sectors;
    data_start_lba = fat_start_lba + (bpb->fat_size_32 * bpb->fat_count);

    return true;
}



typedef struct {
    uint8_t  status;
    uint8_t  chs_first[3];
    uint8_t  type;               
    uint8_t  chs_last[3];
    uint32_t lba_start;          
    uint32_t sector_count;
} __attribute__((packed)) mbr_partition_t;

void mount_filesystem(void) {
    uint8_t mbr_sector[512];
    
    // Read the MBR
    if (!ata_read_sector(0, mbr_sector)) {
        printf("CRITICAL: Failed to read disk!\n");
        return;
    }

    // Go to offset 446 to read Partition 1
    mbr_partition_t *part1 = (mbr_partition_t *)(mbr_sector + 0x1BE);
    uint32_t fat32_start_lba = part1->lba_start;

    printf("Partition 1 found at LBA: %u\n", fat32_start_lba);

    // Initialize FAT32 at that LBA
    if (fat32_mount_root(fat32_start_lba)) {
        printf("FAT32 initialized successfully!\n");
    } else {
        printf("Failed to mount FAT32!\n");
    }
}

file_operations_t fat32_file_ops = {
    .read = fat32_vfs_read,
    .write = fat32_vfs_write, // Implement write later
    .open = NULL,  // Optional depending on your VFS implementation
    .close = NULL
};

inode_operations_t fat32_inode_ops = {
    .lookup = fat32_vfs_lookup,
    .create = fat32_vfs_create,
    .mkdir = fat32_vfs_mkdir,
    .unlink = fat32_vfs_unlink,
    .getdents = fat32_dir_getdents
};


// --- VFS: Read ---
long fat32_vfs_read(file_t *file, void *buf, size_t len, uint64_t offset) {
    if (!file || !file->inode || !buf) return -1;
    
    uint32_t file_size = file->inode->size;
    uint32_t current_cluster = file->inode->ino; // We stored the start cluster here

    // Bounds checking
    if (offset >= file_size) return 0; // EOF
    if (offset + len > file_size) len = file_size - offset; // Truncate read

    uint32_t cluster_size = sectors_per_cluster * 512;
    uint32_t clusters_to_skip = offset / cluster_size;
    uint32_t offset_in_cluster = offset % cluster_size;

    // Fast-forward the FAT chain to the requested offset
    for (uint32_t i = 0; i < clusters_to_skip; i++) {
        current_cluster = get_next_cluster(current_cluster);
        if (current_cluster >= FAT32_EOC_MARKER) return 0; // Unexpected EOF
    }

    uint8_t *dest = (uint8_t *)buf;
    uint32_t bytes_read = 0;
    uint8_t sector_buf[512];

    // Read the data spanning clusters
    while (bytes_read < len && current_cluster < FAT32_EOC_MARKER) {
        uint32_t cluster_lba = cluster_to_lba(current_cluster);
        
        for (int i = 0; i < sectors_per_cluster && bytes_read < len; i++) {
            ata_read_sector(cluster_lba + i, sector_buf);

            // Calculate how much to copy from this sector
            uint32_t start_byte = (bytes_read == 0) ? (offset_in_cluster % 512) : 0;
            uint32_t bytes_to_copy = 512 - start_byte;
            if (bytes_to_copy > (len - bytes_read)) bytes_to_copy = len - bytes_read;

            memcpy(dest + bytes_read, sector_buf + start_byte, bytes_to_copy);
            bytes_read += bytes_to_copy;
        }
        current_cluster = get_next_cluster(current_cluster);
    }
    return bytes_read;
}

// --- VFS: Lookup ---

void format_83_name(const char *input, char *output) {
    memset(output, ' ', 11); // Fill with spaces
    int i = 0, j = 0;

    // Copy name
    while (input[i] != '.' && input[i] != '\0' && j < 8) {
        output[j++] = toupper(input[i++]);
    }

    // Skip to extension if there is a dot
    while (input[i] != '.' && input[i] != '\0') i++;
    
    if (input[i] == '.') {
        i++; // Skip the dot
        j = 8; // Move to extension area
        while (input[i] != '\0' && j < 11) {
            output[j++] = toupper(input[i++]);
        }
    }
}

inode_t* fat32_vfs_lookup(inode_t *parent, const char *name) {
  if (!parent || parent->type != FT_DIR) return NULL;

  char target_name[11];
  format_83_name(name, target_name); 

  uint32_t current_cluster = parent->ino;
  uint8_t sector_buf[512];

  while (current_cluster < FAT32_EOC_MARKER) {
      uint32_t cluster_lba = cluster_to_lba(current_cluster);

      for (int i = 0; i < sectors_per_cluster; i++) {
          ata_read_sector(cluster_lba + i, sector_buf);
          fat32_dir_t *entries = (fat32_dir_t *)sector_buf;

          for (int e = 0; e < 16; e++) {
              if (entries[e].name[0] == 0x00) return NULL; // End of dir
              if ((unsigned char)entries[e].name[0] == 0xE5) continue;
              if (entries[e].attributes == FAT_ATTR_LFN) continue;

              if (compare_name_83(entries[e].name, target_name)) {
                  // Match found! Create the new VFS Inode
                  inode_t *new_inode = (inode_t *)kmalloc(sizeof(inode_t));
                  memset(new_inode, 0, sizeof(inode_t));

                  new_inode->ino = ((uint32_t)entries[e].cluster_high << 16) | entries[e].cluster_low;
                  new_inode->size = entries[e].file_size;
                  
                  if (entries[e].attributes & FAT_ATTR_DIRECTORY) {
                      new_inode->type = FT_DIR;
                      new_inode->is_directory = 1;
                  } else {
                      new_inode->type = FT_REG;
                      new_inode->is_directory = 0;
                  }

                  new_inode->f_ops = &fat32_file_ops;
                  new_inode->i_ops = &fat32_inode_ops;
                  fat32_node_info_t *info = (fat32_node_info_t *)kmalloc(sizeof(fat32_node_info_t));
                  info->dir_entry_lba = cluster_lba + i;
                  info->dir_entry_offset = e * sizeof(fat32_dir_t); // e * 32
                  new_inode->private = info;
                  return new_inode;
              }
          }
      }
      current_cluster = get_next_cluster(current_cluster);
  }
  return NULL; // Not found
}

// Define the operation tables

superblock_t root_sb;
dentry_t     root_dentry;
inode_t      root_inode;

int fat32_mount_root(uint32_t partition_lba) {
    // 1. Initialize the raw FAT32 driver
    if (!fat32_init(partition_lba)) {
        return -1; // Failed to parse boot sector
    }

    // 2. Setup the Root Inode
    memset(&root_inode, 0, sizeof(inode_t));
    root_inode.ino = root_dir_cluster; // Cluster 2 usually
    root_inode.size = 0;               // Directories don't have a strict size in FAT32
    root_inode.type = FT_DIR;
    root_inode.is_directory = 1;
    root_inode.f_ops = &fat32_file_ops;
    root_inode.i_ops = &fat32_inode_ops;

    // 3. Setup the Root Dentry
    memset(&root_dentry, 0, sizeof(dentry_t));
    strcpy(root_dentry.name, "/");
    root_dentry.inode = &root_inode;

    // 4. Setup the Superblock
    memset(&root_sb, 0, sizeof(superblock_t));
    strcpy(root_sb.fs_type, "fat32");
    root_sb.root = &root_dentry;
    // vfs_mkdir("/hdd"); 
    // 5. Register with your VFS!
    return vfs_mount(&root_sb, "/");
}



// Helper macro if you don't have it globally defined yet
#define ALIGN_UP(val, align) (((val) + (align) - 1) & ~((align) - 1))

static void parse_83_name(const char *fat_name, char *out_name) {
    int out_idx = 0;
    
    // 1. Copy the name (up to 8 characters), ignoring trailing spaces
    for (int i = 0; i < 8 && fat_name[i] != ' '; i++) {
        // Convert to lowercase for standard POSIX output
        out_name[out_idx++] = (fat_name[i] >= 'A' && fat_name[i] <= 'Z') ? fat_name[i] + 32 : fat_name[i];
    }

    // 2. Check if there is an extension (the 9th byte is not a space)
    if (fat_name[8] != ' ') {
        out_name[out_idx++] = '.'; // Add the dot

        // 3. Copy the extension (up to 3 characters)
        for (int i = 8; i < 11 && fat_name[i] != ' '; i++) {
            out_name[out_idx++] = (fat_name[i] >= 'A' && fat_name[i] <= 'Z') ? fat_name[i] + 32 : fat_name[i];
        }
    }
    
    // 4. Null-terminate the string so printf() and strlen() don't read garbage
    out_name[out_idx] = '\0'; 
}

long fat32_dir_getdents(inode_t *inode, uint64_t *offset, void *buf_ptr, uint32_t count) {
    if (!inode || inode->type != FT_DIR) return -1;

    char *buf = (char *)buf_ptr;
    uint32_t bytes_written = 0;
    
    uint32_t current_cluster = inode->ino;
    uint8_t sector_buf[512];

    // Optimization: Fast-forward the FAT chain based on the current offset
    // Every 512-byte sector holds 16 entries (512 / 32)
    uint32_t entries_per_cluster = sectors_per_cluster * 16;
    uint32_t clusters_to_skip = (*offset) / entries_per_cluster;
    
    for (uint32_t i = 0; i < clusters_to_skip; i++) {
        current_cluster = get_next_cluster(current_cluster);
        if (current_cluster >= FAT32_EOC_MARKER) return bytes_written; // EOF
    }

    while (current_cluster < FAT32_EOC_MARKER) {
        uint32_t cluster_lba = cluster_to_lba(current_cluster);

        // Calculate which sector in this cluster we should start reading from
        uint32_t start_sector = ((*offset) % entries_per_cluster) / 16;
        
        for (uint32_t i = start_sector; i < sectors_per_cluster; i++) {
            ata_read_sector(cluster_lba + i, sector_buf);
            fat32_dir_t *entries = (fat32_dir_t *)sector_buf;

            // Calculate which entry index in this sector to start from
            uint32_t start_entry = ((*offset) % 16);
            
            for (uint32_t e = start_entry; e < 16; e++) {
                if (entries[e].name[0] == 0x00) {
                    return bytes_written; // 0x00 means end of directory!
                }
                
                // Skip deleted files and Long File Names
                if ((unsigned char)entries[e].name[0] == 0xE5 || entries[e].attributes == FAT_ATTR_LFN) {
                    (*offset)++; // Still increment offset so we don't read it again
                    continue;
                }

                // We have a valid file! Parse its name.
                char parsed_name[13];
                parse_83_name(entries[e].name, parsed_name); // Assuming you kept this helper
                
                uint32_t name_len = strlen(parsed_name);
                uint16_t reclen = ALIGN_UP(sizeof(struct linux_dirent64) + name_len + 1, 8);

                // If userland didn't give us enough buffer space, pause here.
                // The syscall will be called again and pick up at the current *offset.
                if (bytes_written + reclen > count) {
                    return bytes_written;
                }

                // Populate the dirent64 structure exactly like stripFS
                struct linux_dirent64 *dirent = (struct linux_dirent64 *)(buf + bytes_written);
                dirent->d_ino = ((uint32_t)entries[e].cluster_high << 16) | entries[e].cluster_low;
                dirent->d_off = *offset + 1;
                dirent->d_reclen = reclen;
                dirent->d_type = (entries[e].attributes & FAT_ATTR_DIRECTORY) ? DT_DIR : DT_REG;
                
                memcpy((uint8_t*)dirent->d_name, (const uint8_t*)parsed_name, name_len + 1);

                bytes_written += reclen;
                (*offset)++; 
            }
        }
        current_cluster = get_next_cluster(current_cluster);
    }

    return bytes_written;
}

static void set_next_cluster(uint32_t current_cluster, uint32_t next_cluster) {
    uint32_t fat_sector = fat_start_lba + ((current_cluster * 4) / 512);
    uint32_t fat_offset = (current_cluster * 4) % 512;

    uint8_t sector_buffer[512];
    ata_read_sector(fat_sector, sector_buffer);

    uint32_t *entry = (uint32_t*)&sector_buffer[fat_offset];
    
    // FAT32 requires preserving the top 4 bits of the entry!
    *entry = (*entry & 0xF0000000) | (next_cluster & 0x0FFFFFFF);

    ata_write_sector(fat_sector, sector_buffer);
}

// 2. Find a free cluster, claim it, and wipe its data
static uint32_t fat32_allocate_cluster(void) {
    // Scan the FAT for an empty slot (Cluster 2 is the start of data)
    // Note: A production OS caches the free cluster count to avoid starting at 2 every time.
    for (uint32_t cluster = 2; cluster < 0x0FFFFFF0; cluster++) {
        if (get_next_cluster(cluster) == 0x00000000) { // Free cluster found!
            
            // Mark it as the end of a file chain
            set_next_cluster(cluster, FAT32_EOC_MARKER);
            
            // Wipe the raw data on the disk so old garbage data isn't read
            uint8_t zero_buf[512];
            memset(zero_buf, 0, 512);
            uint32_t lba = cluster_to_lba(cluster);
            
            for (int i = 0; i < sectors_per_cluster; i++) {
                ata_write_sector(lba + i, zero_buf);
            }
            
            return cluster;
        }
    }
    return 0; // Disk is completely full
}

#define FAT_ATTR_ARCHIVE 0x20

// 3. Find a free slot in a directory and write the new file's metadata
static bool fat32_add_entry(uint32_t dir_cluster, const char *filename_83, uint8_t attr, uint32_t start_cluster) {
    uint8_t sector_buf[512];
    uint32_t current_cluster = dir_cluster;
    
    while (current_cluster < FAT32_EOC_MARKER) {
        uint32_t cluster_lba = cluster_to_lba(current_cluster);
        
        for (int i = 0; i < sectors_per_cluster; i++) {
            ata_read_sector(cluster_lba + i, sector_buf);
            fat32_dir_t *entries = (fat32_dir_t *)sector_buf;
            
            for (int e = 0; e < 16; e++) {
                // 0x00 = never used, 0xE5 = deleted file. We can overwrite both!
                if (entries[e].name[0] == 0x00 || (unsigned char)entries[e].name[0] == 0xE5) {
                    
                    memset(&entries[e], 0, sizeof(fat32_dir_t));
                    memcpy(entries[e].name, filename_83, 11); // Copy the 11-byte formatted name
                    entries[e].attributes = attr;
                    entries[e].cluster_high = (start_cluster >> 16) & 0xFFFF;
                    entries[e].cluster_low = start_cluster & 0xFFFF;
                    entries[e].file_size = 0; // Starts empty
                    
                    // Commit the new entry to the hard drive
                    ata_write_sector(cluster_lba + i, sector_buf);
                    return true;
                }
            }
        }
        
        // If the directory cluster is full, we must check the next cluster
        uint32_t next = get_next_cluster(current_cluster);
        if (next >= FAT32_EOC_MARKER) {
            // Advanced: If the directory is completely full, you must allocate a NEW cluster
            // using fat32_allocate_cluster() and link it to current_cluster here.
            // For now, we will just fail if a directory exceeds its pre-allocated space.
            return false; 
        }
        current_cluster = next;
    }
    return false; 
}


int fat32_vfs_create(inode_t *parent, const char *name, uint32_t mode) {
    if (!parent || parent->type != FT_DIR) return -1;
    
    char target_name[11];
    format_83_name(name, target_name);
    if(fat32_vfs_lookup(parent, name) != NULL) return -1;
    // Grab space on the disk
    uint32_t new_cluster = fat32_allocate_cluster();
    if (new_cluster == 0) return -1; 
    
    // Add the file to the parent folder
    if (!fat32_add_entry(parent->ino, target_name, FAT_ATTR_ARCHIVE, new_cluster)) {
        return -1;
    }
    
    return 0;
}

int fat32_vfs_mkdir(inode_t *parent, const char *name) {
    if (!parent || parent->type != FT_DIR) return -1;
    if(fat32_vfs_lookup(parent, name) != NULL) return -1;

    char target_name[11];
    format_83_name(name, target_name);
    
    uint32_t new_cluster = fat32_allocate_cluster();
    if (new_cluster == 0) return -1;
    
    // 1. Add the new folder to the parent folder
    if (!fat32_add_entry(parent->ino, target_name, FAT_ATTR_DIRECTORY, new_cluster)) {
        return -1;
    }
    
    // 2. FAT32 Specification Requirement: 
    // Every new directory must instantly contain a "." and ".." entry.
    fat32_add_entry(new_cluster, ".          ", FAT_ATTR_DIRECTORY, new_cluster);
    fat32_add_entry(new_cluster, "..         ", FAT_ATTR_DIRECTORY, parent->ino);
    
    return 0;
}


long fat32_vfs_write(file_t *file, const void *buf, size_t len, uint64_t offset) {
    if (!file || !file->inode || !buf || len == 0) return -1;
    
    uint32_t current_cluster = file->inode->ino; 
    uint32_t file_size = file->inode->size;
    uint32_t cluster_size = sectors_per_cluster * 512;
    
    // --- 1. FAT CHAIN EXTENSION ---
    // If we are writing past our currently allocated space, we need more clusters!
    uint32_t required_size = offset + len;
    uint32_t current_clusters_owned = (file_size + cluster_size - 1) / cluster_size;
    if (file_size == 0) current_clusters_owned = 1; // Our create() gives us 1 starting cluster
    
    uint32_t needed_clusters = (required_size + cluster_size - 1) / cluster_size;
    
    if (needed_clusters > current_clusters_owned) {
        // Find the end of the current file chain
        uint32_t tail = current_cluster;
        while (get_next_cluster(tail) < FAT32_EOC_MARKER && get_next_cluster(tail) != 0) {
            tail = get_next_cluster(tail);
        }
        
        // Allocate the difference and link it up
        for (uint32_t i = current_clusters_owned; i < needed_clusters; i++) {
            uint32_t new_cluster = fat32_allocate_cluster(); 
            if (new_cluster == 0) return -1; // Disk full
            
            // Link the old tail to the new cluster
            set_next_cluster(tail, new_cluster);
            tail = new_cluster;
        }
    }
    
    // --- 2. FAST-FORWARD TO OFFSET ---
    uint32_t clusters_to_skip = offset / cluster_size;
    uint32_t offset_in_cluster = offset % cluster_size;
    
    uint32_t target_cluster = current_cluster;
    for (uint32_t i = 0; i < clusters_to_skip; i++) {
        target_cluster = get_next_cluster(target_cluster);
    }
    
    // --- 3. WRITE THE SECTORS ---
    const uint8_t *src = (const uint8_t *)buf;
    uint32_t bytes_written = 0;
    uint8_t sector_buf[512];
    
    while (bytes_written < len && target_cluster < FAT32_EOC_MARKER) {
        uint32_t cluster_lba = cluster_to_lba(target_cluster);
        
        for (int i = 0; i < sectors_per_cluster && bytes_written < len; i++) {
            uint32_t current_lba = cluster_lba + i;
            
            uint32_t start_byte = (bytes_written == 0) ? (offset_in_cluster % 512) : 0;
            uint32_t bytes_to_copy = 512 - start_byte;
            if (bytes_to_copy > (len - bytes_written)) bytes_to_copy = len - bytes_written;
            
            // Critical: If we are doing a partial sector write, read it first so we don't erase existing data!
            if (bytes_to_copy < 512) {
                ata_read_sector(current_lba, sector_buf);
            }
            
            memcpy(sector_buf + start_byte, src + bytes_written, bytes_to_copy);
            ata_write_sector(current_lba, sector_buf);
            
            bytes_written += bytes_to_copy;
        }
        target_cluster = get_next_cluster(target_cluster);
    }
    
    // --- 4. THE METADATA MAGIC ---
    // If we grew the file, update the RAM Inode AND the Hard Drive!
    if (offset + bytes_written > file->inode->size) {
        file->inode->size = offset + bytes_written;
        
        if (file->inode->private) {
            fat32_node_info_t *info = (fat32_node_info_t *)file->inode->private;
            
            // Read the exact sector where the 32-byte directory entry lives
            ata_read_sector(info->dir_entry_lba, sector_buf);
            
            // Point a struct to the exact offset and update the size!
            fat32_dir_t *entry = (fat32_dir_t *)(sector_buf + info->dir_entry_offset);
            entry->file_size = file->inode->size;
            
            // Commit it to disk. Now it survives reboots!
            ata_write_sector(info->dir_entry_lba, sector_buf);
        }
    }
    
    return bytes_written;
}


static void fat32_free_chain(uint32_t start_cluster) {
    if (start_cluster < 2 || start_cluster >= FAT32_EOC_MARKER) return;

    uint32_t current = start_cluster;
    while (current < FAT32_EOC_MARKER && current != 0) {
        uint32_t next = get_next_cluster(current);
        
        // Mark the current cluster as completely free!
        set_next_cluster(current, 0x00000000); 
        
        current = next;
    }
}

int fat32_vfs_unlink(inode_t *parent, const char *name) {
    if (!parent || parent->type != FT_DIR) return -1;

    char target_name[11];
    format_83_name(name, target_name);

    uint32_t current_cluster = parent->ino;
    uint8_t sector_buf[512];

    while (current_cluster < FAT32_EOC_MARKER) {
        uint32_t cluster_lba = cluster_to_lba(current_cluster);

        for (int i = 0; i < sectors_per_cluster; i++) {
            ata_read_sector(cluster_lba + i, sector_buf);
            fat32_dir_t *entries = (fat32_dir_t *)sector_buf;

            for (int e = 0; e < 16; e++) {
                if (entries[e].name[0] == 0x00) return -1; // Reached end of dir, file not found
                if ((unsigned char)entries[e].name[0] == 0xE5) continue;
                if (entries[e].attributes == FAT_ATTR_LFN) continue;

                if (compare_name_83(entries[e].name, target_name)) {
                    // WE FOUND IT!
                    uint32_t target_start_cluster = ((uint32_t)entries[e].cluster_high << 16) | entries[e].cluster_low;
                    
                    // Optional POSIX Check: If it's a directory, you usually want to ensure it's empty first.
                    // For now, we will forcefully delete it.
                    
                    // 1. Mark the directory entry as deleted
                    entries[e].name[0] = 0xE5;
                    
                    // 2. Commit the deleted marker to the hard drive
                    ata_write_sector(cluster_lba + i, sector_buf);
                    
                    // 3. Free the actual disk space!
                    fat32_free_chain(target_start_cluster);
                    
                    return 0; // Success
                }
            }
        }
        current_cluster = get_next_cluster(current_cluster);
    }
    return -1; // File not found
}
