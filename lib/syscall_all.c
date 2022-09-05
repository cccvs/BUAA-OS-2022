#include "../drivers/gxconsole/dev_cons.h"
#include <mmu.h>
#include <env.h>
#include <printf.h>
#include <pmap.h>
#include <sched.h>

extern char *KERNEL_SP;
extern struct Env *curenv;
extern struct Thread *curthread;

int sys_set_thread_status(int sysno, u_int threadid, u_int status);
/* Overview:
 * 	This function is used to print a character on screen.
 *
 * Pre-Condition:
 * 	`c` is the character you want to print.
 */
void sys_putchar(int sysno, int c, int a2, int a3, int a4, int a5)
{
	printcharc((char) c);
	return ;
}

void *memcpy(void *destaddr, void const *srcaddr, u_int len)
{
	char *dest = destaddr;
	char const *src = srcaddr;

	while (len-- > 0) {
		*dest++ = *src++;
	}

	return destaddr;
}


u_int sys_getenvid(void)
{
	return curenv->env_id;
}

u_int sys_getthreadid(void) {
	return curthread->thread_id;
}

void sys_yield(void)
{
	bcopy((void *)KERNEL_SP - sizeof(struct Trapframe),
              (void *)TIMESTACK - sizeof(struct Trapframe),
              sizeof(struct Trapframe));
	sched_yield();
}

int sys_env_destroy(int sysno, u_int envid)
{
	int r;
	struct Env *e;

	if ((r = envid2env(envid, &e, 1)) < 0) {
		return r;
	}

	// printf("env [%08x] destroying %08x\n", curenv->env_id, e->env_id);
	env_destroy(e);
	return 0;
}

int sys_thread_destroy(int sysno, u_int threadid)
{
	int r;
	struct Thread *t, *tmpt;
	if ((r = threadid2thread(threadid, &t)) < 0) 
		return r;
	if(t->thread_status == ENV_FREE)
		return -E_BAD_THREAD;
	while(!LIST_EMPTY(&t->thread_join_list)) {
		tmpt = LIST_FIRST(&t->thread_join_list);
		LIST_REMOVE(tmpt, thread_join_link);
		if(tmpt->thread_join_val_ptr) {
			*(tmpt->thread_join_val_ptr) = t->thread_exit_ptr;
			t->thread_exit_ptr = NULL;
		}		//**val in pthread_join
		sys_set_thread_status(sysno, tmpt->thread_id, ENV_RUNNABLE);
	}
	// printf("thread [%08x] destroying %08x\n", curthread->thread_id, t->thread_id);
	thread_destroy(t);
	return 0;
}

/* Overview:
 * 	Set envid's pagefault handler entry point and exception stack.
 *
 * Pre-Condition:
 * 	xstacktop points one byte past exception stack.
 *
 * Post-Condition:
 * 	The envid's pagefault handler will be set to `func` and its
 * 	exception stack will be set to `xstacktop`.
 * 	Returns 0 on success, < 0 on error.
 */
/*** exercise 4.12 ***/
int sys_set_pgfault_handler(int sysno, u_int envid, u_int func, u_int xstacktop)
{
	// Your code here.
	struct Env *env;
	int ret;
	if((ret = envid2env(envid, &env, 0)) < 0)
		return ret;
	env->env_pgfault_handler = func;
	env->env_xstacktop = xstacktop;
	return 0;
	//	panic("sys_set_pgfault_handler not implemented");
}

/*** exercise 4.3 ***/
int sys_mem_alloc(int sysno, u_int envid, u_int va, u_int perm)
{
	// Your code here.
	struct Env *env;
	struct Page *ppage;
	int ret;
	ret = 0;
	if(va >= UTOP || (perm & PTE_COW) || !(perm & PTE_V))
		return -E_INVAL;
	if((ret = envid2env(envid, &env, 1)) < 0)
		return ret;
	if((ret = page_alloc(&ppage)) < 0)
		return ret;
	if((ret = page_insert(env->env_pgdir, ppage, va, perm)) < 0)
		return ret;
	return ret;
}

