/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"

#include "bitmap.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in(struct page *page, void *kva);
static bool anon_swap_out(struct page *page);
static void anon_destroy(struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void vm_anon_init(void)
{
	/* TODO: Set up the swap_disk. */

	/* Edited Code - Jinhyen Kim
	   We retrieve a disk with disk_get, and we initialize the memFrameTable
		  by creating a new bitmap. */

	swap_disk = disk_get(1, 1);
	memFrameTable = bitmap_create(disk_size(swap_disk) / (PGSIZE / DISK_SECTOR_SIZE));

	/* Edited Code - Jinhyen Kim (Project 3 - Swap In/Out) */
}

/* Initialize the file mapping */
bool anon_initializer(struct page *page, enum vm_type type, void *kva)
{
	/* Set up the handler */
	page->operations = &anon_ops;

	/* Edited Code - Jinhyen Kim
	   We use memset() to set a page's worth of bits as 0. */

	memset(kva, 0, PGSIZE);
	return true;

	/* Edited Code - Jinhyen Kim (Project 3 - Swap In/Out) */
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in(struct page *page, void *kva)
{
	struct uninit_page *anonPage = &((*(page)).anon);

	/* Edited Code - Jinhyen Kim
	   We use bitmap_test() to see whether swapping in is valid or not.
	   If it is, we iterate through the pages and read it.
	   Once this is done, we no longer allow any further swap ins by
		  marking the bitmap and setting the pageIndex to an invalid number. */

	bool contFlag = bitmap_test(memFrameTable, (*(anonPage)).pageIndex);

	// printf("anon_swap_in\n"); // debug edit(+ printf("anon_swap_in\n");)

	if (contFlag)
	{
		for (int i = 0; i < (PGSIZE / DISK_SECTOR_SIZE); ++i)
		{
			disk_read(swap_disk, ((*(anonPage)).pageIndex) * (PGSIZE / DISK_SECTOR_SIZE) + i, kva + DISK_SECTOR_SIZE * i);
		}

		bitmap_set(memFrameTable, (*(anonPage)).pageIndex, false);
		(*(anonPage)).pageIndex = -1;
	}

	return contFlag;

	/* Edited Code - Jinhyen Kim (Project 3 - Swap In/Out) */
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out(struct page *page)
{
	struct uninit_page *anonPage = &((*(page)).anon);

	/* Edited Code - Jinhyen Kim
	   We use bitmap_scan() to find a page that can be swapped out.
	   If successful, we iterate through the pages and write it.
	   Once this is done, we allow swap ins and clear the page to
		  complete the Swap Out process. */

	size_t pageNumber = bitmap_scan(memFrameTable, 0, 1, false);

	// printf("anon_swap_out\n"); // debug edit(+ printf("anon_swap_out\n");)

	bool contFlag = !(pageNumber == BITMAP_ERROR);

	if (contFlag)
	{

		for (int i = 0; i < (PGSIZE / DISK_SECTOR_SIZE); ++i)
		{
			disk_write(swap_disk, pageNumber * (PGSIZE / DISK_SECTOR_SIZE) + i, page->frame->kva + DISK_SECTOR_SIZE * i);
		}

		bitmap_set(memFrameTable, pageNumber, true);
		pml4_clear_page(page->thread->pml4, page->va);

		(*(anonPage)).pageIndex = pageNumber;
	}
	return contFlag;

	/* Edited Code - Jinhyen Kim (Project 3 - Swap In/Out) */
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy(struct page *page)
{
	struct uninit_page *anonPage = &((*(page)).anon);

	/* Edited Code - Jinhyen Kim
	   When an anonymous page is destroyed, the information needed for
		  lazy loading is no longer needed.
	   So, we free the memory space.*/

	struct binInfoLoader *info = (struct binInfoLoader *)(anonPage->aux);

	free(info);

	/* Edited Code - Jinhyen Kim (Project 3 - Swap In/Out) */
}
