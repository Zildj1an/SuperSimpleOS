/*
 * `````````````````````````````````````````````````````````````````````````````
	Multi-tasking support, non-preemptive scheduler (cooperative).
	Authors Carlos Bilbao and Ashwin Krishnakumar

 * `````````````````````````````````````````````````````````````````````````````
*/
#include <scheduler.h>
#include <kernel_syscall.h>
#include <printf.h>

#define assert(x) if (!(x)) asm __volatile__("hlt")

#define NAME_CHECK(x) case x: return #x

const char* policy_name(sched_policy_t policy){

	switch(policy) {
		
		NAME_CHECK(FIFO_POLICY);
		NAME_CHECK(LIFO_POLICY);
		NAME_CHECK(SJF_POLICY);
		NAME_CHECK(PBP_POLICY);
		NAME_CHECK(RM_POLICY);
		NAME_CHECK(DM_POLICY);

	default:
		return "UNKNOWN POLICY";
	};
}
void *init_trampoline_ptr;

/* Provide synchronisation mechanisms, assuming 1 CPU for now */

void global_lock(void) 
{
#ifndef SMP
    /* Disable interrupts */
    asm __volatile__ ("cli"); 
    IRQ_disable_num++;

#endif
}
 
void global_unlock(void) 
{
#ifndef SMP
    
    IRQ_disable_num--;

    if(!IRQ_disable_num) {
        /* Enable interrupts */
        asm __volatile__ ("sti"); 
    }
#endif
}

int get_priority(void)
{
	int ret;
	global_lock();
	ret = current_task->info.priority;
	global_unlock();
	return ret;
}

void renice(int new_prio)
{
	global_lock();
	current_task->info.priority = new_prio;
	global_unlock();
}

void __print_tasks(void)
{
	for(int i = 0 ; i < registered_task;i++)
	{
		printf("Task number %d with stack addres %p\n",tasks[i].pid,tasks[i].stack);	
	}
}

/* Deadline Monotonic (real-time) */
task_struct* __dm_next(int pid)
{
	task_struct *next;
	int highest_id = -1, shortest_deadline, i;

	for (i = 0; i < registered_task; ++i)
	{
		if (pid == i) continue;

		/* In case of a tie, first come wins */
		if (highest_id == -1 || shortest_deadline > tasks[i].info.deadline)
		{
			if (tasks[i].process_state != PROCESS_SLEEP && 
			    tasks[i].process_state != PROCESS_COMPLETED)
			{
				highest_id = i;
				shortest_deadline = tasks[i].info.deadline;
			}
		}
	}

	/* Schedule first work if we don't have anything else... */
	if (highest_id == -1){
		highest_id = 0;
	}

	next = &tasks[highest_id];

	return next;
}

/* First Come First Served */
task_struct* __fifo_next(int pid)
{
	task_struct *next;
	int i = current_task->pid + 1;

	do {
		next = &tasks[i % registered_task];
		i++;
		if ((i%registered_task)==pid) break;
	}
	while (next->process_state == PROCESS_SLEEP || 
	       next->process_state == PROCESS_COMPLETED);

	if (!next) next = &tasks[0];

	return next;
}

/* Shortest Job First */
task_struct* __sjf_next(int pid)
{
	task_struct *next;
	int shortest_id = -1, shortest_cpu_burst, i;

	for (i = 0; i < registered_task; ++i)
	{
		if (pid == i) continue;
		
		/* In case of a tie, first come wins */
		if (shortest_id == -1 || shortest_cpu_burst > tasks[i].info.cpu_burst)
		{
			if (tasks[i].process_state != PROCESS_SLEEP &&
			    tasks[i].process_state != PROCESS_COMPLETED)
			{
				shortest_id = i;
				shortest_cpu_burst = tasks[i].info.cpu_burst;
			}
		}
	}

	/* Schedule first work if we don't have anything else... */
	if (shortest_id == -1){
		shortest_id = 0;	
	}

	next = &tasks[shortest_id];

	return next;
}

