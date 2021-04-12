	.file	"prueba.c"
	.text
	.p2align 4
	.globl	test_tss
	.type	test_tss, @function
test_tss:
.LFB0:
	.cfi_startproc
	endbr64
	movq	%fs:0, %rdx
	xorl	%eax, %eax
	.p2align 4,,10
	.p2align 3
.L2:
	movl	%eax, a@tpoff(%rdx,%rax,4)
	addq	$1, %rax
	cmpq	$100, %rax
	jne	.L2
	ret
	.cfi_endproc
.LFE0:
	.size	test_tss, .-test_tss
	.section	.rodata.str1.8,"aMS",@progbits,1
	.align 8
.LC0:
	.string	"The page fault was correctly handled!\n"
	.text
	.p2align 4
	.globl	force_pagefault
	.type	force_pagefault, @function
force_pagefault:
.LFB1:
	.cfi_startproc
	endbr64
	movb	$0, -4097
	movslq	%edi, %rdi
	leaq	.LC0(%rip), %rsi
	xorl	%eax, %eax
	jmp	__syscall1@PLT
	.cfi_endproc
.LFE1:
	.size	force_pagefault, .-force_pagefault
	.section	.rodata.str1.8
	.align 8
.LC1:
	.string	"This is a system call from user space!\n"
	.align 8
.LC2:
	.string	"This is another system call from user space!\n"
	.text
	.p2align 4
	.globl	user_start
	.type	user_start, @function
user_start:
.LFB2:
	.cfi_startproc
	endbr64
	subq	$8, %rsp
	.cfi_def_cfa_offset 16
	leaq	.LC1(%rip), %rsi
	movl	$1, %edi
	xorl	%eax, %eax
	call	__syscall1@PLT
	leaq	.LC2(%rip), %rsi
	movl	$1, %edi
	xorl	%eax, %eax
	call	__syscall1@PLT
	movq	%fs:0, %rdx
	testl	%eax, %eax
	jne	.L13
	xorl	%eax, %eax
.L7:
	movl	%eax, a@tpoff(%rdx,%rax,4)
	addq	$1, %rax
	cmpq	$100, %rax
	jne	.L7
.L8:
	jmp	.L8
	.p2align 4,,10
	.p2align 3
.L13:
	addq	$8, %rsp
	.cfi_def_cfa_offset 8
	ret
	.cfi_endproc
.LFE2:
	.size	user_start, .-user_start
	.globl	a
	.section	.tbss,"awT",@nobits
	.align 16
	.type	a, @object
	.size	a, 400
a:
	.zero	400
	.ident	"GCC: (Ubuntu 9.3.0-17ubuntu1~20.04) 9.3.0"
	.section	.note.GNU-stack,"",@progbits
	.section	.note.gnu.property,"a"
	.align 8
	.long	 1f - 0f
	.long	 4f - 1f
	.long	 5
0:
	.string	 "GNU"
1:
	.align 8
	.long	 0xc0000002
	.long	 3f - 2f
2:
	.long	 0x3
3:
	.align 8
4:
