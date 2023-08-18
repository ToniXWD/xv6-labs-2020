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
struct spinlock map_lock;                   
char page_map[(PHYSTOP - KERNBASE) >> 12] = {0}; // page的引用计数

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&map_lock, "page_map");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    uint64 map_idx = PA2MAPIDX(PGROUNDDOWN((uint64)p));
    // printf("%d",map_idx);
    page_map[map_idx] = 1;
    kfree(p);
  }
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

  // 更改其引用计数
  uint64 map_idx = PA2MAPIDX(PGROUNDDOWN((uint64)pa));

  acquire(&map_lock);
  if (page_map[map_idx] > 0) {
      page_map[map_idx] --;
  }
  release(&map_lock);

  if (page_map[map_idx] == 0) {
    // 只有引用计数<=0时才将其放回空闲链表

    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);
    
    r = (struct run*)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
    // 记录其引用次数
    acquire(&map_lock);
    uint64 map_idx = PA2MAPIDX(PGROUNDDOWN((uint64)r));
    page_map[map_idx] = 1;
    release(&map_lock);
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
