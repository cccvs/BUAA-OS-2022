/* See COPYRIGHT for copyright information. */

#ifndef _ENV_H_
#define _ENV_H_

#include "types.h"
#include "queue.h"
#include "trap.h"
#include "mmu.h" 

#define LOG2NENV	10
#define NENV		(1 << LOG2NENV)
#define LOG2THREAD	3
#define NTHREAD		(1 << LOG2THREAD)
#define ENVX(envid)	((envid) & (NENV - 1))
#define THREADX(threadid)	((threadid) & (NTHREAD - 1))
#define GET_ENV_ASID(envid) (((envid)>> 11)<<6)
#define TSTACKSIZE	(4 * BY2PG)

// Values of thread_status in struct Thread
#define ENV_FREE	0
#define ENV_RUNNABLE		1
#define ENV_NOT_RUNNABLE	2

//thread_cancelstate
#define THREAD_CANCEL_ENABLE	0
#define THREAD_CANCEL_DISABLE	1
//thread_canceltype
#define THREAD_CANCEL_ASYNCHRONOUS	0
#define THREAD_CANCEL_DEFERRED	1
//cancel exit val
#define CANCEL_EXIT 233

//sem_waiting_count
#define SEM_WAITQ_SIZE	10
//sem_status
#define SEM_FREE 0
#define SEM_VALID 1

LIST_HEAD(Thread_list, Thread);

struct Thread {
	// basic
	struct Trapframe thread_tf;		//cp0_status, reg[29] init in thread_alloc, pc init in load_icode
	struct Env *thread_parent_env;	//init in thread_alloc
	u_int thread_id;				//init in thread_alloc
	u_int thread_status;			//init in thread_alloc/sys_thread_alloc
	u_int thread_pri;				//init in env_create_priority
	LIST_ENTRY(Thread) thread_sched_link;

	//pthread_join
	u_int thread_detach;
	void **thread_join_val_ptr;		//used to get joined thread exit val
	struct Thread_list thread_join_list;
	LIST_ENTRY(Thread) thread_join_link;

	//pthread_exit
	void *thread_exit_ptr;
	int thread_exit_val;			//may be minus

	//pthread_cancel
	u_int thread_cancelstate;
	u_int thread_canceltype;
	u_int thread_to_be_canceled;
};

struct Env {
	// struct Trapframe env_tf;        // Saved registers
	LIST_ENTRY(Env) env_link;       // Free list
	u_int env_id;                   // Unique environment identifier
	u_int env_parent_id;            // env_id of this env's parent
	// u_int env_status;               // Status of the environment
	Pde  *env_pgdir;                // Kernel virtual address of page dir
	u_int env_cr3;
	// LIST_ENTRY(Env) env_sched_link;
    // u_int env_pri;

	// Lab 4 IPC
	u_int env_ipc_threadid;			
	u_int env_ipc_value;            // data value sent to us 
	u_int env_ipc_from;             // envid of the sender  
	u_int env_ipc_recving;          // env is blocked receiving
	u_int env_ipc_dstva;			// va at which to map received page
	u_int env_ipc_perm;				// perm of page mapping received

	// Lab 4 fault handling
	u_int env_pgfault_handler;      // page fault state
	u_int env_xstacktop;            // top of exception stack

	// Lab 6 scheduler counts
	u_int env_runs;					// number of times been env_run'ed
	u_int env_nop;                  // align to avoid mul instruction

	// Lab 4 challenge
	u_int env_thread_count;			//only changed in alloc and free
	struct Thread env_threads[NTHREAD];
};

struct Sem {
	u_int sem_envid;
	char sem_name[32];
	u_int sem_status;
	u_int sem_value;
	int sem_shared;
	//waiting queue, [front, rear) contains elements
	u_int sem_wait_count;
	u_int sem_front;
	u_int sem_rear;
	struct Thread *sem_wait_list[SEM_WAITQ_SIZE];
};

LIST_HEAD(Env_list, Env);
extern struct Env *envs;		// All environments
extern struct Env *curenv;	        // the current env
extern struct Thread *curthread;
// extern struct Env_list env_sched_list[2]; // runnable env list
extern struct Thread_list thread_sched_list[2];	// runnable thread list

void env_init(void);
int env_alloc(struct Env **e, u_int parent_id);
void env_free(struct Env *);
void env_create_priority(u_char *binary, int size, int priority);
void env_create(u_char *binary, int size);
void env_destroy(struct Env *e);

int envid2env(u_int envid, struct Env **penv, int checkperm);

u_int mkthreadid(struct Thread *t);
int threadid2thread(u_int threadid, struct Thread **pthread);
int thread_alloc(struct Thread **new, struct Env *e);
void thread_free(struct Thread *t);
void thread_destroy(struct Thread *t);
void thread_run(struct Thread *t);

void print_thread(struct Thread *t);
void print_env(struct Env *e);
void print_trapframe(struct Trapframe *tf);

// for the grading script
#define ENV_CREATE2(x, y) \
{ \
	extern u_char x[], y[]; \
	env_create(x, (int)y); \
}
#define ENV_CREATE_PRIORITY(x, y) \
{\
        extern u_char binary_##x##_start[]; \
        extern u_int binary_##x##_size;\
        env_create_priority(binary_##x##_start, \
                (u_int)binary_##x##_size, y);\
}
#define ENV_CREATE(x) \
{ \
	extern u_char binary_##x##_start[];\
	extern u_int binary_##x##_size; \
	env_create(binary_##x##_start, \
		(u_int)binary_##x##_size); \
}

#endif // !_ENV_H_

