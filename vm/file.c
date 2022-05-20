/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "threads/mmu.h"

static bool file_backed_swap_in(struct page *page, void *kva);
static bool file_backed_swap_out(struct page *page);
static void file_backed_destroy(struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void vm_file_init(void)
{
}

/* Initialize the file backed page */
bool file_backed_initializer(struct page *page, enum vm_type type, void *kva)
{
	/*by Jin-Hyuk Jang
	update info on page struct */
	struct uninit_page *uninit = &page->uninit;
	void *aux = uninit->aux;

	/* Set up the handler */
	page->operations = &file_ops;

	memset(uninit, 0, sizeof(struct uninit_page));

	struct binLoadInfo *info = (struct binLoadInfo *)aux;
	struct file_page *file_page = &page->file;
	file_page->file = info->fileInfo;
	file_page->length = info->read_bytesInfo;
	file_page->offset = info->ofsInfo;
	/*by Jin-Hyuk Jang - memory mapped files*/
	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in(struct page *page, void *kva)
{
	struct file_page *file_page UNUSED = &page->file;
	/*by Jin-Hyuk Jang
	First check if page is NULL.
	Then we save file, bytes read, zero bytes, and offset into binLoadInfo structure.
	We read read_bytesInfo bytes of the file into page, and set */
	if (page == NULL)
		return false;

	struct binLoadInfo *info = (struct binLoadInfo *)page->uninit.aux;
	struct file *fileInfo = info->fileInfo;
	size_t read_bytesInfo = info->read_bytesInfo;
	size_t zero_bytesInfo = info->zero_bytesInfo;
	off_t ofsInfo = info->ofsInfo;

	file_seek(file, ofsInfo);

	if (file_read(file, kva, read_bytesInfo) != (int)read_bytesInfo)
	{
		return false;
	}

	memset(kva + read_bytesInfo, 0, zero_bytesInfo);

	return true;

	/*by Jin-Hyuk Jang - project 3(swap in/out)*/
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out(struct page *page)
{
	struct file_page *file_page UNUSED = &page->file;
	struct thread *curr = thread_current();

	if (page == NULL)
		return false;

	struct binLoadInfo *info = (struct binLoadInfo *)page->uninit.aux;
	struct file *fileInfo = info->fileInfo;
	size_t read_bytesInfo = info->read_bytesInfo;
	size_t zero_bytesInfo = info->zero_bytesInfo;
	off_t ofsInfo = info->ofsInfo;

	if (pml4_is_dirty(curr()->pml4, page->va))
	{
		file_write_at(info->fileInfo, page->va, info->read_bytesInfo, info->ofsInfo);
		pml4_set_dirty(curr()->pml4, page->va, 0);
	}

	pml4_clear_page(curr()->pml4, page->va)
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy(struct page *page)
{
	struct file_page *file_page UNUSED = &page->file;
	// close(file_page->file); //? 이거 아닌 거 같은데
}

/* Do the mmap */

/* Edited Code - Jinhyen Kim */

void *
do_mmap(void *addr, size_t length, int writable,
		struct file *file, off_t offset)
{

	/* For do_mmap, the return value is the original address. */
	void *returnPointer = addr;

	struct file *mapFile = file_reopen(file);

	/* We cannot map beyond the file size, so if
		  length exceeds file length, we restrict
		  the size of length. */
	if (length > file_length(mapFile))
	{
		length = file_length(mapFile);
	}

	size_t readBytes;

	/* exitFlag is a boolean variable that indicates
		  whether the mapping process is finished or not. */
	bool exitFlag = true;

	while (exitFlag)
	{

		if (length < PGSIZE)
		{
			/* If this is called, it means we are at the
				  final while loop of mapping. Therefore, we
				  set exitFlag as false. */
			readBytes = length;
			exitFlag = false;
		}
		else
		{
			/* If this is called, it means we still need to map
				  more than a page-size worth of data. Since we cannot
				  map two pages at once, we restrict readBytes
				  to the size of a single page. */
			readBytes = PGSIZE;
			length = length - PGSIZE;
		}

		/* We already have a struct designed for lazy loading, so we use
			  it here. We update the struct variables accordingly. */
		struct binLoadInfo *info = malloc(sizeof(struct binLoadInfo));

		(*(info)).fileInfo = mapFile;
		(*(info)).ofsInfo = offset;
		(*(info)).read_bytesInfo = readBytes;
		(*(info)).zero_bytesInfo = PGSIZE - readBytes;

		/* vm_alloc_page_with_initializer determines whether the page
			  creation proecss was successful or not. If this returns
			  false, it means there was something wrong and so we return. */
		if (!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_segment, info))
		{
			return NULL;
		}

		/* Once we finish mapping for a page, we increment both the
			  offset and the address by a page size for the next
			  mapping process. */
		offset = offset + PGSIZE; // offset = 파일의 처음부터 끝까지 쓰여진 데이타의 길이 --> length를 더하는 것이 맞을 수 있다.~(맨 마지막에...)
		addr = addr + PGSIZE;
	}

	return returnPointer;
}

/* Edited Code - Jinhyen Kim (Project 3 - Memory Mapped Files) */

/* Edited Code - Jinhyen Kim */

/* Do the munmap */
void do_munmap(void *addr)
{

	struct page *mapPage = spt_find_page(&((*(thread_current())).spt), addr);

	/* We run this function as long as there is a mapped page we need to
		  unmap. */
	while (mapPage != NULL)
	{

		/* We can check for any "dirty" (used) bits by calling pml4_is_dirty.
			  if this is the case, we undo the mapping and mark the pml4 table
			  as "clean". */

		if (pml4_is_dirty((*(thread_current())).pml4, (*(mapPage)).va))
		{
			struct binLoadInfo *info = ((*(mapPage)).uninit).aux;
			file_write_at((*(info)).fileInfo, (*(mapPage)).va, (*(info)).read_bytesInfo, (*(info)).ofsInfo);
			pml4_set_dirty((*(thread_current())).pml4, (*(mapPage)).va, false);
		}

		/* Lastly, we iterate through the next address and repeat
			  the process. */

		addr = addr + PGSIZE;
		mapPage = spt_find_page(&((*(thread_current())).spt), addr);
	}
}

/* Edited Code - Jinhyen Kim (Project 3 - Memory Mapped Files) */
