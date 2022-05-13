#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "filesys/off_t.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include <list.h>
#include "threads/palloc.h"
#include "lib/kernel/stdio.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "threads/synch.h"
#include "vm/vm.h"
#include "vm/file.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);

	/* Edited Code - Jinhyen Kim
	   For synchronization purposes, we initialize a lock. */

	lock_init(&fileLock);

	/* Edited Code - Jinhyen Kim (Project 2 - System Call) */
}

/* Edited Code - Jinhyen Kim */

void halt(void) {

	/*Terminates Pintos by calling power_off(). */

	power_off();
	return;

}

void exit(int status) {

	/* Edited Code - Jinhyen Kim
	   If the current process is a child process, we need
	      to store the status of termination to return
	      at SYS_WAIT to the parent process. 
	   (To store this, we added a new entity to the
	      definition of the thread.) */

	(*(thread_current ())).exitStatus = status;
	printf("%s: exit(%d)\n", thread_name(), status);	
	thread_exit();
	return;

}

/* Edited Code - Jinhyen Kim (Project 2 - System Call) */

/* Edited Code - Jinhyen Kim
   There are three cases where we should terminate the User Process:
      1: When the pointer is a NULL pointer
      2: When the pointer is pointing to an unmapped virtual memory
      3: When the pointer is pointer to Kernel virtual memory address space
   For these three cases, we terminate the user process.*/

struct page *checkUMA (void* userAddress) {

	if (userAddress == NULL) {
		exit(-1);
	}
	else if (!(is_user_vaddr(userAddress))) {
		exit(-1);
	}

	/* Edited Code - Jinhyen Kim
	   This function is further edited in project 3 as 
	      checkUMA now needs to check the mapping of a virtual memory
	      through spt_find_page.
	   Additionally, it now returns the page returned by the function
	      spt_find_page. */

	if (pml4_get_page(((*(thread_current ())).pml4), userAddress) == NULL) {	
		exit(-1);
	}

	else {
		struct page *page = spt_find_page(&((*(thread_current())).spt), userAddress);

		if (page == NULL) {
			exit(-1);
		}
		else {
			return page;
		}
	}

	/* Edited Code - Jinhyen Kim (Project 3 - Anonymous Page) */

}

/* Edited Code - Jinhyen Kim (Project 2 - User Memory) */

/* Edited Code - Jinhyen Kim
   For the system call read, we need to ensure that the 
      buffer memory used to read data is a valid virtual address. 
   In addition, this buffer memory should also allow writes.
      (writable must be true) */

void checkReadBuffer (void *buffer, unsigned size) {

	for (int i = 0; i < size; i++) {
		struct page *page = checkUMA(buffer + i);
		if ((*(page)).writable == false) {
			exit(-1);
		}
	}
}

/* Edited Code - Jinhyen Kim (Project 3 - Anonymous Page) */

/* Edited Code - Jinhyen Kim
   For the system call write, we need to ensure that the 
      buffer memory used to write data is a valid virtual address. */

void checkWriteBuffer (void *buffer, unsigned size) {

	for (int i = 0; i < size; i++) {
		struct page *page = checkUMA(buffer + i);
	}
}

/* Edited Code - Jinhyen Kim (Project 3 - Anonymous Page) */

/* Edited Code - Jinhyen Kim*/

pid_t fork (const char *thread_name, struct intr_frame *f) {

	/* Edited Code - Jinhyen Kim
	   fork() is performed at process.c. */

	return process_fork(thread_name, f);

}

int exec (char *file_name) {

	/* When we have a file as parameter, we need to check if 
	      the file location is a valid User Memory. */
	checkUMA(file_name);

	int size = strlen(file_name);

	/* We need to convert the current process to an executable
	      file. This is done by:
	         1. Calling palloc_get_page() to get a free page to store
	               the executable. If this returns NULL, it means no
	               pages are avaliable.
	         2. Calling strlcpy() to copy the file into the page.
	         3. Calling process_exec() to see if the file is executable. */
	
	char *fn_copy = palloc_get_page(PAL_ZERO);
	
	if (fn_copy == NULL)
		exit(-1);

	strlcpy(fn_copy, file_name, size + 1);

	if (process_exec(fn_copy) == -1)
		return -1;

}

