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
static uint32_t next_free_cluster_hint = 2;
/* ---------------------------------------------------------------------
 * FAT sector cache
 * --------------------------------------------------------------------- */

#define FAT_CACHE_INVALID 0xFFFFFFFFu

static uint32_t fat_cache_sector = FAT_CACHE_INVALID;
static uint8_t  fat_cache_buf[512];
static bool     fat_cache_dirty = false;

static void fat32_fat_sync(void) {
    if (fat_cache_dirty && fat_cache_sector != FAT_CACHE_INVALID) {
        ata_write_sector(fat_cache_sector, fat_cache_buf);
        fat_cache_dirty = false;
    }
}

/* Returns a pointer to the (possibly newly loaded) cached sector buffer. */
static uint8_t *fat_cache_get(uint32_t sector) {
    if (sector != fat_cache_sector) {
        fat32_fat_sync();
        ata_read_sector(sector, fat_cache_buf);
        fat_cache_sector = sector;
    }
    return fat_cache_buf;
}

static uint32_t cluster_to_lba(uint32_t cluster) {
    return data_start_lba + ((cluster - 2) * sectors_per_cluster);
}

static uint32_t get_next_cluster(uint32_t current_cluster) {
    uint32_t fat_sector = fat_start_lba + ((current_cluster * 4) / 512);
    uint32_t fat_offset = (current_cluster * 4) % 512;

    uint8_t *buf = fat_cache_get(fat_sector);
    uint32_t next_cluster = *((uint32_t *)&buf[fat_offset]);
    return next_cluster & 0x0FFFFFFF;
}

static void set_next_cluster(uint32_t current_cluster, uint32_t next_cluster) {
    uint32_t fat_sector = fat_start_lba + ((current_cluster * 4) / 512);
    uint32_t fat_offset = (current_cluster * 4) % 512;

    uint8_t *buf = fat_cache_get(fat_sector);
    uint32_t *entry = (uint32_t *)&buf[fat_offset];

    /* FAT32 requires preserving the top 4 bits of the entry! */
    *entry = (*entry & 0xF0000000) | (next_cluster & 0x0FFFFFFF);
    fat_cache_dirty = true;
}

static bool compare_name_83(const char *entry_name, const char *search_name) {
    for (int i = 0; i < 11; i++) {
        if (entry_name[i] != search_name[i]) return false;
    }
    return true;
}

/* Forward declaration — definition lives further down, grouped with the
 * write-path code it logically belongs to, but create()/mkdir() need to
 * call it on the cleanup path if fat32_add_entry_named() fails. */
static void fat32_free_chain(uint32_t start_cluster);

/* ---------------------------------------------------------------------
 * Long File Name (LFN) support
 * --------------------------------------------------------------------- */

#define LFN_LAST_ENTRY_FLAG 0x40
#define LFN_MAX_ENTRIES      20   /* 20 * 13 = 260 chars, matches spec cap */
#define LFN_CHARS_PER_ENTRY  13

/* Overlay of a 32-byte directory slot when attributes == FAT_ATTR_LFN. */
typedef struct {
    uint8_t  seq;
    uint16_t name1[5];
    uint8_t  attr;
    uint8_t  type;
    uint8_t  checksum;
    uint16_t name2[6];
    uint16_t first_cluster;
    uint16_t name3[2];
} __attribute__((packed)) fat32_lfn_t;

static uint8_t lfn_checksum(const char short_name[11]) {
    uint8_t sum = 0;
    for (int i = 0; i < 11; i++) {
        sum = (uint8_t)(((sum & 1) ? 0x80 : 0) + (sum >> 1) + (uint8_t)short_name[i]);
    }
    return sum;
}

/* Running state while scanning a directory: accumulates an in-progress
 * LFN chain so that when we hit the following short entry we can hand
 * back the reconstructed long name (and, for unlink, the locations of
 * every slot involved so they can all be marked deleted). */
typedef struct {
    char     chunks[LFN_MAX_ENTRIES][LFN_CHARS_PER_ENTRY + 1];
    uint32_t slot_lba[LFN_MAX_ENTRIES];
    uint8_t  slot_idx[LFN_MAX_ENTRIES];
    uint8_t  total_chunks;
    uint8_t  next_expected_seq;
    bool     in_progress;
    uint8_t  checksum;
} lfn_accum_t;

static void lfn_accum_reset(lfn_accum_t *acc) {
    acc->in_progress = false;
    acc->next_expected_seq = 0;
    acc->total_chunks = 0;
}

/* ASCII-subset extraction of the 13 UCS-2 chars in one LFN entry. */
static void lfn_extract_chars(fat32_lfn_t *lfn, char *out13) {
    uint16_t buf[13];
    memcpy(&buf[0],  lfn->name1, sizeof(lfn->name1));   /* 5 chars */
    memcpy(&buf[5],  lfn->name2, sizeof(lfn->name2));   /* 6 chars */
    memcpy(&buf[11], lfn->name3, sizeof(lfn->name3));   /* 2 chars */

    for (int idx = 0; idx < 13; idx++) {
        uint16_t ch = buf[idx];
        if (ch == 0x0000 || ch == 0xFFFF) {
            out13[idx] = '\0';
            return;
        }
        out13[idx] = (ch < 0x80) ? (char)ch : '_';
    }
    out13[13] = '\0';
}


