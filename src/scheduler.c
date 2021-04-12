/*
 * `````````````````````````````````````````````````````````````````````````````
	Multi-tasking support.
	Authors Carlos Bilbao and Ashwin Krishnakumar

 * `````````````````````````````````````````````````````````````````````````````
*/
#include <scheduler.h>

void init_scheduler(page_tables_t *kinfo_pages)
{
	kernel_task.page_table = kinfo_pages;
	kernel_task.pid        = 0;
	kernel_task.user_space = 0;
	kernel_task.policy     = DEFAUT_POLICY;

	// The kernel has a 1:1 mapping for code and stack, do we need this?
	// Perhaps we need to save the kernel stack before calling user_jump()?
	//kernel_task.stack
	//kernel_task.code 

	// TODO More?
}

// void jump_to_user(){}