/* Priority Based Policy */
task_struct* __pbp_next(int pid)
{
	task_struct *next;
	int highest_id = -1, highest_prio, i;

	for (i = 0; i < registered_task; ++i)
	{
		if (pid == i) continue;
		
		if (highest_id == -1 || highest_prio > tasks[i].info.priority)
		{	
			if (tasks[i].process_state != PROCESS_SLEEP &&
			    tasks[i].process_state != PROCESS_COMPLETED)
			{
				highest_id = i;
				highest_prio = tasks[i].info.priority;
			}
		}
	}

	/* Schedule first work if we don't have anything else... */
	if (highest_id == -1){
		highest_id = 0;
	}

	next = &tasks[highest_id];

	return next;
}

/* Last In First Out */
task_struct* __lifo_next(int pid)
{
	task_struct *next=NULL;
	int id_next = current_task->pid ;

	do {
		id_next--;

		if (id_next < 0){
			id_next = registered_task - 1;
		}
		
		if (id_next == pid){
			break;
		}

		next = &tasks[id_next];
	}
	while (next->process_state == PROCESS_SLEEP ||
	       next->process_state == PROCESS_COMPLETED);
	
	if (!next) next = &tasks[0];

	return next;
}

/* Rate Monotonic (real-time)     */
task_struct* __rm_next(int pid)
{
	task_struct *next;
	int highest_id = -1, highest_frequency, i;

	for (i = 0; i < registered_task; ++i)
	{
		if (pid == i) continue;
		
		/* In case of a tie, first come wins */
		if (highest_id == -1 || highest_frequency > tasks[i].info.frequency)
		{
			if (tasks[i].process_state != PROCESS_SLEEP &&
			    tasks[i].process_state != PROCESS_COMPLETED)
			{
				highest_id = i;
				highest_frequency = tasks[i].info.frequency;
			}
		}
	}

	/* Schedule first work if we don't have anything else... */
	if (highest_id == -1){
		highest_id = 0;
	}

	next = &tasks[highest_id];

	return next;
}

/* If all task completed, repeat the process */
void __check_completed(void)
{
	int i = 1;

	for (i = 0; i < registered_task; ++i){
		if (tasks[i].process_state != PROCESS_COMPLETED) return; 
	}
	
	/* Reinit */
	for (i = 0; i < registered_task; ++i){
		if (tasks[i].process_state != PROCESS_SLEEP){
			tasks[i].process_state = PROCESS_SUSPENDED;
		}
	}
}

void __scheduler(int pid, sched_policy_t policy)
{
	task_struct *curr, *next=NULL;

	curr = current_task;

	curr->process_state = PROCESS_COMPLETED; 
	printf("Current task is %d and stack is %p\n",current_task->pid,current_task->stack);
	
	__check_completed();
	/* Pick the next registered task */
	switch(policy){
	case FIFO_POLICY: /* First In First Out */
		next = __fifo_next(curr->pid);
		printf("[%s] Schedules task %d\n",policy_name(FIFO_POLICY),next->pid + 1);
	break;
	case LIFO_POLICY: /* Last In First Out */
		next = __lifo_next(curr->pid);
		printf("[%s] Schedules task %d\n",policy_name(LIFO_POLICY),next->pid + 1);
	break;
	case SJF_POLICY: /* Shortest Job First */
		next = __sjf_next(curr->pid);
		printf("[%s] Schedules task %d\n",policy_name(SJF_POLICY),next->pid + 1);
	break;
	case PBP_POLICY: /* Priority Based Policy */
		next = __pbp_next(curr->pid);
		printf("[%s] Schedules task %d\n",policy_name(PBP_POLICY),next->pid + 1);
	break;
	case RM_POLICY:  /* Rate Monotonic (real-time)     */ 
		next = __rm_next(curr->pid);
		printf("[%s] Schedules task %d\n",policy_name(RM_POLICY),next->pid + 1);
	break;
	case DM_POLICY: /* Deadline Monotonic (real-time) */
		next = __dm_next(curr->pid);
		printf("[%s] Schedules task %d\n",policy_name(DM_POLICY),next->pid + 1);
	break;
	default:
	break;
	}

	current_task = next;
	current_task->num_runs++;
	next->process_state = PROCESS_RUNNING;
	printf("The next task to be picked is %d \n",next->pid);
	printf("The previous task's parameter is %p and the value contained is %x\n", &curr->stack,curr->stack);
	context_switch(&curr->stack,&next->stack);	
}