/*** exercise 4.4 ***/
int sys_mem_map(int sysno, u_int srcid, u_int srcva, u_int dstid, u_int dstva,
				u_int perm)
{
	int ret;
	u_int round_srcva, round_dstva;
	struct Env *srcenv;
	struct Env *dstenv;
	struct Page *ppage;
	Pte *ppte;

	ppage = NULL;
	ret = 0;
	round_srcva = ROUNDDOWN(srcva, BY2PG);
	round_dstva = ROUNDDOWN(dstva, BY2PG);

	//your code here
	if(round_srcva >= UTOP || round_dstva >= UTOP || !(perm & PTE_V))
		return -E_INVAL;
	if((ret = envid2env(srcid, &srcenv, 0)) < 0)
		return ret;
	if((ret = envid2env(dstid, &dstenv, 0)) < 0)
		return ret;
	if((ppage = page_lookup(srcenv->env_pgdir, round_srcva, &ppte)) == NULL)
		return -E_INVAL;
	if((ret = page_insert(dstenv->env_pgdir, ppage, round_dstva, perm)) < 0)
		return ret;
	return ret;
}
/*** exercise 4.5 ***/
int sys_mem_unmap(int sysno, u_int envid, u_int va)
{
	// Your code here.
	int ret;
	struct Env *env;
	if(va >= UTOP)
		return -E_INVAL;
	if((ret = envid2env(envid, &env, 0)) < 0)
		return ret;
	page_remove(env->env_pgdir, va);
	return ret;
	//	panic("sys_mem_unmap not implemented");
}

/*** exercise 4.8 ***/
int sys_env_alloc(void)
{
	// Your code here.
	int r;
	struct Env *e;
	struct Thread *t;
	if((r = env_alloc(&e, curenv->env_id)) < 0)
		return r;
	t = &e->env_threads[0];			// main thread of the Env
	// make thread
	bcopy((void *)(KERNEL_SP - sizeof(struct Trapframe)), (void *)(&t->thread_tf), sizeof(struct Trapframe));
	t->thread_tf.pc = t->thread_tf.cp0_epc;
	t->thread_tf.regs[2] = 0;
	t->thread_status = ENV_NOT_RUNNABLE;
	t->thread_pri = curthread->thread_pri;
	// print_trapframe(&t->thread_tf);
	return e->env_id;
	//	panic("sys_env_alloc not implemented");
}

int sys_thread_alloc(void)
{
	// Your code here.
	int r;
	struct Thread *t;
	if((r = thread_alloc(&t, curenv)) < 0)
		return r;
	// make thread
	bcopy((void *)(KERNEL_SP - sizeof(struct Trapframe)), (void *)(&t->thread_tf), sizeof(struct Trapframe));
	t->thread_tf.pc = t->thread_tf.cp0_epc;
	t->thread_tf.regs[2] = 0;
	t->thread_status = ENV_NOT_RUNNABLE;
	t->thread_pri = curthread->thread_pri;

	return t->thread_id;
	//	panic("sys_thread_alloc not implemented");
}

//don't use any more
int sys_set_env_status(int sysno, u_int envid, u_int status)
{
	// Your code here.
	struct Env *env;
	struct Thread *t;
	int ret;
	if(status != ENV_FREE && status != ENV_RUNNABLE && status != ENV_NOT_RUNNABLE)
		return -E_INVAL;
	if((ret = envid2env(envid, &env, 0)) < 0)
		return ret;
	t = &env->env_threads[0];
	if(t->thread_status == ENV_NOT_RUNNABLE && status == ENV_RUNNABLE)
		LIST_INSERT_HEAD(&thread_sched_list[0], t, thread_sched_link);
	else if(t->thread_status == ENV_RUNNABLE && status != ENV_RUNNABLE)
		LIST_REMOVE(t, thread_sched_link);
	t->thread_status = status;
	return 0;
	//	panic("sys_env_set_status not implemented");
}

int sys_set_thread_status(int sysno, u_int threadid, u_int status) {
	struct Thread *t;
	int ret;
	if(status != ENV_FREE && status != ENV_RUNNABLE && status != ENV_NOT_RUNNABLE)
		return -E_INVAL;
	if((ret = threadid2thread(threadid, &t)) < 0)
		return ret;
	if(t->thread_status == ENV_FREE)
		return -E_BAD_THREAD;
	if(t->thread_status == ENV_NOT_RUNNABLE && status == ENV_RUNNABLE)
		LIST_INSERT_HEAD(&thread_sched_list[0], t, thread_sched_link);
	else if(t->thread_status == ENV_RUNNABLE && status != ENV_RUNNABLE)
		LIST_REMOVE(t, thread_sched_link);
	// printf("thread %08x switch status from %d to %d\n", t->thread_id ,t->thread_status, status);
	t->thread_status = status;
	return 0;
}

