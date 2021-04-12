/*
 * `````````````````````````````````````````````````````````````````````````````
 * Bootloader by Carlos Bilbao <bilbao@vt.edu>
 * based on a template by Dr. Ruslan Nikolaev <rnikola@vt.edu> 
 *
 * `````````````````````````````````````````````````````````````````````````````
 */
#include <Uefi.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/GraphicsOutput.h>
#include <Guid/FileInfo.h>

#include <page_table.h>
#include <interrupts.h>

typedef struct 
{
	void *page_1;
	void *page_2;

} grant_pages_t;

#define ALIGN_PAGE(size) ((unsigned int)((size + EFI_PAGE_SIZE - 1)/ EFI_PAGE_SIZE))

/* Set this to zero for a non-verbose output (only an error will print) */
#define VERBOSE_BOOT (1)

/* Use GUID names 'gEfi...' that are already declared in Protocol headers */
EFI_GUID gEfiLoadedImageProtocolGuid      = EFI_LOADED_IMAGE_PROTOCOL_GUID;
EFI_GUID gEfiSimpleFileSystemProtocolGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
EFI_GUID gEfiGraphicsOutputProtocolGuid   = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
EFI_GUID gEfiFileInfoGuid                 = EFI_FILE_INFO_ID;

/* Keep these variables global */
static EFI_HANDLE ImageHandle;
static EFI_SYSTEM_TABLE *SystemTable;
static EFI_BOOT_SERVICES *BootServices;
static EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;

/* Clean the console */
static VOID clean_console(void) { 
	ConOut->Reset(ConOut,FALSE); 
}

/* Print error messages */
static VOID printerr(CHAR16 *msg) { 
	ConOut->OutputString(ConOut,msg); 
	BootServices->Stall(5 * 1000000); // 5 seconds
}

#ifdef VERBOSE_BOOT
static VOID print(CHAR16 *msg) { 
	ConOut->OutputString(ConOut,msg); 
	BootServices->Stall(5 * 10000); // 0.05 seconds
}
#else /* No verbose boot */
static VOID print(CHAR16 *msg){}
#endif

typedef enum {RUNTIME_CODE, RUNTIME_DATA, BOOT_DATA} type_mem_t;

/* Note: I set my System Base memory on VirtualBox to 4096 MB 
   You only need to specify num_pages for RUNTIME_DATA, and then don't specify
   size. 
*/
static VOID *Allocate(UINTN size, int prevent_free, int num_pages)
{
	VOID *ptr;
	EFI_STATUS ret;
	EFI_PHYSICAL_ADDRESS physical_buffer;

	switch(prevent_free) 
	{
	case RUNTIME_CODE:
	
		/* See Uefi/UefiMultiPhase.h for descriptions. I make sure not to
		   accidentally reclaim memory for the kernel code. 
		*/
		ret = BootServices->AllocatePool(EfiRuntimeServicesCode, size, &ptr);
	
	break;
	case RUNTIME_DATA:
			
		ret = BootServices->AllocatePages(AllocateAnyPages,
										  EfiRuntimeServicesData, 
										  num_pages, 
										  &physical_buffer);

		/* Physical address to pointer */
		ptr = (VOID *) (UINTN) physical_buffer;
	
	break;
	case BOOT_DATA:
	default:
		ret = BootServices->AllocatePool(EfiBootServicesData, size, &ptr);
	}

	if (EFI_ERROR(ret)){
		switch(ret){
			case EFI_OUT_OF_RESOURCES:
			printerr(L"Cannot allocate memory: The pool couldn't be allocated\r\n");
			break;
			case EFI_INVALID_PARAMETER:
			printerr(L"Cannot allocate memory: PoolType was invalid\r\n");
			break;
		}
		ptr = NULL;
	}

	return ptr;
} 

__attribute__((used)) static VOID FreePool(VOID *buf) 
{
	BootServices->FreePool(buf);
}

