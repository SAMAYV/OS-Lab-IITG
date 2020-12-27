#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int main(int argc, char* argv[])
{	
	// get_burst_time() system call is used from user.h which returns the burst time of currently running process.
	int res = get_burst_time(); 

	if(res < 1) printf(1, "Couldn't set the Burst Time\n");
	else printf(1, "Burst time = %d\n", res);
	exit();
}
