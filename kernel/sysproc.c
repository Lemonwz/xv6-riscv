#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
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
  backtrace();
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

uint64
sys_sigalarm(void)
{
  struct proc *p;
  int ticks;
  uint64 handler;

  p = myproc();
  if(argint(0, &ticks) < 0){
    return -1;
  }
  if(argaddr(1, &handler) < 0){
    return -1;
  }
  p->interval = ticks;
  p->handler = handler;
  return 0;
}

uint64
sys_sigreturn(void)
{
  struct proc *p;

  p = myproc();
  printf("epc: %d\nhatrid: %d\nra: %d\nsp: %d\ns0: %d\ns1: %d\na0: %d\na1: %d\na2: %d\n",
    p->trapframe->epc, p->trapframe->kernel_hartid, p->trapframe->ra, p->trapframe->sp, p->trapframe->s0, p->trapframe->s1, p->trapframe->a0, p->trapframe->a1, p->trapframe->a2);
  printf("epc: %d\nhatrid: %d\nra: %d\nsp: %d\ns0: %d\ns1: %d\na0: %d\na1: %d\na2: %d\n",
    p->prevtrapframe->epc, p->prevtrapframe->kernel_hartid, p->prevtrapframe->ra, p->prevtrapframe->sp, p->prevtrapframe->s0, p->prevtrapframe->s1, p->prevtrapframe->a0, p->prevtrapframe->a1, p->prevtrapframe->a2);
  *p->trapframe = *p->prevtrapframe;
  p->ticks = 0;
  return 0;
}