static EFI_STATUS OpenKernel(EFI_FILE_PROTOCOL **pvh, EFI_FILE_PROTOCOL **pfh)
{
	EFI_LOADED_IMAGE *li = NULL;
	EFI_FILE_IO_INTERFACE *fio = NULL;
	EFI_FILE_PROTOCOL *vh;
 	EFI_STATUS efi_status, ret = EFI_SUCCESS;

	*pvh = NULL;
	*pfh = NULL;

	efi_status = BootServices->HandleProtocol(ImageHandle,
					&gEfiLoadedImageProtocolGuid, (void **) &li);

	if (EFI_ERROR(efi_status)) {
		printerr(L"Cannot get LoadedImage for BOOTx64.EFI\r\n");
		ret = efi_status;
		goto end_open_kernel;
	}

	efi_status = BootServices->HandleProtocol(li->DeviceHandle,
					&gEfiSimpleFileSystemProtocolGuid, (void **) &fio);

	if (EFI_ERROR(efi_status)) {
		printerr(L"Cannot get fs I/O interface fio\r\n");
		ret = efi_status;
		goto end_open_kernel;
	}

	efi_status = fio->OpenVolume(fio, pvh);

	if (EFI_ERROR(efi_status)) {
		printerr(L"Cannot get the volume handle!\r\n");
		ret = efi_status;
		goto end_open_kernel;
	}

	vh = *pvh;

	efi_status = vh->Open(vh, pfh, L"\\EFI\\BOOT\\KERNEL",EFI_FILE_MODE_READ, 0);

	if (EFI_ERROR(efi_status)) {
		printerr(L"Cannot get the file handle!\r\n");
		ret = efi_status;
		goto end_open_kernel;
	}

end_open_kernel:

	return ret;
}

static EFI_STATUS LoadKernel(EFI_FILE_PROTOCOL *fh, VOID **kbuffer)
{
	EFI_STATUS efi_status, ret = EFI_SUCCESS;
	UINTN read_size = 0, buf_size = 0;
	EFI_FILE_INFO *file_info = NULL;

	efi_status  = fh->GetInfo(fh,&gEfiFileInfoGuid,&buf_size,NULL);

	/* BufferSize will be updated with the size needed for the request.*/
	if (efi_status == EFI_BUFFER_TOO_SMALL) 
	{
		file_info  = Allocate(buf_size,BOOT_DATA,0);
		efi_status = fh->GetInfo(fh,&gEfiFileInfoGuid,&buf_size,file_info);

		if (EFI_ERROR(efi_status)) {
			printerr(L"The kernel info couldn't be retrieved! (Second try)\r\n");
			ret = efi_status;
			goto end_load_kernel;
		}
	}
	else if (EFI_ERROR(efi_status)) {
		printerr(L"The kernel info couldn't be retrieved!\r\n");
		ret = efi_status;
		goto end_load_kernel;
	}

	read_size  = file_info->FileSize;

	/* Page aligned */
	*kbuffer   = Allocate(0,RUNTIME_DATA,ALIGN_PAGE(read_size));

	if (!kbuffer){
		printerr(L"Cannot allocate memory for the kernel file!\r\n");
		ret = efi_status;
		goto end_load_kernel;
	}

	read_size = ALIGN_PAGE(file_info->FileSize) * EFI_PAGE_SIZE;
	efi_status = fh->Read(fh,&read_size,*kbuffer); 

	if (EFI_ERROR(efi_status) || read_size != file_info->FileSize) {
		printerr(L"Cannot read the kernel file!\r\n");
		ret = efi_status;
		goto end_load_kernel;
	}

	FreePool(file_info);

end_load_kernel:

	return ret;
}

static void CloseKernel(EFI_FILE_PROTOCOL *vh, EFI_FILE_PROTOCOL *fh)
{
	vh->Close(vh);
	fh->Close(fh);
}

