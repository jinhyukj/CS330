#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#include "threads/fp-arithmetic.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* Random value for basic thread
   Do not modify this value. */
#define THREAD_BASIC 0xd42df210

/*Edited by Jin-Hyuk Jang
"load_avg" is a global variable that shows how many threads are competing to run*/
int load_avg;
/*Edited by Jin-Hyuk Jang(project 1 - advanced scheduler)*/

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of processes that is in sleep state
Created for threads put to sleep so they don't busy wait in RUNNING_STATE
by Jin_hyuk Jang (project 1 - alarm clock)*/
static struct list sleep_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Thread destruction requests */
static struct list destruction_req;

/* Statistics. */
static long long idle_ticks;   /* # of timer ticks spent idle. */
static long long kernel_ticks; /* # of timer ticks in kernel threads. */
static long long user_ticks;   /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4		  /* # of timer ticks to give each thread. */
static unsigned thread_ticks; /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread(thread_func *, void *aux);

static void idle(void *aux UNUSED);
static struct thread *next_thread_to_run(void);
static void init_thread(struct thread *, const char *name, int priority);
static void do_schedule(int status);
static void schedule(void);
static tid_t allocate_tid(void);

/* Returns true if T appears to point to a valid thread. */
#define is_thread(t) ((t) != NULL && (t)->magic == THREAD_MAGIC)

/* Returns the running thread.
 * Read the CPU's stack pointer `rsp', and then round that
 * down to the start of a page.  Since `struct thread' is
 * always at the beginning of a page and the stack pointer is
 * somewhere in the middle, this locates the curent thread. */
#define running_thread() ((struct thread *)(pg_round_down(rrsp())))

