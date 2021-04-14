/*
 * `````````````````````````````````````````````````````````````````````````````
 * Kernel by Carlos Bilbao <bilbao@vt.edu>
 * based on a template by Dr. Ruslan Nikolaev <rnikola@vt.edu>
 *
 * `````````````````````````````````````````````````````````````````````````````
*/

#include <fb.h>
#include <printf.h>
#include <kernel_syscall.h>
#include <interrupts.h>
#include <apic.h>
#include <flag.h>
#include <page_table.h>
#include <tls.h>
#include <msr.h>
#include <cpuid.h>
#include <basic_xen.h>
#include <hypercall.h>
#include <gnttab.h>
#include <scheduler.h>

/* Detect the Xen hypervisor, initialize hypercalls and map the shared info data
   structure.
*/
int init_xen(void)
{
	int ret, xen_init = 0;
	uint32_t xen_base, eax, ebx, ecx, edx;
	uint64_t page = (unsigned long) _minios_hypercall_page;

	/* Detect the Xen hypervisor */
	ret = detect_xen();

	if (ret != 0) {
			printf(">> Detection of Xen failed!\n");
			goto end_init_xen;
	}

	printf(">> Xen hypervisor detected!\n");

	xen_base = hypervisor_base();

	if (!xen_base){
		printf(">> Xen base failed!\n");
		goto end_init_xen;
	}

	x86_cpuid(xen_base + 2, &eax, &ebx, &ecx, &edx);
	
	if (eax != 1){
		printf(">> Xen base failed!\n");
		goto end_init_xen;
	}

	/* Initialize hypercalls page. We dont _really_ write to the real MSR, 
	   since this is intercepted and emulated by Xen.
	 */	
	asm __volatile__("wrmsr" ::
					 "c" (ebx),
					 "a" ((uint32_t)(page)),
					 "d" ((uint32_t)(page >> 32))
	);

	/* Our first hypercall: Map the shared info data structure.*/
	ret = xen_init_shared(_minios_shared_info); 

	if (ret != 0){
		printf(">> Xen hypercall failed!\n");
		goto end_init_xen;
	}

	/* Hypercall to obtain Xen version (Print major and minor nums) */
	print_xen_version(); 

	if (0) {
end_init_xen:
		xen_init = -1;
	}
		 
	return xen_init;
}

/* Implement and test the Xen PV clock (monotonic and wall) */
void xen_pv_clock(void)
{
	bmk_time_t wall, monotonic, ret;

	init_clock_stuff();

	monotonic = xen_monotonic_clock(); /* Keeps increasing       */
	wall      = xen_wall_clock();      /* Specific time and date */

	/* Wait one sec... (Testing purposes) */
	do {

		ret = xen_monotonic_clock(); 

	} while (ret < monotonic + 1);

	printf(">> Monotonic clock (secs): Initial %lld Now %lld\n",ret,monotonic);
	printf(">> Wall clock (secs): Initial %lld Now %lld\n",monotonic + wall,ret + wall);
}

