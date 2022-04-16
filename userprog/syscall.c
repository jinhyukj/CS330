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
#include "threads/synch.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "userprog/process.h"
#include "lib/kernel/stdio.h"

void syscall_entry(void);
void syscall_handler(struct intr_frame *);


/* Edited Code - Jinhyen Kim */

struct file
{
	struct inode *inode;
	off_t pos;
	bool deny_write;
};

static struct lock fileLock;

/* Edited Code - Jinhyen Kim (Project 2 - System Call) */


/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081			/* Segment selector msr */
#define MSR_LSTAR 0xc0000082		/* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void syscall_init(void)
{
	lock_init(&fileLock);
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 |
							((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			  FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}



/* Edited Code - Jinhyen Kim */

void halt(void)
{

	/*Terminates Pintos by calling power_off(). */

	power_off();
	return;

}

void exit(int status)
{

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

pid_t fork(const char *thread_name, struct intr_frame *f)
{

	return process_fork(thread_name, f);

}

int exec(const char *cmd_line)
{

	return process_exec(cmd_line);

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

int open (const char *file) {

	/* When we open a file, we need to check if the file
	   location is a valid User Memory. */
	checkUMA(file);

	/* When we open the file, we need to store a file descriptor
	      and check whether the maximum file descriptor value is reached.
	   If this is the case, we return -1.
	   Otherwise, we return the value equal to the file descriptor. */

	if (file == NULL) {
		exit(-1);
	}
	
	lock_acquire(&fileLock);

	struct file *targetFile = filesys_open(file);

	if (targetFile == NULL) {
		lock_release(&fileLock);
		return -1;
	}

	for (int i = 2; i < 14; i++) {
		
		struct thread *curr = thread_current();
		if ((*(thread_current())).fd[i] == NULL) {

			if (strcmp(curr->name, file) == false)
				file_deny_write(targetFile);

			(*(thread_current())).fd[i] = targetFile;
			lock_release(&fileLock);

			return i;
		}
	}
	lock_release(&fileLock);
	return -1;

}

int filesize (int fd) {

	if ((fd < 0) || (fd > 13) || ((*(thread_current())).fd[fd] == NULL)) {
		exit(-1);
	}
	else {
		return file_length((*(thread_current())).fd[fd]);
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
	lock_acquire(&fileLock);
	if (fd == 0) {
		*(char *)buffer = input_getc();
		lock_release(&fileLock);
		return size;
	}	

	if ((fd < 0) || (fd > 13) || ((*(thread_current())).fd[fd] == NULL)) {
		lock_release(&fileLock);
		return -1;
	}

	else {
		bytesRead = file_read(((*(thread_current())).fd[fd]), buffer, size);
		lock_release(&fileLock);
		return bytesRead;
	}

}

int write (int fd, const void *buffer, unsigned size) {

	/* When we write to an open file, we need to check if 
	      the file location is a valid User Memory. */
	checkUMA(buffer);

	/* While we work at a file, we need to lock it first. */
	lock_acquire(&fileLock);

	if (fd == 1) {
		putbuf(buffer, size);
		lock_release(&fileLock);
		return size;
	}
			
	/* The return value of write() is the number of 
	      bytes we write (-1 if the command is invalid). */
	int bytesWritten = 0;



	/* fd = 1 writes to the console with the function
	      putbuf(). 
	   Otherwise, we write to the file with the corresponding
	      fd index to the current thread's fd table. */

	if (fd != 1) {
		if ((fd < 0) || (fd > 13) || ((*(thread_current())).fd[fd] == NULL)) {
			bytesWritten = -1;
		}
		else {	
			bytesWritten = file_write((*(thread_current())).fd[fd], buffer, size);
		}
	}

	/* Once we are done with the file, we unlock it. */
	lock_release(&fileLock);
	return bytesWritten;
	
}

void seek (int fd, unsigned position) {

	if ((fd < 2) || (fd > 13) || ((*(thread_current())).fd[fd] == NULL)) {
		return;
	}
	else {
		lock_acquire(&fileLock);

		file_seek((*(thread_current())).fd[fd], position);

		lock_release(&fileLock);
		return;
	}

}

unsigned tell (int fd) {

	if ((fd < 2) || (fd > 13) || ((*(thread_current())).fd[fd] == NULL)) {
		exit(-1);
	}
	else {
		lock_acquire(&fileLock);

		file_tell((*(thread_current())).fd[fd]);

		lock_release(&fileLock);
		return;
	}

}

void close (int fd) {

	if ((fd < 0) || (fd > 13) || ((*(thread_current())).fd[fd] == NULL)) {
		exit(-1);
	}
	else {
		file_close((*(thread_current())).fd[fd]);		
		(*(thread_current())).fd[fd] = NULL;
		return;
	}

}


/* The main system call interface */
void syscall_handler(struct intr_frame *f UNUSED)
{

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

			exec(((*(f)).R).rdi);
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

		/* Extra for Project 2:*/
		/*case SYS_DUP2:*/

			/*(((*(f)).R).rax) = dup2((((*(f)).R).rdi), (((*(f)).R).rsi));*/
			/*break;*/			

		default:

			/* The default is only called if we call an invalid
			   system call number. We exit the current process
			   with an invalid status. */
			exit(-1);
			break;
	}

}
