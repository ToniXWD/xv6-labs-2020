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

#define BUCKETNUM 13
#define BUCKETSIZE 4

struct map {
  struct spinlock bcache_map_lock;
  struct buf buf[BUCKETSIZE];
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
};

struct {
  struct map buckets[BUCKETNUM];
} bcache;

uint
get_bucket_id(uint dev, uint blockno) {
  return (dev * blockno + dev + blockno) % BUCKETNUM;

}
void
binit(void)
{
  struct buf *b;

  for (int i = 0; i< BUCKETNUM; i++) {
    initlock(&bcache.buckets[i].bcache_map_lock, "bcache_map");

    // Create linked list of buffers
    bcache.buckets[i].head.prev = &bcache.buckets[i].head;
    bcache.buckets[i].head.next = &bcache.buckets[i].head;
    for(b = bcache.buckets[i].buf; b < bcache.buckets[i].buf+BUCKETSIZE; b++){
      b->next = bcache.buckets[i].head.next;
      b->prev = &bcache.buckets[i].head;
      b->refcnt = 0;
      initsleeplock(&b->lock, "buffer");
      bcache.buckets[i].head.next->prev = b;
      bcache.buckets[i].head.next = b;
    }
  }

}

void update_bucket_use(struct buf *cur, struct buf *head) {
  struct buf *prev = cur->prev;

  cur->next->prev = prev;
  prev->next = cur->next;

  cur->next = head;
  cur->prev = head->prev;

  head->prev->next = cur;
  head->prev = cur;
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

  uint bucket_id = get_bucket_id(dev, blockno);

  acquire(&bcache.buckets[bucket_id].bcache_map_lock);

  // Is the block already cached?
  for(b = bcache.buckets[bucket_id].head.next; b != &bcache.buckets[bucket_id].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      update_bucket_use(b, &bcache.buckets[bucket_id].head);
      release(&bcache.buckets[bucket_id].bcache_map_lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  // 将第一个元素驱逐使用, 置于链表末尾
  b = bcache.buckets[bucket_id].head.next;
  update_bucket_use(b, &bcache.buckets[bucket_id].head);
  b->dev = dev;
  b->blockno = blockno;
  b->valid = 0;
  b->refcnt = 1;
  release(&bcache.buckets[bucket_id].bcache_map_lock);
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
  
  uint bucket_id = get_bucket_id(b->dev, b->blockno);

  b->refcnt--;
  if (b->refcnt == 0) {
    
    // no one is waiting for it.
    acquire(&bcache.buckets[bucket_id].bcache_map_lock);
    update_bucket_release(b, &bcache.buckets[bucket_id].head);
    release(&bcache.buckets[bucket_id].bcache_map_lock);
  }
  releasesleep(&b->lock);
}

void
bpin(struct buf *b) {
  uint bucket_id = get_bucket_id(b->dev, b->blockno);
  acquire(&bcache.buckets[bucket_id].bcache_map_lock);
  b->refcnt++;
  release(&bcache.buckets[bucket_id].bcache_map_lock);
}

void
bunpin(struct buf *b) {
  uint bucket_id = get_bucket_id(b->dev, b->blockno);
  acquire(&bcache.buckets[bucket_id].bcache_map_lock);
  b->refcnt--;
  release(&bcache.buckets[bucket_id].bcache_map_lock);
}


