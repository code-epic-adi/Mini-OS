#ifndef TASK_H
#define TASK_H

#include <stdint.h>

typedef struct task {
    uint32_t *stack;
    struct task *next;
    int id;
} task_t;

void task_init();
void task_create(void (*entry)(void));
void task_list();
void task_switch_first(void);
void task_yield(void);
int  task_current_id(void);

#endif

