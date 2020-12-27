//to generate child processes to test our scheduling algorithm

#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int main(int argc, char * argv[])
{
	int pid, k, n, x, z;
	
	if(argc != 2){
		printf(2, "Usage: %s n\n", argv[0]);	
	}
	
	n = atoi(argv[1]);
	
	for(k = 0; k < n; k++){
		pid = fork(); // Creates new process, copying lots from its parent, and set stack as if returning from a system-call.
		
		if(pid < 0){
			printf(1, "%d failed in fork!\n", getpid());
			exit();
		}
		else if(pid == 0){
			// child
			printf(1, "Child %d created \n", getpid());
			for(z = 0; z < 10000.0; z += 0.01)
        		 x =  x + 3.14 * 89.64;   // useless calculations to consume CPU time
			exit();
		}	
	}
	for(k = 0; k < n; k++) {
		wait();
	}
	exit();
}

