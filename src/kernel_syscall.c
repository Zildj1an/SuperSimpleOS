/*
 * `````````````````````````````````````````````````````````````````````````````
 *  System call management.
 *  author Carlos Bilbao, bilbao at vt.edu
 *
 * `````````````````````````````````````````````````````````````````````````````
*/

#include <kernel_syscall.h>
#include <types.h>
#include <printf.h>
#include <msr.h>
#include <printf.h>

void *kernel_stack;      /* Initialized in kernel_entry.S */
void *user_stack = NULL; /* Must be initialized to a user stack region */

/* Point to syscall_entry_ptr(), initialized in kernel_entry.S; 
   use it rather than the function when obtaining its address 
*/
void *syscall_entry_ptr; 

void init_user_stack(void *ustack)
{
	/* ustack is a virtual address */
	user_stack = ustack;
}

long do_syscall_entry(long n, long a1, long a2, long a3, long a4, long a5)
{
	/* Make sure it is valid */
	if (n >= IMPLEMENTED_SYSCALLS){
		printf("That is not a valid syscall number!");
		return -EINVAL; 
	}

	/* Handle the system call */
	switch(n){
		case PRINTF_SYSCALL:
			// TODO In future versions, would be nice to add extra memory security here.
			if (!(const char*)a1) return -EINVAL;
			printf((const char*)a1);
		break;
		default:
			return -EINVAL; 
		break;
	}

	return 0; /* Success */
}

void syscall_init(void)
{
	printf(">> Initializing system calls...\n");

	/* Enable SYSCALL/SYSRET */
	wrmsr(MSR_EFER, rdmsr(MSR_EFER) | 0x1);

	/* GDT descriptors for SYSCALL/SYSRET (USER descriptors are implicit) */
	wrmsr(MSR_STAR,((uint64_t)GDT_KERNEL_DATA << 48)|((uint64_t) GDT_KERNEL_CODE << 32));

	/* Register a system call entry point */
	wrmsr(MSR_LSTAR, (uint64_t)syscall_entry_ptr);

	/* Disable interrupts (IF) while in a syscall */
	wrmsr(MSR_SFMASK, 1U << 9);
}
