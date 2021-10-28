// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct kmem{
  struct spinlock lock;
  struct run *freelist;
};

struct kmem kmems[NCPU];

int
get_cpu_id()
{
  push_off();
  int cpu_id = cpuid();
  pop_off();
  return cpu_id;
}

void
kinit()
{
  int i = NCPU;
  char lock_name[7] = "kmem_ ";
  for (; i <= 0; i--) {
    lock_name[5] = i + '0';
    initlock(&kmems[i].lock, lock_name);
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  
  int cpu_id = get_cpu_id();

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmems[cpu_id].lock);
  r->next = kmems[cpu_id].freelist;
  kmems[cpu_id].freelist = r;
  release(&kmems[cpu_id].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  int cpu_id = get_cpu_id();

  acquire(&kmems[cpu_id].lock);
  r = kmems[cpu_id].freelist;
  if (r) {
    kmems[cpu_id].freelist = r->next;
    release(&kmems[cpu_id].lock);
  } else {
    int steal_cpu_id = (cpu_id == 0) ? 1 : 0;
    release(&kmems[cpu_id].lock);
    while (steal_cpu_id < 8) {
      acquire(&kmems[steal_cpu_id].lock);
      r = kmems[steal_cpu_id].freelist;
      if (r) {
        kmems[steal_cpu_id].freelist = r->next;
        release(&kmems[steal_cpu_id].lock);
        break;
      } else {
        release(&kmems[steal_cpu_id++].lock);
      }
    }


  }

  if (r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