void scheduler(void)
{
	int pid = current_task->pid;
	unsigned long long stack = current_task->stack;

	/* We could add here the wall clock for performance evaluation of tasks */
	printf("Scheduling triggered by task %d (stack %d)\n",pid,stack);
	printf("Policy = %d\n",current_task->policy);

	switch(current_task->policy){
	case DEFAULT_POLICY:
	case FIFO_POLICY: /* First In First Out */
		__scheduler(pid,FIFO_POLICY);
	break;
	case LIFO_POLICY: /* Last In First Out */
		__scheduler(pid,LIFO_POLICY);
	break;
	case SJF_POLICY:  /* Shortest Job First */
		__scheduler(pid,LIFO_POLICY);
	break;
	case PBP_POLICY: /* Priority Based Policy */
		__scheduler(pid,PBP_POLICY);
	break;
	case RM_POLICY: /* Rate Monotonic (real-time)     */ 
		__scheduler(pid,RM_POLICY);
	break;
	case DM_POLICY: /* Deadline Monotonic (real-time) */
		__scheduler(pid,DM_POLICY);
	break;
	default:
		/* The first time we print the stack, the kernel stack will be garbage */
		printf("Wrong task scheduling policy (task %d, stack %p)\n",pid,stack);
	}
}

void init_scheduler(void)
{
	current_task = &tasks[0];
	printf("Scheduler initialized and registered as task %d\n",current_task->pid);
}

static void
stack_push(void **stackp, unsigned long value)
{
        unsigned long *stack = *stackp;

        stack--;
        *stack = value;
        *stackp = stack;
}

/* Leave argument info as NULL for the default policy */
void register_task(void *code,  unsigned long long stack, 
		           void *ppage_table, thread_info *info)
{
	/* Kernel has id zero, and we can't have more than MAX_NUM_THREADS */
	assert(registered_task < MAX_NUM_THREADS);

	printf("\nStack value provided for task %d is %p\n",registered_task,stack);
	tasks[registered_task].ppage_table = ppage_table;
	tasks[registered_task].pid        = registered_task;
	tasks[registered_task].user_space = 1;
	tasks[registered_task].policy     = DEFAULT_POLICY;
	tasks[registered_task].code       = code;
	tasks[registered_task].process_state = PROCESS_SUSPENDED;
	tasks[registered_task].inst_ptr = (long long unsigned int) init_trampoline_ptr;
	tasks[registered_task].num_runs = 0;

	stack_push((void**)&stack,(unsigned long) code);
	stack_push((void**)&stack , (unsigned long)0x0);

	if (info)
	{
		tasks[registered_task].policy          = info->policy;
		tasks[registered_task].info.cpu_burst  = info->cpu_burst;
		tasks[registered_task].info.priority   = info->priority;
		tasks[registered_task].info.frequency  = info->frequency;
		tasks[registered_task].info.deadline   = info->deadline;
	}

	tasks[registered_task].stack      = stack;
	printf("Stack value registered is %p",stack);
	printf("\nNumber of task registered is %d [stack %p]\n",registered_task, tasks[registered_task].stack);
	registered_task++;

}

void sched_start(long unsigned int stack_for_init)
{
	static task_struct entry_task;
	entry_task.stack = (unsigned long long) stack_for_init + 0x1000;

	printf("Number of tasks registered is %p\n",registered_task);
	printf("The instruction pointer in new task is %p\n", tasks[0].inst_ptr);
	printf("The stack pointer in new task is %p\n", tasks[0].stack);

	context_switch(&entry_task.stack,&tasks[0].stack);
}

void sched_sleep(int nsecs)
{
	/* Future work */
}

void idle_task(void)
{
	for (;;){
		asm("");
		scheduler();
	}
}


