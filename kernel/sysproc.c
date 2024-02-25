#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "fcntl.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// length 参数指定了映射的字节数。尽管 length 无需是一个系统分页大小（sysconf(_SC_PAGESIZE)返回值）的倍数，但内核会以分页大小为单位来创建映射，因此实际上 length 会被向上提升为分页大小的下一个倍数。
uint64
sys_mmap(void)
{
  uint64 addr;
  int length, prot, flags, fd, offset;
  if (argaddr(0, &addr) < 0 || argint(1, &length) < 0 || argint(2, &prot) < 0 || argint(3, &flags) < 0 || argint(4, &fd) < 0 || argint(5, &offset) < 0)
    return -1;
  struct proc *p = myproc();
  int ret = PGROUNDUP(p->sz);
  int i;
  for ( i = 0; i < NVMA; i++)
  {
    if (!(p->vmas[i].valid)){
      break;
    }
  }
  if(i == NVMA)
    panic("no more vma");
  else{
    if(prot&PROT_READ)
      if (!p->ofile[fd]->readable)
        return -1;
    if (prot & PROT_WRITE)
      if (!p->ofile[fd]->writable&&flags !=MAP_PRIVATE)
        return -1;
    p->vmas[i].valid = 1;
    p->vmas[i].addr = PGROUNDUP(p->sz);//sbrk导致p->sz不一定页面对齐
    p->vmas[i].length = PGROUNDUP(length);
    p->vmas[i].prot = prot;
    p->vmas[i].flags = flags;
    p->vmas[i].mapfile= p->ofile[fd];
    p->vmas[i].offset = offset;
  }
  p->sz = p->vmas[i].addr+length;
  filedup(p->ofile[fd]);
  return ret;
}

// length 参数是一个非负整数，它指定了待解除映射区域的大小（字节数）。范围为系统分页大小的下一个倍数的地址空间将会被解除映射。
uint64
sys_munmap(void){
  uint64 addr;
  int length;
  if (argaddr(0, &addr) < 0 || argint(1, &length) < 0)
    return -1;
  return myunmap(addr, PGROUNDUP(length));
}