int wait (pid_t pid) {

	/* Edited Code - Jinhyen Kim
	   wait() is performed at process.c. */

	return process_wait(pid);

}

bool create (const char *file, unsigned initial_size) {

	/* When we create a new file, we need to check if the file
	      location is a valid User Memory. */
	checkUMA(file);

	/* We create a new file with filesys_create().
	   If it was successful, we return true.
	   Otherwise, we return false. */
	if (filesys_create(file, initial_size)) {
		return true;
	}
	else {
		return false;
	}

}

bool remove (const char *file) {

	/* When we delete a file, we need to check if the file
	      location is a valid User Memory. */
	checkUMA(file);

	/* We delete the file with filesys_remove().
	   If it was successful, we return true.
	   Otherwise, we return false. */
	if (filesys_remove(file)) {
		return true;
	}
	else {
		return false;
	}

}


int open(const char *file) {

	/* When we open a file, we need to check if the file
	   location is a valid User Memory. */
	checkUMA(file);

	/* While we work at a file, we need to lock it first. */
	lock_acquire(&fileLock);

	struct file *targetFile = filesys_open(file);

	if (targetFile == NULL)
		return -1;

	/* When we open the file, we need to store a file descriptor
	      and check whether the maximum file descriptor value is reached.
	   If this is the case, we return -1.
	   Otherwise, we return the value equal to the file descriptor. */

	int returnValue;

	struct file **fdt = (*(thread_current())).fdTable;

	while ((*(thread_current())).fdIndex < 1536 && fdt[(*(thread_current())).fdIndex])
		(*(thread_current())).fdIndex = (*(thread_current())).fdIndex + 1;

	if ((*(thread_current())).fdIndex >= 1536) {
		returnValue = -1;
	}
	else {
		fdt[(*(thread_current())).fdIndex] = targetFile;
		returnValue = (*(thread_current())).fdIndex;
	}
	
	if (returnValue == -1)
		file_close(targetFile);

	/* Once we are done with the file, we unlock it. */
	lock_release(&fileLock);

	return returnValue;

}

int filesize(int fd) {

	/* Edited Code - Jinhyen Kim
	   This is invalid when either the fd index value is beyond the
	      maximum value, or when there is no file at the given 
	      fd index.
	    Otherwise, we calculate file size through file_length(). */

	if (fd < 0 || fd >= 1536)
		return -1;

	struct file *targetFile = (*(thread_current())).fdTable[fd];

	if (targetFile == NULL)
		return -1;

	return file_length(targetFile);

}

int read(int fd, void *buffer, unsigned size) {

	/* When we read to an open file, we need to check if 
	      the file location is a valid User Memory. */	
	checkUMA(buffer);

	/* This is invalid when either the fd index value is beyond the
	      maximum value, or when there is no file at the given 
	      fd index. */

	if (fd < 0 || fd >= 1536)
		return -1;

	struct file *targetFile = (*(thread_current())).fdTable[fd];

	if (targetFile == NULL)
		return -1;

	/* By our design, targetFile == 0 represents reading from 
	      keyboard.
	   As such, we call input_getc. */

	if (targetFile == 0)
	{
		*(char *)buffer = input_getc();
		return size;
	}

	/* By our design, targetFile == 1 represents writing to the
	      console.
	   This is invalid for read(), so we return -1. */

	else if (targetFile == 1)
	{
		return -1;
	}

	/* Otherwise, we call file_read. */

	else
	{
		/* While we work at a file, we need to lock it first. */
		lock_acquire(&fileLock);

		int bytesRead = file_read(targetFile, buffer, size);

		/* Once we are done with the file, we unlock it. */
		lock_release(&fileLock);

		return bytesRead;
	}
}

