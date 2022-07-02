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

#define NBUCKET 13

struct {
  struct spinlock lock[NBUCKET];
  struct buf buf[NBUF];
  struct buf hash[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;

  for(int i = 0; i < NBUCKET; ++i) {
    initlock(&bcache.lock[i], "bcache");
  }

  b = &bcache.hash[0];
  for(int i = 0; i < NBUF; ++i) {
    b->next = &bcache.buf[i];
    b = b->next;
    initsleeplock(&b->lock, "buffer");
  }
}

int can_lock(int cur_idx, int req_idx){
  int mid = NBUCKET / 2;
  //non-reentrant
  if(cur_idx == req_idx){
    return 0;
  }else if(cur_idx < req_idx){
    if(req_idx <= (cur_idx + mid)){
      return 0;
    }
  }else{
    if(cur_idx >= (req_idx + mid)){
      return 0;
    }
  }
  return 1;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int bucket = blockno % NBUCKET;

  acquire(&bcache.lock[bucket]);
  b = bcache.hash[bucket].next;
  // Is the block already cached?
  while(b) {
    if(b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.lock[bucket]);
      acquiresleep(&b->lock);
      return b;
    }
    b = b->next;
  }

  int index = -1;
  uint64 min_tick = 0xffffffff;
  for(int i = 0; i < NBUCKET; i++){
    if(!can_lock(bucket, i)){
      continue;
    }else{
      acquire(&bcache.lock[i]);
    }
    b = bcache.hash[i].next;
    while(b){
      if(b->refcnt == 0){
        if(b->ticks < min_tick){
          min_tick = b->ticks;
          //release bucket lock[index] whose buf's time is the second smallest.
          if(index != -1 && index != i && holding(&bcache.lock[index])){
            release(&bcache.lock[index]);
          }
          index = i;
        }
      }
      b = b->next;
    }
    //release bucket lock[j] if j not referenced by index.
    if(i != index && holding(&bcache.lock[i])){
      release(&bcache.lock[i]);
    }
  }

  if(index == -1){
    panic("bget: no buffers");
  }

  //move buf from bucket[index] to bucket[bucket_id]
  struct buf *move = 0;
  b = &bcache.hash[index];
  while(b->next){
    if(b->next->refcnt == 0 && b->next->ticks == min_tick){
      b->next->dev = dev;
      b->next->blockno = blockno;
      b->next->valid = 0;
      b->next->refcnt = 1;
      //remove buf from the old bucket
      move = b->next;
      b->next = b->next->next;
      release(&bcache.lock[index]);
      break;
    }
    b = b->next;
  }

  // append buf to the new bucket
  b = &bcache.hash[bucket];
  while(b->next){
    b = b->next;
  }
  move->next = 0;
  b->next = move;
  release(&bcache.lock[bucket]);
  acquiresleep(&move->lock);
  return move;
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

  uint64 bucket = b->blockno % NBUCKET;
  acquire(&bcache.lock[bucket]);
  b->refcnt--;
  if (b->refcnt == 0) {
    b->ticks = ticks;
  }

  release(&bcache.lock[bucket]);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


