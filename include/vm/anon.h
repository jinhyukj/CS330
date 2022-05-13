#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"
struct page;
enum vm_type;

/* Edited Code - Jinhyen Kim 
   For Swap In/Out, we need to track down memory frames that are
      not being used.
   In pintos, the structure bitmap is used to track usage. */

struct bitmap *memFrameTable;

/* Edited Code - Jinhyen Kim (Project 3 - Swap In/Out) */

struct anon_page {
};

void vm_anon_init (void);
bool anon_initializer (struct page *page, enum vm_type type, void *kva);

#endif
