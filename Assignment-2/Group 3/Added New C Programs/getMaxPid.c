#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int main()
{
	// getMaxPid() system call is used from user.h to get the process having maximum Process Id.
	printf(1, "Max PID: %d\n", getMaxPid());
	exit();
}
