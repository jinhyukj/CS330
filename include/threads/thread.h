#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#ifdef VM
#include "vm/vm.h"
#endif

/* States in a thread's life cycle. */
enum thread_status
{
	THREAD_RUNNING, /* Running thread. */
	THREAD_READY,	/* Not running but ready to run. */
	THREAD_BLOCKED, /* Waiting for an event to trigger. */
	THREAD_DYING	/* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t)-1) /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0	   /* Lowest priority. */
#define PRI_DEFAULT 31 /* Default priority. */
#define PRI_MAX 63	   /* Highest priority. */

/*Edited by Jin-Hyuk Jang
Default values of "nice", "recent_cpu", and "load_avg" are all 0*/
#define NICE_DEFAULT 0
#define RECENT_CPU_DEFAULT 0
#define LOAD_AVG_DEFAULT 0
/*Edited by Jin-Hyuk Jang(Project 1 - advanced scheduler)*/

/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * The upshot of this is twofold:
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 *
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
 * the run queue (thread.c), or it can be an element in a
 * semaphore wait list (synch.c).  It can be used these two ways
 * only because they are mutually exclusive: only a thread in the
 * ready state is on the run queue, whereas only a thread in the
 * blocked state is on a semaphore wait list. */
struct thread
{
	/* Owned by thread.c. */
	tid_t tid;				   /* Thread identifier. */
	enum thread_status status; /* Thread state. */
	char name[16];			   /* Name (for debugging purposes). */
	int priority;			   /* Priority. */

	/* Shared between thread.c and synch.c. */
	struct list_elem elem; /* List element. */

	/*Thread needs to know when to wake up
	We give it the wake_tick element to save the tick to wake up
	by Jin-Hyuk Jang(project 1 - alarm clock)*/
	int64_t wake_tick; /* "tick" to wake up to */

	/* Edited Code - Jinhyen Kim
	   A thread has its own priority value, but it can also receive
		  a priority donation from a thread with a different priority.
	   We add two elements priorityBase and priorityDonated to store
		  these two values.
	   Note: priority = max (priorityBase, priorityDonated) */

	int priorityBase;
	int priorityDonated;

	/* Edited Code - Jinhyen Kim (Project 1 - Priority Donation) */

	/* Edited Code - Jinhyen Kim
	   A thread may receive multiple donations, of which only the thread
		  with the highest priority effectively matters.
	   In order to keep track of all the donating threads, we add a list
		  priorityDonors and a list_elem priorityDonorsElement.
	   Note: To find the highest priority, we can simply sort priorityDonors. */

	struct list priorityDonors;
	struct list_elem priorityDonorsElement;

	/* Edited Code - Jinhyen Kim (Project 1 - Priority Donation) */

	/* Edited Code - Jinhyen Kim
	   To implement Nested Priority Donation, we need a way to learn what lock
		  each thread is waiting on, as well as what thread each lock is being
		  held by.
	   Although each lock stores their holder, the threads does not store any
		  information about what lock they are waiting on.
	   We add a new element targetLock, a lock pointer, to store this information. */

	struct lock *targetLock;

	/* Edited Code - Jinhyen Kim (Project 1 - Priority Donation) */

	/*Edited by Jin-Hyuk Jang
	We now add "nice" and "recent_cpu" to thread structure.
	"nice": An integer ranging from -20 to 20. In default, it is 0, but it also depends on what "nice" value the parent thread has.
	The bigger the "nice" is, the lower the priority.

	"recent_cpu": An integer(orignally a floating point, but changed into integer through fixed-point arithmetic)
	 that shows how much CPU time the thread has been using.
	The bigger the "recent_cpu", the lower the priority*/

	int nice;
	int recent_cpu;

	/*Edited by Jin-Hyuk Jang(Project 1 - advanced scheduler)*/

	/* Edited Code - Jinhyen Kim
	   For SYS_EXIT, We need to store the status of termination 
	   to return at SYS_WAIT.
	   exitStatus of 0 indicates success,
	   exitStatus of non-zero value indicates failure.*/

	int exitStatus;

	/* Edited Code - Jinhyen Kim (Project 2 - System Call) */

	/* Edited Code - Jinhyen Kim
	   To implement File Descriptor, we need to have a storage
	   space for the fd's.
	   The pointer fdTable points to the table storing all fd's. 
	   Additionally, we add another integer that stores the
	   first open spot of the fd table.*/

	struct file **fdTable;
	int fdIndex;

	/* Edited Code - Jinhyen Kim (Project 2 - System Call) */

#ifdef USERPROG
	/* Owned by userprog/process.c. */
	uint64_t *pml4; /* Page map level 4 */
#endif
#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	struct supplemental_page_table spt;
#endif

	/* Owned by thread.c. */
	struct intr_frame tf; /* Information for switching */
	unsigned magic;		  /* Detects stack overflow. */
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init(void);
void thread_start(void);

void thread_tick(void);
void thread_print_stats(void);

typedef void thread_func(void *aux);
tid_t thread_create(const char *name, int priority, thread_func *, void *);

void thread_block(void);
void thread_unblock(struct thread *);

struct thread *thread_current(void);
tid_t thread_tid(void);
const char *thread_name(void);

void thread_exit(void) NO_RETURN;
void thread_yield(void);

int thread_get_priority(void);
void thread_set_priority(int);

int thread_get_nice(void);
void thread_set_nice(int);
int thread_get_recent_cpu(void);
int thread_get_load_avg(void);

void do_iret(struct intr_frame *tf);

/* We need a function that turns thread states to THREAD_BLOCKED
This makes the thread sleep and insert it into the sleep_list.
by Jin-Hyuk Jang (project 1 - alarm clock) */
void thread_sleep(int64_t ticks);

/*We need a function that turns thread states back to THREAD_READY
This will wake up the threads that need to wake up by turnning the thread states to
THREAD_READY, and allocating them on the ready list
by Jin-Hyuk Jang (project 1 - alarm clock) */
void thread_wake(int64_t ticks);

#endif /* threads/thread.h */
