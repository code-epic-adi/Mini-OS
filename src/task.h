#ifndef TASK_H
#define TASK_H

#include <stdint.h>

/* Task control block
   NOTE: The first two fields (stack, next) are used by inline asm in task_yield.
*/
typedef struct task {
    uint32_t *stack;        /* saved ESP */
    struct task *next;      /* next task in circular list */
    int       id;           /* task id */
    uint32_t  run_ticks;    /* how many timer ticks this task has run */
} task_t;

void task_init(void);
void task_create(void (*entry)(void));
void task_list(void);

/* enter task world for the first time (used only from kernel_main) */
void task_switch_first(void);

/* cooperative yield (used from tasks & shell) */
void task_yield(void);

/* info / stats */
int  task_current_id(void);
void task_on_tick(void);
void task_stats_print(void);
void scheduler_maybe_yield(void);

#endif

