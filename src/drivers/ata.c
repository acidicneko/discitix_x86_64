#include <drivers/ata.h>
#include <arch/ports.h>

static void ata_wait_400ns(void) {
    inb(ATA_PRIMARY_CTRL);
    inb(ATA_PRIMARY_CTRL);
    inb(ATA_PRIMARY_CTRL);
    inb(ATA_PRIMARY_CTRL);
}

static bool ata_wait_ready(void) {
    uint32_t timeout = 100000; // Arbitrary timeout limit
    while (--timeout) {
        uint8_t status = inb(ATA_PRIMARY_IO + 7);
        
        // If the error bit is set, fail immediately
        if (status & ATA_SR_ERR) return false;
        if (status & ATA_SR_DF)  return false;
        
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) return true;
    }
    return false; // Timed out
}

bool ata_read_sector(uint32_t lba, uint8_t *buffer) {
    // Select Master Drive (0xE0) + Highest 4 bits of LBA
    outb(ATA_PRIMARY_IO + 6, 0xE0 | ((lba >> 24) & 0x0F));
    ata_wait_400ns();

    // Send Sector Count
    outb(ATA_PRIMARY_IO + 2, 1);

    // Send LBA (Low, Mid, High)
    outb(ATA_PRIMARY_IO + 3, (uint8_t) lba);
    outb(ATA_PRIMARY_IO + 4, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_IO + 5, (uint8_t)(lba >> 16));

    // Send Read Command
    outb(ATA_PRIMARY_IO + 7, ATA_CMD_READ_PIO);

    // Wait for drive to be ready
    if (!ata_wait_ready()) {
        return false; 
    }

    // Read 256 16-bit words (512 bytes)
    for (int i = 0; i < 256; i++) {
        uint16_t word = inw(ATA_PRIMARY_IO + 0);
        buffer[i * 2]     = (uint8_t)(word & 0xFF);
        buffer[i * 2 + 1] = (uint8_t)(word >> 8);
    }

    // Read alt status to clear pending interrupts (good practice for PIO)
    inb(ATA_PRIMARY_CTRL);
    
    return true;
}

bool ata_write_sector(uint32_t lba, const uint8_t *buffer) {
    // Select Master Drive (0xE0) + Highest 4 bits of LBA
    outb(ATA_PRIMARY_IO + 6, 0xE0 | ((lba >> 24) & 0x0F));
    ata_wait_400ns();

    // Send Sector Count
    outb(ATA_PRIMARY_IO + 2, 1);

    //Send LBA (Low, Mid, High)
    outb(ATA_PRIMARY_IO + 3, (uint8_t) lba);
    outb(ATA_PRIMARY_IO + 4, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_IO + 5, (uint8_t)(lba >> 16));

    //Send Write Command
    outb(ATA_PRIMARY_IO + 7, ATA_CMD_WRITE_PIO);

    if (!ata_wait_ready()) {
        return false;
    }

    for (int i = 0; i < 256; i++) {
        uint16_t word = buffer[i * 2] | (buffer[i * 2 + 1] << 8);
        outw(ATA_PRIMARY_IO + 0, word);
    }

    // Flush the Cache
    outb(ATA_PRIMARY_IO + 7, ATA_CMD_CACHE_FLUSH);

    // Wait for the flush to complete (BSY to clear)
    uint32_t timeout = 100000;
    while (--timeout) {
        uint8_t status = inb(ATA_PRIMARY_IO + 7);
        if (!(status & ATA_SR_BSY)) break;
    }

    return (timeout > 0);
}
