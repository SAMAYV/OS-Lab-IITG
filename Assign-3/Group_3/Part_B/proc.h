	// Segments in proc->gdt.	
#define NSEGS     7	
#define MAX_PSYC_PAGES 15	
#define MAX_TOTAL_PAGES 30	



// Per-CPU state
struct cpu {
  uchar apicid;                // Local APIC ID
  struct context *scheduler;   // swtch() here to enter scheduler
  struct taskstate ts;         // Used by x86 to find stack for interrupt
  struct segdesc gdt[NSEGS];   // x86 global descriptor table
  volatile uint started;       // Has the CPU started?
  int ncli;                    // Depth of pushcli nesting.
  int intena;                  // Were interrupts enabled before pushcli?
  struct proc *proc;           // The process running on this cpu or null
  
  // Cpu-local storage variables; see below	
  struct cpu *cpu;	
  //struct proc *proc;           // The currently-running process.
};

extern struct cpu cpus[NCPU];
extern int ncpu;


	
// Per-CPU variables, holding pointers to the	
// current cpu and to the current process.	
// The asm suffix tells gcc to use "%gs:0" to refer to cpu	
// and "%gs:4" to refer to proc.  seginit sets up the	
// %gs segment register so that %gs refers to the memory	
// holding those two variables in the local cpu's struct cpu.	
// This is similar to how thread-local variables are implemented	
// in thread libraries such as Linux pthreads.	
extern struct cpu *cpu asm("%gs:0");       // &cpus[cpunum()]	
extern struct proc *proc asm("%gs:4");     // cpus[cpunum()].proc

//PAGEBREAK: 17
// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc),
// because they are constant across kernel contexts.
// Don't need to save %eax, %ecx, %edx, because the
// x86 convention is that the caller has saved them.
// Contexts are stored at the bottom of the stack they
// describe; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save eip explicitly,
// but it is on the stack and allocproc() manipulates it.
struct context {
  uint edi;
  uint esi;
  uint ebx;
  uint ebp;
  uint eip;
};

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };


		
struct pgdesc {	
  uint swaploc;	
  int age;	
  char *va;	
};	
struct freepg {	
  char *va;	
  int age;	
  struct freepg *next;	
  struct freepg *prev;	
};	


// Per-process state
struct proc {
  uint sz;                     // Size of process memory (bytes)
  pde_t* pgdir;                // Page table
  char *kstack;                // Bottom of kernel stack for this process
  enum procstate state;        // Process state
  int pid;                     // Process ID
  struct proc *parent;         // Parent process
  struct trapframe *tf;        // Trap frame for current syscall
  struct context *context;     // swtch() here to run process
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)
  
  	
  //Swap file. must initiate with create swap file	
  struct file *swapFile;			//page file	
  int pagesinmem;             // No. of pages in physical memory	
  int pagesinswapfile;        // No. of pages in swap file	
  int totalPageFaultCount;    // Total number of page faults for this process	
  int totalPagedOutCount;     // Total number of pages that were placed in the swap file	
  struct freepg freepages[MAX_PSYC_PAGES];  // Pre-allocated space for the pages in physical memory linked list	
  struct pgdesc swappedpages[MAX_PSYC_PAGES];// Pre-allocated space for the pages in swap file array	
  struct freepg *head;        // Head of the pages in physical memory linked list	
  struct freepg *tail;        // End of the pages in physical memory linked list
  
};

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