int write(int fd, const void *buffer, unsigned size) {

	/* When we write to an open file, we need to check if 
	      the file location is a valid User Memory. */
	checkUMA(buffer);

	/* This is invalid when either the fd index value is beyond the
	      maximum value, or when there is no file at the given 
	      fd index. */

	if (fd < 0 || fd >= 1536)
		return -1;

	struct file *targetFile = (*(thread_current())).fdTable[fd];

	if (targetFile == NULL)
		return -1;

	/* By our design, targetFile == 1 represents writing to the
	      console.
	   As such, we call putbuf. */
	
	if (targetFile == 1)
	{
		putbuf(buffer, size);
		return size;
	}

	/* By our design, targetFile == 0 represents reading from 
	      keyboard.
	   This is invalid for write(), so we return -1. */

	else if (targetFile == 0)
	{
		return -1;
	}

	/* Otherwise, we call file_write. */

	else
	{
		/* While we work at a file, we need to lock it first. */
		lock_acquire(&fileLock);

		int bytesWritten = file_write(targetFile, buffer, size);

		/* Once we are done with the file, we unlock it. */
		lock_release(&fileLock);

		return bytesWritten;
	}
}

void seek(int fd, unsigned position) {

	/* This is invalid when either the fd index value is beyond the
	      maximum value, or when there is no file at the given 
	      fd index. */

	if (fd < 0 || fd >= 1536)
		return;
	
	struct file *targetFile = (*(thread_current())).fdTable[fd];

	if (targetFile == NULL)
		return;

	/* targetFile < 2 represents reading from keyboard and writing
	      to console, both of which are invalid for seek. */

	if (targetFile < 2)
		return;

	/* Seek is done through file_seek. */

	file_seek(targetFile, position);

}

unsigned tell(int fd) {

	/* This is invalid when either the fd index value is beyond the
	      maximum value, or when there is no file at the given 
	      fd index. */

	if (fd < 0 || fd >= 1536)
		return;
	
	struct file *targetFile = (*(thread_current())).fdTable[fd];

	if (targetFile == NULL)
		return;

	/* targetFile < 2 represents reading from keyboard and writing
	      to console, both of which are invalid for seek. */

	if (targetFile < 2)
		return;

	/* Tell is done through file_tell. */

	return file_tell(targetFile);

}

void close(int fd) {

	/* This is invalid when either the fd index value is beyond the
	      maximum value, or when there is no file at the given 
	      fd index. */

	if (fd < 0 || fd >= 1536)
		return;

	struct file *targetFile = (*(thread_current())).fdTable[fd];

	if (targetFile == NULL)
		return;

	/* We do not need to perform this for when fd == 0 or 1.
	   (Special case reserved by pintos) */

	if (fd < 2 || targetFile < 2)
		return;

	/* Otherwise, we clear the fdTable. */

	(*(thread_current())).fdTable[fd] = NULL;

}

/* Edited Code - Jinhyen Kim (Project 2 - System Call) */

/* Edited Code - Jinhyen Kim
   New System Calls for Project 3 */

void *mmap (void *addr, size_t length, int writable, int fd, off_t offset) {

	/* There are many invalid conditions we must check:
	   1. We check whether the given address is NULL.
	   2. We check whether the given address is not user address. 
	   3. We check whether the given address has any offset.

	   4. We check whether length is not positive.
	   5. We check whether the offset is not a multiple of page size.

	   6. We check whether the file descriptor value is within limit.
	   7. We check whether the retrieved file is NULL.
	   8. We check whether the retrieved file are console input / output.
	   9. We check whether the retrieved file has a positive length. 

	   For any of the invalid conditions met, we return NULL. */

	if ((addr == NULL) || !(is_user_vaddr(addr)) || !(pg_ofs(addr) == 0) || !(length > 0) || !(offset % PGSIZE == 0) || (fd < 0) || (fd >= 1536)) {
		return NULL;
	}	

	struct file *targetFile = (*(thread_current())).fdTable[fd];

	if ((targetFile == NULL) || (targetFile < 2) || !(file_length(targetFile) > 0)) {
		return NULL;
	}

	/* The actual process is done over at do_mmap at vm/file.c. */

	return do_mmap(addr, length, writable, targetFile, offset);

}

