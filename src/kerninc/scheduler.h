/*
 * `````````````````````````````````````````````````````````````````````````````
	Multi-tasking support.
	Authors Carlos Bilbao and Ashwin Krishnakumar

 * `````````````````````````````````````````````````````````````````````````````
*/

#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include <types.h>

#define MAX_NUM_THREADS (64)

extern void *init_trampoline_ptr;

typedef unsigned long long u64;

typedef enum {DEFAULT_POLICY=0,   /* First In First Out                       */
			  FIFO_POLICY,        /* First In First Out                       */
			  LIFO_POLICY,        /* Last In First Out                        */
			  SJF_POLICY,         /* Shortest Job First                       */
			  PBP_POLICY,         /* Priority Based Policy                    */
			  RM_POLICY,          /* Rate Monotonic (real-time)               */
			  DM_POLICY,          /* Deadline Monotonic (real-time)           */
			  NUM_SCHED_POLICIES
} sched_policy_t;


typedef enum {PROCESS_INIT=0,
	      PROCESS_RUNNING,
	      PROCESS_SUSPENDED,
	      PROCESS_COMPLETED,
	      PROCESS_SLEEP, 
	      NUM_PROC_STATES} process_state_t;

/* Extra fields for non default policies */
typedef struct {

	sched_policy_t policy; /* In case we register a non-default task          */

	int cpu_burst; /* Old-style SJF (Shortest-Job First), WCET                */

	int priority; /* Piorirty Based Policy    (0 to 10)                       */

	int frequency; /* Rate Monotonic (real-time)                              */

	int deadline;  /* Deadline Monotonic (real-time)                          */

} thread_info;

typedef struct {

	unsigned long long stack; /* Mapped virtual address                       */
	unsigned long long inst_ptr; /*Immediately after the stack                */

	void *code;               /* Mapped virtual address                       */

	void *tls_addr;           /* TLS address                                  */

 	uint32_t pid;             /* Process IDentfier  (array position)          */
 	uint8_t  user_space:1;    /* Flag for the privilege level (kernel/user)   */

	u64 * ppage_table; 

	process_state_t process_state;

	sched_policy_t policy;    /* Scheduling policy for this task              */

	thread_info info;    /* Extra thread info for certain policies            */

	int num_runs; /* Times used the CPU */

} task_struct;

/* Update priority */
void renice(int new_prio);

/* Get priority */
int get_priority(void);

#define RUNS_MAX (9000)

static volatile int registered_task = 0; // Update in a wrapper of user_jump? 

static task_struct tasks[MAX_NUM_THREADS] __attribute__((used));
static task_struct *current_task __attribute__((used));

void init_scheduler(void);

/* Leave argument info as NULL for the default policy */
void register_task(void *code, unsigned long long stack, 
	                 void *ppage_table,thread_info *info);

void scheduler(void);

void sched_sleep(int nsecs); /* Yield execution */

void sched_start(long unsigned int stack_for_init); 

void idle_task(void);

void init_trampoline(void);

void context_switch(unsigned long long  * a,unsigned long long *b) ;

/* Synchronisation mechanisms */

static int IRQ_disable_num __attribute__((used)) = 0;

void global_lock(void);
void global_unlock(void); 


#endif