/* Feed one raw 32-byte directory slot into the accumulator.
 * If this slot is a short entry preceded by a complete, checksum-valid
 * LFN chain, returns true and fills out_name (must be >= 256 bytes). */
static bool lfn_feed_entry(lfn_accum_t *acc, fat32_dir_t *raw, uint32_t lba,
                            uint8_t idx, char *out_name) {
    if (raw->attributes == FAT_ATTR_LFN) {
        fat32_lfn_t *lfn = (fat32_lfn_t *)raw;
        uint8_t seq = lfn->seq & 0x1F;

        if (seq == 0 || seq > LFN_MAX_ENTRIES) { lfn_accum_reset(acc); return false; }

        if (lfn->seq & LFN_LAST_ENTRY_FLAG) {
            lfn_accum_reset(acc);
            acc->in_progress       = true;
            acc->checksum          = lfn->checksum;
            acc->total_chunks      = seq;
            acc->next_expected_seq = seq;
        } else if (!acc->in_progress || lfn->checksum != acc->checksum ||
                   seq != acc->next_expected_seq - 1) {
            lfn_accum_reset(acc);
            return false;
        } else {
            acc->next_expected_seq = seq;
        }

        lfn_extract_chars(lfn, acc->chunks[seq - 1]);
        acc->slot_lba[seq - 1] = lba;
        acc->slot_idx[seq - 1] = idx;
        return false;
    }

    /* Short (8.3) entry. */
    bool have_name = false;
    if (acc->in_progress && acc->next_expected_seq == 1 &&
        lfn_checksum(raw->name) == acc->checksum) {
        out_name[0] = '\0';
        for (int i = 0; i < acc->total_chunks; i++) {
            strcat(out_name, acc->chunks[i]);
        }
        have_name = true;
    }
    lfn_accum_reset(acc);
    return have_name;
}

/* Does `name` fit losslessly in 8.3 (upper-case, no spaces, <=8 base
 * chars, <=3 ext chars, at most one dot)? If so no LFN chain is needed. */
static bool name_fits_83(const char *name) {
    int len = 0, dots = 0, base_len = 0, ext_len = 0;
    bool in_ext = false;
    for (const char *p = name; *p; p++, len++) {
        if (len > 12) return false;
        char c = *p;
        if (c == '.') {
            dots++;
            if (dots > 1) return false;
            in_ext = true;
            continue;
        }
        if (c == ' ' || islower((unsigned char)c)) return false;
        if (in_ext) { if (++ext_len > 3) return false; }
        else        { if (++base_len > 8) return false; }
    }
    return base_len > 0;
}

void format_83_name(const char *input, char *output) {
    memset(output, ' ', 11); /* Fill with spaces */
    int i = 0, j = 0;

    while (input[i] != '.' && input[i] != '\0' && j < 8) {
        output[j++] = toupper((unsigned char)input[i++]);
    }
    while (input[i] != '.' && input[i] != '\0') i++;

    if (input[i] == '.') {
        i++;
        j = 8;
        while (input[i] != '\0' && j < 11) {
            output[j++] = toupper((unsigned char)input[i++]);
        }
    }
}

/* Builds a unique "BASE~N.EXT" style short alias for a long name.
 * `alias_num` is the caller-supplied ~N to try; callers loop N upward
 * until fat32_short_name_exists() says it's free. */
static void generate_short_alias(const char *long_name, uint32_t alias_num, char out11[11]) {
    memset(out11, ' ', 11);

    char base[9] = {0};
    char ext[4]  = {0};
    int  bi = 0, ei = 0;

    int dot = -1;
    for (int j = 0; long_name[j]; j++) if (long_name[j] == '.') dot = j;

    for (int i = 0; long_name[i] && (dot == -1 || i < dot) && bi < 8; i++) {
        char c = long_name[i];
        if (c == ' ' || c == '.') continue;
        base[bi++] = (char)toupper((unsigned char)c);
    }
    if (bi == 0) { base[0] = '_'; bi = 1; }

    if (dot != -1) {
        for (int j = dot + 1; long_name[j] && ei < 3; j++) {
            ext[ei++] = (char)toupper((unsigned char)long_name[j]);
        }
    }

    char numtail[8];
    int  ntlen = 0;
    {
        uint32_t n = alias_num;
        char tmp[8];
        int t = 0;
        do { tmp[t++] = (char)('0' + (n % 10)); n /= 10; } while (n);
        numtail[ntlen++] = '~';
        while (t) numtail[ntlen++] = tmp[--t];
    }

    int name_room = 8 - ntlen;
    if (name_room < 1) name_room = 1;
    if (bi > name_room) bi = name_room;

    memcpy(out11, base, bi);
    memcpy(out11 + bi, numtail, ntlen);
    memcpy(out11 + 8, ext, ei);
}

static void parse_83_name(const char *fat_name, char *out_name) {
    int out_idx = 0;

    for (int i = 0; i < 8 && fat_name[i] != ' '; i++) {
        out_name[out_idx++] = (fat_name[i] >= 'A' && fat_name[i] <= 'Z')
                                   ? fat_name[i] + 32 : fat_name[i];
    }
    if (fat_name[8] != ' ') {
        out_name[out_idx++] = '.';
        for (int i = 8; i < 11 && fat_name[i] != ' '; i++) {
            out_name[out_idx++] = (fat_name[i] >= 'A' && fat_name[i] <= 'Z')
                                       ? fat_name[i] + 32 : fat_name[i];
        }
    }
    out_name[out_idx] = '\0';
}

