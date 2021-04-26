/*
 * `````````````````````````````````````````````````````````````````````````````
	Multi-tasking support.
	Authors Carlos Bilbao and Ashwin Krishnakumar

 * `````````````````````````````````````````````````````````````````````````````
*/
#include <scheduler.h>
#include <kernel_syscall.h>

static int current_task; 
struct task_struct * current;

void init_scheduler(page_tables_t *kinfo_pages)
{
	task_struct *kernel_task = tasks;

	kernel_task->page_table = kinfo_pages;
	kernel_task->pid        = 0;
	kernel_task->user_space = 0;
	kernel_task->policy     = DEFAUT_POLICY;
}

void __register_task(void *code, void *stack, page_tables_t *page_table)
{
	/* Kernel has id zero, and we can't have more than MAX_NUM_THREADS */
	assert(registered_tasks < MAX_NUM_THREADS);

	tasks[registered_tasks].page_table = NULL;
	tasks[registered_tasks].pid        = registered_tasks;
	tasks[registered_tasks].user_space = 1;
	tasks[registered_tasks].policy     = DEFAUT_POLICY;
	tasks[registered_tasks].stack      = stack;
	tasks[registered_tasks].code       = code;
}

void scheduler()
{
	int prev_task = current_task;
		
	if(current_task == registered_tasks)
	{
		/*Resetting from beginning*/
		current_task = 0 ;
	}
	else
	{

		current_task++;
	}
	printf("Context switching from task %d to task %d\n",prev_task,current_task);
//	context_switch(tasks[prev_task].stack,tasks[current_task].stack);		
	current = &tasks[current_task];  // This is linux kernel equivalent of current pointer
}

void jump_to_user(void *code, void *stack, page_tables_t *page_table)
{

	__register_task(code,stack,page_table);
	registered_tasks++; /*Increment post registration*/
	
	// Perhaps we need to save the kernel stack and code before calling user_jump()?

	user_jump(code);
}
