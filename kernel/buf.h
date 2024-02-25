struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  struct buf *prev; // LRU cache list
  struct buf *next;
  uchar data[BSIZE];//进阶：把这个换成一个指向4096字节块的指针，如果有映射了指向pa，没有映射就kalloc给页表分配映射。
};

