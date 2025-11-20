#include <stddef.h>
#include <stdint.h>
#include "kalloc.h"
#include "rtc.h"
#include "kalloc.h"
#include "paging.h"
#include "task.h"

#define HISTORY_MAX 10
#define CMD_MAX_LEN 128

static char history[HISTORY_MAX][CMD_MAX_LEN];
static int history_count = 0;   // number of stored commands (<= HISTORY_MAX)
static int history_start = 0;   // index of the oldest entry

static void history_add(const char* cmd);
static void history_print(void);

void vga_clear(); void vga_write(const char*); void vga_writeln(const char*); void vga_putc(char);
void vga_set_color(uint8_t); void vga_write_color(const char*, uint8_t);
char kbd_getch();
void irq_init(); uint32_t timer_ticks();
static inline void outb(uint16_t p, uint8_t v){ __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }
static inline void qemu_poweroff(){
    __asm__ volatile("outw %0,%1" :: "a"((uint16_t)0), "Nd"((uint16_t)0xF4));
}
static inline void cpuid(uint32_t leaf,uint32_t* a,uint32_t* b,uint32_t* c,uint32_t* d){
    uint32_t A,B,C,D; __asm__ volatile("cpuid":"=a"(A),"=b"(B),"=c"(C),"=d"(D):"a"(leaf),"c"(0));
    if(a)*a=A; 
    if(b)*b=B; 
    if(c)*c=C; 
    if(d)*d=D;
}

static size_t my_strlen(const char*s){size_t n=0;while(s[n])n++;return n;}
static int my_streq(const char*a,const char*b){while(*a&&*b&&*a==*b){a++;b++;}return *a==0&&*b==0;}
static int my_starts(const char*s,const char*p){while(*p){if(*s++!=*p++)return 0;}return 1;}
static void utoa32(uint32_t x,char*b){char t[16];int i=0;if(x==0){b[0]='0';b[1]=0;return;}while(x){t[i++]='0'+(x%10u);x/=10u;}for(int j=0;j<i;j++)b[j]=t[i-1-j];b[i]=0;}
static void hex8(uint32_t x,char*b){static const char h[16]="0123456789ABCDEF";for(int i=7;i>=0;i--){b[7-i]=h[(x>>(i*4))&0xF];}b[8]=0;}
static void hex16_64(uint64_t x,char*b){ static const char h[16]="0123456789ABCDEF"; for(int i=15;i>=0;i--){ b[15-i]=h[(x>>(i*4))&0xF]; } b[16]=0; }

static void prompt(){vga_write("> ");}

/* Multiboot2 constants and structs */
#define MULTIBOOT_TAG_TYPE_END 0
#define MULTIBOOT_TAG_TYPE_MMAP 6

struct mb2_tag { uint32_t type; uint32_t size; } __attribute__((packed));
struct mb2_tag_mmap { uint32_t type; uint32_t size; uint32_t entry_size; uint32_t entry_version; } __attribute__((packed));
struct mb2_mmap_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t reserved;
} __attribute__((packed));

/* helper prints */
static void print_mmap_entry(struct mb2_mmap_entry *e){
    char s[32];
    hex16_64(e->addr, s); vga_write("base=0x"); vga_writeln(s);
    hex16_64(e->len, s);  vga_write(" len =0x"); vga_writeln(s);
    char tbuf[16]; utoa32(e->type, tbuf); vga_write(" type="); vga_writeln(tbuf);
    vga_writeln("------------------------");
}

static const char* mmap_type_name(uint32_t t){
    switch(t){
        case 1: return "usable";
        case 2: return "reserved";
        case 3: return "acpi-reclaimable";
        case 4: return "acpi-nvs";
        case 5: return "badram";
        default: return "other";
    }
}


