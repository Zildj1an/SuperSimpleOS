#include <basic_xen.h>

volatile static struct pvclock_wall_clock *pvclock_wc;
volatile static struct pvclock_vcpu_time_info *pvclock_ti[1]; /* Assume 1 CPU */

shared_info_t *HYPERVISOR_shared_info;

int detect_xen(void)
{
	uint32_t eax, ebx, ecx, edx;

	/* Check for generic "hypervisor present" bits */
	x86_cpuid(0x0, &eax, &ebx, &ecx, &edx);

	if (eax >= 0x1){

		x86_cpuid(0x1, &eax, &ebx, &ecx, &edx);
		
		if (!(ecx && (1 << 31))){
			goto no_hypervisor_detected;
		}
	}
	else goto no_hypervisor_detected;

	/* Check hypervisor vendor signature */
	x86_cpuid(HYP_VENDOR_SIGNATURE, &eax, &ebx, &ecx, &edx);

	if (!(eax > HYP_VENDOR_SIGNATURE)) goto no_hypervisor_detected;

	if (ebx != XEN_NUMS1 ||
		ecx != XEN_NUMS2 ||
		edx != XEN_NUMS3) goto bad_hypervisor_detected;

	return 0;

bad_hypervisor_detected:
	printf(">> Bad Hypervisor (not Xen) detected!\n");

	if (0){
no_hypervisor_detected:
		printf(">> No Hypervisor detected!\n");
	}

	return -1;
}

uint32_t hypervisor_base(void)
{
	uint32_t base, eax, ebx, ecx, edx;

	/* Find the base. */
	for (base = 0x40000000; base < 0x40010000; base += 0x100) {
		x86_cpuid(base, &eax, &ebx, &ecx, &edx);
		if ((eax - base) >= XEN_MIN_LEAVES &&
				ebx == XEN_NUMS1 &&
				ecx == XEN_NUMS2  &&
				edx == XEN_NUMS3) {
			return base;
		}
	}

	return 0;
}

int xen_init_shared(char *_minios_shared_info)
{
	struct xen_add_to_physmap xatp;
	int ret = 0;

	HYPERVISOR_shared_info = (shared_info_t *) _minios_shared_info;
	xatp.domid = DOMID_SELF;
	xatp.idx = 0;
	xatp.space = XENMAPSPACE_shared_info;
	xatp.gpfn = (unsigned long) HYPERVISOR_shared_info >> PAGE_SHIFT;
	
	if (HYPERVISOR_memory_op(XENMEM_add_to_physmap, &xatp)) {
		printf(">> Xen: Cannot get shared info!\n");
		ret = -1;
	}

	return ret;
}

/* Version (major.minor: 16 bits each) */
void print_xen_version(void)
{
	int version, major, minor;

	version = HYPERVISOR_xen_version(0, NULL);

	major = version >> 16;
	minor = version & 0xff;

	printf(">> XEN VERSION: major = %d : minor = %d\n",major,minor);
}

static inline uint64_t rdtsc(void)                                                                        
{                                                                                  
        uint64_t val;                                                              
        unsigned long eax, edx;                                                    
                                                                                   
        asm __volatile__("rdtsc" : "=a"(eax), "=d"(edx));                      
        val = ((uint64_t)edx<<32)|(eax);                                           
        return val;                                                                
}   

/* See rumprun-smp OS 
 * Calculate prod = (a * b) where a is (64.0) fixed point and b is (0.32) fixed
 * point.  The intermediate product is (64.32) fixed point, discarding the
 * fractional bits leaves us with a (64.0) fixed point result.
 *
 * XXX Document what range of (a, b) is safe from overflow in this calculation.
 */
static inline uint64_t
mul64_32(uint64_t a, uint32_t b)
{
	uint64_t prod;
#if defined(__x86_64__)
	/* For x86_64 the computation can be done using 64-bit multiply and
	 * shift. */
	__asm__ (
		"mul %%rdx ; "
		"shrd $32, %%rdx, %%rax"
		: "=a" (prod)
		: "0" (a), "d" ((uint64_t)b)
	);
#elif defined(__i386__)
	/* For i386 we compute the partial products and add them up, discarding
	 * the lower 32 bits of the product in the process. */
	uint32_t h = (uint32_t)(a >> 32);
	uint32_t l = (uint32_t)a;
	uint32_t t1, t2;
	__asm__ (
		"mul  %5       ; "  /* %edx:%eax = (l * b)                    */
		"mov  %4,%%eax ; "  /* %eax = h                               */
		"mov  %%edx,%4 ; "  /* t1 = ((l * b) >> 32)                   */
		"mul  %5       ; "  /* %edx:%eax = (h * b)                    */
		"xor  %5,%5    ; "  /* t2 = 0                                 */
		"add  %4,%%eax ; "  /* %eax = (h * b) + t1 (LSW)              */
		"adc  %5,%%edx ; "  /* %edx = (h * b) + t1 (MSW)              */
		: "=A" (prod), "=r" (t1), "=r" (t2)
		: "a" (l), "1" (h), "2" (b)
	);
#else
#error mul64_32 not supported for target architecture
#endif

	return prod;
}

void init_clock_stuff(void)
{
	pvclock_ti[0] = (struct pvclock_vcpu_time_info *) &HYPERVISOR_shared_info->vcpu_info[0].time;
	pvclock_wc = (struct pvclock_wall_clock *) &HYPERVISOR_shared_info->wc_version;
}

bmk_time_t xen_monotonic_clock(void) /* See rumprun-smp OS */
{
	unsigned long cpu = 0;  /* Assume 1 CPU */
	uint32_t version;
	uint64_t delta, time_now;

	do {
		version = pvclock_ti[cpu]->version;
		__asm__ ("mfence" ::: "memory");
		delta = rdtsc() - pvclock_ti[cpu]->tsc_timestamp;
		if (pvclock_ti[cpu]->tsc_shift < 0){
			delta >>= -pvclock_ti[cpu]->tsc_shift;
		}
		else {
			delta <<= pvclock_ti[cpu]->tsc_shift;
		}
		time_now = mul64_32(delta, pvclock_ti[cpu]->tsc_to_system_mul) +
			pvclock_ti[cpu]->system_time;
		__asm__ ("mfence" ::: "memory");

	} 
	while ((pvclock_ti[cpu]->version & 1) || (pvclock_ti[cpu]->version != version));

	return (bmk_time_t)(time_now + NSEC_PER_SEC - 1)/NSEC_PER_SEC; 
}

bmk_time_t xen_wall_clock(void) /* See rumprun-smp OS */
{
	uint32_t version;
	bmk_time_t wc_boot;

	do {
		version = pvclock_wc->version;
		__asm__ ("mfence" ::: "memory");
		wc_boot = pvclock_wc->sec * NSEC_PER_SEC;
		wc_boot += pvclock_wc->nsec;
		__asm__ ("mfence" ::: "memory");
	} 
	while ((pvclock_wc->version & 1) || (pvclock_wc->version != version));

	return (wc_boot + NSEC_PER_SEC - 1)/NSEC_PER_SEC; 
}