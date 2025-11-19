#include <stddef.h>
#include <stdint.h>
static volatile uint16_t* const VGA=(uint16_t*)0xB8000;
static uint8_t cx=0, cy=0, color=0x0F;

void vga_set_color(uint8_t c){ color = c; }
void vga_write_color(const char* s, uint8_t c){
    uint8_t old = color;
    vga_set_color(c);
    while(*s) {
        if(*s == '\n'){ cx = 0; cy++; if(cy>=25) { /* scroll */ 
                for(int y=1;y<25;y++) for(int x=0;x<80;x++) VGA[(y-1)*80+x]=VGA[y*80+x];
                for(int x=0;x<80;x++) VGA[24*80+x]=(' ' | ((uint16_t)color<<8));
                cy=24;
            } 
            s++; continue;
        }
        VGA[cy*80+cx]=(uint16_t)*s | ((uint16_t)color<<8);
        cx++; if(cx>=80){cx=0; cy++; if(cy>=25){ /* scroll */
                for(int y=1;y<25;y++) for(int x=0;x<80;x++) VGA[(y-1)*80+x]=VGA[y*80+x];
                for(int x=0;x<80;x++) VGA[24*80+x]=(' ' | ((uint16_t)color<<8));
                cy=24;
            }}
        s++;
    }
    vga_set_color(old);
}

static void scroll(){
    if(cy<25) return;
    for(int y=1;y<25;y++) for(int x=0;x<80;x++) VGA[(y-1)*80+x]=VGA[y*80+x];
    for(int x=0;x<80;x++) VGA[24*80+x]=(' ' | ((uint16_t)color<<8));
    cy=24;
}

void vga_putc(char c){
    if(c=='\n'){cx=0; cy++; scroll(); return;}
    if(c=='\b'){ if(cx>0){cx--; VGA[cy*80+cx]=(' ' | ((uint16_t)color<<8));} return; }
    VGA[cy*80+cx]=(uint16_t)c | ((uint16_t)color<<8);
    cx++; if(cx>=80){cx=0; cy++;} scroll();
}

void vga_clear(){
    for(int y=0;y<25;y++) for(int x=0;x<80;x++) VGA[y*80+x]=(' ' | ((uint16_t)color<<8));
    cx=0; cy=0;
}

void vga_write(const char* s){ while(*s) vga_putc(*s++); }
void vga_writeln(const char* s){ vga_write(s); vga_putc('\n'); }

