// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

// #define PAGEID(add) (((add) - (KERNBASE)) >> 12)


void freerange(void *pa_start, void *pa_end);

void kfree_cpu(void *pa, int id);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct cpu_freelist {
  struct spinlock lock;
  struct run *freelist;
};

// kmem被设计为一个cpu_freelist的数组
struct cpu_freelist kmem[NCPU];

void
kinit()
{
  char name[8];
  for (int i = 0 ; i < NCPU; i++) {
    snprintf(name, sizeof(name), "kmem_%d", i);
    initlock(&kmem[i].lock, name);
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    // 获取page序号
    // 初始化时按照对 NCPU 取余的方式选取空闲链表
    // uint64 list_id = PAGEID((uint64)p) % NCPU;
    uint64 list_id = (uint64)p % NCPU;
    
    kfree_cpu(p, list_id);
  }
}

void
kfree_cpu(void *pa, int id)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);
  
  // 增加对应的空闲列表
  r = (struct run*)pa;

  acquire(&kmem[id].lock);
  r->next = kmem[id].freelist;
  kmem[id].freelist = r;
  release(&kmem[id].lock);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  push_off();
  int id = cpuid();
  kfree_cpu(pa, id);
  pop_off();
}

void *
kalloc_cpu(int id)
{
  struct run *r;

  acquire(&kmem[id].lock);
  r = kmem[id].freelist;
  if(r) {
    // 如果当前 cpu 的空闲链表还有剩余
    kmem[id].freelist = r->next;
  } else {
    // 否则向别的cpu的空闲列表寻找地址
    for (int i = 0; i < NCPU; i++) {
      if (id == i) {
        continue;
      }
      acquire(&kmem[i].lock);
      r = kmem[i].freelist;
      if (r) {
        // 如果第 i 个空闲列表不为空, 借用它
        kmem[i].freelist = r->next;
        release(&kmem[i].lock);
        break;
      }
      release(&kmem[i].lock);
    }
  }
  release(&kmem[id].lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  push_off();
  int id = cpuid();
  void * addr = kalloc_cpu(id);
  pop_off();
  return addr;
}
