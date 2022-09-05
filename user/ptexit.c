#include "lib.h"
int end1;
pthread_t t1, t2;

void ptexit(void *a) {
	int retval1 = -1 * ((int *)a)[2];
	writef("thread %08x exit\n", t1);
	pthread_exit(&retval1);
}

void umain() {
	int a[3] = {1, 2, 3};
	end1 = 0;
	int ret1 = pthread_create(&t1, NULL, ptexit,(void *)a);
	if(!ret1)
		writef("thread create successful!\n");
	while(env->env_threads[THREADX(t1)].thread_status != ENV_FREE) {
		writef("");
		// writef("%d\n", env->env_threads[THREADX(t1)].thread_status);
	}
	writef("got retval: %d\n", *((int*)env->env_threads[THREADX(t1)].thread_exit_ptr));
	writef("exit test end!\n");
}