/* Shared memory between two Xen guest domains (Grant tables) One side creates a
   grant reference, the other side maps this reference to its address space
*/ 
void shared_memory_xen(grant_pages_t *grant_pages)
{
	char *page_2 = grant_pages->page_2;
	int rc, i;
	struct gnttab_map_grant_ref op;
	grant_ref_t ref = GRANT_REFERENCE;

	unsigned long frame = ((unsigned long) grant_pages->page_2 >> 12);
	void *host_addr = grant_pages->page_2; 

	if (I_SHARE_DATA) /* Share message */
	{
		gnttab_table = grant_pages->page_1;
		
		init_gnttab();

		page_2[0] = 'H';
		page_2[1] = 'e';
		page_2[2] = 'y';
		page_2[3] = '\0';

		printf("[%d] Writting message 'Hey' to domid %d via frame %d (page %p)\n",
			xen_monotonic_clock(),OTHER_SIDE_DOMID, frame, grant_pages->page_2);

		ref = gnttab_grant_access(OTHER_SIDE_DOMID,frame,0);

		printf("[%d] Shared with grant reference %d\n",xen_monotonic_clock(),ref);
	
		if (ref != GRANT_REFERENCE){
			printf("[%d] You must update GRANT_REFERENCE!!\n",xen_monotonic_clock());
		}
	}
	else  /* Receive message */
	{
	    op.ref       = (grant_ref_t) ref;
	    op.dom       = (domid_t) OTHER_SIDE_DOMID;
	    op.host_addr = (uint64_t) host_addr;
	    op.flags     = GNTMAP_host_map;
	  
	    rc = HYPERVISOR_grant_table_op(GNTTABOP_map_grant_ref, &op, 1);

	    if (rc != 0 || op.status != GNTST_okay) {
	        printf("[%d] GNTTABOP_map_grant_ref failed (returns %d)\n",
	        	xen_monotonic_clock(), rc);
	    }

	    printf("[%d] Message read from domid %d (on page %p):\n",
	    	xen_monotonic_clock(),OTHER_SIDE_DOMID, host_addr);

	    for (i = 0; i < MAX_GNT_MSG && ((char*)host_addr)[i] != '\0'; ++i){
	    	printf("%c",((char*)host_addr)[i]);
	    }

	    printf("\n");
	} 
}

void init_tls(void *tls_va_addr)
{
	struct tls_block *tls;

	tls = (struct tls_block*) tls_va_addr;
	tls->myself = tls;

	wrmsr(MSR_FS_BASE, (uint64_t) tls->myself);
}

/* Initialize the local APIC (or x2APIC) controller for periodic mode */
void init_apic(void)
{
	printf(">> APIC driver: ");
	x86_lapic_enable();

	/* divider 128=0x6, 64=0x5, 32=0x4,16=0x3 */
	x86_lapic_write(X86_LAPIC_TIMER_DIVIDE,0x6);
	
    /* Set APIC init counter to 4194304 */
    x86_lapic_write(X86_LAPIC_TIMER_INIT, 0x400000);

    /* Set vector entry 40 of IDT */
    x86_lapic_write(X86_LAPIC_TIMER, 0x28);
}

/* Interrupt Descriptor Table (IDT) see exceptions.c */
void init_idt(void *upage_fault)
{
	idt_pointer_t idtp;
	int i;

	/* Initialize the DT before loading it */

	for (i = 0; i < 256; ++i){
		__builtin_memset(&idt[i],0,sizeof(struct instruction_descriptor));
	}

	/* The first 32 entries correspond to CPU exceptions and must be initialized
	   to the default CPU exception handler (default_trap)
	*/
	for (i = 0; i < 32; ++i)
	{
		idt[i].hioffset = (unsigned long) default_trap_ptr >> 16;
		idt[i].looffset = (unsigned long) default_trap_ptr & 0xffff;
		idt[i].selector = 0x8; 
		idt[i].type     = 14;
		idt[i].present  = 1;  
	}

	/* Special handler for page fault */
	idt[0xe].hioffset = (unsigned long) pagefault_trap_ptr >> 16;
	idt[0xe].looffset = (unsigned long) pagefault_trap_ptr & 0xffff;

	/* Special handler for IRQ timer (40) */ 
	idt[0x28].hioffset = (unsigned long) apic_interrupt_ptr >> 16;
	idt[0x28].looffset = (unsigned long) apic_interrupt_ptr & 0xffff;
	idt[0x28].selector = 0x8; 
	idt[0x28].type     = 14;
	idt[0x28].present  = 1;  

	lazy_page = upage_fault;
	
	idtp.limit = sizeof(idt) - 1;
	idtp.base  = (uintptr_t) (void*) idt;

	load_idt(&idtp);
}

