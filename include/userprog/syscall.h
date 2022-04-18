#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/synch.h"

void syscall_init (void);

typedef int pid_t;

static struct lock fileLock;

#endif /* userprog/syscall.h */