/* mem summary: total usable bytes (type==1) and quick print */
static void handle_mem_tag(uint32_t mbi_addr){
    if(mbi_addr == 0){ vga_writeln("no multiboot info"); return; }
    uint8_t *base = (uint8_t*)(uintptr_t)mbi_addr;
    uint32_t total_size = *(uint32_t*)(base + 0);
    if(total_size < 8){ vga_writeln("invalid mbi size"); return; }
    uint8_t *tagp = base + 8;
    uint8_t *endp = base + total_size;
    uint64_t usable_bytes = 0;
    vga_write("mbi total_size="); char tb[16]; utoa32(total_size,tb); vga_writeln(tb);
    while(tagp + sizeof(struct mb2_tag) <= endp){
        struct mb2_tag *tag = (struct mb2_tag*)tagp;
        if(tag->type == MULTIBOOT_TAG_TYPE_END) break;
        if(tag->size < 8) break;
        if(tag->type == MULTIBOOT_TAG_TYPE_MMAP){
            struct mb2_tag_mmap *mmaptag = (struct mb2_tag_mmap*)tag;
            uint8_t *entryp = tagp + sizeof(struct mb2_tag_mmap);
            while(entryp + mmaptag->entry_size <= tagp + tag->size){
                struct mb2_mmap_entry *e = (struct mb2_mmap_entry*)entryp;
                if(e->type == 1) usable_bytes += e->len;
                entryp += mmaptag->entry_size;
            }
        }
        tagp += (tag->size + 7) & ~7;
    }
    /* print summary in MiB */
    uint32_t mib = (uint32_t)(usable_bytes / (1024*1024));
    char out[32]; utoa32(mib, out);
    vga_write("Usable RAM: "); vga_write(out); vga_writeln(" MiB");
}

static void memmap_print(uint32_t mbi_addr){
    if(mbi_addr == 0){
        vga_writeln("no multiboot info");
        return;
    }

    uint8_t *base = (uint8_t*)(uintptr_t)mbi_addr;
    uint32_t total_size = *(uint32_t*)(base + 0);
    if(total_size < 8){
        vga_writeln("invalid mbi size");
        return;
    }

    uint8_t *tagp = base + 8;
    uint8_t *endp = base + total_size;

    vga_writeln("Memory map entries:");
    char idxbuf[16];
    char hexbuf[32];
    char decbuf[32];

    int idx = 0;

    while(tagp + sizeof(struct mb2_tag) <= endp){
        struct mb2_tag *tag = (struct mb2_tag*)tagp;
        if(tag->type == MULTIBOOT_TAG_TYPE_END) break;
        if(tag->size < 8) break;

        if(tag->type == MULTIBOOT_TAG_TYPE_MMAP){
            struct mb2_tag_mmap *mmaptag = (struct mb2_tag_mmap*)tag;
            uint8_t *entryp = tagp + sizeof(struct mb2_tag_mmap);

            while(entryp + mmaptag->entry_size <= tagp + tag->size){
                struct mb2_mmap_entry *e = (struct mb2_mmap_entry*)entryp;

                idx++;
                utoa32((uint32_t)idx, idxbuf);
                vga_write("#"); vga_write(idxbuf); vga_write(": ");

                // base
                hex16_64(e->addr, hexbuf);
                vga_write("base=0x"); vga_write(hexbuf);

                // length
                hex16_64(e->len, hexbuf);
                vga_write(" len=0x"); vga_write(hexbuf);

                // length in MiB (approx)
                uint64_t mib = e->len / (1024*1024);
                utoa32((uint32_t)mib, decbuf);
                vga_write(" ("); vga_write(decbuf); vga_write(" MiB)");

                // type
                const char* name = mmap_type_name(e->type);
                utoa32(e->type, decbuf);
                vga_write(" type="); vga_write(decbuf); vga_write(" ");
                vga_writeln(name);

                entryp += mmaptag->entry_size;
            }
        }

        tagp += (tag->size + 7) & ~7;
    }

    if(idx == 0){
        vga_writeln("no mmap entries found");
    }
}


/* CPUID command preserved */
static void cmd_cpuid(){
    uint32_t eax,ebx,ecx,edx; cpuid(0,&eax,&ebx,&ecx,&edx);
    char vendor[13]; ((uint32_t*)vendor)[0]=ebx;((uint32_t*)vendor)[1]=edx;((uint32_t*)vendor)[2]=ecx;vendor[12]=0;
    vga_write("vendor: "); vga_writeln(vendor);
    cpuid(1,&eax,&ebx,&ecx,&edx);
    char t[16]; vga_write("eax: ");hex8(eax,t);vga_writeln(t);
    vga_write("ecx: ");hex8(ecx,t);vga_writeln(t);
    vga_write("edx: ");hex8(edx,t);vga_writeln(t);
}

