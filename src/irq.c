#include <stdint.h>

static inline void outb(uint16_t p, uint8_t v){ __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }
static inline uint16_t get_cs(){ uint16_t s; __asm__ volatile("mov %%cs,%0":"=r"(s)); return s; }

struct idt_entry{ uint16_t off_lo; uint16_t sel; uint8_t zero; uint8_t flags; uint16_t off_hi; } __attribute__((packed));
struct idt_ptr{ uint16_t limit; uint32_t base; } __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr idtp;

static volatile uint32_t ticks=0;

void timer_isr(){ ticks++; }

__attribute__((naked)) void irq0_stub(){
    __asm__ volatile(
        "pusha\n"
        "call timer_isr\n"
        "popa\n"
        "movb $0x20, %al\n"
        "outb %al, $0x20\n"
        "iret\n"
    );
}

static void idt_set_gate(int n, uint32_t base, uint16_t sel, uint8_t flags){
    idt[n].off_lo = base & 0xFFFF;
    idt[n].sel = sel;
    idt[n].zero = 0;
    idt[n].flags = flags;
    idt[n].off_hi = (base >> 16) & 0xFFFF;
}

static void idt_load(){
    idtp.limit = sizeof(idt)-1;
    idtp.base = (uint32_t)idt;
    __asm__ volatile("lidt (%0)"::"r"(&idtp));
}

static void pic_remap_and_mask(){
    outb(0x20,0x11);
    outb(0xA0,0x11);
    outb(0x21,0x20);
    outb(0xA1,0x28);
    outb(0x21,0x04);
    outb(0xA1,0x02);
    outb(0x21,0x01);
    outb(0xA1,0x01);
    outb(0x21,0xFE);
    outb(0xA1,0xFF);
}

static void pit_init(uint32_t hz){
    uint16_t div = (uint16_t)(1193182u / hz);
    outb(0x43,0x36);
    outb(0x40,div & 0xFF);
    outb(0x40,div >> 8);
}

void irq_init(){
    for(int i=0;i<256;i++) idt[i]=(struct idt_entry){0,0,0,0,0};
    uint16_t cs = get_cs();
    idt_set_gate(32, (uint32_t)irq0_stub, cs, 0x8E);
    pic_remap_and_mask();
    idt_load();
    pit_init(100);
}

uint32_t timer_ticks(){ return ticks; }

