/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
   */

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
   decrement it.

   - up or "V": increment the value (and wake up one waiting
   thread, if any). */
void sema_init(struct semaphore *sema, unsigned value)
{
	ASSERT(sema != NULL);

	sema->value = value;
	list_init(&sema->waiters);
}

/* Edited Code - Jinhyen Kim
   The following function takes two threads A and B as a list_elem.
   If thread A has a higher priority than thread B, the function
	  returns True.
   Otherwise, the function returns False.
   Note: The function is only declared; it is defined in thread.c */

bool threadPriorityCompare(struct list_elem *tA, struct list_elem *tB);

/* Edited Code - Jinhyen Kim (Project 1 - Priority Scheduling) */

/* Edited Code - Jinhyen Kim
   This code checks the priority of the current thread
	  and the highest-priority thread at ready_list.
   If the thread at ready_list has a higher priority,
	  thread_yield is executed.
   We use this function because ready_list is a static variable
	  that can only be used in thread.c */

void checkForThreadYield(void);

/* Edited Code - Jinhyen Kim (Project 1 - Priority Scheduling) */

/* Edited Code - Jinhyen Kim
   The following function is identical to the above function
	  threadPriorityCompare.
   The only difference is that the sort takes place over
	  the list_elem priorityDonorsElement instead of elem.
   Note: The function is only declared; it is defined in thread.c */

bool threadDonorsPriorityCompare(struct list_elem *tA, struct list_elem *tB);

/* Edited Code - Jinhyen Kim (Project 1 - Priority Donation) */

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. This is
   sema_down function. */