/* ---------------------------------------------------------------------
 * Raw file read (pre-VFS / boot-time helper) — unchanged, 8.3 only.
 * This is a low-level bring-up path (used before the VFS is mounted),
 * so it intentionally stays 8.3-only rather than pulling in LFN parsing.
 * --------------------------------------------------------------------- */

bool fat32_read_file(const char *filename_83, uint8_t *output_buffer) {
    uint32_t current_cluster = root_dir_cluster;
    uint8_t  sector_buf[512];

    uint32_t file_start_cluster = 0;
    uint32_t file_size = 0;
    bool     file_found = false;

    while (current_cluster < FAT32_EOC_MARKER && !file_found) {
        uint32_t cluster_lba = cluster_to_lba(current_cluster);

        for (int i = 0; i < sectors_per_cluster; i++) {
            ata_read_sector(cluster_lba + i, sector_buf);
            fat32_dir_t *entries = (fat32_dir_t *)sector_buf;

            for (int e = 0; e < 16; e++) {
                if (entries[e].name[0] == 0x00) return false;
                if ((unsigned char)entries[e].name[0] == 0xE5) continue;
                if (entries[e].attributes == FAT_ATTR_LFN) continue;

                if (compare_name_83(entries[e].name, filename_83)) {
                    file_start_cluster = ((uint32_t)entries[e].cluster_high << 16) | entries[e].cluster_low;
                    file_size = entries[e].file_size;
                    file_found = true;
                    break;
                }
            }
            if (file_found) break;
        }

        if (!file_found) current_cluster = get_next_cluster(current_cluster);
    }

    if (!file_found) return false;

    uint32_t bytes_read = 0;
    current_cluster = file_start_cluster;
    uint8_t *write_ptr = output_buffer;

    while (current_cluster < FAT32_EOC_MARKER && bytes_read < file_size) {
        uint32_t cluster_lba = cluster_to_lba(current_cluster);

        for (int i = 0; i < sectors_per_cluster; i++) {
            ata_read_sector(cluster_lba + i, sector_buf);

            uint32_t bytes_to_copy = 512;
            if ((file_size - bytes_read) < 512) bytes_to_copy = file_size - bytes_read;

            for (uint32_t b = 0; b < bytes_to_copy; b++) *write_ptr++ = sector_buf[b];

            bytes_read += bytes_to_copy;
            if (bytes_read >= file_size) break;
        }

        current_cluster = get_next_cluster(current_cluster);
    }

    return true;
}

bool fat32_init(uint32_t partition_lba) {
    uint8_t boot_sector[512];

    if (!ata_read_sector(partition_lba, boot_sector)) return false;

    fat32_bpb_t *bpb = (fat32_bpb_t *)boot_sector;

    if (bpb->boot_signature != 0x28 && bpb->boot_signature != 0x29) return false;

    sectors_per_cluster = bpb->sectors_per_cluster;
    root_dir_cluster    = bpb->root_cluster;
    next_free_cluster_hint = root_dir_cluster + 1;
    fat_start_lba  = partition_lba + bpb->reserved_sectors;
    data_start_lba = fat_start_lba + (bpb->fat_size_32 * bpb->fat_count);

    fat_cache_sector = FAT_CACHE_INVALID;
    fat_cache_dirty  = false;

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

    if (!ata_read_sector(0, mbr_sector)) {
        printf("CRITICAL: Failed to read disk!\n");
        return;
    }

    mbr_partition_t *part1 = (mbr_partition_t *)(mbr_sector + 0x1BE);
    uint32_t fat32_start_lba = part1->lba_start;

    printf("Partition 1 found at LBA: %u\n", fat32_start_lba);

    if (fat32_mount_root(fat32_start_lba)) {
        printf("FAT32 initialized successfully!\n");
    } else {
        printf("Failed to mount FAT32!\n");
    }
}

file_operations_t fat32_file_ops = {
    .read  = fat32_vfs_read,
    .write = fat32_vfs_write,
    .open  = NULL,
    .close = NULL
};

inode_operations_t fat32_inode_ops = {
    .lookup   = fat32_vfs_lookup,
    .create   = fat32_vfs_create,
    .mkdir    = fat32_vfs_mkdir,
    .unlink   = fat32_vfs_unlink,
    .getdents = fat32_dir_getdents
};

/* ---------------------------------------------------------------------
 * VFS: Read (unchanged — offset is by data content, not by name)
 * --------------------------------------------------------------------- */

