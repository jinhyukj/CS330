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

	(*(thread_current ())).exit_status = status;
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

void checkUMA (void* userAddress) {

	if (userAddress == NULL) {
		exit(-1);
	}
	if (is_kernel_vaddr (userAddress)) {
		exit(-1);
	}
	if (pml4_get_page (((*(thread_current ())).pml4), userAddress) == NULL) {	
		exit(-1);
	}
	return;

}

/* Edited Code - Jinhyen Kim (Project 2 - User Memory) */

/* Edited Code - Jinhyen Kim*/

pid_t fork (const char *thread_name, struct intr_frame *f) {

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

	while ((*(thread_current())).fdIdx < FDCOUNT_LIMIT && fdt[(*(thread_current())).fdIdx])
		(*(thread_current())).fdIdx = (*(thread_current())).fdIdx + 1;

	if ((*(thread_current())).fdIdx >= FDCOUNT_LIMIT) {
		returnValue = -1;
	}
	else {
		fdt[(*(thread_current())).fdIdx] = targetFile;
		returnValue = (*(thread_current())).fdIdx;
	}
	
	if (returnValue == -1)
		file_close(targetFile);

	/* Once we are done with the file, we unlock it. */
	lock_release(&fileLock);

	return returnValue;

}

int filesize(int fd) {

	if (fd < 0 || fd >= FDCOUNT_LIMIT)
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


	if (fd < 0 || fd >= FDCOUNT_LIMIT)
		return -1;

	struct file *targetFile = (*(thread_current())).fdTable[fd];

	if (targetFile == NULL)
		return -1;


	if (targetFile == 1)
	{
		*(char *)buffer = input_getc();
		return size;
	}
	else if (targetFile == 2)
	{
		return -1;
	}
	else{
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


	if (fd < 0 || fd >= FDCOUNT_LIMIT)
		return -1;

	struct file *targetFile = (*(thread_current())).fdTable[fd];

	if (targetFile == NULL)
		return -1;

	
	if (targetFile == 2)
	{
		putbuf(buffer, size);
		return size;
	}
	else if (targetFile == 1)
	{
		return -1;
	}
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

	if (fd < 0 || fd >= FDCOUNT_LIMIT)
		return;
	
	struct file *targetFile = (*(thread_current())).fdTable[fd];

	if (targetFile == NULL)
		return;

	if (targetFile <= 2)
		return;

	file_seek(targetFile, position);

}

unsigned tell(int fd) {

	if (fd < 0 || fd >= FDCOUNT_LIMIT)
		return;
	
	struct file *targetFile = (*(thread_current())).fdTable[fd];

	if (targetFile == NULL)
		return;

	if (targetFile <= 2)
		return;

	return file_tell(targetFile);

}

void close(int fd) {

	if (fd < 0 || fd >= FDCOUNT_LIMIT)
		return;

	struct file *targetFile = (*(thread_current())).fdTable[fd];

	if (targetFile == NULL)
		return;
	
	(*(thread_current())).fdTable[fd] = NULL;


	if (fd <= 1 || targetFile <= 2)
		return;

	/*
	if (targetFile -> dupCount == 0)
		file_close(targetFile);
	else
		targetFile->dupCount--;
	*/

}

/* Edited Code - Jinhyen Kim (Project 2 - System Call) */

void
syscall_handler (struct intr_frame *f UNUSED) {

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
	SYS_CLOSE,                  Close a file.

	Extra for Project 2:

	SYS_DUP2,                   Duplicate the file descriptor */

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

			if (exec(((*(f)).R).rdi) == -1) {
				exit(-1);
			}
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

			(((*(f)).R).rax) = read((((*(f)).R).rdi), (((*(f)).R).rsi), (((*(f)).R).rdx));
			break;

		case SYS_WRITE:

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

		default:

			/* The default is only called if we call an invalid
			   system call number. We exit the current process
			   with an invalid status. */
			exit(-1);
			break;
	}
}
