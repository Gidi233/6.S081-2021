// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;//哨兵节点
} bcache;
#define NBUCKET 13
extern uint ticks;

struct buf table[NBUCKET];//桶为哨兵结点
struct spinlock lock[NBUCKET]; // 给每个桶加个锁，写入的时候先拿锁
char name[NBUCKET][16];//锁里的name是指针，所以不能再栈里分配数组

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  for(int i =0; i < NBUCKET; i++){//还用双向循环链表只是为了插入删除方便
    table[i].prev = &table[i];
    table[i].next = &table[i];    
    snprintf(name[i], 16, "lock[%d]", i);
    initlock(&lock[i], name[i]);
  }
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = table[0].next;
    b->prev = &table[0];
    initsleeplock(&b->lock, "buffer");
    table[0].next->prev = b;
    table[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int index = blockno % NBUCKET;
  acquire(&lock[index]);
  for (b = table[index].next; b != &table[index]; b = b->next)
  {
    if (b->blockno == blockno)
      break;
  }
  if (b!=&table[index])//找到了
  {
    b->refcnt++;
    release(&lock[index]);
    acquiresleep(&b->lock);
    return b;
  }
  else
  {
    release(&lock[index]);// 让其他线程可以遍历
    acquire(&bcache.lock); // 代表需要遍历找符合LRU的buf并steal出来,缓存到目标bucket

    // 
    acquire(&lock[index]);
    for (b = table[index].next; b != &table[index]; b = b->next)
    {
      if (b->blockno == blockno)
        break;
    }
    if (b != &table[index]) // 找到了
    {
      b->refcnt++;
      release(&lock[index]);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
    // 防止其他CPU与当前CPU需要的blockno相同，在该CPU拿到遍历锁后其他CPU已经把该block加入该桶

    struct buf *min = 0;
    uint8 flag = 0;
    for (int i = 0; i < NBUCKET; i++)
    {
      if(i==index)
        continue; // 注释掉是因为，之前依赖最开始加的锁，但如果两个cpu同时遍历，一直拿取自己需要的锁，就会导致，CPU1在等待CPU2释放最开始拿的锁，CPU2在等待CPU1遍历完成//->//之前没有上面再检查的那一段代码所以要注释掉
      acquire(&lock[i]);
    }
    for (struct buf *b = bcache.buf; b < &bcache.buf[NBUF]; b++) // 在遍历时必须拿到全部的锁
    {
      if (b->refcnt == 0)
      {
        if (!flag)
        {
          min = b;
          flag = 1;
        }
        else if (min->ticks > b->ticks)
          min = b;
      }
    }
    for (int i = 0; i < NBUCKET; i++)
    {
      if (i == index || i == min->blockno%NBUCKET)
        continue;
      release(&lock[i]);
    }
    // 看到了一个解决方案 一个一个桶遍历就不用拿取所有的锁，只用拿已找到的最小值的锁和当前在检查的桶的锁，不需要总的大锁也不需要用双向链表了，只用记录一个prev节点就可以了，比我这从总的缓冲区中找高效。（也更麻烦，而且我看他有些地方也可以优化）
    // https://www.cnblogs.com/duile/p/16389164.html

    if (flag)
    {
      min->next->prev = min->prev;
      min->prev->next = min->next;
      if (index != min->blockno % NBUCKET)
        release(&lock[min->blockno % NBUCKET]); // 之前在release(&lock[index]);之后，导致之前已经把min->blockno更改，导致sched locks（少释放一个锁）

      min->next = table[index].next;
      min->prev = &table[index];
      table[index].next->prev = min;
      table[index].next = min;
      //插入到另一个桶

      min->dev = dev;
      min->blockno = blockno;
      min->valid = 0;
      min->refcnt = 1;
      release(&lock[index]);
      release(&bcache.lock);
      acquiresleep(&min->lock);
      return min;
    }
  }
  panic("bget: no buffers");
}
// panic: freeing free block，假设两个进程同时要找同一个buf，并都缓存到同一个bucket[buk_id]中，这就是重复缓存，从而导致二次释放。

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&lock[b->blockno%NBUCKET]);
  b->refcnt--;
  if (b->refcnt == 0)
    b->ticks = ticks;
  release(&lock[b->blockno%NBUCKET]);
}

void
bpin(struct buf *b) {
  acquire(&lock[b->blockno%NBUCKET]);
  b->refcnt++;
  release(&lock[b->blockno%NBUCKET]);
}

void
bunpin(struct buf *b) {
  acquire(&lock[b->blockno%NBUCKET]);
  b->refcnt--;
  release(&lock[b->blockno%NBUCKET]);
}


