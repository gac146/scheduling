/* mykernel2.c: your portion of the kernel
 *
 *   	Below are procedures that are called by other parts of the kernel. 
 * 	Your ability to modify the kernel is via these procedures.  You may
 *  	modify the bodies of these procedures any way you wish (however,
 *  	you cannot change the interfaces).  
 */

#include "aux.h"
#include "sys.h"
#include "mykernel2.h"

#define TIMERINTERVAL 1		// in ticks (tick = 10 msec)
#define MIN_CPU_REQUEST 0
#define MAX_CPU_REQUEST 100
#define L 100000
#define MAX_INT 2147483647 

/* 	A sample process table. You may change this any way you wish.  
 */

static struct {
	int valid;		// is this entry valid: 1 = yes, 0 = no
	int pid;		// process ID (as provided by kernel)

	// only use for proportion policy
	int requested;
	int stride;
	int percent;
	int runnable;
 	int passValue;
} proctab[MAXPROCS];

// Varaibles to keep track of Stack/Queue
static int head;	
static int tail;	
static int isEmpty;

// Variables to control proportional policy
static int currCPURequested = 0;

// Enables comments in the program
static int comment = 0;

/*--------------- FIFO Procedures ------------------------*/

/*
 * Function to keep track of the tail of the queue
 */
int enterQueue(int p) {
	if(tail % MAXPROCS == head && !isEmpty) return 0;
	else isEmpty = 0;

	proctab[tail].valid = 1;
	proctab[tail].pid = p;
	tail = ++tail % MAXPROCS;
		
	return (1);
}


/*
 * Function to keep track of the head of the queue 
 */

int exitQueue(int p) {
	if(!proctab[head].valid || proctab[head].pid != p) return 0;
	
	proctab[head].valid = 0;
	head++;
	head = head % MAXPROCS;
	
	if(head == tail) {
		isEmpty = 1;
		head = tail = 0;
	}

	return(1);
}
	
/*------------------- LIFO Procedures -------------------------*/

/*
 * Function to increment and keep track of the head of the Stack
 */
int enterStack(int p) {
	
	if(head % MAXPROCS == tail && !isEmpty) return 0;
	else isEmpty = 0;

 	proctab[head].valid = 1;
	proctab[head].pid = p;
	head = ++head % MAXPROCS;
 	
	return(1);
}

/*
 * Function to keep track of the tail of the Stack
 */
int exitStack(int p) {
	if(!proctab[--head].valid || proctab[head].pid != p) return 0;

	proctab[head].valid = 0;
	head = head % MAXPROCS;

	if(head == tail) {
		isEmpty = 0;
		head = tail = 0;
	}

	return (1);
}

/*-------------------- Round Robin Procedures -------------------*/

/*
 * Moves the next procedure to the top of the queue and sends the current
 * one to the tail
 */
int switchProc() {
	if(isEmpty) return 0;
	if(head == tail) return 1;

	int prevHead = head % MAXPROCS;

	proctab[prevHead].valid = 0;
	head = ++head % MAXPROCS;
	tail = ++tail % MAXPROCS;

	int tmpTail = ((tail-1) < 0) ? 9 : (tail - 1);	
	
	proctab[tmpTail] = proctab[prevHead];
	proctab[tmpTail].valid = 1;

	return (1);
}



/*-------------------- Proportion Procedures -----------------*/


/*
 * Determines if an index is in range
 */
int isInRange(int i) {
	if(i < 0 || i >= MAXPROCS) return 0;
 	else return 1;
}


/*
 * Substract the minPassValue from all valid procs
 */ 
void resetOverflow(int minPassValue) {
	
	int i;
	for(i=0; i < MAXPROCS; i++) {
		if(proctab[i].valid)	proctab[i].passValue -= minPassValue;
	}
}


/*
 * Distribute the CPU usage proportionally
 */
void distributeCPU() {
	
	currCPURequested = 0; 		// Recalculating usage of CPU
	int unrequestedCount = 0;
	int percent = 0;
	int i;

	// distribuiting requested procs
	for(i=0; i < MAXPROCS; i++) {
		if(proctab[i].valid) {
			if(proctab[i].requested) {
				currCPURequested += proctab[i].percent;
				proctab[i].runnable = 1;
			} else  unrequestedCount++;
		}
	}

	// distribuiting unrequested proc
	int numerator = MAX_CPU_REQUEST - currCPURequested;
	if(unrequestedCount > 0)	percent = (numerator >= unrequestedCount) ? numerator / unrequestedCount : 0;
	
	for(i=0; i < MAXPROCS; i++) {
		if(proctab[i].valid && !proctab[i].requested) {
			if(percent > 0) {
				proctab[i].stride = L / percent;
				proctab[i].runnable = 1;
			} else  proctab[i].runnable = 0;
		}
	}
		
}

