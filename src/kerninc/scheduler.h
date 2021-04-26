/*
 * `````````````````````````````````````````````````````````````````````````````
	Multi-tasking support.
	Authors Carlos Bilbao and Ashwin Krishnakumar

 * `````````````````````````````````````````````````````````````````````````````
*/

#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include <page_table.h>
#include <types.h>

#define MAX_NUM_THREADS (6)

extern struct task_struct * current;

extern void scheduler();

typedef enum {DEFAUT_POLICY, NUM_SCHED_POLICIES} sched_policy_t;

typedef struct {

	page_tables_t *page_table; /* Page table, which should have code, stack, and
	                              TLS mapped, see set_upage_table() */
	      
	void *stack;              /* Mapped virtual address                       */
	void *code;               /* Mapped virtual address                       */

	void *tls_addr;           /* TLS address                                  */

 	uint32_t pid;             /* Process IDentfier  (array position)          */
 	uint8_t  user_space:1;    /* Flag for the privilege level (kernel/user)   */

	sched_policy_t policy;    /* Scheduling policy for this task              */

} task_struct;

static volatile int registered_tasks = 0; // Update in a wrapper of user_jump? 

static task_struct tasks[MAX_NUM_THREADS];

void init_scheduler(page_tables_t *kinfo_pages); 

void jump_to_user(void *code, void *stack, page_tables_t *page_table);

#endif
