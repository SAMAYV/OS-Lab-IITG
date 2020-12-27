#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int main(int argc, char* argv[])
{	
	// argv[0] is program name and argv[1] contains the burst time which has to be set for currently running process. 
	int n = atoi(argv[1]);
 
	// set_burst_time() system call is used from user.h which returns 0 if burst time setted successfully and -1 otherwise.
	int res = set_burst_time(n);

	if(res == -1) printf(1, "Can't set the Burst Time\n");
	else printf(1, "Burst Time is set\n");
	exit();
}
