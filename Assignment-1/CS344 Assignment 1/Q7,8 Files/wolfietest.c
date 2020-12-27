#include "types.h"
#include "stat.h"
#include "user.h"

int
main(void)
{
	// declaring a buffer of 2997 bytes (enough to hold wolfie) to hold wolfie.
	char *buf = malloc(2997);

	// p = value returned from sys_wolfie function in sysproc.c file which is size of wolfie
	int p = wolfie(buf,2997);

	if(p == -1){
		// if size is less due to unavailability of buffer then print small size
		printf(1, "ERROR: Size Too Small.\n");
	}
	else {
		// is size is enough to hold wolfie print the wolfie buffer size
		printf(1,"Wolfie Size = %d Bytes\n",p);
		// then printing the wolfie
		printf(1,"%s\n",buf);
	}
	return 0;
}