int sys_set_trapframe(int sysno, u_int envid, struct Trapframe *tf)
{

	return 0;
}

void sys_panic(int sysno, char *msg)
{
	// no page_fault_mode -- we are trying to panic!
	panic("%s", TRUP(msg));
}

/*** exercise 4.7 ***/
void sys_ipc_recv(int sysno, u_int dstva)
{
	if(dstva >= UTOP){
		printf("ipc receving address too high!\n");
		return;
	}
	while(curenv->env_ipc_recving == 1) {
		printf("ipc receving busy!\n");
		sys_yield();
	}
	curenv->env_ipc_recving = 1;
	curenv->env_ipc_dstva = dstva;
	curenv->env_ipc_threadid = curthread->thread_id;
	curthread->thread_status = ENV_NOT_RUNNABLE;
	LIST_REMOVE(curthread, thread_sched_link);
	sys_yield();
}

/*** exercise 4.7 ***/
int sys_ipc_can_send(int sysno, u_int envid, u_int value, u_int srcva,
					 u_int perm)
{
	int r;
	struct Env *e;
	struct Page *p;
	struct Thread *t;
	if(srcva >= UTOP)
		return -E_INVAL;
	if((r = envid2env(envid, &e, 0)) < 0)
		return r;
	if(!e->env_ipc_recving)
		return -E_IPC_NOT_RECV;

	e->env_ipc_recving = 0;
	e->env_ipc_perm = perm;
	e->env_ipc_from = curenv->env_id;
	e->env_ipc_value = value;

	if((r = threadid2thread(e->env_ipc_threadid, &t)) < 0)
		return r;
	if(t->thread_status == ENV_FREE)
		return -E_BAD_THREAD;
	t->thread_status = ENV_RUNNABLE;
	LIST_INSERT_HEAD(&thread_sched_list[0], t, thread_sched_link);
	//please ignore the 'Hint' in this part, it's misleading!!!
	Pte *pte;
	if(srcva != 0){
		if((p = page_lookup(curenv->env_pgdir, srcva, &pte)) == NULL)
			return -E_INVAL;
		if((r = page_insert(e->env_pgdir, p, e->env_ipc_dstva, perm)) < 0)
			return r;
	}

	return 0;
}

int sys_thread_create(int sysno, pthread_t *threadid, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg, void *retptr) {
    int r;
    u_int new_threadid, envid;
    struct Thread *t;
    struct Env *e;
    if((r = sys_thread_alloc()) < 0) {
        *threadid = 0;
        return r;
    }
    //get according t
    new_threadid = r;
	envid = sys_getenvid();
	e = &envs[ENVX(envid)];       //assure e is curenv
    t = &e->env_threads[THREADX(new_threadid)];
    //init
    t->thread_tf.pc = (u_int)start_routine;
    t->thread_tf.regs[4] = (u_int)arg;     //$a0
    t->thread_tf.regs[29] = USTACKTOP - THREADX(new_threadid) * TSTACKSIZE;
    t->thread_tf.regs[31] = (u_int)retptr;   //return point
    //set status
    sys_set_thread_status(sysno, t->thread_id, ENV_RUNNABLE);
    *threadid = t->thread_id;
    return 0;
}

void sys_thread_exit(int sysno, void *retval) {
    u_int threadid, envid;
    struct Thread *t;
    struct Env *e;
    //get current t
    envid = sys_getenvid();
    threadid = sys_getthreadid();
    e = &envs[ENVX(envid)]; 
    t = &e->env_threads[THREADX(threadid)];
    //change t
    t->thread_exit_ptr = retval;
    sys_thread_destroy(sysno, threadid);
}