/*
 * Determines which proc to run next. Increments pass value of min proc by its stride
 */
int runProc() {
	
	int min = MAX_INT;
	int minIndex;
	int i;
	
	// Finding min and saving its value
	for(i=0; i < MAXPROCS; i++) {
		if(proctab[i].runnable) {
			if(proctab[i].passValue < min) {
				min = proctab[i].passValue;
				minIndex = i;		
			}
		}	
	}

	// Checking for overflow
	if(proctab[minIndex].passValue + proctab[minIndex].stride < proctab[minIndex].passValue) {
		resetOverflow(proctab[minIndex].passValue);
	}

	proctab[minIndex].passValue += proctab[minIndex].stride;
	
	return minIndex;
}


/*------------------------------------------------------------*/

/*  	InitSched () is called when kernel starts up. First, set the
 * 	scheduling policy (see sys.h). Make sure you follow the rules
 *   	below on where and how to set it.  Next, initialize all your data
 * 	structures (such as the process table).  Finally, set the timer
 *  	to interrupt after a specified number of ticks. 
 */

void InitSched ()
{
	int i;

	/* First, set the scheduling policy.  You should only set it
	 * from within this conditional statement. While you are working
	 * on this assignment, GetSchedPolicy () will return NOSCHEDPOLICY. 
	 * Thus, the condition will be true and you may set the scheduling
	 * policy to whatever you choose (i.e., you may replace ARBITRARY).  
	 * After the assignment is over, during the testing phase, we will
	 * have GetSchedPolicy () return the policy we wish to test.  Thus
	 * the condition will be false and SetSchedPolicy (p) will not be
	 * called, thus leaving the policy to whatever we chose to test. 
	 */
	if (GetSchedPolicy () == NOSCHEDPOLICY) {	// leave as is
		SetSchedPolicy (ROUNDROBIN);			// set policy here
	}
	
	/* Initialize all your data structures here */
	head = 0;
	tail = 0;
	isEmpty = 1;
	
	for (i = 0; i < MAXPROCS; i++) {
		proctab[i].valid = 0;
	}

	/* Set the timer last */
	SetTimer (TIMERINTERVAL);
}


/*  	StartingProc (p) is called by the kernel when the process
 * 	identified by PID p is starting.  This allows you to record the
 *  	arrival of a new process in the process table, and allocate
 * 	any resources (if necessary). Returns 1 if successful, 0 otherwise. 
 */

int StartingProc (p)
	int p;				// process that is starting
{
	int i;
	int returnVal;

	if(GetSchedPolicy() == ARBITRARY) {
		for (i = 0; i < MAXPROCS; i++) {
			if (! proctab[i].valid) {
				proctab[i].valid = 1;
				proctab[i].pid = p;
				return (1);
			}
		}
	} else if(GetSchedPolicy() == FIFO) {
		returnVal = enterQueue(p);
	} else if(GetSchedPolicy() == LIFO) {
		DoSched();
		returnVal = enterStack(p);
	} else if(GetSchedPolicy() == ROUNDROBIN) {
		returnVal = enterQueue(p);
	} else {
		proctab[p-1].pid = p;		// Initializing standard vars
		proctab[p-1].valid = 1;
	
		proctab[p-1].stride = 0;	// Initializing proportion vars
		proctab[p-1].runnable = 1;
		proctab[p-1].requested = 0;
		proctab[p-1].percent = 0;
		
		distributeCPU();		// Distribuiting CPU
		returnVal = 1;
	}
	
	if(!returnVal) {
		DPrintf ("Error in StartingProc: no free table entries\n");
	}
	
	return (returnVal);
}
			

/*   	EndingProc (p) is called by the kernel when the process
 * 	identified by PID p is ending.  This allows you to update the
 *  	process table accordingly, and deallocate any resources (if
 *  	necessary).  Returns 1 if successful, 0 otherwise. 
 */


int EndingProc (p)
	int p;				// process that is ending
{
	int i;
	int returnVal;
	
	if(GetSchedPolicy() == ARBITRARY) {
		for (i = 0; i < MAXPROCS; i++) {
			if (proctab[i].valid && proctab[i].pid == p) {
				proctab[i].valid = 0;
				return (1);
			}
		}
	} else if(GetSchedPolicy() == FIFO) {
		returnVal = exitQueue(p);
	} else if(GetSchedPolicy() == LIFO) {
		returnVal = exitStack(p);
		DoSched();
	} else if(GetSchedPolicy() == ROUNDROBIN) {
		returnVal = exitQueue(p);
	} else {
		proctab[p-1].pid = p;			// Setting standard vars
		proctab[p-1].valid = 0;
		
		proctab[p-1].stride = 0;		// Setting proportion vars upon exit
		proctab[p-1].runnable = 0;
		proctab[p-1].requested = 0;
		proctab[p-1].percent = 0;

		distributeCPU();			// Distribuiting cpu
		returnVal = 1;
	}
	
	if(!returnVal) {
		DPrintf ("Error in StartingProc: no free table entries\n");
	}
	
	return (returnVal);


}


