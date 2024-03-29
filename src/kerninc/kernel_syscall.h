#pragma once

/* This enum is duplicated at syscall.h for the user */
typedef enum {
	
	/* Implemented system calls */
	PRINTF_SYSCALL = 0,
 	IMPLEMENTED_SYSCALLS,

 	/* Error messages */
 	EINVAL,                /* Invalid argument */
 	NUM_ERROR_MESSAGES

} system_calls_t;

#ifdef __cplusplus
extern "C" {
#endif

extern void *kernel_stack; /* the kernel stack, must be allocated */
extern void *user_stack;   /* the user stack, must be allocated */

/* an initial jump to user mode, addr is a VIRTUAL address of user's _start */
void user_jump(void * addr); 

/*
 * A pointer to syscall_entry(),
 * do not expose syscall_entry() directly to avoid linker
 * issues/bugs due to the use of a 'pure binary';
 *
 * this pointer is already initialized to syscall_entry() in kernel_entry.S
 */
extern void *syscall_entry_ptr;

/* the system call handler */
long do_syscall_entry(long n, long a1, long a2, long a3, long a4, long a5);

/* initialize system calls */
void syscall_init(void);

/* Initialize user stack pointer */
void init_user_stack(void*ustack);

#ifdef __cplusplus
}
#endif
