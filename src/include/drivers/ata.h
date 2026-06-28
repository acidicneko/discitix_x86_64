#ifndef __ATA__
#define __ATA__

#include <stdint.h>
#include <stdbool.h>

#define ATA_PRIMARY_IO      0x1F0
#define ATA_PRIMARY_CTRL    0x3F6

#define ATA_CMD_READ_PIO    0x20
#define ATA_CMD_WRITE_PIO   0x30
#define ATA_CMD_CACHE_FLUSH 0xE7

#define ATA_SR_ERR          0x01    // Error
#define ATA_SR_DRQ          0x08    // Data Request Ready
#define ATA_SR_SRV          0x10    // Overlapped Mode Service Request
#define ATA_SR_DF           0x20    // Drive Fault Error
#define ATA_SR_RDY          0x40    // Drive Ready
#define ATA_SR_BSY          0x80    // Busy

bool ata_read_sector(uint32_t lba, uint8_t *buffer);
bool ata_write_sector(uint32_t lba, const uint8_t *buffer);

#endif // __ATA__
