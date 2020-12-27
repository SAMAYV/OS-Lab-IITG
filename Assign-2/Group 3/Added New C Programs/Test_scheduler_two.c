#include "types.h"
#include "stat.h"
#include "user.h"
#include "processInfo.h"

void delay(int count)
{
    int* data = (int*) malloc(sizeof(int) * 10240);
    if(data <= 0)
        printf(1, "Memory not allocated due to some error\n");

    for(int i = 0; i < count; i++){
        for(int k = 0; k < 570; k++)
            for(int j = 0; j < 10240; j++)
                data[j]++;
    }
}
void io_delay(int cnt)
{
	cnt += cnt+100;
	int x = 0, y = 100,z = 0,k = 0;
	
	int* data = (int*)malloc(sizeof(int) * 10240);
	for(int i=0;i<=10000*cnt*cnt;++i){
		x += y*10;
		x /= 10;
		z++;
		int t = k%10240;
		data[t]++;
		k++;
		k %= 31000;
	}
}
int main(int argc, char *argv[])
{
    if(argc < 2){
        printf(2, "usage: %s n\n", argv[0]);
        exit();
    }
    
    printf(1, " ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~First Testcase:~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ \n\n");
    
    int N = atoi(argv[1]);
    int pids[N];            // contains the order in which child completes 
    int rets[N];            // contains the order in which child terminates and returned to parent
    int burst_t[N];
    int context_switches[N];
    struct processInfo p_info;
    
    for(int i = 0; i < N; i++)
    {
        int burst_time = (23*(i+1)) % 20;
        
        burst_t[i] = burst_time;
        int id = fork();

        // fork() returns 0 when OS runs the child process 
        if(id == 0){
            set_burst_time(burst_time);         // setting the burst time of newly created child process
            delay(burst_time);                  // to waste CPU time so that child doesn't terminate 
            exit();                             // terminating child process
        }
        else if(id > 0){
            // Returned to parent. pids[i] contains process ID of the child process just completed and returned back to parent.
            pids[i] = id;
        }
        else {
            // creation of a child process was unsuccessful
            exit();
        }
    }
	
    for(int i = 0; i < N; i++){
        // wait() returns process ID of the terminated child process. 
        rets[i] = wait();
        getProcInfo(pids[i], &p_info);
        context_switches[i] = p_info.numberContextSwitches;
    }

    printf(1, "\nOrder in which children completed\n");
    for (int i = 0; i < N; i++){
        printf(2, "pid = %d 	burst time = %d  ContextSwitches =  %d\n", pids[i], burst_t[i],context_switches[i]);
    }  

    printf(1, "\nOrder in which children terminates and returns back to parent\n");
    for(int i = 0; i < N; i++){
        printf(2, "pid = %d 	burst time = %d 	\n", rets[i], burst_t[rets[i]-pids[0]]);
    }
	

	printf(1, "\n\n\n ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~2nd Testcase: ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ \n\n\n");
		
	for(int i = 0; i < N; i++)
    {
        int burst_time = (23*(i+1)) % 20;
        burst_t[i] = burst_time;
        int id = fork();

        // fork() returns 0 when OS runs the child process 
        if(id == 0){
            if(i%2 == 0){                           //heavy cpu processes
		        set_burst_time(burst_time);         // setting the burst time of newly created child process
		        delay(burst_time);                  // to waste CPU time so that child doesn't terminate
			}
			else {
				set_burst_time(burst_time);
				io_delay(burst_time);
				sleep(1);
				yield();
			}
			exit();                                  // terminating child process
        }
        else if(id > 0){
            // Returned to parent. pids[i] contains process ID of the child process just completed and returned back to parent.
            pids[i] = id;
        }
        else {
            // creation of a child process was unsuccessful
            exit();
        }
    }
    
    for(int i = 0; i < N; i++){
        // wait() returns process ID of the terminated child process. 
        rets[i] = wait();
        getProcInfo(pids[i], &p_info);
        context_switches[i] = p_info.numberContextSwitches;
    }
	
	printf(1, "\nOrder in which children completed\n");
	for(int i = 0; i < N; i++){ 
		   printf(2, "pid = %d 	burst time = %d  ContextSwitches =  %d\n", pids[i], burst_t[i],context_switches[i]);
    }  

    for(int itr = 0;itr < 2; ++itr){
    	if(itr == 0) printf(1, "\n *************Heavy CPU Bound Processes************* \n\n");
    	else printf(1, "\n *************IO Bound Processes*************\n\n");
    	
    	printf(1, "\nOrder in which children terminates and returns back to parent\n");
    	for(int i = 0; i < N; i++){
    		if((rets[i]-pids[0])%2 == itr)		
        	   printf(2, "pid = %d 	   burst time = %d 	   \n", rets[i], burst_t[rets[i]-pids[0]]);
        }
    }
    exit();
}
