#include <stdint.h>
#include <drivers/rtc.h>
#include <arch/x86_64/regs.h>
#include <stdio.h>
#include <arch/x86_64/irq.h>
#include <libk/utils.h>

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}


static uint8_t cmos_read(uint8_t reg) {
    outb(0x70, reg | 0x80);   // disable NMI
    return inb(0x71);
}

static void cmos_write(uint8_t reg, uint8_t val) {
    outb(0x70, reg | 0x80);
    outb(0x71, val);
}


struct rtc_time {
    uint8_t sec;
    uint8_t min;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint16_t year;
};


static inline int rtc_updating(void) {
    return cmos_read(0x0A) & 0x80;
}

static inline uint8_t bcd_to_bin(uint8_t bcd) {
    return (bcd & 0x0F) + ((bcd >> 4) * 10);
}


void rtc_read(struct rtc_time *t) {
    uint8_t sec, min, hour, day, month, year;
    uint8_t regB;

    /* Wait until RTC is stable */
    while (rtc_updating());

    sec   = cmos_read(0x00);
    min   = cmos_read(0x02);
    hour  = cmos_read(0x04);
    day   = cmos_read(0x07);
    month = cmos_read(0x08);
    year  = cmos_read(0x09);

    regB = cmos_read(0x0B);

    /* Convert from BCD if needed */
    if (!(regB & 0x04)) {
        sec   = bcd_to_bin(sec);
        min   = bcd_to_bin(min);
        hour  = bcd_to_bin(hour & 0x7F);
        day   = bcd_to_bin(day);
        month = bcd_to_bin(month);
        year  = bcd_to_bin(year);
    }

    /* Handle 12-hour mode */
    if (!(regB & 0x02) && (hour & 0x80)) {
        hour = ((hour & 0x7F) + 12) % 24;
    }

    t->sec   = sec;
    t->min   = min;
    t->hour  = hour;
    t->day   = day;
    t->month = month;
    t->year  = 2000 + year;   // assume 20xx
}

void rtc_enable_irq(void) {
    uint8_t prev = cmos_read(0x0B);
    cmos_write(0x0B, prev | 0x40);  // enable update-ended interrupt
}

/* MUST be called in IRQ8 handler */
void rtc_irq_handler(register_t *r) {
    (void)r;
    cmos_read(0x0C);
}

void rtc_print_time(void) {
    struct rtc_time t;
    rtc_read(&t);

    printf("RTC: %ul-%ul-%ul %ul:%ul:%ul\n",
           t.year, t.month, t.day,
           t.hour, t.min, t.sec);
}

void rtc_init(void) {
    irq_install_handler(8, rtc_irq_handler);
    dbgln("RTC initialized with IRQ8\n\r");
}

uint64_t get_unix_epoch(){
    struct rtc_time t;
    rtc_read(&t);

    uint64_t epoch = 0;
    epoch += (t.year - 1970) * 365 * 24 * 3600; // years
    epoch += (t.month - 1) * 30 * 24 * 3600;     // months (approx)
    epoch += (t.day - 1) * 24 * 3600;           // days
    epoch += t.hour * 3600;                     // hours
    epoch += t.min * 60;                       // minutes
    epoch += t.sec;                            // seconds

    return epoch;
}