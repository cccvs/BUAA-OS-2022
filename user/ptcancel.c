#include "lib.h"
int cancel = 0, set = 0;
pthread_t t1, t2;

void deferred(){
	int i = 0;
	while(1) {
		// writef("t1: %08x, t2: %08x\n", t1, t2);
		// writef("thread run %08x\n", syscall_getthreadid());
		if(syscall_getthreadid() == t1) {
			pthread_setcancelstate(THREAD_CANCEL_ENABLE, NULL);
			pthread_setcanceltype(THREAD_CANCEL_DEFERRED, NULL);
			if(!set) {
				set = 1;
			}
		}
		if(cancel)
	        pthread_testcancel();
	}
}
 
void umain()
{
	void *ret;
	int time = 1000;
	// writef("t1: %08x, t2: %08x\n", t1, t2);
	pthread_create(&t1, NULL, deferred, NULL);
	pthread_create(&t2, NULL, deferred, NULL);
	while(!set) {
		writef("");
		// writef("set is %d\n", set);
	}
	pthread_cancel(t1);
	// writef("excute t1 cancel\n");
	pthread_cancel(t2);
	// writef("excute t2 cancel\n");
	while(env->env_threads[THREADX(t2)].thread_status != ENV_FREE);
	writef("t2 canceled!\n");
	writef("t1 status: %d\n", env->env_threads[THREADX(t1)].thread_status);
	cancel = 1;
	writef("test cancel t1...\n");
	syscall_yield();	
	syscall_yield();	
	writef("t1 status: %d\n", env->env_threads[THREADX(t1)].thread_status);
}
