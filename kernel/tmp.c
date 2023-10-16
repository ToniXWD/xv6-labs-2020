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

#define BIOBUCKNUM 13
#define BIOBUCKSIZE 4

struct bucket {
  struct spinlock  bucket_lock;
  struct buf buf[BIOBUCKSIZE];
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
  
  uint time; // 每一个bucket有自己的时间戳
};

// void
// timeinit(void)
// {
//   initlock(&time_lock, "bcache_time");
// }

// void
// increaseTime(uint* lastUse)
// {
//   acquire(&time_lock);
//   time++;
//   *lastUse = time;
//   wakeup(&time);
//   release(&time_lock);
// }

uint hash_bucket(uint dev, uint blockno) {
  return ((dev * blockno + dev + blockno)  + (dev & 0x2) + (blockno &0x3)) % BIOBUCKNUM;
}

struct {
  struct bucket buckets[BIOBUCKNUM];
} bcache;

void
binit(void)
{
  // timeinit();

  struct buf *b;
  char lockname[16];
  for (int i = 0 ; i < BIOBUCKNUM; i++) {
    snprintf(lockname, sizeof(lockname), "bcache_bucket%d", i);
    initlock(&bcache.buckets[i].bucket_lock, "bcache");

    bcache.buckets[i].time = 0;

    // 初始化bucket中的链表
    // Create linked list of buffers
    bcache.buckets[i].head.prev = &bcache.buckets[i].head;
    bcache.buckets[i].head.next = &bcache.buckets[i].head;

    for(b = bcache.buckets[i].buf; b < bcache.buckets[i].buf+BIOBUCKSIZE; b++) {
      b->next = bcache.buckets[i].head.next;
      b->prev = &bcache.buckets[i].head;

      initsleeplock(&b->lock, "buffer");

      bcache.buckets[i].head.next->prev = b;
      bcache.buckets[i].head.next = b;
    }
  }
}

void update_bucket_eliminate(struct buf *first, struct buf *head) {
  first->next->prev = head;
  head->next = first->next;
  
  first->next = head;
  first->prev = head->prev;

  head->prev->next = first;
  head->prev = first;
}

void update_bucket_release(struct buf *cur, struct buf *head) {
  cur->prev->next = cur->next;
  cur->next->prev = cur->prev;

  head->next->prev = cur;
  cur->next = head->next;

  head->next = cur;
  cur->prev = head;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint bucket_id = hash_bucket(dev, blockno);

  acquire(&bcache.buckets[bucket_id].bucket_lock);

  // Is the block already cached?
  bcache.buckets[bucket_id].time ++;

  for(b = bcache.buckets[bucket_id].head.next; b != &bcache.buckets[bucket_id].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      // increaseTime()
      b->refcnt++;
      b->lastUse = bcache.buckets[bucket_id].time;
      release(&bcache.buckets[bucket_id].bucket_lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  // 驱逐头部 buf
  b = bcache.buckets[bucket_id].head.next;
  update_bucket_eliminate(b, &bcache.buckets[bucket_id].head);
  b->dev = dev;
  b->blockno = blockno;
  b->valid = 0;
  b->lastUse = bcache.buckets[bucket_id].time;
  release(&bcache.buckets[bucket_id].bucket_lock);
  acquiresleep(&b->lock);
  return b;
}

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
  
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->lastUse = 0;
    uint bucket_id = hash_bucket(b->dev, b->blockno);
    acquire(&bcache.buckets[bucket_id].bucket_lock);
    update_bucket_release(b, &bcache.buckets[bucket_id].head);
    release(&bcache.buckets[bucket_id].bucket_lock);
  }
  releasesleep(&b->lock);
}

void
bpin(struct buf *b) {
  uint bucket_id = hash_bucket(b->dev, b->blockno);
  acquire(&bcache.buckets[bucket_id].bucket_lock);
  b->refcnt++;
  release(&bcache.buckets[bucket_id].bucket_lock);
}

void
bunpin(struct buf *b) {
  uint bucket_id = hash_bucket(b->dev, b->blockno);
  acquire(&bcache.buckets[bucket_id].bucket_lock);
  b->refcnt--;
  release(&bcache.buckets[bucket_id].bucket_lock);
}


