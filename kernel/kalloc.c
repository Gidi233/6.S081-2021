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

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct{
  struct spinlock lock;
  uint8 **ref_cnt;
} cowmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&cowmem.lock, "cowmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  uint64 num = 0;
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE,num++)
    kfree(p);
  cowmem.ref_cnt  = (uint8 **)kalloc();//就三万多空闲页，所以二维数组分配一页就可以（能存1024（uint8*）个4096(uint8)）
  memset(cowmem.ref_cnt, 0, PGSIZE);
  for (int i = 0; i < num / 4096 + 1; i++){
    cowmem.ref_cnt[i] = (uint8 *)kalloc();
    memset(cowmem.ref_cnt[i], 0, PGSIZE); //记得置为0
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

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
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
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

uint8 *
get_ref_cnt(void *pa)
{
  uint64 offset = ((uint64)pa - PGROUNDUP((uint64)end)) >> 12;
  return &cowmem.ref_cnt[offset >> 12][offset % 4096];
}

void *
cowalloc(void *pa)
{
  acquire(&cowmem.lock);
  uint8 *cnt = get_ref_cnt(pa);
  if (!*cnt)
    *cnt = 2;
  else
    (*cnt)++;
  release(&cowmem.lock);
  return (void *)pa;
}

int
cowcopy(pte_t *pte)
{
  uint8 *cnt;
  void *pa = (void *)PTE2PA(*pte);
  acquire(&cowmem.lock);
  if (*(cnt = get_ref_cnt(pa)) == 1){//因为可能有直接退出的，无法得知自己的引用计数为1，所以这里也可以不去除PTE_COW标识(去除是和cowfree一样的逻辑)
    *pte |= PTE_W;
    *pte &= ~PTE_COW;
    (*cnt)--;
  }
  else
  {
    uint64 *newpg;
    if((newpg= kalloc()) == 0)
      return 0;
    memmove(newpg, pa, PGSIZE); // (char*)
    *pte = (PA2PTE(newpg) | PTE_FLAGS(*pte) | PTE_W);//这就相当于unmap和map了
    *pte &= ~PTE_COW;
    (*cnt)--;
  }
  release(&cowmem.lock);
  return 1;
}

int
cowfree(void *pa)
{
  acquire(&cowmem.lock);
  uint8 *cnt = get_ref_cnt(pa);
  if (!(*cnt))
    return 0;
  if (!--(*cnt))
    kfree(pa);
  release(&cowmem.lock);
  return 1;
}