// Global descriptor table for the thread_start.
// Because the gdt will be setup after the thread_init, we should
// setup temporal gdt first.
static uint64_t gdt[3] = {0, 0x00af9a000000ffff, 0x00cf92000000ffff};

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void thread_init(void)
{
	ASSERT(intr_get_level() == INTR_OFF);

	/* Reload the temporal gdt for the kernel
	 * This gdt does not include the user context.
	 * The kernel will rebuild the gdt with user context, in gdt_init (). */
	struct desc_ptr gdt_ds = {
		.size = sizeof(gdt) - 1,
		.address = (uint64_t)gdt};
	lgdt(&gdt_ds);

	/* Init the globla thread context */
	lock_init(&tid_lock);
	list_init(&ready_list);

	/* We need to initialize the sleep_list
	by Jin-Hyuk Jang(Project 1 - alarm clock) */
	list_init(&sleep_list);

	list_init(&destruction_req);

	/* Set up a thread structure for the running thread. */
	initial_thread = running_thread();
	init_thread(initial_thread, "main", PRI_DEFAULT);
	initial_thread->status = THREAD_RUNNING;
	initial_thread->tid = allocate_tid();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void thread_start(void)
{
	/* Create the idle thread. */
	struct semaphore idle_started;
	sema_init(&idle_started, 0);
	thread_create("idle", PRI_MIN, idle, &idle_started);

	/*Edited by Jin-Hyuk Jang
	We set the "load_avg" value to default(LOAD_AVG_DEFAULT)*/
	load_avg = LOAD_AVG_DEFAULT;
	/*Edited by Jin-Hyuk Jang (project 1 - advanced scheduling)*/

	/* Start preemptive thread scheduling. */
	intr_enable();

	/* Wait for the idle thread to initialize idle_thread. */
	sema_down(&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void thread_tick(void)
{
	struct thread *t = thread_current();

	/* Update statistics. */
	if (t == idle_thread)
		idle_ticks++;
#ifdef USERPROG
	else if (t->pml4 != NULL)
		user_ticks++;
#endif
	else
		kernel_ticks++;

	/* Enforce preemption. */
	if (++thread_ticks >= TIME_SLICE)
		intr_yield_on_return();
}

/* Prints thread statistics. */
void thread_print_stats(void)
{
	printf("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
		   idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t thread_create(const char *name, int priority,
					thread_func *function, void *aux)
{
	struct thread *t;
	tid_t tid;

	ASSERT(function != NULL);

	/* Allocate thread. */
	t = palloc_get_page(PAL_ZERO);
	if (t == NULL)
		return TID_ERROR;

	/* Initialize thread. */
	init_thread(t, name, priority);
	tid = t->tid = allocate_tid();

	
	/* Edited Code - Jinhyen Kim
	   We perform two things here:
	      1. We store the childThreadElem of t (child thread) 
	            to the current thread (parent thread)'s list. 
	      2. We initialize the fdTable of t. */

	list_push_back(&((*(thread_current())).childThreadList),&((*(t)).childThreadElem));

    	(*(t)).fdTable = palloc_get_multiple(PAL_ZERO, 3);

    	if ((*(t)).fdTable == NULL)
        	return TID_ERROR;

	/* The first index value is set to be 2.
	   This is because fd = 0 and 1 are already reserved 
	      by pintos:
	   fd = 0 reads from the keyboard,
	   fd = 1 writes to the console.
	   To represent these two cases, we define the value of 
	      fdTable[0] as 0 and fdTable[1] as 1. */

    	(*(t)).fdIndex = 2;
   	(*(t)).fdTable[0] = 0;
   	(*(t)).fdTable[1] = 1;

	/* Edited Code - Jinhyen Kim (Project 2 - System Call) */


	/* Call the kernel_thread if it scheduled.
	 * Note) rdi is 1st argument, and rsi is 2nd argument. */
	t->tf.rip = (uintptr_t)kernel_thread;
	t->tf.R.rdi = (uint64_t)function;
	t->tf.R.rsi = (uint64_t)aux;
	t->tf.ds = SEL_KDSEG;
	t->tf.es = SEL_KDSEG;
	t->tf.ss = SEL_KDSEG;
	t->tf.cs = SEL_KCSEG;
	t->tf.eflags = FLAG_IF;

	/* Add to run queue. */
	thread_unblock(t);

	/* Edited Code - Jinhyen Kim
	   The newly created thread may have a higher priority than
		  the currently running thread.
	   To test this, we compare the priority of the first thread
		  at ready_list and the current thread.
	   If the thread at ready_list has a higher priority, we
		  call thread_yield. */

	if (!(list_empty(&ready_list)))
	{
		if (((*(list_entry(list_front(&ready_list), struct thread, elem))).priority) > ((*(thread_current())).priority))
		{
			thread_yield();
		}
	}

	/* Edited Code - Jinhyen Kim (Project 1 - Priority Scheduling) */

	return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void thread_block(void)
{
	ASSERT(!intr_context());
	ASSERT(intr_get_level() == INTR_OFF);
	thread_current()->status = THREAD_BLOCKED;
	schedule();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */

/* Edited Code - Jinhyen Kim
   The following function takes two threads A and B as a list_elem.
   If thread A has a higher priority than thread B, the function
	  returns True.
   Otherwise, the function returns False. */

bool threadPriorityCompare(struct list_elem *tA, struct list_elem *tB)
{
	if (((*(list_entry(tA, struct thread, elem))).priority) > ((*(list_entry(tB, struct thread, elem))).priority))
	{
		return true;
	}
	else
	{
		return false;
	}
}

/* Edited Code - Jinhyen Kim (Project 1 - Priority Scheduling) */

/* Edited Code - Jinhyen Kim
   The following function is identical to the above function
	  threadPriorityCompare.
   The only difference is that the sort takes place over
	  the list_elem priorityDonorsElement instead of elem. */

bool threadDonorsPriorityCompare(struct list_elem *tA, struct list_elem *tB)
{
	if (((*(list_entry(tA, struct thread, priorityDonorsElement))).priority) > ((*(list_entry(tB, struct thread, priorityDonorsElement))).priority))
	{
		return true;
	}
	else
	{
		return false;
	}
}

/* Edited Code - Jinhyen Kim (Project 1 - Priority Donation) */

void thread_unblock(struct thread *t)
{
	enum intr_level old_level;

	ASSERT(is_thread(t));

	old_level = intr_disable();
	ASSERT(t->status == THREAD_BLOCKED);
	list_push_back(&ready_list, &t->elem);

	/* Edited Code - Jinhyen Kim
	   For Priority Scheduling, we wish to have the thread with the highest
		  priority to always be at the front of ready_list.
	   However, the original code, list_push_back, adds every unblocked
		  threads to the end of ready_list instead.
	   In order to maintain a descending hierarchy sort, we call the function
		  list_sort every time a new thread is pushed at the end of ready_list.
	   Note: We use threadPriorityCompare to sort by descending order. */

	list_sort(&ready_list, threadPriorityCompare, 0);

	/* Edited Code - Jinhyen Kim (Project 1 - Priority Scheduling) */

	t->status = THREAD_READY;
	intr_set_level(old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name(void)
{
	return thread_current()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current(void)
{
	struct thread *t = running_thread();

	/* Make sure T is really a thread.
	   If either of these assertions fire, then your thread may
	   have overflowed its stack.  Each thread has less than 4 kB
	   of stack, so a few big automatic arrays or moderate
	   recursion can cause stack overflow. */
	ASSERT(is_thread(t));
	ASSERT(t->status == THREAD_RUNNING);

	return t;
}

/* Returns the running thread's tid. */
tid_t thread_tid(void)
{
	return thread_current()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void thread_exit(void)
{
	ASSERT(!intr_context());

#ifdef USERPROG
	process_exit();
#endif

	/* Just set our status to dying and schedule another process.
	   We will be destroyed during the call to schedule_tail(). */
	intr_disable();
	do_schedule(THREAD_DYING);
	NOT_REACHED();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void thread_yield(void)
{
	struct thread *curr = thread_current();
	enum intr_level old_level;

	ASSERT(!intr_context());

	old_level = intr_disable();
	if (curr != idle_thread)
		list_push_back(&ready_list, &curr->elem);

	/* Edited Code - Jinhyen Kim
	   For Priority Scheduling, we wish to have the thread with the highest
			  priority to always be at the front of ready_list.
	   However, the original code, list_push_back, adds every yielded
			  threads to the end of ready_list instead.
	   In order to maintain a descending hierarchy sort, we call the function
			  list_sort every time a new thread is pushed at the end of ready_list.
	   Note: We use threadPriorityCompare to sort by descending order. */

	list_sort(&ready_list, threadPriorityCompare, 0);

	/* Edited Code - Jinhyen Kim (Project 1 - Priority Scheduling) */

	do_schedule(THREAD_READY);
	intr_set_level(old_level);
}

/* We need a function that turns thread states to THREAD_BLOCKED
This makes the thread sleep and insert it into the sleep_list.
by Jin-Hyuk Jang (project 1 - alarm clock) */
void thread_sleep(int64_t ticks)
{
	struct thread *curr = thread_current();
	enum intr_level old_level;

	old_level = intr_disable();

	if (curr != idle_thread)
	{
		curr->wake_tick = ticks;
		list_push_back(&sleep_list, &curr->elem);
		thread_block();
	}

	intr_set_level(old_level);
}

/*We need a function that turns thread states back to THREAD_READY
This will wake up the threads that need to wake up by turnning the thread states to
THREAD_READY, and allocating them on the ready list
by Jin-Hyuk Jang (project 1 - alarm clock) */
void thread_wake(int64_t ticks)
{
	struct list_elem *e = list_begin(&sleep_list);

	while (e != list_end(&sleep_list))
	{
		struct thread *t = list_entry(e, struct thread, elem);
		if (t->wake_tick <= ticks)
		{
			e = list_remove(e);
			thread_unblock(t);
		}

		else
		{
			e = list_next(e);
		}
	}
}

/* Edited Code - Jinhyen Kim
   The following function takes a thread and compares its base priority
	  priorityBase and the donated priority priorityDonated.
   It then sets the thread's priority as the higher of the two.
   Note: The code is only declared; It is defined in synch.c */

void checkForHigherPriority(struct thread *targetThread);

/* Edited Code - Jinhyen Kim (Project 1 - Priority Donation) */

/* Sets the current thread's priority to NEW_PRIORITY. */
void thread_set_priority(int new_priority)
{

	/* Edited Code - Jinhyen Kim
	   A thread that has received priority donation could have its own
		  base priority be changed into a higher value than the donated
		  priority.
	   Because of this, we need to compare priority. */

	(*(thread_current())).priorityBase = new_priority;

	/* If the priorityDonors list is empty, then we can change the
		  value of priorityDonated as new_priority as well.
	   If not, then we cannot change the value of priorityDonated
		  as there is still a thread donating its own priority. */

	if (list_empty(&((*(thread_current())).priorityDonors)))
	{
		(*(thread_current())).priorityDonated = new_priority;
	}

	checkForHigherPriority(thread_current());

	/* Edited Code - Jinhyen Kim (Project 1 - Priority Donation) */

	/* Edited Code - Jinhyen Kim
	   This command could set the priority to be higher than
		  the current running thread.
	   To test this, we compare the priority of the first thread
		  at ready_list and the current thread.
	   If the thread at ready_list has a higher priority, we
		  call thread_yield. */

	if (!(list_empty(&ready_list)))
	{
		if (((*(list_entry(list_front(&ready_list), struct thread, elem))).priority) > ((*(thread_current())).priority))
		{
			thread_yield();
		}
	}

	/* Edited Code - Jinhyen Kim (Project 1 - Priority Scheduling) */
}

/* Returns the current thread's priority. */
int thread_get_priority(void)
{
	return thread_current()->priority;
}

/*Edited by Jin-Hyuk Jang
We need a function that calculates priority according to mlfqs scheduler*/
void calculate_priority(struct thread *t)
{
	if (t != idle_thread)
		t->priority = fti(addif(divif(t->recent_cpu, -4), PRI_MAX - t->nice * 2));
}
/*Edited by Jin-Hyuk Jang (project 1 - advanced scheduler)*/

/*Edited by Jin-Hyuk Jang
We need a function that calculates "recent_cpu" according to mlfqs scheduler*/
void calculate_recent_cpu(struct thread *t)
{
	if (t != idle_thread)
		t->recent_cpu = addif(multf(divf(multif(load_avg, 2), addif(multif(load_avg, 2), 1)), t->recent_cpu), t->nice);
}
/*Edited by Jin-Hyuk Jang (project 1 - advanced scheduler)*/

/*Edited by Jin-Hyuk Jang
We need a function that calculates "load_avg" according to mlfqs scheduler*/
void calculate_load_avg()
{
	if (thread_current() != idle_thread)
		load_avg = addf(multf(divf(itf(59), itf(60)), load_avg), multif(divf(itf(1), itf(60)), list_size(&ready_list) + 1));

	else
		load_avg = addf(multf(divf(itf(59), itf(60)), load_avg), multif(divf(itf(1), itf(60)), list_size(&ready_list)));
}
/*Edited by Jin-Hyuk Jang (project 1 - advanced scheduler)*/

/*Edited by Jin-Hyuk Jang
We need a function that increments "recent_cpu" by 1 every tick*/
void increment_recent_cpu()
{
	if (thread_current() != idle_thread)
		thread_current()->recent_cpu = addif(thread_current()->recent_cpu, 1);
}
/*Edited by Jin-Hyuk Jang (project 1 - advanced scheduler)*/

/*Edited by Jin-Hyuk Jang
We need a function that updates priority for all threads each 4th tick
It also yields control of current_thread to first element in the ready queue if priority is smaller*/
void update_priority()
{
	for (struct list_elem *e = list_begin(&ready_list); e != list_end(&ready_list); e = list_next(e))
	{
		struct thread *t = list_entry(e, struct thread, elem);
		calculate_priority(t);
		list_sort(&ready_list, threadPriorityCompare, 0);
	}

	for (struct list_elem *e = list_begin(&sleep_list); e != list_end(&sleep_list); e = list_next(e))
	{
		struct thread *t = list_entry(e, struct thread, elem);
		calculate_priority(t);
	}

	calculate_priority(thread_current());
	// calculate_priory(idle_thread);
	//   checkForThreadYield();
}
/*Edited by Jin-Hyuk Jang(project 1 - advnaced scheduler)*/

/*Edited by Jin-Hyuk Jang
We need a function that updates "recent_cpu" for all threads every second*/
void update_recent_cpu()
{
	for (struct list_elem *e = list_begin(&ready_list); e != list_end(&ready_list); e = list_next(e))
	{
		struct thread *t = list_entry(e, struct thread, elem);
		calculate_recent_cpu(t);
	}

	for (struct list_elem *e = list_begin(&sleep_list); e != list_end(&sleep_list); e = list_next(e))
	{
		struct thread *t = list_entry(e, struct thread, elem);
		calculate_recent_cpu(t);
	}

	calculate_recent_cpu(thread_current());
	// calculate_recent_cpu(idle_thread);
}
/*Edited by Jin-Hyuk jang(project 1 - advanced scheduler)*/

/* Sets the current thread's nice value to NICE. */
void thread_set_nice(int nice UNUSED)
{
	/* TODO: Your implementation goes here */
	/*Edited by Jin-Hyuk Jang
	sets the "nice" value of current thread to given "nice"*/
	enum intr_level old_level = intr_disable();
	thread_current()->nice = nice;
	calculate_priority(thread_current());
	checkForThreadYield();
	intr_set_level(old_level);
	/*Edited by Jin-Hyuk Jang(project 1 - advanced scheduler)*/
}

/* Returns the current thread's nice value. */
int thread_get_nice(void)
{
	/* TODO: Your implementation goes here */
	/*Edited by Jin-Hyuk Jang
	Gets the "nice" value of current thread*/
	enum intr_level old_level = intr_disable();
	int nice = thread_current()->nice;
	intr_set_level(old_level);
	return nice;
	/*Edited by Jin-Hyuk Jang (Project 1 - advanced scheduler)*/
}

/* Returns 100 times the system load average. */
int thread_get_load_avg(void)
{
	/* TODO: Your implementation goes here */
	/*Edited by Jin-Hyuk Jang
	Returns the load_avg * 100 result*/
	enum intr_level old_level = intr_disable();
	int result = ftir(multif(load_avg, 100));
	intr_set_level(old_level);
	return result;
	/*Edited by Jin-Hyuk Jang (Project 1 - advanced scheduler)*/
}

/* Returns 100 times the current thread's recent_cpu value. */
int thread_get_recent_cpu(void)
{
	/* TODO: Your implementation goes here */
	/*Edited by Jin-Hyuk Jang
	Returns the "recent_cpu" of current thread * 100*/
	enum intr_level old_level = intr_disable();
	int result = ftir(multif(thread_current()->recent_cpu, 100));
	intr_set_level(old_level);
	return result;
	/*Edited by Jin-Hyuk jang(Project 1 - advanced scheduler)*/
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle(void *idle_started_ UNUSED)
{
	struct semaphore *idle_started = idle_started_;

	idle_thread = thread_current();
	sema_up(idle_started);

	for (;;)
	{
		/* Let someone else run. */
		intr_disable();
		thread_block();

		/* Re-enable interrupts and wait for the next one.

		   The `sti' instruction disables interrupts until the
		   completion of the next instruction, so these two
		   instructions are executed atomically.  This atomicity is
		   important; otherwise, an interrupt could be handled
		   between re-enabling interrupts and waiting for the next
		   one to occur, wasting as much as one clock tick worth of
		   time.

		   See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
		   7.11.1 "HLT Instruction". */
		asm volatile("sti; hlt"
					 :
					 :
					 : "memory");
	}
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread(thread_func *function, void *aux)
{
	ASSERT(function != NULL);

	intr_enable(); /* The scheduler runs with interrupts off. */
	function(aux); /* Execute the thread function. */
	thread_exit(); /* If function() returns, kill the thread. */
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread(struct thread *t, const char *name, int priority)
{
	ASSERT(t != NULL);
	ASSERT(PRI_MIN <= priority && priority <= PRI_MAX);
	ASSERT(name != NULL);

	memset(t, 0, sizeof *t);
	t->status = THREAD_BLOCKED;
	strlcpy(t->name, name, sizeof t->name);
	t->tf.rsp = (uint64_t)t + PGSIZE - sizeof(void *);
	t->priority = priority;

	/* Edited Code - Jinhyen Kim
	   priorityBase - The base priority value of the thread
	   priorityDonated - The priority value donated to the thread (if any)
	   Note: Initially, priorityDonated and priorityBase = priority */

	(*t).priorityBase = priority;
	(*t).priorityDonated = priority;

	/* Edited Code - Jinhyen Kim (Project 1 - Priority Donation) */

	/* Edited Code - Jinhyen Kim
	   priorityDonors - The list of all the threads donating priority
	   To create this list, we run list_init. */

	list_init(&((*t).priorityDonors));

	/* Edited Code - Jinhyen Kim (Project 1 - Priority Donation) */

	/* Edited Code - Jinhyen Kim
	   targetLock - The pointer that points to the lock that the thread
		  is waiting on, if any.
	   Note: Initially, targetLock points to NULL. */

	(*t).targetLock = NULL;

	/* Edited Code - Jinhyen Kim (Project 1 - Priority Donation) */

	/*Edited by Jin-Hyuk Jang
	the default value of "nice" and "recent_cpu" is set*/
	t->nice = NICE_DEFAULT;
	t->recent_cpu = RECENT_CPU_DEFAULT;
	/*Edited by Jin-Hyuk Jang(Project 1 - advanced scheduler)*/

	t->magic = THREAD_MAGIC;


	/* Edited Code - Jinhyen Kim
	   We create the childThreadList which will store all
	      of the child threads of t. */

	list_init(&((*t).childThreadList));

	/* Edited Code - Jinhyen Kim (Project 2 - System Call) */

	/* Edited Code - Jinhyen Kim
	   exitStatus - Status of termination.
	   exitStatus of 0 indicates success,
	   exitStatus of non-zero value indicates failure.*/

	(*t).exitStatus = 0;

	/* Edited Code - Jinhyen Kim (Project 2 - System Call) */

	/* Edited Code - Jinhyen Kim
	   We initialize the semaphores with 0 as these semaphores
	      will be used for signalling. */

	sema_init(&((*t).forkLock), 0);
	sema_init(&((*t).removeLock), 0);
	sema_init(&((*t).waitLock), 0);

	/* Edited Code - Jinhyen Kim (Project 2 - System Call) */	

}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run(void)
{
	if (list_empty(&ready_list))
		return idle_thread;
	else
		return list_entry(list_pop_front(&ready_list), struct thread, elem);
}

/* Use iretq to launch the thread */
void do_iret(struct intr_frame *tf)
{
	__asm __volatile(
		"movq %0, %%rsp\n"
		"movq 0(%%rsp),%%r15\n"
		"movq 8(%%rsp),%%r14\n"
		"movq 16(%%rsp),%%r13\n"
		"movq 24(%%rsp),%%r12\n"
		"movq 32(%%rsp),%%r11\n"
		"movq 40(%%rsp),%%r10\n"
		"movq 48(%%rsp),%%r9\n"
		"movq 56(%%rsp),%%r8\n"
		"movq 64(%%rsp),%%rsi\n"
		"movq 72(%%rsp),%%rdi\n"
		"movq 80(%%rsp),%%rbp\n"
		"movq 88(%%rsp),%%rdx\n"
		"movq 96(%%rsp),%%rcx\n"
		"movq 104(%%rsp),%%rbx\n"
		"movq 112(%%rsp),%%rax\n"
		"addq $120,%%rsp\n"
		"movw 8(%%rsp),%%ds\n"
		"movw (%%rsp),%%es\n"
		"addq $32, %%rsp\n"
		"iretq"
		:
		: "g"((uint64_t)tf)
		: "memory");
}

/* Switching the thread by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function. */
static void
thread_launch(struct thread *th)
{
	uint64_t tf_cur = (uint64_t)&running_thread()->tf;
	uint64_t tf = (uint64_t)&th->tf;
	ASSERT(intr_get_level() == INTR_OFF);

	/* The main switching logic.
	 * We first restore the whole execution context into the intr_frame
	 * and then switching to the next thread by calling do_iret.
	 * Note that, we SHOULD NOT use any stack from here
	 * until switching is done. */
	__asm __volatile(
		/* Store registers that will be used. */
		"push %%rax\n"
		"push %%rbx\n"
		"push %%rcx\n"
		/* Fetch input once */
		"movq %0, %%rax\n"
		"movq %1, %%rcx\n"
		"movq %%r15, 0(%%rax)\n"
		"movq %%r14, 8(%%rax)\n"
		"movq %%r13, 16(%%rax)\n"
		"movq %%r12, 24(%%rax)\n"
		"movq %%r11, 32(%%rax)\n"
		"movq %%r10, 40(%%rax)\n"
		"movq %%r9, 48(%%rax)\n"
		"movq %%r8, 56(%%rax)\n"
		"movq %%rsi, 64(%%rax)\n"
		"movq %%rdi, 72(%%rax)\n"
		"movq %%rbp, 80(%%rax)\n"
		"movq %%rdx, 88(%%rax)\n"
		"pop %%rbx\n" // Saved rcx
		"movq %%rbx, 96(%%rax)\n"
		"pop %%rbx\n" // Saved rbx
		"movq %%rbx, 104(%%rax)\n"
		"pop %%rbx\n" // Saved rax
		"movq %%rbx, 112(%%rax)\n"
		"addq $120, %%rax\n"
		"movw %%es, (%%rax)\n"
		"movw %%ds, 8(%%rax)\n"
		"addq $32, %%rax\n"
		"call __next\n" // read the current rip.
		"__next:\n"
		"pop %%rbx\n"
		"addq $(out_iret -  __next), %%rbx\n"
		"movq %%rbx, 0(%%rax)\n" // rip
		"movw %%cs, 8(%%rax)\n"	 // cs
		"pushfq\n"
		"popq %%rbx\n"
		"mov %%rbx, 16(%%rax)\n" // eflags
		"mov %%rsp, 24(%%rax)\n" // rsp
		"movw %%ss, 32(%%rax)\n"
		"mov %%rcx, %%rdi\n"
		"call do_iret\n"
		"out_iret:\n"
		:
		: "g"(tf_cur), "g"(tf)
		: "memory");
}

/* Schedules a new process. At entry, interrupts must be off.
 * This function modify current thread's status to status and then
 * finds another thread to run and switches to it.
 * It's not safe to call printf() in the schedule(). */
static void
do_schedule(int status)
{
	ASSERT(intr_get_level() == INTR_OFF);
	ASSERT(thread_current()->status == THREAD_RUNNING);
	while (!list_empty(&destruction_req))
	{
		struct thread *victim =
			list_entry(list_pop_front(&destruction_req), struct thread, elem);
		palloc_free_page(victim);
	}
	thread_current()->status = status;
	schedule();
}

static void
schedule(void)
{
	struct thread *curr = running_thread();
	struct thread *next = next_thread_to_run();

	ASSERT(intr_get_level() == INTR_OFF);
	ASSERT(curr->status != THREAD_RUNNING);
	ASSERT(is_thread(next));
	/* Mark us as running. */
	next->status = THREAD_RUNNING;

	/* Start new time slice. */
	thread_ticks = 0;

#ifdef USERPROG
	/* Activate the new address space. */
	process_activate(next);
#endif

	if (curr != next)
	{
		/* If the thread we switched from is dying, destroy its struct
		   thread. This must happen late so that thread_exit() doesn't
		   pull out the rug under itself.
		   We just queuing the page free reqeust here because the page is
		   currently used bye the stack.
		   The real destruction logic will be called at the beginning of the
		   schedule(). */
		if (curr && curr->status == THREAD_DYING && curr != initial_thread)
		{
			ASSERT(curr != next);
			list_push_back(&destruction_req, &curr->elem);
		}

		/* Before switching the thread, we first save the information
		 * of current running. */
		thread_launch(next);
	}
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid(void)
{
	static tid_t next_tid = 1;
	tid_t tid;

	lock_acquire(&tid_lock);
	tid = next_tid++;
	lock_release(&tid_lock);

	return tid;
}

/* Edited Code - Jinhyen Kim
   This code checks the priority of the current thread
	  and the highest-priority thread at ready_list.
   If the thread at ready_list has a higher priority,
	  thread_yield is executed.
   This function exists solely so that we can access
	  the static ready_list from other files. */

void checkForThreadYield(void)
{
	if (!(list_empty(&ready_list)))
	{
		if (((*(list_entry(list_front(&ready_list), struct thread, elem))).priority) > ((*(thread_current())).priority))
		{
			thread_yield();
		}
	}
}

/* Edited Code - Jinhyen Kim (Project 1 - Priority Scheduling) */
