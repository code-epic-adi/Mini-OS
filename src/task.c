#include "task.h"
#include "kalloc.h"
#include <stdint.h>

/* We don't have a vga.h header, so just declare what we use */
extern void vga_writeln(const char* s);
extern void vga_write(const char* s);

/* Local decimal converter (like kernel's utoa32) */
static void utoa32_local(uint32_t x, char* b){
    char t[16];
    int i = 0;
    if(x == 0){
        b[0] = '0';
        b[1] = 0;
        return;
    }
    while(x){
        t[i++] = '0' + (x % 10u);
        x /= 10u;
    }
    for(int j=0;j<i;j++) b[j] = t[i-1-j];
    b[i] = 0;
}

/* Task list and current task */
static task_t *task_head = 0;
static task_t *current_task = 0;
static int next_id = 1;

void task_init(){
    task_head = 0;
    current_task = 0;
    next_id = 1;
}

void task_list(){
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

void task_create(void (*entry)(void)){
    task_t *t = (task_t*)kmalloc(sizeof(task_t));
    if(!t){
        vga_writeln("task_create: alloc failed");
        return;
    }

    uint32_t *stack = (uint32_t*)kmalloc(4096);
    if(!stack){
        vga_writeln("task_create: stack alloc failed");
        return;
    }

    /*
     * We want the stack to look exactly like it would
     * right AFTER a task called task_yield():
     *
     *   [saved regs by pusha] (8 * 4 bytes)
     *   [return address]      (entry)
     *
     * task_yield will do:
     *   popa
     *   ret
     *
     * So we set up regs (dummy values) + entry as return addr.
     */
    uint32_t *sp = stack + 1024;  // 4096 / 4 = 1024 uint32s

    /* push return address = entry */
    *(--sp) = (uint32_t)entry;

    /* push 8 dummy registers (EDI,ESI,EBP,ESP_s,EBX,EDX,ECX,EAX) */
    for(int i=0;i<8;i++){
        *(--sp) = 0;
    }

    t->stack = sp;
    t->id = next_id++;

    if(!task_head){
        task_head = t;
        t->next = t;   // single-node circle
    } else {
        /* insert at end of circular list */
        task_t *tail = task_head;
        while(tail->next != task_head) tail = tail->next;
        tail->next = t;
        t->next = task_head;
    }

    char n[16];
    utoa32_local((uint32_t)t->id, n);
    vga_write("Created task ");
    vga_writeln(n);
}

/* first-time entry: expects stack = [regs][ret] */
__attribute__((noreturn))
static void task_initial_enter(uint32_t *new_stack){
    __asm__ volatile(
        "mov %0, %%esp\n"
        "popa\n"    /* restore dummy regs */
        "ret\n"     /* jump to entry */
        :
        : "r"(new_stack)
    );
    __builtin_unreachable();
}

/* first switch from kernel into task world */
void task_switch_first(void){
    if(!task_head){
        vga_writeln("task_switch: no tasks");
        return;
    }
    current_task = task_head;
    vga_writeln("switching to first task...");
    task_initial_enter(current_task->stack);
}

/*
 * cooperative yield: save ESP of current_task, move to next,
 * restore its ESP, then popa+ret into that task.
 */
__attribute__((naked))
void task_yield(void){
    __asm__ volatile(
        "pusha\n"                      /* save regs */

        "movl current_task, %eax\n"    /* eax = current_task */
        "testl %eax, %eax\n"
        "je 1f\n"                      /* if no current_task, just return */

        /* save ESP into current_task->stack (field at offset 0) */
        "movl %esp, (%eax)\n"

        /* advance to next task: current_task = current_task->next */
        "movl 4(%eax), %eax\n"         /* eax = current_task->next (offset 4) */
        "movl %eax, current_task\n"    /* current_task = eax */

        /* load ESP from new current_task->stack */
        "movl (%eax), %esp\n"

        "popa\n"                       /* restore regs for new task */
        "ret\n"                        /* return to wherever that task was */

        "1:\n"
        "popa\n"
        "ret\n"
    );
}

/* helper to ask: which task is currently running? */
int task_current_id(void){
    return current_task ? current_task->id : -1;
}

