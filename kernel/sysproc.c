#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

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


#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  uint64 va;
  int count;
  uint64 buffer;
  if(argaddr(0, &va) < 0 ||
     argint(1, &count) < 0 ||
     argaddr(2, &buffer) < 0)
    return -1;

  int byte_count = count / 8;
  char *buf = kalloc();
  /*
    * Here, it is important to set the value to be 0.
    * Because `kalloc` writes some junk values.
  */
  memset(buf, 0, byte_count);

  va = PGROUNDDOWN(va);

  for(int i = 0; i < count; i++) {
    pagetable_t pagetable = myproc()->pagetable;
    for(int level = 2; level > 0; level--) {
      pte_t *pte = &pagetable[PX(level, va)];
      if(*pte & PTE_V) {
        pagetable = (pagetable_t)PTE2PA(*pte);
      } else {
        kfree((void*)buf);
        return -1;
      }
    }
    pte_t *pte = &pagetable[PX(0, va)];

    /*
      * Pay attention to the bit operation
    */
    buf[i / 8] |= (((*pte & PTE_A) >> 6) << (i % 8));
    *pte = (*pte) & (~PTE_A);
    va += PGSIZE;
  }
  int exit_value = copyout(myproc()->pagetable, buffer, buf, byte_count);
  kfree((void*)buf);

  return exit_value;
}
#endif

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