static void sleep_ticks(uint32_t count){
    for(uint32_t i=0;i<count;i++){
        task_yield();
    }
}

static void test_task(){
    uint32_t counter = 0;
    while(1){
        int id = task_current_id();
        char idbuf[16], cntbuf[16];
        utoa32((uint32_t)id, idbuf);
        utoa32(counter++, cntbuf);

        vga_write("[task ");
        vga_write(idbuf);
        vga_write("] tick ");
        vga_writeln(cntbuf);

        for(volatile uint32_t i = 0; i < 20000000u; i++){
            if((i & 0x3FFFu) == 0u){
                scheduler_maybe_yield();
            }
        }

        /* one last check before next iteration */
        scheduler_maybe_yield();
    }
}



static void run_cmd(const char* buf, uint32_t mbi_addr){
    if(my_streq(buf,"help"))
        vga_writeln("commands: help, echo <text>, clear, halt, uptime, cpuid, reboot, mem, memmap, alloc <n>, heap, kmstat, taskrun, tasks, tstat, switch, time, history, !!, poweroff");

    else if(my_starts(buf,"echo "))
        vga_writeln(buf+5);

    else if(my_streq(buf,"clear"))
        vga_clear();

    else if(my_streq(buf,"halt")){
        vga_writeln("halting");
        for(;;)__asm__ volatile("hlt");
    }

    else if(my_streq(buf,"uptime")){
        char t[16];
        utoa32(timer_ticks()/100u, t);
        vga_write("seconds: "); vga_writeln(t);
    }

    else if(my_streq(buf,"cpuid"))
        cmd_cpuid();

    else if(my_streq(buf,"reboot")){
        vga_writeln("rebooting");
        for(;;){ outb(0x64, 0xFE); }
    }

    else if(my_streq(buf,"mem"))
        handle_mem_tag(mbi_addr);

    else if(my_streq(buf,"memmap"))
        memmap_print(mbi_addr);

    else if(my_starts(buf,"alloc ")){
        uint32_t x = 0; 
        const char *p = buf + 6;
        while(*p >= '0' && *p <= '9'){ x = x * 10 + (*p - '0'); p++; }
        void *r = kmalloc(x);
        if(!r) vga_writeln("alloc failed");
        else {
            char h[16];
            hex8((uint32_t)(uintptr_t)r, h);
            vga_write("allocated @ 0x"); 
            vga_writeln(h);
        }
    }

    else if(my_streq(buf,"heap")){
        char h[16], s[16];
        hex8(kalloc_get_start(), s);
        hex8(kalloc_get_ptr(), h);
        vga_write("heap_start=0x"); vga_writeln(s);
        vga_write("heap_ptr  =0x"); vga_writeln(h);
    }

    else if(my_streq(buf,"kmstat")){
        char h[16], d[16];
        uint32_t used = kalloc_bytes_used();
        hex8(kalloc_get_start(), h); vga_write("heap_start=0x"); vga_writeln(h);
        hex8(kalloc_get_ptr(),  h); vga_write("heap_ptr  =0x"); vga_writeln(h);
        utoa32(used, d);           vga_write("used bytes="); vga_writeln(d);
    }
    
    else if(my_streq(buf,"taskrun"))
        task_create(test_task);

    else if(my_streq(buf,"tasks"))
        task_list();
    
    else if(my_streq(buf,"tstat"))
        task_stats_print();
    
    else if(my_streq(buf,"switch"))
        task_yield();
        
    else if(my_streq(buf,"time")){
        struct rtc_time t;
        rtc_read(&t);
        char tmp[16];
        vga_write("Time: ");
        utoa32(t.hour, tmp); vga_write(tmp); vga_putc(':');
        utoa32(t.min, tmp);  vga_write(tmp); vga_putc(':');
        utoa32(t.sec, tmp);  vga_write(tmp);

        vga_write("  Date: ");
        utoa32(t.day, tmp); vga_write(tmp); vga_putc('/');
        utoa32(t.month, tmp); vga_write(tmp); vga_putc('/');
        utoa32(t.year, tmp); vga_writeln(tmp);
    }

    else if(my_streq(buf,"history"))
        history_print();

    else if(my_streq(buf,"!!")){
        if(history_count == 0) vga_writeln("no history");
        else {
            int idx = (history_start + history_count - 1) % HISTORY_MAX;
            vga_write("> "); vga_writeln(history[idx]);  // display command
            run_cmd(history[idx], mbi_addr);            // 🔥 RE-EXECUTE
        }
    }

    else if(my_streq(buf,"poweroff")){
        vga_writeln("powering off...");
        qemu_poweroff();
    }

    else if(my_strlen(buf) > 0)
        vga_writeln("unknown");
}

