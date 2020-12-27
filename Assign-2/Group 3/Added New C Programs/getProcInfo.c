#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "processInfo.h"

int main(int argc, char* argv[])
{	
	//argv[0] is program name and argv[1][0] contains the process id from command line.
	int pid = (int)(argv[1][0]-'0');

	// declaring the user memory space for a process
	struct processInfo *p = malloc(sizeof(struct processInfo));

	// getProcInfo() system call is used from user.h which returns 0 if required process is found in the system and -1 otherwise. 
	int value = getProcInfo(pid, p);

	if(value == -1){
		printf(1, "Process Not Found\n");
	}
	else {
		// printing the information for the required process.
		printf(1, "Process ID: %d\nProcess Size: %d\nNumber of Context Switches: %d\n", p->ppid, p->psize, p->numberContextSwitches);	
	}
	exit();
}