static EFI_STATUS load_user_app(VOID **ubuffer, 
       				            EFI_FILE_PROTOCOL *vh, 
       				            EFI_FILE_PROTOCOL *fh,
       				            int *user_pages)
{
	EFI_LOADED_IMAGE *li = NULL;
	EFI_FILE_IO_INTERFACE *fio = NULL;
	EFI_STATUS efi_status, ret = EFI_SUCCESS;
	UINTN read_size = 0, buf_size = 0;
	EFI_FILE_INFO *file_info = NULL;

	/* Open user application -------------------------------------------------*/

	efi_status = BootServices->HandleProtocol(ImageHandle,
					&gEfiLoadedImageProtocolGuid, (void **) &li);

	if (EFI_ERROR(efi_status)) {
		printerr(L"Cannot get LoadedImage for USER binary\r\n");
		ret = efi_status;
		goto end_load_user_app;
	}

	efi_status = BootServices->HandleProtocol(li->DeviceHandle,
					&gEfiSimpleFileSystemProtocolGuid, (void **) &fio);

	if (EFI_ERROR(efi_status)) {
		printerr(L"Cannot get fs I/O interface fio\r\n");
		ret = efi_status;
		goto end_load_user_app;
	}

	efi_status = fio->OpenVolume(fio, &vh);

	if (EFI_ERROR(efi_status)) {
		printerr(L"Cannot get the volume handle!\r\n");
		ret = efi_status;
		goto end_load_user_app;
	}

	efi_status = vh->Open(vh, &fh, L"\\EFI\\BOOT\\USER",EFI_FILE_MODE_READ, 0);

	if (EFI_ERROR(efi_status)) {
		printerr(L"Cannot get the file handle!\r\n");
		ret = efi_status;
		goto end_load_user_app;
	}

	/* Load user application -------------------------------------------------*/

	efi_status  = fh->GetInfo(fh,&gEfiFileInfoGuid,&buf_size,NULL); 

	/* BufferSize will be updated with the size needed for the request.*/
	if (efi_status == EFI_BUFFER_TOO_SMALL) 
	{
		file_info  = Allocate(buf_size,BOOT_DATA,0);
		efi_status = fh->GetInfo(fh,&gEfiFileInfoGuid,&buf_size,file_info);

		if (EFI_ERROR(efi_status)) {
			printerr(L"The user app info couldn't be retrieved! (2nd try)\r\n");
			ret = efi_status;
			goto end_load_user_app;
		}
	}
	else if (EFI_ERROR(efi_status)) {
		printerr(L"The user app info couldn't be retrieved!\r\n");
		ret = efi_status;
		goto end_load_user_app;
	}

	read_size  = file_info->FileSize;
	*user_pages = ALIGN_PAGE(read_size);
	*ubuffer   = Allocate(0,RUNTIME_DATA,ALIGN_PAGE(read_size));

	if (!ubuffer){
		printerr(L"Cannot allocate memory for the user file!\r\n");
		ret = efi_status;
		goto end_load_user_app;
	}

	efi_status = fh->Read(fh,&read_size,*ubuffer);

	if (EFI_ERROR(efi_status) || read_size != file_info->FileSize) {
		printerr(L"Cannot read the user file!\r\n");
		ret = efi_status;
		goto end_load_user_app;
	}

	FreePool(file_info);

	/* Close user app file */
	vh->Close(vh);
	fh->Close(fh);

end_load_user_app:

	return ret;
}

