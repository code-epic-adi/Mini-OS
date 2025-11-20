#include "task.h"
#include "kalloc.h"
#include <stdint.h>

/* extern vga helpers (defined in vga.c) */
extern void vga_writeln(const char* s);
extern void vga_write(const char* s);

/* Local helpers */
static void utoa32_local(uint32_t x, char* b){
    char t[16]; int i=0;
    if(x==0){ b[0]='0'; b[1]=0; return; }
    while(x){ t[i++] = '0' + (x % 10u); x/=10u; }
    for(int j=0;j<i;j++) b[j]=t[i-1-j];
    b[i]=0;
}

static void hex8_local(uint32_t x, char* b){
    static const char h[] = "0123456789ABCDEF";
    for(int i=0;i<8;i++) b[i] = h[(x >> ((7-i)*4)) & 0xF];
    b[8] = 0;
}

/* Scheduler state */
static task_t *task_head = 0;
static task_t *current_task = 0;
static int next_id = 1;
static uint32_t sched_total_ticks = 0;
static volatile uint32_t sched_ticks_hint = 0;
static volatile int need_resched = 0;

void task_init(void){
    task_head = 0;
    current_task = 0;
    next_id = 1;
    sched_total_ticks = 0;
}

void task_list(void){
    task_t *t = task_head;
    if(!t){
        vga_writeln("No tasks");
        return;
    }
    char n[16];
    do {
        utoa32_local((uint32_t)t->id, n);
        vga_write("task ");
        vga_writeln(n);
        t = t->next;
    } while(t != task_head);
}

/* Create task stack in the format expected by task_yield / initial_enter:
   top-of-stack:
      [dummy regs for popa] (8 * 4 bytes)
      [return address = entry]
   Then initial_enter / yield will:
      mov esp, stack;
      popa;
      ret;   --> jumps into entry()
*/
void task_create(void (*entry)(void)){
    task_t *t = (task_t*)kmalloc(sizeof(task_t));
    if(!t){
        vga_writeln("task_create: alloc failed for task_t");
        return;
    }
    uint32_t *stack = (uint32_t*)kmalloc(4096);
    if(!stack){
        vga_writeln("task_create: stack alloc failed");
        return;
    }

    uint32_t *sp = stack + (4096/4);   /* 4096 bytes / 4 = 1024 uint32_t */

    /* push return address = entry (will be used by 'ret') */
    *(--sp) = (uint32_t)entry;

    /* push 8 dummy registers (EDI,ESI,EBP,ESP_s,EBX,EDX,ECX,EAX) */
    for(int i=0;i<8;i++){
        *(--sp) = 0;
    }

    t->stack     = sp;
    t->id        = next_id++;
    t->run_ticks = 0;

    if(!task_head){
        task_head = t;
        t->next = t;   /* single node circle */
    } else {
        task_t *tail = task_head;
        while(tail->next != task_head) tail = tail->next;
        tail->next = t;
        t->next = task_head;
    }

    char nbuf[16]; utoa32_local((uint32_t)t->id, nbuf);
    vga_write("Created task ");
    vga_writeln(nbuf);
}

/* First-time enter: restore dummy regs, then ret -> entry() */
__attribute__((noreturn))
static void task_initial_enter(uint32_t *new_stack){
    __asm__ volatile(
        "mov %0, %%esp\n"
        "popa\n"
        "ret\n"
        :: "r"(new_stack)
    );
    __builtin_unreachable();
}

/* Enter task world from kernel_main, into the first task (shell task) */
void task_switch_first(void){
    if(!task_head){
        vga_writeln("task_switch_first: no tasks");
        return;
    }
    current_task = task_head;
    vga_writeln("switching to first task...");
    task_initial_enter(current_task->stack);
}

/* Cooperative yield: save ESP of current_task, advance to next,
   restore its ESP, return into that task. */
__attribute__((naked))
void task_yield(void){
    __asm__ volatile(
        "pusha\n"
        "movl current_task, %eax\n"
        "testl %eax, %eax\n"
        "je 1f\n"

        /* save ESP into current_task->stack (offset 0) */
        "movl %esp, (%eax)\n"

        /* advance to next task: current_task = current_task->next */
        "movl 4(%eax), %eax\n"        /* eax = current_task->next */
        "movl %eax, current_task\n"   /* current_task = eax */

        /* load ESP from new current_task->stack */
        "movl (%eax), %esp\n"

        "popa\n"
        "ret\n"

        "1:\n"
        "popa\n"
        "ret\n"
    );
}

int task_current_id(void){
    return current_task ? current_task->id : -1;
}

/* called from timer ISR every tick */
void task_on_tick(void){
    sched_total_ticks++;
    if(current_task){
        current_task->run_ticks++;
    }

    /* NEW: every ~10 timer ticks, ask for a reschedule */
    sched_ticks_hint++;
    if(sched_ticks_hint >= 10){
        sched_ticks_hint = 0;
        need_resched = 1;
    }
}


/* print per-task stats: ticks and share% */
void task_stats_print(void){
    if(!task_head){
        vga_writeln("No tasks");
        return;
    }

    uint32_t total = 0;
    task_t *t = task_head;
    do {
        total += t->run_ticks;
        t = t->next;
    } while(t != task_head);

    if(total == 0){
        vga_writeln("No run ticks yet");
        return;
    }

    char idbuf[16], tickbuf[16], pctbuf[16];
    t = task_head;
    do {
        utoa32_local((uint32_t)t->id, idbuf);
        utoa32_local(t->run_ticks, tickbuf);
        uint32_t pct = (t->run_ticks * 100u) / total;
        utoa32_local(pct, pctbuf);

        vga_write("task ");
        vga_write(idbuf);
        vga_write("  ticks=");
        vga_write(tickbuf);
        vga_write("  share=");
        vga_write(pctbuf);
        vga_writeln("%");

        t = t->next;
    } while(t != task_head);
}

void scheduler_maybe_yield(void){
    if(need_resched){
        need_resched = 0;
        task_yield();      /* context switch using the existing, stable path */
    }
}

