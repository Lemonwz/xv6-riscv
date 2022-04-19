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
  struct run *stealist;
};

struct kmem kmems[NCPU];

void
kinit()
{
  // initlock(&kmem.lock, "kmem");
  // freerange(end, (void*)PHYSTOP);
  for(int i=0; i<NCPU; i++){
    char name[20];
    snprintf(name, sizeof(name), "kmem%d", i);
    initlock(&kmems[i].lock, name);
  }
  // give all free memory to the CPU running freerange. 
  freerange(end, (void*)PHYSTOP);
  printf("page num: %d\n", (PHYSTOP-(uint64)end)/PGSIZE);
  kmems->stealist = 0;
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

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  push_off();
  int id = cpuid();
  acquire(&kmems[id].lock);
  r->next = kmems[id].freelist;
  kmems[id].freelist = r;
  release(&kmems[id].lock);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int id = cpuid();
  acquire(&kmems[id].lock);
  if(!kmems[id].freelist){
    // Release currently held cpu lock first
    // Steal 64 free pages from other cpu
    // And put it into current cpu`s stealist
    // No need to hold a lock for stealist, because it is exclusive to current cpu
    // With push_off(no process switch), it is safe to modify stealist
    int stealnum = 64;
    release(&kmems[id].lock);
    for(int bid=0; bid<NCPU; bid++){
      if(bid == id) continue;
      acquire(&kmems[bid].lock);
      struct run *rr = kmems[bid].freelist;
      while(rr && stealnum){
        kmems[bid].freelist = rr->next;
        rr->next = kmems[id].stealist;
        kmems[id].stealist = rr;
        rr = kmems[bid].freelist;
        stealnum--;
      }
      release(&kmems[bid].lock);
      if(stealnum == 0) break;
    }
    // Put free pages in stealist into current cpu`s freelist
    r = kmems[id].stealist;
    acquire(&kmems[id].lock);
    while(r != 0){
      kmems[id].stealist = r->next;
      r->next = kmems[id].freelist;
      kmems[id].freelist = r;
      r = kmems[id].stealist;
    }
  }
  
  r = kmems[id].freelist;
  if(r)
    kmems[id].freelist = r->next;
  release(&kmems[id].lock);
  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
