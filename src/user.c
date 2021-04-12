/* 
 * `````````````````````````````````````````````````````````````````````````````
 * User program by Carlos Bilbao <bilbao@vt.edu> based on an application template 
 * (Assignment 2, ECE 6504) with Copyright 2021 Ruslan Nikolaev <rnikola@vt.edu>
 *
 * `````````````````````````````````````````````````````````````````````````````
*/

#include <syscall.h>

/* Array to test the TLS (Thread Local Storage) */
__thread int a[100];

void test_tss(void)
{
	int i = 0;

	for (; i < 100; ++i) {
		a[i] = i;
	}
}

int force_pagefault(system_calls_t syscall_num)
{
	int ret;
	const char *exception = "The page fault was correctly handled!\n";

	/* Force a page-fault exception to test the IDT table */
	*((char *)(0xffffffffffffffff - 0x1000)) = 0;

	ret = __syscall1((long)syscall_num,(long)exception);

	return ret;
}

void user_start(void)
{
	int ret  __attribute__((unused));
	const char *msg1      = "This is a system call from user space!\n";
	const char *msg2      = "This is another system call from user space!\n";

	system_calls_t syscall_num = PRINTF_SYSCALL;

	ret = __syscall1((long)syscall_num,(long)msg1);
	ret = __syscall1((long)syscall_num,(long)msg2);

	if (ret != 0){
		/* If the printing system call failed there is not much point in trying
		   to print an error message here... */
		return;
	}

	// ret = force_pagefault(syscall_num); // UNCOMMENT FOR PAGE FAULT HANDLING

	test_tss();

	/* Never exit */
	while (1) {};
}
