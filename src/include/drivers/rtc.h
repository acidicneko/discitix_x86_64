#ifndef __RTC_H__
#define __RTC_H__

#include <stdint.h>
#include <arch/x86_64/regs.h>

struct rtc_time {
    uint8_t sec;
    uint8_t min;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint16_t year;
};

void rtc_read(struct rtc_time *t);
void rtc_enable_irq(void);
void rtc_irq_handler(register_t *r);
void rtc_print_time(void);
void rtc_init(void);
uint64_t get_unix_epoch();

#endif /* __RTC_H__ */