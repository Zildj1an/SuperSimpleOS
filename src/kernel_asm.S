/*
 * kernel_asm.S - kernel assembly routines (Assignment 2, ECE 6504)
 * Copyright 2021 Ruslan Nikolaev <rnikola@vt.edu>
 */

.global syscall_entry, user_jump, pagefault_trap, default_trap, timer_apic, divzero_trap, timer_entry_ptr, context_switch , init_trampoline
.code64

.align 64
.type syscall_entry,%function
syscall_entry:
	/* Set up the kernel stack */
	movq %rsp, user_stack(%rip)
	movq kernel_stack(%rip), %rsp

	/* Save SYSCALL/SYSRET registers */
	pushq %rcx
	pushq %r11

	/* Save other registers */
	pushq %rdi
	pushq %rsi
	pushq %rdx
	pushq %r8
	pushq %r9
	pushq %r10

	/* Call the internal handler */
	movq %r10, %rcx			/* r10 is used in lieu of rcx for syscalls */
	call do_syscall_entry

	/* Restore other registers */
	popq %r10
	popq %r9
	popq %r8
	popq %rdx
	popq %rsi
	popq %rdi

	/* Restore SYSCALL/SYSRET registers */
	popq %r11
	popq %rcx

	movq user_stack(%rip), %rsp
	sysretq	/* Return the value */

.align 64
.type user_jump,%function
user_jump:
	pushfq 
	pop %r11 /* Will be used for RFLAGS by sysret */
	movq %rdi, %rcx /* Will be used for the instruction pointer by sysret */
	movq user_stack(%rip), %rsp
	sysretq

/*
 * These macros save and restore volatile registers
 * (assuming you do not modify any other registers except
 * as used by the C calling convention).
 */
#define SAVE_REGS					 \
	pushq %rax						;\
	pushq %rcx						;\
	pushq %rdx						;\
	pushq %rdi						;\
	pushq %rsi						;\
	pushq %r8						;\
	pushq %r9						;\
	pushq %r10						;\
	pushq %r11

#define RESTORE_REGS				 \
	popq %r11						;\
	popq %r10						;\
	popq %r9						;\
	popq %r8						;\
	popq %rsi						;\
	popq %rdi						;\
	popq %rdx						;\
	popq %rcx						;\
	popq %rax

/* Part 2 */
.align 64
.type default_trap,%function
default_trap:
	cli
	SAVE_REGS
	/* TODO: Do something here */
	movq %cr2, %rdi
	movq %rsp, %rsi
	movq (%rip), %rdx
	call handler
	RESTORE_REGS
	hlt

.align 64
.type divzero_trap,%function
divzero_trap:
	cli
	SAVE_REGS
	/* TODO: Do something here */
	movq %cr2, %rdi
	movq %rsp, %rsi
	call divzero_handler
	RESTORE_REGS
	hlt

.align 64
.type pagefault_trap,%function
pagefault_trap:
	cli
	SAVE_REGS
	/* TODO: Do something here */
	movq %cr2, %rdi
	movq %rsp, %rsi
	call pg_handler
	RESTORE_REGS
	sti
	addq $8, %rsp	/* skip the page-fault error code */
	iretq

/* Part 3 */
.align 64
.type timer_apic,%function
timer_apic:
	cli
	SAVE_REGS
	/* TODO: Do something here */
	call apic_timer_handler
	RESTORE_REGS
	sti
	iretq

/*
 * %rdi: prev task -> first argument
 * %rsi: next task -> second argument
 */
.align 64
.type context_switch,%function
context_switch:
		/* 
		 * [1] Save all the register to the current thread stack            
	     */
	pushq %rbp
        pushq %rbx
        pushq %r12
        pushq %r13
        pushq %r14
        pushq %r15
		/* 
		 * [2] Save current SP into *prev_task.stack, since the parenthesis are 	
		 *     derreferencing it is *(rdi). Then load *next_task.stack into 
		 *     the SP. 
		 */
        movq %rsp, (%rdi)               /* save ESP */
        movq (%rsi), %rsp               /* restore ESP */
	/* [3] Save the position-independent sched_resume() address into the 
         *     next field of the task_struct (the instruction pointer field).
         *     Remember that 8(*rdi) == (*(%rdi) + 8)
         */
	leaq sched_resume(%rip), %rax
	movq %rax , 8(%rdi) 
	/* [4] We cannot directly update the instruction pointer so we put 
		 *     in the stack the address we want to go to, so RET fetches it.
		 */ 
	pushq 8(%rsi)                   /* restore EIP */
        ret
sched_resume:
        popq %r15
        popq %r14
        popq %r13
        popq %r12
        popq %rbx
        popq %rbp
        ret

/*
	Method to initialize the stack when the
	task is initialized into our array. 
*/
.align 64
.type init_trampoline,%function
init_trampoline:
	popq %rdi
        popq %rbx
        pushq $0                        /* correct stack alignment for SSE */
        pushq $0
        xorq %rbp,%rbp	
        call *%rbx
        call task_exit


/*	movq 8(%rsi) , %rcx	
        movq (%rip),%r8	
	pop %rdx
	push %rdx
	call debug_print  	debug code ends */