void sema_down(struct semaphore *sema)
{
	enum intr_level old_level;

	ASSERT(sema != NULL);
	ASSERT(!intr_context());

	old_level = intr_disable();
	while (sema->value == 0)
	{
		list_push_back(&sema->waiters, &thread_current()->elem);

		/* Edited Code - Jinhyen Kim
		   For Priority Scheduling, we wish to have the thread with the highest
				  priority to always be at the front of waiters.
		   However, the original code, list_push_back, adds every waiting
				  threads to the end of waiters instead.
		   In order to maintain a descending hierarchy sort, we call the function
				  list_sort every time a new thread is pushed at the end of waiters.
		   Note: We use threadPriorityCompare to sort by descending order. */

		list_sort(&sema->waiters, threadPriorityCompare, 0);

		/* Edited Code - Jinhyen Kim (Project 1 - Priority Scheduling) */

		thread_block();
	}
	sema->value--;
	intr_set_level(old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool sema_try_down(struct semaphore *sema)
{
	enum intr_level old_level;
	bool success;

	ASSERT(sema != NULL);

	old_level = intr_disable();
	if (sema->value > 0)
	{
		sema->value--;
		success = true;
	}
	else
		success = false;
	intr_set_level(old_level);

	return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void sema_up(struct semaphore *sema)
{
	enum intr_level old_level;

	ASSERT(sema != NULL);

	old_level = intr_disable();
	if (!list_empty(&sema->waiters))
	{

		/* Edited Code - Jinhyen Kim
		   The code is calling list_pop_front to waiters, which retrives
			  the first element of waiters.
		   To ensure the first element has the highest priority, we need
			  to first sort the list by priority.
		   Note: We use threadPriorityCompare to sort by descending order. */

		list_sort(&sema->waiters, threadPriorityCompare, 0);

		/* Edited Code - Jinhyen Kim (Project 1 - Priority Scheduling) */

		thread_unblock(list_entry(list_pop_front(&sema->waiters),
								  struct thread, elem));

		/* Edited Code - Jinhyen Kim
		   When there is a thread at waiters, we need to check whether
			  the unblocked thread has a higher priority than the
			  currently running thread.
		   To test this, we compare the priority of the first thread
				  at ready_list and the current thread.
		   If the thread at ready_list has a higher priority, we
				  call thread_yield. */

		sema->value++;
		if (!intr_context()) {
			checkForThreadYield();
		}
		intr_set_level(old_level);

		/* Edited Code - Jinhyen Kim (Project 1 - Priority Scheduling) */
	}

	/* Edited Code - Jinhyen Kim
	   When there is nothing at waiters, we have no need to check
		  for priority, and so we can skip checkForThreadYield. */

	else
	{
		sema->value++;

		intr_set_level(old_level);
	}

	/* Edited Code - Jinhyen Kim (Project 1 - Priority Scheduling) */
}

static void sema_test_helper(void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void sema_self_test(void)
{
	struct semaphore sema[2];
	int i;

	printf("Testing semaphores...");
	sema_init(&sema[0], 0);
	sema_init(&sema[1], 0);
	thread_create("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
	for (i = 0; i < 10; i++)
	{
		sema_up(&sema[0]);
		sema_down(&sema[1]);
	}
	printf("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper(void *sema_)
{
	struct semaphore *sema = sema_;
	int i;

	for (i = 0; i < 10; i++)
	{
		sema_down(&sema[0]);
		sema_up(&sema[1]);
	}
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void lock_init(struct lock *lock)
{
	ASSERT(lock != NULL);

	lock->holder = NULL;
	sema_init(&lock->semaphore, 1);
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void lock_acquire(struct lock *lock)
{
	ASSERT(lock != NULL);
	ASSERT(!intr_context());
	ASSERT(!lock_held_by_current_thread(lock));

	/* Edited Code - Jinhyen Kim
	   A thread attempting to acquire a lock may find out that another
		  thread with a lower priority is holding the lock.
	   We add an if statement to test this. If true, we:
		  1. Add current thread to the lock holder's list priorityDonors
		  2. Sort the list priorityDonors
		  3. Have the current thread's pointer targetLock to point to the lock
		  4. Run priorityDonate */

	if (((*lock).holder) != NULL && !thread_mlfqs) // Edited by Jin-Hyuk Jang: Does not priority donate when thread_mlfqs is true(project 1 - advanced scheduler)
	{
		if (((*(thread_current())).priority) > ((*((*lock).holder)).priority))
		{
			list_push_back((&((*((*lock).holder)).priorityDonors)), &((*(thread_current())).priorityDonorsElement));

			list_sort((&((*((*lock).holder)).priorityDonors)), threadDonorsPriorityCompare, 0);

			(*(thread_current())).targetLock = lock;

			priorityDonate();
		}
	}

	/* Edited Code - Jinhyen Kim (Project 1 - Priority Donation) */

	sema_down(&lock->semaphore);
	lock->holder = thread_current();
}

/* Edited Code - Jinhyen Kim
   The following function starts from the current thread and finds
	  what thread to donate priority to by tracking down what lock
	  the current thread is waiting on, and then tracking down what
	  thread is owning that lock.
   Once the thread is located, we donate the priority then run the
	  function checkForHigherPriority to see whether the donated
	  priority is higher than the base priority of the thread.
   Note: We perform this until we encounter a thread that is not
	  waiting on a lock to implement nested priority donation. */

void priorityDonate(void)
{
	struct thread *targetThread;
	targetThread = thread_current();
	while (((*(targetThread)).targetLock) != NULL)
	{
		(*((*((*(targetThread)).targetLock)).holder)).priorityDonated = (*(targetThread)).priority;
		targetThread = (*((*(targetThread)).targetLock)).holder;
		checkForHigherPriority(targetThread);
	}
}

/* Edited Code - Jinhyen Kim (Project 1 - Priority Donation) */

/* Edited Code - Jinhyen Kim
   The following function takes a thread and compares its base priority
	  priorityBase and the donated priority priorityDonated.
   It then sets the thread's priority as the higher of the two. */

void checkForHigherPriority(struct thread *targetThread)
{
	if (((*(targetThread)).priorityBase) > ((*(targetThread)).priorityDonated))
	{
		(*(targetThread)).priority = (*(targetThread)).priorityBase;
	}
	else
	{
		(*(targetThread)).priority = (*(targetThread)).priorityDonated;
	}
}

/* Edited Code - Jinhyen Kim (Project 1 - Priority Donation) */

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool lock_try_acquire(struct lock *lock)
{
	bool success;

	ASSERT(lock != NULL);
	ASSERT(!lock_held_by_current_thread(lock));

	success = sema_try_down(&lock->semaphore);
	if (success)
		lock->holder = thread_current();
	return success;
}

/* Releases LOCK, which must be owned by the current thread.
   This is lock_release function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void lock_release(struct lock *lock)
{
	ASSERT(lock != NULL);
	ASSERT(lock_held_by_current_thread(lock));

	/* Edited Code - Jinhyen Kim
	   Before we set the lock's holder as NULL, we need to revert any
		  priority donations.
	   We can check whether there was a priority donationn by iterating
		  through priorityDonors and checking if any of the donor threads
		  were waiting on the lock.
	   If we find the donor thread, we need to remove that thread from
		  priorityDonors and revert the donated priority. */
	if (!thread_mlfqs)
	{
		struct list_elem *temp;
		temp = list_begin(&((*(thread_current())).priorityDonors));

		while (temp != (list_end(&((*(thread_current())).priorityDonors))))
		{
			if ((*(list_entry(temp, struct thread, priorityDonorsElement))).targetLock == lock)
			{
				list_remove(&((*(list_entry(temp, struct thread, priorityDonorsElement))).priorityDonorsElement));
			}
			temp = list_next(temp);
		}

		if ((*(list_entry(temp, struct thread, priorityDonorsElement))).targetLock == lock)
		{
			list_remove(&((*(list_entry(temp, struct thread, priorityDonorsElement))).priorityDonorsElement));
		}

		/* If there is no threads at priorityDonors, we can simply set
			  both priorityDonated and priority to be the base value.
		   Otherwise, the priority is set to be the highest priority
			  amongst the remaining threads at priorityDonors. */

		if (list_empty(&((*(thread_current())).priorityDonors)))
		{
			(*(thread_current())).priorityDonated = (*(thread_current())).priorityBase;
			(*(thread_current())).priority = (*(thread_current())).priorityBase;
			checkForHigherPriority(thread_current());
		}
		else
		{
			list_sort(&((*(thread_current())).priorityDonors), threadDonorsPriorityCompare, 0);

			(*(thread_current())).priorityDonated = (*(list_entry(list_front(&((*(thread_current())).priorityDonors)), struct thread, priorityDonorsElement))).priority;

			checkForHigherPriority(thread_current());
		}
	}
	/* Edited Code - Jinhyen Kim (Project 1 - Priority Donation) */

	lock->holder = NULL;
	sema_up(&lock->semaphore);
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool lock_held_by_current_thread(const struct lock *lock)
{
	ASSERT(lock != NULL);

	return lock->holder == thread_current();
}

/* One semaphore in a list. */
struct semaphore_elem
{
	struct list_elem elem;		/* List element. */
	struct semaphore semaphore; /* This semaphore. */
};

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void cond_init(struct condition *cond)
{
	ASSERT(cond != NULL);

	list_init(&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */

/* Edited Code - Jinhyen Kim
   The following function takes two semaphores A and B as a list_elem.
   If thread A has a higher priority than thread B, the function
	  returns True.
   Otherwise, the function returns False. */

bool semaphorePriorityCompare(struct list_elem *sA, struct list_elem *sB)
{

	/* Currently, sA and sB is a pointer to a doubly linked list
		  of semaphore_elem.
	   Inside the semaphore_elem is a semaphore.
	   Inside the semaphore is the struct list waiters.
	   The first entry in waiters represent the priority of
		  the conditon variable. */

	struct semaphore *semA = &(*(list_entry(sA, struct semaphore_elem, elem))).semaphore;
	struct semaphore *semB = &(*(list_entry(sB, struct semaphore_elem, elem))).semaphore;

	if (((*(list_entry((list_begin(&(semA->waiters))), struct thread, elem))).priority) > ((*(list_entry((list_begin(&(semB->waiters))), struct thread, elem))).priority))
	{
		return true;
	}
	else
	{
		return false;
	}
}

/* Edited Code - Jinhyen Kim (Project 1 - Priority Scheduling) */

void cond_wait(struct condition *cond, struct lock *lock)
{
	struct semaphore_elem waiter;

	ASSERT(cond != NULL);
	ASSERT(lock != NULL);
	ASSERT(!intr_context());
	ASSERT(lock_held_by_current_thread(lock));

	sema_init(&waiter.semaphore, 0);
	list_push_back(&cond->waiters, &waiter.elem);

	/* Edited Code - Jinhyen Kim
	   For Priority Scheduling, we wish to have the thread with the highest
			  priority to always be at the front of waiters.
	   However, the original code, list_push_back, adds every unblocked
			  thread to the end of waiters instead.
	   In order to maintain a descending hierarchy sort, we call the function
			  list_sort every time a new thread is pushed at the end of waiters.
	   Note: We use threadPriorityCompare to sort by descending order. */

	list_sort(&cond->waiters, threadPriorityCompare, 0);

	/* Edited Code - Jinhyen Kim (Project 1 - Priority Scheduling) */

	lock_release(lock);
	sema_down(&waiter.semaphore);
	lock_acquire(lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void cond_signal(struct condition *cond, struct lock *lock UNUSED)
{
	ASSERT(cond != NULL);
	ASSERT(lock != NULL);
	ASSERT(!intr_context());
	ASSERT(lock_held_by_current_thread(lock));

	if (!list_empty(&cond->waiters))

		/* Edited Code - Jinhyen Kim
		   To ensure the semaphore with the highest priority is
			  retrieved when we call list_pop_front, we call
			  list_sort first.
		   This command sorts the semaphores in descending order
			  of priority. */

		list_sort(&cond->waiters, semaphorePriorityCompare, 0);

	/* Edited Code - Jinhyen Kim (Project 1 - Priority Scheduling) */

	sema_up(&list_entry(list_pop_front(&cond->waiters),
						struct semaphore_elem, elem)
				 ->semaphore);
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void cond_broadcast(struct condition *cond, struct lock *lock)
{
	ASSERT(cond != NULL);
	ASSERT(lock != NULL);

	while (!list_empty(&cond->waiters))
		cond_signal(cond, lock);
}