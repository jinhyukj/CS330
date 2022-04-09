#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

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
}

/* Edited Code - Jinhyen Kim */

void halt (void) {

	/*Terminates Pintos by calling power_off(). */

	power_off();
	return;

}

void exit (int status) {

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

void checkUMA (void* userAddress) {

	if (userAddress == NULL) {
		exit(-1);
	}
	if (pml4_get_page (((*(thread_current ())).pml4), userAddress) == NULL) {	
		exit(-1);
	}
	if (is_kernel_vaddr (userAddress)) {
		exit(-1);
	}
	return;

}

/* Edited Code - Jinhyen Kim (Project 2 - User Memory) */

/* Edited Code - Jinhyen Kim*/

tid_t fork (const char *thread_name) {
}

int exec (const char *cmd_line) {

	/* When we execute a file, we need to check if the file
	   location is a valid User Memory. */
	checkUMA(cmd_line);
			
	/* We need to convert the current process to an executable
	   file. This is done by:
	      1. Calling palloc_get_page() to get a free page to store 
	            the executable. If this returns NULL, it means no 
	            pages are avaliable.
	      2. Calling strlcpy() to copy the file into the page.
	      3. Calling process_exec() to see if the file is executable. */

	char* newPage = palloc_get_page(2);

	if (newPage == NULL) {
		exit(-1);
	}

	strlcpy(newPage, cmd_line, strlen(cmd_line) + 1);

	if (process_exec(newPage) == -1) {
		exit(-1);
	}

	return;

}

/* Edited Code - Jinhyen Kim*/

int wait (tid_t pid) {
}

/* Edited Code - Jinhyen Kim (Project 2 - System Call) */

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

int open (const char *file) {

	/* When we open a file, we need to check if the file
	   location is a valid User Memory. */
	checkUMA(file);

	/* When we open the file, we need to store a file descriptor
	      and check whether the maximum file descriptor value is reached.
	   If this is the case, we return -1.
	   Otherwise, we return the value equal to the file descriptor. */

	struct file *targetFile = filesys_open(file);
	if (targetFile == NULL) {
		return -1;
	}

	while (((*(thread_current())).fdIndex < 1536) && ((*(thread_current())).fdTable[(*(thread_current())).fdIndex] != NULL)) {
		(*(thread_current())).fdIndex = (*(thread_current())).fdIndex + 1;
	}

	if ((*(thread_current())).fdIndex > 1535) {
		file_close(targetFile);
		return -1;
	}
	else {
		(*(thread_current())).fdTable[(*(thread_current())).fdIndex] = targetFile;
		return (*(thread_current())).fdIndex;
	}

}

int filesize (int fd) {

	/* When we check the filesize, we need to check whether the provided
	      file descriptor is less than the maximum file descriptor value.
	   If this is the case, we return -1.
	   Otherwise, we return the file size.*/

	if ((fd < 0) || (fd > 1535) || ((*(thread_current())).fdTable[fd] == NULL)) {
		return -1;
	}
	else {					
		return file_length(&((*(thread_current())).fdTable[fd]));
	}

}

int read (int fd, void *buffer, unsigned size) {

	/* When we read a file, we need to check if the file
	      location is a valid User Memory. */
	checkUMA(buffer);

	/* The return value of read() is the number of 
	      bytes we read (-1 if the command is invalid). */
	int bytesRead = 0;

	/* fd = 0 reads from the keyboard with the function
	      input_getc(). 
	   Otherwise, we read the file with the corresponding
	      fd index to the current thread's fd table. */
	if (fd == 0) {
		*(char *)buffer = input_getc();
		return size;
	}	

	if ((fd < 0) || (fd > 1535) || ((*(thread_current())).fdTable[fd] == NULL)) {
		return -1;
	}

	else {
		lock_acquire(&fileLock);
		bytesRead = file_read((&((*(thread_current())).fdTable[fd])), buffer, size);
		lock_release(&fileLock);
		return bytesRead;
	}

}

int write (int fd, const void *buffer, unsigned size) {

	/* When we write to an open file, we need to check if 
	      the file location is a valid User Memory. */
	checkUMA(buffer);
			
	/* The return value of write() is the number of 
	      bytes we write (-1 if the command is invalid). */
	int bytesWritten = 0;

	/* While we work at a file, we need to lock it first. */
	lock_acquire(&fileLock);

	/* fd = 1 writes to the console with the function
	      putbuf(). 
	   Otherwise, we write to the file with the corresponding
	      fd index to the current thread's fd table. */
	if (fd == 1) {
		putbuf(buffer, size);
		bytesWritten = size;
	}
	else {
		if ((fd < 0) || (fd > 1535) || ((*(thread_current())).fdTable[fd] == NULL)) {
			bytesWritten = -1;
		}
		else {	
			bytesWritten = file_write((*(thread_current())).fdTable[fd], buffer, size);
		}
	}

	/* Once we are done with the file, we unlock it. */
	lock_release(&fileLock);
	return bytesWritten;
	
}

void seek (int fd, unsigned position) {

	if ((fd < 2) || (fd > 1535) || ((*(thread_current())).fdTable[fd] == NULL)) {
		return;
	}
	else {
		lock_acquire(&fileLock);

		file_seek((*(thread_current())).fdTable[fd], position);

		lock_release(&fileLock);
		return;
	}

}

unsigned tell (int fd) {

	if ((fd < 2) || (fd > 1535) || ((*(thread_current())).fdTable[fd] == NULL)) {
		return 0;
	}
	else {
		lock_acquire(&fileLock);

		file_tell((*(thread_current())).fdTable[fd]);

		lock_release(&fileLock);
		return;
	}

}

void close (int fd) {

	if ((fd < 0) || (fd > 1535) || ((*(thread_current())).fdTable[fd] == NULL)) {
		return;
	}
	else {
		file_close(&((*(thread_current())).fdTable[fd]));		
		(*(thread_current())).fdTable[fd] == NULL;
		return;
	}

}

/* Edited Code - Jinhyen Kim (Project 2 - System Call) */

/* The main system call interface */

/* Edited Code - Jinhyen Kim */

void
syscall_handler (struct intr_frame *f) {
	
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

			/*(((*(f)).R).rax) = fork((((*(f)).R).rdi), f);*/
			break;

		case SYS_EXEC:

			exec(((*(f)).R).rdi);
			break;
			
		case SYS_WAIT:
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

		/* Extra for Project 2:*/
		case SYS_DUP2:
			break;			

		default:

			/* The default is only called if we call an invalid
			   system call number. We exit the current process
			   with an invalid status. */
			exit(-1);
			break;

	}
	
}

/* Edited Code - Jinhyen Kim (Project 2 - System Call) */