static UINT32 *SetGraphicsMode(UINT32 width, UINT32 height)
{
	EFI_GRAPHICS_OUTPUT_PROTOCOL *graphics;
	EFI_STATUS efi_status;
	EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
	UINTN size;
	UINT32 mode, *ret = NULL, max_node;

	efi_status = BootServices->LocateProtocol(&gEfiGraphicsOutputProtocolGuid,
                    NULL, (VOID **) &graphics);

	if (EFI_ERROR(efi_status)) {
		printerr(L"Cannot get the GOP handle!\r\n");
		goto end_set_graphics;
	}

	if (!graphics->Mode || !graphics->Mode->MaxMode) {
		printerr(L"Incorrect GOP mode information!\r\n");
		goto end_set_graphics;
	}

	max_node = graphics->Mode->MaxMode;
	info = graphics->Mode->Info;

	for (mode = 0; mode < max_node; mode++) {

		efi_status = graphics->QueryMode(graphics,mode,&size,&info);

		if (EFI_ERROR(efi_status)) {
			printerr(L"Couldn't query a graphics mode!\r\n");
			goto end_set_graphics;
		}

		/* Locate BlueGreenRedReserved, aka BGRA (8-bit per color) and
		   resolution 800x600
		*/
		if (info->PixelFormat != PixelBlueGreenRedReserved8BitPerColor ||
			info->HorizontalResolution != width ||
			info->VerticalResolution   != height){
			continue;
		}	

		/* Activate (set) this graphics mode, also clears screen */
		efi_status = graphics->SetMode(graphics,mode);

		if (EFI_ERROR(efi_status)) {
			printerr(L"Couldn't set the graphics mode!\r\n");
			goto end_set_graphics;
		}

		clean_console();

		/* Return the frame buffer base address */
		ret = (UINT32 *) graphics->Mode->FrameBufferBase;

		goto end_set_graphics;
	}

	printerr(L"The graphics mode could not be found!\n");

end_set_graphics:

	return ret;
}

/* Exit Boot Services before moving to the kernel. 
   We can't use BootServices after calling this function !!!
*/
static EFI_STATUS exit_boot_services(void)
{
	EFI_STATUS ret = EFI_SUCCESS;
	EFI_MEMORY_DESCRIPTOR *memory_map = NULL;
	UINTN mmap_size = sizeof(memory_map), des_size = 0, map_key = 0;
    UINT32 des_version;

try_again_get_mem:

    /* Return the current memory map. */
	ret = BootServices->GetMemoryMap(&mmap_size,
									 memory_map,
									 &map_key,
									 &des_size,&des_version);

	if (EFI_ERROR(ret)) {
		if (ret == EFI_BUFFER_TOO_SMALL) {
				if (!memory_map){
					memory_map = Allocate(mmap_size + des_size,
										  BOOT_DATA,
										  0);
				}
				goto try_again_get_mem;
		}
		printerr(L"Couldn't get the memory map!\r\n");
		goto end_exit_boot;
	}

	/* Do NOTHING in between GetMemoryMap and ExitBootServices! */

	/* It's advisable to iterate here for real machines. Not needed for VBox */
	ret = BootServices->ExitBootServices(ImageHandle,map_key);

	if (EFI_ERROR(ret)) {
		if (ret == EFI_INVALID_PARAMETER) {
			printerr(L"Invalid parameter to exit boot services!");
			goto try_again_get_mem;
		}
		printerr(L"Could not exit boot services!");	
		goto end_exit_boot;
	}

end_exit_boot:
	
	return ret;
}

/* Use System V ABI rather than EFI/Microsoft ABI. */
typedef void (*kernel_entry_t)
             (void  *,                   /* Kernel stack      		         */
              void  *,                   /* Kernel buffer                    */
              page_tables_t *,	         /* kernel page table 			     */
			  ucode_info_t *,            /* User code, stack, pages info,TLS */
              tss_info_t *,              /* TSS segment and stack            */
              grant_pages_t *grant_pages /* Xen Grant pages                  */
              ) __attribute__((sysv_abi));

