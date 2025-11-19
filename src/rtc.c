#include "rtc.h"
#include <stdint.h>

static inline void outb(uint16_t p, uint8_t v){
    __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));
}
static inline uint8_t inb(uint16_t p){
    uint8_t r;
    __asm__ volatile("inb %1,%0":"=a"(r):"Nd"(p));
    return r;
}

static uint8_t cmos_read(uint8_t reg){
    outb(0x70, reg);
    return inb(0x71);
}

static uint8_t bcd_to_bin(uint8_t b){
    return (b & 0x0F) + ((b >> 4) * 10);
}

void rtc_read(struct rtc_time* t){
    if(!t) return;

    uint8_t sec = cmos_read(0x00);
    uint8_t min = cmos_read(0x02);
    uint8_t hour = cmos_read(0x04);
    uint8_t day = cmos_read(0x07);
    uint8_t month = cmos_read(0x08);
    uint8_t year = cmos_read(0x09);
    uint8_t regB = cmos_read(0x0B);

    int bcd = !(regB & 0x04);   // if bit 2 == 0, values are BCD
    int hour24 = regB & 0x02;   // if bit 1 == 1, 24-hour mode

    if(bcd){
        sec = bcd_to_bin(sec);
        min = bcd_to_bin(min);
        hour = bcd_to_bin(hour);
        day = bcd_to_bin(day);
        month = bcd_to_bin(month);
        year = bcd_to_bin(year);
    }

    if(!hour24){
        int pm = hour & 0x80;
        hour &= 0x7F;
        if(pm && hour < 12) hour += 12;
        else if(!pm && hour == 12) hour = 0;
    }

    t->sec = sec;
    t->min = min;
    t->hour = hour;
    t->day = day;
    t->month = month;
    t->year = (year < 70) ? (2000u + year) : (1900u + year);
}
