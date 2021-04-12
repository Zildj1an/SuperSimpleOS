/*
 * `````````````````````````````````````````````````````````````````````````````
	Multi-tasking support.
	Authors Carlos Bilbao and Ashwin Krishnakumar

 * `````````````````````````````````````````````````````````````````````````````
*/

#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

typedef enum {DEFAUT_POLICY, NUM_SCHED_POLICIES} sched_policy_t;

typedef struct {

	page_tables_t *page_table; /* Page table, which should have code, stack, and
	                              TLS mapped, see set_upage_table() */
	      
	void *stack;              /* Mapped virtual address                       */
	void *code;               /* Mapped virtual address                       */

	void *tls_addr;           /* TLS address                                  */

 	uint32_t pid;             /* Process IDentfier                            */
 	uint8_t  user_space:1;    /* Flag for the privilege level (kernel/user)   */

	sched_policy_t policy;    /* Scheduling policy for this task              */

} task_struct;


static task_struct kernel_task; 

// static task_struct current_task; // Update in a wrapper of user_jump? 

void init_scheduler(page_tables_t *kinfo_pages); 

// void jump_to_user();

#endif