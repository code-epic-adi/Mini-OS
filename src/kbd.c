#include <stdint.h>

uint8_t inb(uint16_t p){
    uint8_t r;
    __asm__ volatile("inb %1,%0":"=a"(r):"Nd"(p));
    return r;
}

static int ready(){ return inb(0x64)&1; }

char kbd_getch(){
    static int shift = 0;

    while(!ready()){}
    uint8_t s = inb(0x60);

    if(s == 0xE0)
        return 0; // ignore extended for now

    // key release (break code)
    if(s & 0x80){
        uint8_t code = s & 0x7F;
        if(code == 0x2A || code == 0x36) // left/right shift released
            shift = 0;
        return 0;
    }

    // key press (make code)
    if(s == 0x2A || s == 0x36){ // left/right shift pressed
        shift = 1;
        return 0;
    }

    static const char normal_map[128] = {
        0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', 8, '\t',
        'q','w','e','r','t','y','u','i','o','p','[',']','\n', 0, 'a','s',
        'd','f','g','h','j','k','l',';','\'','`', 0,'\\','z','x','c','v',
        'b','n','m',',','.','/', 0, '*', 0,' ', 0,
        /* rest */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                   0,0,0,0,0,0,0,0
    };

    static const char shift_map[128] = {
        0, 27, '!','@','#','$','%','^','&','*','(',')','_','+', 8, '\t',
        'Q','W','E','R','T','Y','U','I','O','P','{','}','\n', 0, 'A','S',
        'D','F','G','H','J','K','L',':','"','~', 0,'|','Z','X','C','V',
        'B','N','M','<','>','?', 0, '*', 0,' ', 0,
        /* rest */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                   0,0,0,0,0,0,0,0
    };

    const char* map = shift ? shift_map : normal_map;
    if(s < 128)
        return map[s];
    return 0;
}