int sys_thread_cancel(int sysno, pthread_t threadid) {
    u_int envid;
    struct Thread *t;
    struct Env *e;
    //get according t
    // writef("get e!\n");
    envid = sys_getenvid();
    e = &envs[ENVX(envid)]; 
    t = &e->env_threads[THREADX(threadid)];
    // writef("get t!\n");
    //judging state
    if((t->thread_id != threadid) || (t->thread_status == ENV_FREE))
        return -E_BAD_THREAD;
    if(t->thread_cancelstate == THREAD_CANCEL_DISABLE)
        return -E_THREAD_CANCEL_FORBIDDEN;
    t->thread_exit_val = -CANCEL_EXIT;
    // writef("destroying!\n");
    if(t->thread_canceltype == THREAD_CANCEL_ASYNCHRONOUS)
        sys_thread_destroy(sysno, threadid);
    else
        t->thread_to_be_canceled = 1;
    return 0;
}

int	sys_thread_setcancelstate(int sysno, int state, int *oldstate) {
    u_int threadid, envid;
    struct Thread *t;
    struct Env *e;
    //get current t   
    envid = sys_getenvid();
    threadid = sys_getthreadid();
    e = &envs[ENVX(envid)]; 
    t = &e->env_threads[THREADX(threadid)];
    //set state
    if(state != THREAD_CANCEL_ENABLE && state != THREAD_CANCEL_DISABLE)
        return -E_INVAL;
    if(t->thread_id != threadid)
        return -E_BAD_THREAD;
    if(oldstate)
        *oldstate = t->thread_cancelstate;
    t->thread_cancelstate = state;
    return 0;
}

int	sys_thread_setcanceltype(int sysno, int type, int *oldtype) {
    u_int threadid, envid;
    struct Thread *t;
    struct Env *e;
    //get current t   
    envid = sys_getenvid();
    threadid = sys_getthreadid();
    e = &envs[ENVX(envid)]; 
    t = &e->env_threads[THREADX(threadid)];
    //set type
    if(type != THREAD_CANCEL_DEFERRED && type != THREAD_CANCEL_ASYNCHRONOUS)
        return -E_INVAL;
    if(t->thread_id != threadid)
        return -E_BAD_THREAD;
    if(oldtype)
        *oldtype = t->thread_canceltype;
    t->thread_canceltype = type;
    return 0;
}

void sys_thread_testcancel(void) {
    u_int threadid, envid;
    struct Thread *t;
    struct Env *e;
    //get current t   
    envid = sys_getenvid();
    threadid = sys_getthreadid();
    e = &envs[ENVX(envid)]; 
    t = &e->env_threads[THREADX(threadid)];
    //test cancel
    if(t->thread_id != threadid)
        panic("bad thread %08x", threadid);
    if(t->thread_to_be_canceled && t->thread_cancelstate == THREAD_CANCEL_ENABLE && t->thread_canceltype == THREAD_CANCEL_DEFERRED) {
        t->thread_exit_val = -CANCEL_EXIT;
        sys_thread_destroy(0, t->thread_id);
    }
}

int sys_thread_detach(int sysno, pthread_t threadid) {
    u_int envid;
    struct Thread *t;
    struct Env *e;
    //get according t   
    envid = sys_getenvid();
    e = &envs[ENVX(envid)]; 
    t = &e->env_threads[THREADX(threadid)];
    //detach
    if(t->thread_id != threadid)
        return -E_BAD_THREAD;
    if(t->thread_status == ENV_FREE)    //the sp will be refresh at alloc part
        bzero(t, sizeof(struct Thread));
    else
        t->thread_detach = 1;
    return 0;
}

int sys_thread_join(int sysno, u_int threadid, void **retval) {
	struct Thread *t;
	int r;
	//must lay after free judging!!
	if((r = threadid2thread(threadid, &t)) < 0 && t->thread_status != ENV_FREE)
		return r;
	if(t->thread_detach)
		return -E_THREAD_JOIN_FAILED;
	if(t->thread_status == ENV_FREE) {
		if(retval) {
			*retval = t->thread_exit_ptr;
			t->thread_exit_ptr = NULL;
		}	
		return 0;
	}
	if(t->thread_id == curthread->thread_id)	//can not join itself
		return 0;
	//add to joined list
	if(!LIST_EMPTY(&t->thread_join_list))
		panic("too many thread join!\n");
	LIST_INSERT_HEAD(&t->thread_join_list, curthread, thread_join_link);
	curthread->thread_join_val_ptr = retval;
	sys_set_thread_status(sysno, curthread->thread_id, ENV_NOT_RUNNABLE);
	struct Trapframe *tf = (struct Trapframe *)(KERNEL_SP - sizeof(struct Trapframe));
	tf->regs[2] = 0;
	tf->pc = tf->cp0_epc;
	sys_yield();			//switch
	return 0;
}