EFI_STATUS EFIAPI
efi_main(EFI_HANDLE imageHandle, EFI_SYSTEM_TABLE *systemTable)
{
	EFI_FILE_PROTOCOL *vh, *fh;
	EFI_STATUS efi_status, ret = EFI_SUCCESS;
	kernel_entry_t kfunc;
	page_tables_t *kinfo_pages;
	void *uinfo_pages, *upage_fault, *tls;
	UINT32 *fb;
	int pages_user;
	ucode_info_t info_ucode;
	VOID  *kbuffer = NULL, *ubuffer = NULL, *ustack = NULL, *kstack = NULL;
	tss_segment_t *tss;
	uint64_t *tss_stack;
	tss_info_t tss_info;
	grant_pages_t grant_pages;
	VOID *page_1, *page_2;

	ImageHandle  = imageHandle;
	SystemTable  = systemTable;
	BootServices = systemTable->BootServices;
	ConOut       = systemTable->ConOut;

	clean_console();

	/*  1. Open the kernel -------------------------------------------------- */

	print(L"[1/6] Opening kernel...\r\n");

	efi_status = OpenKernel(&vh, &fh);

	if (EFI_ERROR(efi_status)) {
		ret = efi_status;
		goto end_efi_main;
	}

	/*  2. Load the kernel -------------------------------------------------- */

	print(L"[2/6] Loading kernel...\r\n");

	efi_status = LoadKernel(fh,&kbuffer);

	if (EFI_ERROR(efi_status)) {
		ret = efi_status;
		goto end_efi_main;
	}

	CloseKernel(vh, fh);

	/*  3. Allocate kernel paging memory ------------------------------------ */

	print(L"[3/6] Allocating paging memory...\r\n");

	kinfo_pages = Allocate(0,RUNTIME_DATA,2054);
	uinfo_pages = Allocate(0,RUNTIME_DATA,4); 

	if (!kinfo_pages || !uinfo_pages) {
		ret = efi_status;
		goto end_efi_main;
	}

	/* 4. Prepare user and kernel stack ------------------------------------- */

	print(L"[4/6] Preparing user and kernel stack...\r\n");

	ustack = Allocate(0,RUNTIME_DATA,1);
	kstack = Allocate(0,RUNTIME_DATA,1);

	if (!ustack || !kstack) {
		ret = efi_status;
		goto end_efi_main;
	}

	/* 5. Open and load user application ------------------------------------ */

	print(L"[5/6] Open and load user app...\r\n");

	efi_status = load_user_app(&ubuffer,vh, fh, &pages_user);

	if (EFI_ERROR(efi_status)) {
		ret = efi_status;
		goto end_efi_main;
	}

	/* We will also need a TSS stack to manage interruptions ocurred while in 
	   the user-space.
	*/
	tss = Allocate(0,RUNTIME_DATA,1);
	tss_stack = Allocate(0,RUNTIME_DATA,1);

	tss_info.tss       = tss;
	tss_info.tss_stack = tss_stack + 0x1000;

	/* We also need an extra page for Lazy Allocation (page-fault exceptions) */
	upage_fault = Allocate(0,RUNTIME_DATA,1);

	/* We also need a Thread Local Storage to support __thread <variable>     */
	tls = Allocate(0,RUNTIME_DATA,1);

	/* Xen grant pages */
	page_1 = Allocate(0,RUNTIME_DATA,1);
	page_2 = Allocate(0,RUNTIME_DATA,1);

	/* 6. Set the graphic buffer ------------------------------------------- */

	print(L"[6/6] Setting graphic mode...\r\n");

	fb = SetGraphicsMode(800, 600);

	if (!fb) {
		ret = EFI_INVALID_PARAMETER;
		goto end_efi_main;
	}

	/* 7. Exit boot services ------------------------------------------------ */

	efi_status = exit_boot_services();

	if (EFI_ERROR(efi_status)) {
		ret = efi_status;
		goto end_efi_main;
	}

	/* 8. Cast the entry point of our kernel  --------------------------------*/

	kfunc  = (kernel_entry_t) kbuffer;
	kstack = kstack + 0x1000;

	info_ucode.ubuffer        = ubuffer;
	info_ucode.num_pages_code = pages_user;
	info_ucode.ustack_phys    = ustack;
	info_ucode.upage_fault    = upage_fault;
	info_ucode.tls            = tls;
	info_ucode.uinfo_pages    = uinfo_pages;

	grant_pages.page_1        = page_1;
	grant_pages.page_2        = page_2;

	kfunc(kstack, fb, kinfo_pages, &info_ucode, &tss_info, &grant_pages);

end_efi_main:

	return ret;
}
