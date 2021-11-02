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

#define BUCKETS 13

struct {
  // 为每个哈希桶分配自旋锁
  struct spinlock lock[BUCKETS];
  struct buf buf[NBUF];

  // 为每个哈希桶分配头指针
  struct buf hashbucket[BUCKETS];
} bcache;

void
binit(void)
{
  struct buf *b;

  // 为每个哈希桶的头指针初始化
  for (int i = 0; i < BUCKETS; i++) {
    initlock(&bcache.lock[i], "bcache");
    bcache.hashbucket[i].prev = &bcache.hashbucket[i];
    bcache.hashbucket[i].next = &bcache.hashbucket[i];
  }
  
  // 开始时将所有内存块均放在 0 号哈希桶中
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.hashbucket[0].next;
    b->prev = &bcache.hashbucket[0];
    initsleeplock(&b->lock, "buffer");
    bcache.hashbucket[0].next->prev = b;
    bcache.hashbucket[0].next = b;
  }
}

static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int hashed_blkno = blockno % BUCKETS;

  // 拿自己的哈希桶的自旋锁
  acquire(&bcache.lock[hashed_blkno]);

  // 查看缓存是否命中
  for(b = bcache.hashbucket[hashed_blkno].next; b != &bcache.hashbucket[hashed_blkno]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[hashed_blkno]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // 若没有命中，从本哈希桶中寻找空闲的缓存块
  for (b = bcache.hashbucket[hashed_blkno].prev; b != &bcache.hashbucket[hashed_blkno]; b = b->prev){
    if (b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock[hashed_blkno]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  /**
   * 若自己的哈希桶中没有空闲的缓存块
   * 遍历所有哈希桶，寻找空闲的缓存块
   * 其中，每一次找哈希桶开始遍历前先释放先前的自旋锁
   * 再拿即将遍历的哈希桶的自旋锁
   **/
  release(&bcache.lock[hashed_blkno]);
  for (int i = 0; i < BUCKETS; i++) {
    if (i == hashed_blkno) {
      continue;
    }
    acquire(&bcache.lock[i]);
    for (b = bcache.hashbucket[i].prev; b != &bcache.hashbucket[i]; b = b->prev) {
      if (b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;

        // 将缓存块 b 从它所在的哈希桶中取出来
        b->next->prev = b->prev;
        b->prev->next = b->next;
        release(&bcache.lock[i]);

        // 并放到原本的哈希桶中
        acquire(&bcache.lock[hashed_blkno]);
        b->next = bcache.hashbucket[hashed_blkno].next;
        b->prev = &bcache.hashbucket[hashed_blkno];
        bcache.hashbucket[hashed_blkno].next->prev = b;
        bcache.hashbucket[hashed_blkno].next = b;
        release(&bcache.lock[hashed_blkno]);

        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&bcache.lock[i]);
  }
  // release(&bcache.lock[hashed_blkno]);
  panic("bget: no buffers");
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

  int bucket_no = b->blockno % BUCKETS;

  acquire(&bcache.lock[bucket_no]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.hashbucket[bucket_no].next;
    b->prev = &bcache.hashbucket[bucket_no];
    bcache.hashbucket[bucket_no].next->prev = b;
    bcache.hashbucket[bucket_no].next = b;
  }
  
  release(&bcache.lock[bucket_no]);
}

void
bpin(struct buf *b) {
  int bucket_no = b->blockno % BUCKETS;
  acquire(&bcache.lock[bucket_no]);
  b->refcnt++;
  release(&bcache.lock[bucket_no]);
}

void
bunpin(struct buf *b) {
  int bucket_no = b->blockno % BUCKETS;
  acquire(&bcache.lock[bucket_no]);
  b->refcnt--;
  release(&bcache.lock[bucket_no]);
}


