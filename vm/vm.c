/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/vaddr.h"

/* Edited Code - Jinhyen Kim 
   To store and manage all of the frames, we create a 
      list of frames as well as the list_elem to use 
      for the list. */

struct list frameTable;
struct list_elem *frameElem;

/* Edited Code - Jinhyen Kim (Project 3 - Anonymous Page) */


/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */

	/* Edited Code - Jinhyen Kim 
	   When a virtual machine is initialized, we need to initialize the 
	      frame table as well. */

	list_init(&(frameTable));
	/* list_elem = list_begin(&(frameTable)); */

	/* Edited Code - Jinhyen Kim (Project 3 - Anonymous Page) */

}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */

		/* Edited Code - Jinhyen Kim
		   1. To create a new page, we first need to get memory allocation for
		         the page with malloc(). 
		   2. Once we have the memory space, we define a new page with the
		         given VM type. 
		   3. Depending on the VM type, we use a different initializer. 
		   4. Then, we create a new uninit page by calling uninit_new(). 
		         For the new uninitialized page, we set its "writable"
		         status as given by the function, as well as its 
		         current thread. */

		struct page *newUninitPage = malloc(sizeof(struct page));
		
		bool (*pageInit) (struct page *, enum vm_type);

		if (VM_TYPE(type) == VM_FILE) {
			pageInit = file_backed_initializer;
		}
		else if (VM_TYPE(type) == VM_ANON) {
			pageInit = anon_initializer;
		}

		uninit_new(newUninitPage, upage, init, type, aux, pageInit);

		(*(newUninitPage)).writable = writable;
		(*(newUninitPage)).thread = thread_current();

		/* TODO: Insert the page into the spt. */

		spt_insert_page(spt, newUninitPage);

		return true;

		/* Edited Code - Jinhyen Kim (Project 3 - Anonymous Page) */

	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function. */

	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */

	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
//static void
static bool
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;

	/* TODO: Validate the fault */
	/* TODO: Your code goes here */

	/* Edited Code - Jinhyen Kim
	   Before we declare *page, we need to check for two cases where
	      we should immediately return the function instead.
	   1. If the page address is not user address, it is invalid.
	   2. Alternatively, if not_present is false, then it is
	         also invalid. */

	if (!(is_user_vaddr(addr))) {
		return false;
	}
	else if (!(not_present)) {
		return false;
	}

	/* Now, we can declare *page. */

	struct page *page = spt_find_page(spt, addr);

	/* If we are in user mode, rsp is provided by the interrupt frame.
	   If we are in the kernel mode, rsp is provided by the current 
	      thread's stored user rsp instead. */

	void *rsp = NULL;
	if (user) {
		rsp = (*(f)).rsp;
	}
	else {
		rsp = (*(thread_current())).currentRsp;
	}

	if (page == NULL) {

		/**/
		if ((USER_STACK > addr) && (addr >= (USER_STACK - (1<<20))) && (addr >= rsp - 10)) {

			void *fpage = ((*(thread_current())).stack_bottom) - PGSIZE;

			if (vm_stack_growth (fpage)) {
				page = spt_find_page(spt, fpage);
				return vm_do_claim_page (page);
			} 
		}

		return false;
	}
	else {
		return vm_do_claim_page (page);
	}

	/* Edited Code - Jinhyen Kim (Project 3 - Anonymous Page) */
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */

	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}
