#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int main()
{
	// user.h contains the definition of all system calls and is available to user space programs. 
	// getNumProc() system call is used from user.h to get the number of active processes in the system. 
	printf(1, "Number of Active Process: %d\n", getNumProc());
	exit();
}
