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

#define NBUCKET 13  // maximum number of buckets
#define HASH(bno) (bno % NBUCKET)

struct {
  struct buf buf[NBUF];
  struct spinlock replacelock;

  struct buf table[NBUCKET];
  struct spinlock tablelock[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.replacelock, "bcache_replacelock");
  for(int i=0; i<NBUCKET; i++){
    initlock(&bcache.tablelock[i], "bcache_tablelock");
    bcache.table[i].next = 0;
  }
  
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    initsleeplock(&b->lock, "buffer");
    b->next = bcache.table[0].next;
    bcache.table[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  uint id = HASH(blockno);

  // Is the block already cached?

  acquire(&bcache.tablelock[id]);
  for(b = bcache.table[id].next; b != 0; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.tablelock[id]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.

  // To avoid deadlock, release the currently held lock first
  // But still need to hold another lock to avoid more than one copy per block
  release(&bcache.tablelock[id]);
  acquire(&bcache.replacelock);

  // Check again is the block already cached?
  // No need to acquire bucket lock if only reading data
  // Because we hold the replace lock, data of bucket won`t be modified
  for(b = bcache.table[id].next; b != 0; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      acquire(&bcache.tablelock[id]);
      b->refcnt++;
      release(&bcache.tablelock[id]);
      release(&bcache.replacelock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Still not cached
  // Recycle the least recently used (LRU) unused buffer.
  // before_replace is the buffer before the replace buffer.
  // replace_bucket is the bucket index of replace buffer.
  struct buf *before_replace = 0;
  uint replace_bucket = -1;

  for(int i=0; i<NBUCKET; i++){
    acquire(&bcache.tablelock[i]);
    for(b = &bcache.table[i]; b->next != 0; b = b->next){
      if(b->next->refcnt == 0 && (!before_replace || b->next->tick < before_replace->next->tick)){
        before_replace = b;
        if(replace_bucket == i) continue;
        // if we found replace buffer in another bucket
        // release the previous bucket lock
        if(replace_bucket != -1) release(&bcache.tablelock[replace_bucket]);
        replace_bucket = i;
      }
    }
    if(replace_bucket != i)
      release(&bcache.tablelock[i]);
  }
  if(before_replace == 0 || replace_bucket == -1)
    panic("bget: no buffers");

  b = before_replace->next;
  if(replace_bucket != id){
    // remove cached buffer from replace_bucket
    // then we can release the replace_bucket lock
    before_replace->next = b->next;
    release(&bcache.tablelock[replace_bucket]);
    // acquire target bucket lock and insert buffer
    acquire(&bcache.tablelock[id]);
    b->next = bcache.table[id].next;
    bcache.table[id].next = b;
  }

  b->dev = dev;
  b->blockno = blockno;
  b->valid = 0;
  b->refcnt = 1;
  acquiresleep(&b->lock);
  release(&bcache.tablelock[id]);
  release(&bcache.replacelock);
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

  releasesleep(&b->lock);
  uint id = HASH(b->blockno);
  acquire(&bcache.tablelock[id]);
  b->refcnt--;
  if (b->refcnt == 0) {
    b->tick = ticks;
  }
  release(&bcache.tablelock[id]);
}

void
bpin(struct buf *b) {
  uint id = HASH(b->blockno);
  acquire(&bcache.tablelock[id]);
  b->refcnt++;
  release(&bcache.tablelock[id]);
}

void
bunpin(struct buf *b) {
  uint id = HASH(b->blockno);
  acquire(&bcache.tablelock[id]);
  b->refcnt--;
  release(&bcache.tablelock[id]);
}