long fat32_vfs_read(file_t *file, void *buf, size_t len, uint64_t offset) {
    if (!file || !file->inode || !buf) return -1;

    uint32_t file_size = file->inode->size;
    uint32_t current_cluster = file->inode->ino;

    if (offset >= file_size) return 0;
    if (offset + len > file_size) len = file_size - offset;

    uint32_t cluster_size = sectors_per_cluster * 512;
    uint32_t clusters_to_skip = offset / cluster_size;
    uint32_t offset_in_cluster = offset % cluster_size;

    for (uint32_t i = 0; i < clusters_to_skip; i++) {
        current_cluster = get_next_cluster(current_cluster);
        if (current_cluster >= FAT32_EOC_MARKER) return 0;
    }

    uint8_t *dest = (uint8_t *)buf;
    uint32_t bytes_read = 0;
    uint8_t sector_buf[512];

    while (bytes_read < len && current_cluster < FAT32_EOC_MARKER) {
        uint32_t cluster_lba = cluster_to_lba(current_cluster);

        for (int i = 0; i < sectors_per_cluster && bytes_read < len; i++) {
            ata_read_sector(cluster_lba + i, sector_buf);

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

/* ---------------------------------------------------------------------
 * VFS: Lookup — now resolves against long names too.
 * --------------------------------------------------------------------- */

inode_t *fat32_vfs_lookup(inode_t *parent, const char *name) {
    if (!parent || parent->type != FT_DIR) return NULL;

    char target_83[11];
    format_83_name(name, target_83);

    uint32_t current_cluster = parent->ino;
    uint8_t sector_buf[512];
    lfn_accum_t acc;
    lfn_accum_reset(&acc);
    char long_name[256];

    while (current_cluster < FAT32_EOC_MARKER) {
        uint32_t cluster_lba = cluster_to_lba(current_cluster);

        for (int i = 0; i < sectors_per_cluster; i++) {
            ata_read_sector(cluster_lba + i, sector_buf);
            fat32_dir_t *entries = (fat32_dir_t *)sector_buf;

            for (int e = 0; e < 16; e++) {
                if (entries[e].name[0] == 0x00) return NULL;
                if ((unsigned char)entries[e].name[0] == 0xE5) { lfn_accum_reset(&acc); continue; }

                bool has_long = lfn_feed_entry(&acc, &entries[e], cluster_lba + i, (uint8_t)e, long_name);
                if (entries[e].attributes == FAT_ATTR_LFN) continue;

                bool matched = compare_name_83(entries[e].name, target_83) ||
                               (has_long && strcmp(long_name, name) == 0);

                if (matched) {
                    inode_t *new_inode = (inode_t *)kmalloc(sizeof(inode_t));
                    memset(new_inode, 0, sizeof(inode_t));

                    new_inode->ino  = ((uint32_t)entries[e].cluster_high << 16) | entries[e].cluster_low;
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
                    info->dir_entry_lba    = cluster_lba + i;
                    info->dir_entry_offset = e * sizeof(fat32_dir_t);
                    new_inode->private = info;
                    return new_inode;
                }
            }
        }
        current_cluster = get_next_cluster(current_cluster);
    }
    return NULL;
}

superblock_t root_sb;
dentry_t     root_dentry;
inode_t      root_inode;

int fat32_mount_root(uint32_t partition_lba) {
    if (!fat32_init(partition_lba)) return -1;

    memset(&root_inode, 0, sizeof(inode_t));
    root_inode.ino  = root_dir_cluster;
    root_inode.size = 0;
    root_inode.type = FT_DIR;
    root_inode.is_directory = 1;
    root_inode.f_ops = &fat32_file_ops;
    root_inode.i_ops = &fat32_inode_ops;

    memset(&root_dentry, 0, sizeof(dentry_t));
    strcpy(root_dentry.name, "/");
    root_dentry.inode = &root_inode;

    memset(&root_sb, 0, sizeof(superblock_t));
    strcpy(root_sb.fs_type, "fat32");
    root_sb.root = &root_dentry;

    return vfs_mount(&root_sb, "/");
}

#define ALIGN_UP(val, align) (((val) + (align) - 1) & ~((align) - 1))

/* ---------------------------------------------------------------------
 * VFS: getdents — now reports the reconstructed long name when present.
 * --------------------------------------------------------------------- */

long fat32_dir_getdents(inode_t *inode, uint64_t *offset, void *buf_ptr, uint32_t count) {
    if (!inode || inode->type != FT_DIR) return -1;

    char *buf = (char *)buf_ptr;
    uint32_t bytes_written = 0;

    uint32_t current_cluster = inode->ino;
    uint8_t sector_buf[512];

    uint32_t entries_per_cluster = sectors_per_cluster * 16;
    uint32_t clusters_to_skip = (*offset) / entries_per_cluster;

    for (uint32_t i = 0; i < clusters_to_skip; i++) {
        current_cluster = get_next_cluster(current_cluster);
        if (current_cluster >= FAT32_EOC_MARKER) return bytes_written;
    }

    lfn_accum_t acc;
    lfn_accum_reset(&acc);
    char long_name[256];

    while (current_cluster < FAT32_EOC_MARKER) {
        uint32_t cluster_lba = cluster_to_lba(current_cluster);

        uint32_t start_sector = ((*offset) % entries_per_cluster) / 16;

        for (uint32_t i = start_sector; i < sectors_per_cluster; i++) {
            ata_read_sector(cluster_lba + i, sector_buf);
            fat32_dir_t *entries = (fat32_dir_t *)sector_buf;

            uint32_t start_entry = ((*offset) % 16);

            for (uint32_t e = start_entry; e < 16; e++) {
                if (entries[e].name[0] == 0x00) return bytes_written;

                if ((unsigned char)entries[e].name[0] == 0xE5) {
                    lfn_accum_reset(&acc);
                    (*offset)++;
                    continue;
                }

                bool has_long = lfn_feed_entry(&acc, &entries[e], cluster_lba + i, (uint8_t)e, long_name);

                if (entries[e].attributes == FAT_ATTR_LFN) {
                    (*offset)++;
                    continue;
                }

                char parsed_name[256];
                if (has_long) {
                    strcpy(parsed_name, long_name);
                } else {
                    parse_83_name(entries[e].name, parsed_name);
                }

                uint32_t name_len = strlen(parsed_name);
                uint16_t reclen = ALIGN_UP(sizeof(struct linux_dirent64) + name_len + 1, 8);

                if (bytes_written + reclen > count) return bytes_written;

                struct linux_dirent64 *dirent = (struct linux_dirent64 *)(buf + bytes_written);
                dirent->d_ino    = ((uint32_t)entries[e].cluster_high << 16) | entries[e].cluster_low;
                dirent->d_off    = *offset + 1;
                dirent->d_reclen = reclen;
                dirent->d_type   = (entries[e].attributes & FAT_ATTR_DIRECTORY) ? DT_DIR : DT_REG;

                memcpy((uint8_t *)dirent->d_name, (const uint8_t *)parsed_name, name_len + 1);

                bytes_written += reclen;
                (*offset)++;
            }
        }
        current_cluster = get_next_cluster(current_cluster);
    }

    return bytes_written;
}

/* ---------------------------------------------------------------------
 * Cluster allocation — now with a persistent scan hint instead of
 * restarting from cluster 2 every call.
 * --------------------------------------------------------------------- */



static uint32_t fat32_allocate_cluster(void) {
    const uint32_t first_cluster = 2;
    const uint32_t limit = 0x0FFFFFF0;

    if (next_free_cluster_hint < first_cluster || next_free_cluster_hint >= limit) {
        next_free_cluster_hint = first_cluster;
    }

    uint32_t cluster = next_free_cluster_hint;
    uint32_t scanned = 0;
    uint32_t total_clusters = limit - first_cluster;

    while (scanned < total_clusters) {
        if (get_next_cluster(cluster) == 0x00000000) {
            set_next_cluster(cluster, FAT32_EOC_MARKER);
            fat32_fat_sync();

            next_free_cluster_hint = cluster + 1;
            if (next_free_cluster_hint >= limit) next_free_cluster_hint = first_cluster;

            uint8_t zero_buf[512];
            memset(zero_buf, 0, 512);
            uint32_t lba = cluster_to_lba(cluster);
            for (int i = 0; i < sectors_per_cluster; i++) {
                ata_write_sector(lba + i, zero_buf);
            }

            return cluster;
        }

        cluster++;
        if (cluster >= limit) cluster = first_cluster;
        scanned++;
    }

    return 0; /* Disk is completely full */
}

#define FAT_ATTR_ARCHIVE 0x20

/* Writes a single raw 32-byte entry into the first free slot found. */
static bool fat32_add_entry(uint32_t dir_cluster, const char *filename_83, uint8_t attr, uint32_t start_cluster) {
    uint8_t sector_buf[512];
    uint32_t current_cluster = dir_cluster;
    uint32_t last_cluster = dir_cluster;

    while (current_cluster < FAT32_EOC_MARKER) {
        last_cluster = current_cluster;
        uint32_t cluster_lba = cluster_to_lba(current_cluster);

        for (int i = 0; i < sectors_per_cluster; i++) {
            ata_read_sector(cluster_lba + i, sector_buf);
            fat32_dir_t *entries = (fat32_dir_t *)sector_buf;

            for (int e = 0; e < 16; e++) {
                if (entries[e].name[0] == 0x00 || (unsigned char)entries[e].name[0] == 0xE5) {
                    memset(&entries[e], 0, sizeof(fat32_dir_t));
                    memcpy(entries[e].name, filename_83, 11);
                    entries[e].attributes   = attr;
                    entries[e].cluster_high = (start_cluster >> 16) & 0xFFFF;
                    entries[e].cluster_low  = start_cluster & 0xFFFF;
                    entries[e].file_size    = 0;

                    ata_write_sector(cluster_lba + i, sector_buf);
                    return true;
                }
            }
        }

        uint32_t next = get_next_cluster(current_cluster);
        if (next >= FAT32_EOC_MARKER) {
            /* Directory has no free slot in any existing cluster — grow it
             * by one cluster instead of failing. A freshly allocated
             * cluster is zeroed by fat32_allocate_cluster(), so its first
             * slot is guaranteed free; link it into the chain and retry
             * from there. */
            uint32_t new_cluster = fat32_allocate_cluster();
            if (new_cluster == 0) {
                printf("fat32_add_entry: disk full, cannot grow directory for '%.11s'\n", filename_83);
                return false;
            }
            set_next_cluster(last_cluster, new_cluster);
            fat32_fat_sync();
            next = new_cluster;
        }
        current_cluster = next;
    }
    return false;
}

static bool fat32_short_name_exists(uint32_t dir_cluster, const char short_name[11]) {
    uint8_t sector_buf[512];
    uint32_t current_cluster = dir_cluster;

    while (current_cluster < FAT32_EOC_MARKER) {
        uint32_t cluster_lba = cluster_to_lba(current_cluster);

        for (int i = 0; i < sectors_per_cluster; i++) {
            ata_read_sector(cluster_lba + i, sector_buf);
            fat32_dir_t *entries = (fat32_dir_t *)sector_buf;

            for (int e = 0; e < 16; e++) {
                if (entries[e].name[0] == 0x00) return false;
                if ((unsigned char)entries[e].name[0] == 0xE5) continue;
                if (entries[e].attributes == FAT_ATTR_LFN) continue;
                if (compare_name_83(entries[e].name, short_name)) return true;
            }
        }
        current_cluster = get_next_cluster(current_cluster);
    }
    return false;
}

/* Finds `needed` contiguous free directory slots starting anywhere in the
 * cluster chain and returns their (lba, index) locations via out arrays.
 * If no existing run is long enough, grows the directory by one cluster
 * (freshly allocated clusters come back zeroed, so they trivially satisfy
 * any run up to one cluster's worth of entries) and uses that instead.
 * Only returns false if the disk itself is full or `needed` exceeds what
 * a single cluster can ever hold. */
static bool fat32_find_contiguous_free_slots(uint32_t dir_cluster, int needed,
                                              uint32_t *out_lba, uint8_t *out_idx) {
    /* Flatten the search across sectors within the cluster chain, tracking
     * a run length as we go. */
    uint32_t current_cluster = dir_cluster;
    uint32_t last_cluster = dir_cluster;
    int run_len = 0;
    uint32_t run_lba[LFN_MAX_ENTRIES + 1];
    uint8_t  run_idx[LFN_MAX_ENTRIES + 1];
    uint8_t sector_buf[512];

    while (current_cluster < FAT32_EOC_MARKER) {
        last_cluster = current_cluster;
        uint32_t cluster_lba = cluster_to_lba(current_cluster);

        for (int i = 0; i < sectors_per_cluster; i++) {
            ata_read_sector(cluster_lba + i, sector_buf);
            fat32_dir_t *entries = (fat32_dir_t *)sector_buf;

            for (int e = 0; e < 16; e++) {
                bool free_slot = (entries[e].name[0] == 0x00) ||
                                  ((unsigned char)entries[e].name[0] == 0xE5);
                if (free_slot) {
                    run_lba[run_len] = cluster_lba + i;
                    run_idx[run_len] = (uint8_t)e;
                    run_len++;
                    if (run_len == needed) {
                        memcpy((uint8_t*)out_lba, (uint8_t*)run_lba, sizeof(uint32_t) * needed);
                        memcpy(out_idx, run_idx, sizeof(uint8_t) * needed);
                        return true;
                    }
                } else {
                    run_len = 0;
                }
            }
        }
        current_cluster = get_next_cluster(current_cluster);
    }

    /* No run big enough anywhere in the existing chain — grow the
     * directory by one cluster. A freshly allocated cluster is fully
     * zeroed by fat32_allocate_cluster(), so as long as `needed` fits
     * within one cluster's entry count, the new cluster alone satisfies
     * the request. A partial run collected at the tail of the last
     * existing cluster is intentionally discarded rather than "bridged"
     * across the cluster boundary, to keep this correct rather than
     * maximally space-efficient. */
    uint32_t entries_per_cluster = (uint32_t)sectors_per_cluster * 16;
    if ((uint32_t)needed > entries_per_cluster) {
        printf("fat32_find_contiguous_free_slots: name needs %d slots, "
               "more than one cluster (%u) can ever hold\n", needed, entries_per_cluster);
        return false;
    }

    uint32_t new_cluster = fat32_allocate_cluster();
    if (new_cluster == 0) {
        printf("fat32_find_contiguous_free_slots: disk full, cannot grow directory\n");
        return false;
    }

    set_next_cluster(last_cluster, new_cluster);
    fat32_fat_sync();

    uint32_t new_lba = cluster_to_lba(new_cluster);
    for (int i = 0; i < needed; i++) {
        out_lba[i] = new_lba + (uint32_t)(i / 16);
        out_idx[i] = (uint8_t)(i % 16);
    }
    return true;
}

/* Writes a long name (with LFN chain if needed) + short entry as a block. */
static bool fat32_add_entry_named(uint32_t dir_cluster, const char *long_name,
                                   uint8_t attr, uint32_t start_cluster) {
    if (name_fits_83(long_name)) {
        char short83[11];
        format_83_name(long_name, short83);
        return fat32_add_entry(dir_cluster, short83, attr, start_cluster);
    }

    /* Generate a unique short alias. */
    char short83[11];
    uint32_t alias_num = 1;
    do {
        generate_short_alias(long_name, alias_num, short83);
        alias_num++;
    } while (fat32_short_name_exists(dir_cluster, short83) && alias_num < 1000000);

    int name_len = (int)strlen(long_name);
    int chunk_count = (name_len + LFN_CHARS_PER_ENTRY - 1) / LFN_CHARS_PER_ENTRY;
    if (chunk_count == 0) chunk_count = 1;
    if (chunk_count > LFN_MAX_ENTRIES) return false; /* name too long */

    int needed_slots = chunk_count + 1; /* + the short entry */
    uint32_t slot_lba[LFN_MAX_ENTRIES + 1];
    uint8_t  slot_idx[LFN_MAX_ENTRIES + 1];

    if (!fat32_find_contiguous_free_slots(dir_cluster, needed_slots, slot_lba, slot_idx)) {
        return false; /* fat32_find_contiguous_free_slots already logged why */
    }

    uint8_t checksum = lfn_checksum(short83);

    /* LFN entries are written in reverse order: highest sequence number
     * (with LAST_ENTRY flag) first, down to sequence 1 immediately before
     * the short entry. */
    uint8_t sector_buf[512];
    for (int c = 0; c < chunk_count; c++) {
        int seq = chunk_count - c; /* chunk_count, chunk_count-1, ..., 1 */
        char piece[LFN_CHARS_PER_ENTRY + 1] = {0};
        int base = (seq - 1) * LFN_CHARS_PER_ENTRY;
        for (int k = 0; k < LFN_CHARS_PER_ENTRY; k++) {
            int src_i = base + k;
            piece[k] = (src_i < name_len) ? long_name[src_i] : '\0';
        }

        ata_read_sector(slot_lba[c], sector_buf);
        fat32_lfn_t *lfn = (fat32_lfn_t *)((fat32_dir_t *)sector_buf + slot_idx[c]);
        memset(lfn, 0, sizeof(fat32_lfn_t));

        lfn->seq = (uint8_t)seq | (seq == chunk_count ? LFN_LAST_ENTRY_FLAG : 0);
        lfn->attr = FAT_ATTR_LFN;
        lfn->type = 0;
        lfn->checksum = checksum;
        lfn->first_cluster = 0;

        /* Build the three UCS-2 sub-arrays in ordinary (aligned) local
         * storage first — taking &lfn->name1/2/3 directly would be
         * taking the address of a packed-struct member, which risks an
         * unaligned pointer. Assemble here, then memcpy into the packed
         * struct in one shot per sub-array (memcpy is safe regardless of
         * destination alignment). */
        uint16_t name1[5], name2[6], name3[2];
        uint16_t *parts[3]  = { name1, name2, name3 };
        int       counts[3] = { 5, 6, 2 };
        bool ended = false;
        int idx = 0;
        for (int p = 0; p < 3; p++) {
            for (int k = 0; k < counts[p]; k++) {
                if (!ended && piece[idx] != '\0') {
                    parts[p][k] = (uint16_t)(unsigned char)piece[idx];
                } else if (!ended) {
                    parts[p][k] = 0x0000; /* terminator */
                    ended = true;
                } else {
                    parts[p][k] = 0xFFFF; /* padding */
                }
                idx++;
            }
        }
        memcpy(lfn->name1, name1, sizeof(name1));
        memcpy(lfn->name2, name2, sizeof(name2));
        memcpy(lfn->name3, name3, sizeof(name3));

        ata_write_sector(slot_lba[c], sector_buf);
    }

    /* Finally, write the short entry into the last reserved slot. */
    uint32_t short_lba = slot_lba[chunk_count];
    uint8_t  short_idx = slot_idx[chunk_count];
    ata_read_sector(short_lba, sector_buf);
    fat32_dir_t *short_entry = (fat32_dir_t *)sector_buf + short_idx;
    memset(short_entry, 0, sizeof(fat32_dir_t));
    memcpy(short_entry->name, short83, 11);
    short_entry->attributes   = attr;
    short_entry->cluster_high = (start_cluster >> 16) & 0xFFFF;
    short_entry->cluster_low  = start_cluster & 0xFFFF;
    short_entry->file_size    = 0;
    ata_write_sector(short_lba, sector_buf);

    return true;
}

int fat32_vfs_create(inode_t *parent, const char *name, uint32_t mode) {
    (void)mode;
    if (!parent || parent->type != FT_DIR) {
        printf("fat32_vfs_create: bad parent for '%s'\n", name);
        return -1;
    }
    if (fat32_vfs_lookup(parent, name) != NULL) {
        printf("fat32_vfs_create: '%s' already exists (lookup matched)\n", name);
        return -1;
    }

    uint32_t new_cluster = fat32_allocate_cluster();
    if (new_cluster == 0) {
        printf("fat32_vfs_create: fat32_allocate_cluster failed (disk full?) for '%s'\n", name);
        return -1;
    }

    if (!fat32_add_entry_named(parent->ino, name, FAT_ATTR_ARCHIVE, new_cluster)) {
        printf("fat32_vfs_create: fat32_add_entry_named failed for '%s' (fits_83=%d)\n",
               name, name_fits_83(name));
        fat32_free_chain(new_cluster);
        fat32_fat_sync();
        return -1;
    }
    fat32_fat_sync();

    return 0;
}

int fat32_vfs_mkdir(inode_t *parent, const char *name) {
    if (!parent || parent->type != FT_DIR) return -1;
    if (fat32_vfs_lookup(parent, name) != NULL) return -1;

    uint32_t new_cluster = fat32_allocate_cluster();
    if (new_cluster == 0) return -1;

    if (!fat32_add_entry_named(parent->ino, name, FAT_ATTR_DIRECTORY, new_cluster)) {
        fat32_free_chain(new_cluster);
        fat32_fat_sync();
        return -1;
    }

    /* "." and ".." are always plain 8.3 entries, never long names. */
    fat32_add_entry(new_cluster, ".          ", FAT_ATTR_DIRECTORY, new_cluster);
    fat32_add_entry(new_cluster, "..         ", FAT_ATTR_DIRECTORY, parent->ino);

    fat32_fat_sync();
    return 0;
}

long fat32_vfs_write(file_t *file, const void *buf, size_t len, uint64_t offset) {
    if (!file || !file->inode || !buf || len == 0) return -1;

    uint32_t current_cluster = file->inode->ino;
    uint32_t file_size = file->inode->size;
    uint32_t cluster_size = sectors_per_cluster * 512;

    uint32_t required_size = offset + len;
    uint32_t current_clusters_owned = (file_size + cluster_size - 1) / cluster_size;
    if (file_size == 0) current_clusters_owned = 1;

    uint32_t needed_clusters = (required_size + cluster_size - 1) / cluster_size;

    if (needed_clusters > current_clusters_owned) {
        uint32_t tail = current_cluster;
        while (get_next_cluster(tail) < FAT32_EOC_MARKER && get_next_cluster(tail) != 0) {
            tail = get_next_cluster(tail);
        }

        for (uint32_t i = current_clusters_owned; i < needed_clusters; i++) {
            uint32_t new_cluster = fat32_allocate_cluster();
            if (new_cluster == 0) { fat32_fat_sync(); return -1; }

            set_next_cluster(tail, new_cluster);
            tail = new_cluster;
        }
    }

    uint32_t clusters_to_skip = offset / cluster_size;
    uint32_t offset_in_cluster = offset % cluster_size;

    uint32_t target_cluster = current_cluster;
    for (uint32_t i = 0; i < clusters_to_skip; i++) {
        target_cluster = get_next_cluster(target_cluster);
    }

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

            if (bytes_to_copy < 512) {
                ata_read_sector(current_lba, sector_buf);
            }

            memcpy(sector_buf + start_byte, src + bytes_written, bytes_to_copy);
            ata_write_sector(current_lba, sector_buf);

            bytes_written += bytes_to_copy;
        }
        target_cluster = get_next_cluster(target_cluster);
    }

    if (offset + bytes_written > file->inode->size) {
        file->inode->size = offset + bytes_written;

        if (file->inode->private) {
            fat32_node_info_t *info = (fat32_node_info_t *)file->inode->private;

            ata_read_sector(info->dir_entry_lba, sector_buf);
            fat32_dir_t *entry = (fat32_dir_t *)(sector_buf + info->dir_entry_offset);
            entry->file_size = file->inode->size;
            ata_write_sector(info->dir_entry_lba, sector_buf);
        }
    }

    fat32_fat_sync();
    return bytes_written;
}

static void fat32_free_chain(uint32_t start_cluster) {
    if (start_cluster < 2 || start_cluster >= FAT32_EOC_MARKER) return;

    uint32_t current = start_cluster;
    while (current < FAT32_EOC_MARKER && current != 0) {
        uint32_t next = get_next_cluster(current);
        set_next_cluster(current, 0x00000000);
        current = next;
    }
}

int fat32_vfs_unlink(inode_t *parent, const char *name) {
    if (!parent || parent->type != FT_DIR) return -1;

    char target_83[11];
    format_83_name(name, target_83);

    uint32_t current_cluster = parent->ino;
    uint8_t sector_buf[512];
    lfn_accum_t acc;
    lfn_accum_reset(&acc);
    char long_name[256];

    while (current_cluster < FAT32_EOC_MARKER) {
        uint32_t cluster_lba = cluster_to_lba(current_cluster);

        for (int i = 0; i < sectors_per_cluster; i++) {
            ata_read_sector(cluster_lba + i, sector_buf);
            fat32_dir_t *entries = (fat32_dir_t *)sector_buf;

            for (int e = 0; e < 16; e++) {
                if (entries[e].name[0] == 0x00) return -1;
                if ((unsigned char)entries[e].name[0] == 0xE5) { lfn_accum_reset(&acc); continue; }

                bool has_long = lfn_feed_entry(&acc, &entries[e], cluster_lba + i, (uint8_t)e, long_name);
                if (entries[e].attributes == FAT_ATTR_LFN) continue;

                bool matched = compare_name_83(entries[e].name, target_83) ||
                               (has_long && strcmp(long_name, name) == 0);

                if (matched) {
                    uint32_t target_start_cluster =
                        ((uint32_t)entries[e].cluster_high << 16) | entries[e].cluster_low;

                    /* Mark the short entry deleted. */
                    entries[e].name[0] = 0xE5;
                    ata_write_sector(cluster_lba + i, sector_buf);

                    /* Also mark every LFN slot that made up its long name. */
                    if (has_long) {
                        for (int c = 0; c < acc.total_chunks; c++) {
                            uint8_t lfn_sector[512];
                            ata_read_sector(acc.slot_lba[c], lfn_sector);
                            fat32_dir_t *slot = (fat32_dir_t *)lfn_sector + acc.slot_idx[c];
                            slot->name[0] = 0xE5;
                            ata_write_sector(acc.slot_lba[c], lfn_sector);
                        }
                    }

                    fat32_free_chain(target_start_cluster);
                    fat32_fat_sync();

                    return 0;
                }
            }
        }
        current_cluster = get_next_cluster(current_cluster);
    }
    return -1;
}
