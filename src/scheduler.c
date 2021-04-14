/*
 * `````````````````````````````````````````````````````````````````````````````
	Multi-tasking support.
	Authors Carlos Bilbao and Ashwin Krishnakumar

 * `````````````````````````````````````````````````````````````````````````````
*/
#include <scheduler.h>
#include <kernel_syscall.h>

void init_scheduler(page_tables_t *kinfo_pages)
{
	task_struct *kernel_task = tasks;

	kernel_task.page_table = kinfo_pages;
	kernel_task.pid        = 0;
	kernel_task.user_space = 0;
	kernel_task.policy     = DEFAUT_POLICY;
}

void __register_task(void *code, void *stack, page_tables_t *page_table)
{
	/* Kernel has id zero, and we can't have more than MAX_NUM_THREADS */
	assert(current_task < MAX_NUM_THREADS);

	tasks[current_task].page_table = NULL;
	tasks[current_task].pid        = current_task;
	tasks[current_task].user_space = 1;
	tasks[current_task].policy     = DEFAUT_POLICY;
	tasks[current_task].stack      = stack;
	tasks[current_task].code       = code;
}

void jump_to_user(void *code, void *stack, page_tables_t *page_table)
{
	current_task++;

	__register_task(code,stack,page_table);
	
	// Perhaps we need to save the kernel stack and code before calling user_jump()?

	user_jump(code);
}
