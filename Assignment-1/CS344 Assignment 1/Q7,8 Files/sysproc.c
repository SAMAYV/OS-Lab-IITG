#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
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

int
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

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// added a system call function of wolfie
int
sys_wolfie(void)
{
  // wolfie string
  char *a = "               \\t\\t,ood8888booo,\\n\\                                 \n"                               
            "                              ,od8           8bo,\\n\\                        \n"
            "                           ,od                   bo,\\n\\                     \n"
            "                         ,d8                       8b,\\n\\                   \n"
            "                        ,o                           o,    ,a8b\\n\\          \n"
            "                       ,8                             8,,od8  8\\n\\          \n"
            "                       8'                             d8'     8b\\n\\         \n"
            "                      8                           d8'ba     aP'\\n\\          \n"
            "                      Y,                       o8'         aP'\\n\\           \n"
            "                       Y8,                      YaaaP'    ba\\n\\             \n"
            "                         Y8o                   Y8'         88\\n\\            \n"
            "                         `Y8               ,8\\\"           `P\\n\\             \n"
            "                            Y8o        ,d8P'              ba\\n\\             \n"
            "                       ooood8888888P\\\"\\\"\\\"'                  P'\\n\\          \n"
            "                    ,od                                  8\\n\\               \n"
            "                 ,dP     o88o                           o'\\n\\               \n"
            "                ,dP          8                          8\\n\\                \n"
            "               ,d'   oo       8                       ,8\\n\\                 \n"
            "               $    d$\\\"8      8           Y    Y  o   8\\n\\                 \n"
            "              d    d  d8    od  \\\"\\\"boooooooob   d\\\"\\\" 8   8\\n\\             \n"
            "              $    8  d   ood' ,   8        b  8   '8  b\\n\\                 \n"
            "              $   $  8  8     d  d8        `b  d    '8  b\\n\\                \n"
            "               $  $ 8   b    Y  d8          8 ,P     '8  b\\n\\               \n"
            "               `$$  Yb  b     8b 8b         8 8,      '8  o,\\n\\             \n"
            "                    `Y  b      8o  $$o      d  b        b   $o\\n\\           \n"
            "                    8   '$     8$,,$\\\"      $   $o      '$o$$\\n\\            \n"
            "                      $o$$P\\\"                 $$o$\\n\\n\";                    \n";


  void* buf;
  int size;

  if(argint(1,&size) < 0){
    return -1;
  }

  // buffer will not be valid  
  if(argptr(0,(char**)&buf,size) < 0){
    return -1;
  }
  
  // p = size of wolfie buffer created
  int p = 0;
  for(p = 0; a[p] != '\0'; p++);

  if(p > size) p = -1;
  else 
  {
    // storing wolfie characters in buffer
    for(int i = 0; i < p; i++) ((char*)buf)[i] = a[i];
  }
  // returning size of wolfie
  // -1 is returned if buffer created is not enough to hold wolfie otherwise wolfie size is returned
  return p;
}