/* 	SchedProc () is called by kernel when it needs a decision for
 *  	which process to run next.  It calls the kernel function
 * 	GetSchedPolicy () which will return the current scheduling policy
 *   	which was previously set via SetSchedPolicy (policy). SchedProc ()
 * 	should return a process PID, or 0 if there are no processes to run. 
 */

int SchedProc ()
{
	int i;

	switch (GetSchedPolicy ()) {

	case ARBITRARY:

		for (i = 0; i < MAXPROCS; i++) {
			if (proctab[i].valid) {
				if(comment)	DPrintf("proc %d selected\n", i);
				return (proctab[i].pid);
			}
		}
		break;

	case FIFO: 
		{
		
			if(proctab[head].valid) {
				return (proctab[head].pid);
			}
		}

	case LIFO:
		{

			if(proctab[head-1].valid)
				return (proctab[head-1].pid);
		
			break;
		}

	case ROUNDROBIN:
		{
			if(switchProc()) {
				if(proctab[head].valid) { 
					if(comment)	DPrintf("curr proc = %d\n", proctab[head].pid);
					return (proctab[head].pid);
				}
			} 

			break;
		}

	case PROPORTIONAL:
		{
			int i = runProc();
			if(proctab[i].valid && isInRange(i)) {
				return (proctab[i].pid);
			}
			break;
		}

	}
	
	return (0);
}


/*  	HandleTimerIntr () is called by the kernel whenever a timer
 *  	interrupt occurs.  Timer interrupts should occur on a fixed
 * 	periodic basis.
 */

void HandleTimerIntr ()
{
	SetTimer (TIMERINTERVAL);

	switch (GetSchedPolicy ()) {	// is policy preemptive?

	case ROUNDROBIN: {		// ROUNDROBIN is preemptive
		DoSched();
		break;
	}
	case PROPORTIONAL: {		// PROPORTIONAL is preemptive
		DoSched ();		// make scheduling decision
		break;
	}

	default:			// if non-preemptive, do nothing
		break;
	}
}

/*  	MyRequestCPUrate (p, n) is called by the kernel whenever a process
 * 	identified by PID p calls RequestCPUrate (n).  This is a request for
 *   	n% of CPU time, i.e., requesting a CPU whose speed is effectively
 * 	n% of the actual CPU speed. Roughly n out of every 100 quantums
 *  	should be allocated to the calling process.  n must be at least
 *  	0 and must be less than or equal to 100. MyRequestCPUrate (p, n)
 * 	should return 0 if successful, i.e., if such a request can be
 *  	satisfied, otherwise it should return -1, i.e., error (including if
 * 	n < 1 or n > 100). If MyRequestCPUrate (p, n) fails, it should
 *   	have no effect on scheduling of this or any other process, i.e., AS
 * 	IF IT WERE NEVER CALLED.
 */

int MyRequestCPUrate (p, n)
	int p;				// process whose rate to change
	int n;				// percent of CPU time
{	
	// Case where proc is changing or giving back its share
	if(proctab[p-1].requested && proctab[p-1].valid) {
		if(n == 0) {									// CASE 1: Giving up its share
			proctab[p-1].requested = 0;
			proctab[p-1].passValue = 0;
			distributeCPU();
			return 0;
		} else if((currCPURequested - proctab[p-1].percent + n) > 100) {		// CASE 2: New requested share is not available
			return -1;
		} else {									// CASE 3: New requested share is available
			proctab[p-1].stride = L / n;
			proctab[p-1].percent = n;
			proctab[p-1].passValue = 0;
			distributeCPU();
			return 0;
		}	
	}
	
	// Handling cpu request errors
	if(n < MIN_CPU_REQUEST || n > MAX_CPU_REQUEST)	{
		proctab[p-1].requested = 0;
		distributeCPU();
		return -1;
	}

	// Case where the resquested share exceeds what's available
	if((n + currCPURequested) > MAX_CPU_REQUEST) {
		proctab[p-1].requested = 0;
		distributeCPU();
		return - 1;
	}

	// Seting percentage utilization of cpu
	if(n == 0) {
		proctab[p-1].requested = 0;
	} else {
		if(comment)	DPrintf("in cpu(), numerator = %d, denominator = %d\n", L, n);
		proctab[p-1].requested = 1;
		proctab[p-1].stride = L / n;
		proctab[p-1].percent = n;
	}


	return (0);
}



























