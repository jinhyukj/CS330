#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/synch.h"

void syscall_init (void);

/* Edited Code - Jinhyen Kim
   Since some of the system call functions use the type 
      pid_t, we declare it here. */

typedef int pid_t;

/* Edited Code - Jinhyen Kim (Project 2 - System Call) */

/* Edited Code - Jinhyen Kim 
   When we work with files at System Call, we need a lock.
   We declare a lock here. */

static struct lock fileLock;

/* Edited Code - Jinhyen Kim (Project 2 - System Call) */

#endif /* userprog/syscall.h */
