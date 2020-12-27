// init: The initial user-level program

#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

char *argv[] = { "sh", 0 };

int
main(void)
{
  int pid, wpid;

  if(open("console", O_RDWR) < 0){
    mknod("console", 1, 1);
    open("console", O_RDWR);
  }
  dup(0);  // stdout
  dup(0);  // stderr
  
  #ifdef DEFAULT
    printf(1, "Scheduler policy: DEFAULT\n");
  #else
  #ifdef SJF
    printf(1, "Scheduler policy: SJF\n");
  #else
  #ifdef HBSJF
    printf(1, "Scheduler policy: Hybrid\n");
  #endif
  #endif
  #endif
  
  for(;;){
    printf(1, "init: starting sh\n");

    pid = fork();
    if(pid < 0){
      printf(1, "init: fork failed\n");
      exit();
    }
    if(pid == 0){
      exec("sh", argv);
      printf(1, "init: exec sh failed\n");
      exit();
    }
    while((wpid=wait()) >= 0 && wpid != pid)
      printf(1, "zombie!\n");
  }
}
