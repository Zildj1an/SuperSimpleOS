/*
 * `````````````````````````````````````````````````````````````````````````````
 * Functions to handle exceptions registered in the IDT.
 * author Carlos Bilbao <bilbao@vt.edu>
 *
 * `````````````````````````````````````````````````````````````````````````````
*/
#include <fb.h>
#include <printf.h>
#include <kernel_syscall.h>
#include <interrupts.h>
#include <page_table.h>
#include <apic.h>
#include <scheduler.h>

/* APIC timer interrupt handler (timer IRQ) */
void apic_interrupt(void)
{
	static unsigned int count;
	printf("Timer!\n");
	count++;
	if(!(count%2))
	{
		scheduler();
	}
	
    /* Set APIC init counter to 4194304 */
    x86_lapic_write(X86_LAPIC_TIMER_INIT, 0x400000);

	/* ACK interrupts on PIC */
	x86_lapic_write(X86_LAPIC_TIMER_DIVIDE,0x6);

	/* Acknowledge end of interrupt */
	x86_lapic_write(X86_LAPIC_EOI,0);
}

/* Lazy Allocation
   We assume that the faulty direction is in the last page table entry.
*/
void pagefault_trap_handler(int rsp)
{	
	struct page_pte   *pagetable_entries;
	struct page_pml4e *pml4e;

	printf("\n--- [A page fault has occurred!] -- \n");

	pagetable_entries = user_info_pages;
	pml4e = (struct page_pml4e *) (pagetable_entries + 3 * PAGE_ENTRIES);

	/* Use page allocated at boot */

	pagetable_entries[PAGE_ENTRIES-1].present      = 1; 
	pagetable_entries[PAGE_ENTRIES-1].page_address = ((u64)lazy_page / 0x1000);

	asm __volatile__ ("mov %0, %%cr3" 
				      : /* no output registers */
    			      : "r"(pml4e)
				      : "memory"); 
}

/* Bad news for the user-space developer */
void default_trap_msg(int rsp)
{
	printf("\n--- [An exception has occurred!] -- \n");
	printf("| rsp = %p should be close to the tss_stack printed above.\n",rsp);
	printf("--- [ End of exception ] -- \n");
}