void munmap (void *addr) {

	/* We simply check for two invalid conditions:
	   1. We check whether the given address is NULL.
	   2. We check whether the given address is not user address. */

	if (addr == NULL) {
		exit(-1);
	}
	if (!(is_user_vaddr(addr))) {
		exit(-1);
	}
	
	/* The actual process is done over at do_munmap at vm/file.c. */

	do_munmap(addr);

}

/* Edited Code - Jinhyen Kim (Project 3 - Memory Mapped Files) */

void
syscall_handler (struct intr_frame *f UNUSED) {

#ifdef VM
	(*(thread_current())).currentRsp = f->rsp;
#endif

	/* Edited Code - Jinhyen Kim

	Copied from syscall-nr.h

	Projects 2:

	SYS_HALT,                   Halt the operating system.
	SYS_EXIT,                   Terminate this process.
	SYS_FORK,                   Clone current process.
	SYS_EXEC,                   Switch current process.
	SYS_WAIT,                   Wait for a child process to die.
	SYS_CREATE,                 Create a file.
	SYS_REMOVE,                 Delete a file.
	SYS_OPEN,                   Open a file.
	SYS_FILESIZE,               Obtain a file's size.
	SYS_READ,                   Read from a file.
	SYS_WRITE,                  Write to a file.
	SYS_SEEK,                   Change position in a file.
	SYS_TELL,                   Report current position in a file.
	SYS_CLOSE,                  Close a file. */

	switch (((*(f)).R).rax) {

		case SYS_HALT:

			halt();
			break;

		case SYS_EXIT:
			
			exit(((*(f)).R).rdi);		
			break;

		case SYS_FORK:

			(((*(f)).R).rax) = fork((((*(f)).R).rdi), f);
			break;

		case SYS_EXEC:

			/*
			if (exec(((*(f)).R).rdi) == -1) {
				exit(-1);
			}
			break;
			*/
		
			(((*(f)).R).rax) = exec((((*(f)).R).rdi));
			if ((((*(f)).R).rax) == -1)
				exit(-1);
			break;
			
		case SYS_WAIT:

			(((*(f)).R).rax) = wait((((*(f)).R).rdi));
			break;

		case SYS_CREATE:

			(((*(f)).R).rax) = create((((*(f)).R).rdi), (((*(f)).R).rsi));
			break;

		case SYS_REMOVE:

			(((*(f)).R).rax) = remove((((*(f)).R).rdi));
			break;			
			
		case SYS_OPEN:

			(((*(f)).R).rax) = open((((*(f)).R).rdi));
			break;
		
		case SYS_FILESIZE:
			
			(((*(f)).R).rax) = filesize((((*(f)).R).rdi));
			break;

		case SYS_READ:

			checkReadBuffer((((*(f)).R).rsi), (((*(f)).R).rdx)); 
			(((*(f)).R).rax) = read((((*(f)).R).rdi), (((*(f)).R).rsi), (((*(f)).R).rdx));
			break;

		case SYS_WRITE:

			checkWriteBuffer((((*(f)).R).rsi), (((*(f)).R).rdx)); 
			(((*(f)).R).rax) = write((((*(f)).R).rdi), (((*(f)).R).rsi), (((*(f)).R).rdx));
			break;

		case SYS_SEEK:

			seek((((*(f)).R).rdi), (((*(f)).R).rsi));
			break;

		case SYS_TELL:

			(((*(f)).R).rax) = tell((((*(f)).R).rdi));
			break;
	
		case SYS_CLOSE:

			close((((*(f)).R).rdi));
			break;			

		/* Edited Code - Jinhyen Kim 
		   From here, system calls for Project 3 are included. */

		case SYS_MMAP:
			
			(((*(f)).R).rax) = mmap((((*(f)).R).rdi), (((*(f)).R).rsi), (((*(f)).R).rdx), (((*(f)).R).r10), (((*(f)).R).r8));
			break;

		case SYS_MUNMAP:

			munmap((((*(f)).R).rdi));
			break;

		/* Edited Code - Jinhyen Kim (Project 3 - Memory Mapped Files) */

		default:

			/* The default is only called if we call an invalid
			   system call number. We exit the current process
			   with an invalid status. */
			exit(-1);
			break;
	}
}