/* store Multiboot info globally so shell_task can access it */
static uint32_t g_mbi_addr = 0;

/* Shell as a TASK: same logic as previous inline shell loop but no longer in kernel_main */
static void shell_task(void){
    char buf[128]; size_t n = 0;
    vga_writeln("mini-os shell");
    prompt();

    for(;;){
        char c = kbd_getch();
        if(c==0){
          continue;
        }

        if(c=='\n'){
            buf[n]=0;
            vga_putc('\n');

            if(my_strlen(buf) > 0 && !my_streq(buf,"history") && !my_streq(buf,"!!")){
                history_add(buf);
            }

            /* use global mbi addr */
            run_cmd(buf, g_mbi_addr);
            n = 0;
            prompt();
        }
        else if(c==8){
            if(n>0){ n--; vga_putc('\b'); }
        }
        else {
            if(n<127){ buf[n++]=c; vga_putc(c); }
        }
    }
}


void kernel_main(uint32_t mbi_addr){
    /* store mbi for shell/task commands */
    g_mbi_addr = mbi_addr;

    /* start: clear and banner */
    vga_set_color(0x0F);
    vga_clear();
    vga_set_color(0x1F);
    vga_writeln("==============================================");
    vga_writeln("               ITI Patna OS                   ");
    vga_writeln("==============================================");
    vga_set_color(0x0F);
    vga_writeln("[dbg] after banner");

    /* IRQs / IDT / PIT */
    irq_init();
    vga_writeln("[dbg] after irq_init");
    __asm__ volatile("sti");
    vga_writeln("[dbg] after sti");

    /* heap */
    kalloc_init(0x01000000);
    vga_writeln("[dbg] after kalloc_init");

    /* paging */
    paging_init();
    vga_writeln("[dbg] after paging_init");

    /* task system */
    task_init();
    vga_writeln("[dbg] after task_init");

    /* create shell task (first) */
    vga_writeln("[dbg] about to create shell task");
    task_create(shell_task);
    vga_writeln("[dbg] after create shell task");

    /* don't auto-create demo tasks here (create with `taskrun`) */

    /* final: switch into task world */
    vga_writeln("[dbg] about to switch to first task");
    task_switch_first();

    /* if we ever return, halt and print message */
    vga_writeln("[dbg] returned from task_switch_first (unexpected)");
    for(;;) __asm__ volatile("hlt");
}


static void history_add(const char* cmd){
    size_t len = my_strlen(cmd);
    if(len == 0) return; // don't store empty lines

    if(len >= CMD_MAX_LEN) len = CMD_MAX_LEN-1;

    int idx;
    if(history_count < HISTORY_MAX){
        idx = (history_start + history_count) % HISTORY_MAX;
        history_count++;
    } else {
        // overwrite oldest
        idx = history_start;
        history_start = (history_start + 1) % HISTORY_MAX;
    }

    for(size_t i=0;i<len;i++) history[idx][i] = cmd[i];
    history[idx][len] = 0;
}

static void history_print(){
    if(history_count == 0){
        vga_writeln("no history");
        return;
    }
    char num[16];
    for(int i=0;i<history_count;i++){
        int idx = (history_start + i) % HISTORY_MAX;
        utoa32((uint32_t)(i+1), num);
        vga_write(num);
        vga_write(": ");
        vga_writeln(history[idx]);
    }
}


