#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"

char buf[8192];
char name[3];
char *echoargv[] = { "echo", "ALL", "TESTS", "PASSED", 0 };
int stdout = 1;
#define TOTAL_MEMORY (1 << 20) + (1 << 18)

void
mem(void)
{
	printf(1,"Test Started");	
	int pid;
	printf(1, "\n ** Forking ** \n");
	int size = 4096;
	for(int i=0;i<20;++i){
		pid = fork();
		if(pid > 0) {wait();}
		else if(pid < 0) { printf(1, "Fork Failed\n"); }
		else {
			for(int j=0;j<10;++j){
				char *memory = (char*) malloc(size); //4kb;
				if (memory == 0) goto failed;
				for(int k=0;k<size;++k){
					memory[k] = (char)(65+(k%26));
				}
				for(int k=0;k<size;++k){
					if(memory[k] != (char)(65+(k%26))) goto failed;
				}
			}
		}
	}
	printf(1, "Memory OKKKK %d\n", bstat());
	exit();
	
failed:
	printf(1, "test failed!\n");
	exit();
}

int
main(int argc, char *argv[])
{
	printf(1, "Memtest starting\n");
	mem();
	return 0;
}
