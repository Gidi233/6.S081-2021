struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  uint ticks;//因为散列到哈希桶中就不存在一个总的LRU了，就只能用tick来判断谁是最不常用的
  struct buf *prev; // LRU cache list
  struct buf *next;
  uchar data[BSIZE];
};