int sys_sem_init(int sysno, sem_t *sem, int pshared, u_int value) {
	int i;
	if(sem == NULL) 
		return -E_SEM_NOT_FOUND;
	//basic
	sem->sem_envid = curenv->env_id;
	sem->sem_name[0] = '\0';
	sem->sem_status = SEM_VALID;
	sem->sem_value = value;
	sem->sem_shared = pshared;
	//queue
	sem->sem_front = 0;
	sem->sem_rear = 0;
	sem->sem_wait_count = 0;
	for(i = 0; i < SEM_WAITQ_SIZE; i++)
		sem->sem_wait_list[i] = NULL;
	return 0;
}

int sys_sem_destroy(int sysno, sem_t *sem) {
	if(sem->sem_envid != curenv->env_id && !sem->sem_shared)
		return -E_SEM_NOT_FOUND;
	sem->sem_status = SEM_FREE;
	return 0;
}

int sys_sem_wait(int sysno, sem_t *sem) {
	struct Trapframe *tf;
	// printf("thread %08x comes, sem value is %d, waitcount is %d\n", curthread->thread_id, sem->sem_value, sem->sem_wait_count);
	//judge sem
	if(sem->sem_status == SEM_FREE)
		return -E_BAD_SEM;
	if(sem->sem_value > 0) {
		sem->sem_value--;
		// printf("thread %08x runs, waitcount is %d\n", curthread->thread_id, sem->sem_wait_count);
		return 0;
	} else if(sem->sem_wait_count >= SEM_WAITQ_SIZE)
		return -E_MAX_THREAD;
	//add to queue
	sem->sem_wait_list[sem->sem_rear] = curthread;
	sem->sem_rear = (sem->sem_rear + 1) % SEM_WAITQ_SIZE;
	sem->sem_wait_count++;
	// printf("thread %08x is waiting, waitcount is %d\n", curthread->thread_id, sem->sem_wait_count);
	//block current thread, save trapframe
	sys_set_thread_status(sysno, 0, ENV_NOT_RUNNABLE);
	tf = (struct Trapframe *)(KERNEL_SP - sizeof(struct Trapframe));
	tf->regs[2] = 0;	//return 0;
	tf->pc = tf->cp0_epc;
	//yield
	sys_yield();
	return -E_BAD_SEM;
}

int sys_sem_trywait(int sysno, sem_t *sem) {
	if(sem->sem_status == SEM_FREE)
		return -E_BAD_SEM;
	if(sem->sem_value > 0){
		sem->sem_value--;
		return 0;
	}
	return -E_SEM_EAGAIN;
}

int sys_sem_post(int sysno, sem_t *sem) {
	struct Thread *t;
	int r, index;
	if(sem->sem_status == SEM_FREE)
		return -E_BAD_SEM;
	// no one waiting
	sem->sem_value++;
	while(sem->sem_value > 0 && sem->sem_wait_count > 0) {
		//decrease
		sem->sem_wait_count--;
		sem->sem_value--;
		// printf("thread %08x release, sem value is %d, waitcount is %d\n", curthread->thread_id, sem->sem_value, sem->sem_wait_count);
		//pop queue
		t = sem->sem_wait_list[sem->sem_front];
		sem->sem_wait_list[sem->sem_front] = NULL;
		sem->sem_front = (sem->sem_front + 1) % SEM_WAITQ_SIZE;
		// printf("thread %08x ends waiting, waitcount is %d\n", t->thread_id, sem->sem_wait_count);
		if((r = sys_set_thread_status(sysno, t->thread_id, ENV_RUNNABLE)) < 0)
			return -r; 
	}
	return 0;
}

int sys_sem_getvalue(int sysno, sem_t *sem, int *sval)
{
	if(sem->sem_status == SEM_FREE)
		return -E_BAD_SEM;
	if(sval)
		*sval = sem->sem_value;
	return 0;
}