/* Init Task State Segment (TSS) */
void init_task_state_segment(tss_info_t *tss_info)
{
	tss_segment_t *tss  = tss_info->tss;
	uint64_t *tss_stack = tss_info->tss_stack;

	/* Initialize the TSS segment before loading it */
	
	__builtin_memset(tss,0,sizeof(tss_segment_t));

	tss->iopb_base = sizeof(tss_segment_t);
	tss->rsp[0]    = (uint64_t) tss_stack;
	tss_stack_gbl  = tss_stack;

	/* TSS two pages stack and tss */
	load_tss_segment(0x28,tss);
}

/* Transfer control to a user program (userspace) */
void jump2user(page_tables_t *kinfo_pages, ucode_info_t *ucode_info)
{
	void *ucode = NULL, *ustack = NULL, *tls_addr= NULL, *uinfo_pages;
	int ret, safe_num = 0xef;

	uinfo_pages = ucode_info->uinfo_pages;

	/* The userspace app needs its own page table: Allocate there the code, 
	   stack and TLS. This function returns the virtual address for stack, code 
	   and TLS page.
	*/
	ret = set_upage_table(kinfo_pages, uinfo_pages, 
					     ucode_info, &ustack, &ucode, &tls_addr);

	/* Map a TLS for the user code -------------------------------------------*/

	init_tls(tls_addr);

	user_info_pages = uinfo_pages;

	if (ret != 0){
		printf(">> Setting user page table failed...\n");
		return;
	}

	/* Let's make sure the stack address is properly set */
	((int *) (ucode_info->ustack_phys))[0] = safe_num;

	if (((int*)(ustack - 4096))[0] != safe_num){
		printf(">> Stack address is wrong! (Safe number failed)\n");
		return;
	}

	/* Update pointer to the user stack */
	init_user_stack(ustack);

	printf(">> kernel_stack = %p user_stack = %p tss_stack = %p\n",
		kernel_stack - 0x1000,user_stack - 0x1000, tss_stack_gbl - 0x1000); 
	
	printf(">> Jumping to user-space code...\n\n");

	jump_to_user(ucode, ustack, user_info_pages);
}

void kernel_start(void  *kstack,
				  void  *framebuffer, 
				  page_tables_t *kinfo_pages,
				  ucode_info_t *ucode_info,
				  tss_info_t *tss_info,
				  grant_pages_t *grant_pages)
{
	/* Don't pass >6 arguments so we only use registers (avoid UEFI stack) ---*/
	int width = CONSOLE_WIDTH, height = CONSOLE_HEIGHT, ret;

	/* Set page Table for the kernel -----------------------------------------*/
	
	set_kpage_table(kinfo_pages); // Maps kernel stack (1:1)

	/* Framebuffer console initialization ------------------------------------*/
	
	fb_init(framebuffer,width,height);

	printf(">> Control transferred to the kernel!\n"); 

	/* Initialize system calls -----------------------------------------------*/

	syscall_init();

	/* Initialize Interrupt Descriptor Table----------------------------------*/

	init_idt(ucode_info->upage_fault); 

	/* Initialize Task State Segment -----------------------------------------*/

	init_task_state_segment(tss_info);

	/* Initialize the local APIC (or x2APIC) controller ----------------------*/
	
#if (_USE_APIC_TIMER)

	init_apic(); 

#endif
	/* Initialize Xen resources ----------------------------------------------*/

#if (_USE_XEN_HYPERVISOR) /* basic_xen.h */

	ret = init_xen();

	if (ret == 0){
		xen_pv_clock();
	}

	/* Shared memory between two Xen guests */
	shared_memory_xen(grant_pages);

#elif

	/* Initialize the multi-tasking ------------------------------------------*/

	init_scheduler(kinfo_pages);

	/* Switch to unpriviledged mode and jump to the user app -----------------*/

	jump2user(kinfo_pages,ucode_info); 

#endif

	/* Never exit! */
	while (1) {};
}
