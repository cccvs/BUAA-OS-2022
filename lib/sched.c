#include <env.h>
#include <pmap.h>
#include <printf.h>

/* Overview:
 *  Implement simple round-robin scheduling.
 *
 *
 * Hints:
 *  1. The variable which is for counting should be defined as 'static'.
 *  2. Use variable 'thread_sched_list', which is a pointer array.
 *  3. CANNOT use `return` statement!
 */
void print_list(void) {
	struct Thread *t;
	printf("list 0:\n");
	LIST_FOREACH(t, &thread_sched_list[0], thread_sched_link) {
		printf("threadid: %08x, status: %d\n", t->thread_id, t->thread_status);
	}
	printf("list 1:\n");
	LIST_FOREACH(t, &thread_sched_list[1], thread_sched_link) {
		printf("threadid: %08x, status: %d\n", t->thread_id, t->thread_status);
	}
}
/*** exercise 3.15 ***/
void sched_yield(void) {
	static int count = 0; // remaining time slices of current env
	static int point = 0; // current thread_sched_list index
	static struct Thread *t = NULL;
	// print_list();
	while(count == 0 || t == NULL || t->thread_status != ENV_RUNNABLE) {	//assure that find suitable env
		if(t != NULL){
			LIST_REMOVE(t, thread_sched_link);
			if(t->thread_status == ENV_RUNNABLE)
				LIST_INSERT_TAIL(&thread_sched_list[1 - point], t, thread_sched_link);
		}
		//2 switch between current list and another
		while(1) {
			while (LIST_EMPTY(&thread_sched_list[point]))
				point = 1 - point;
			t = LIST_FIRST(&thread_sched_list[point]);
			if(t->thread_status == ENV_FREE)
				LIST_REMOVE(t, thread_sched_link);
			else if (t->thread_status == ENV_NOT_RUNNABLE)
				LIST_REMOVE(t, thread_sched_link);
			 else {
				count = t->thread_pri;
				break;
			}
		}
	}
	//4
	if(t != NULL){
		count --;
		thread_run(t);
	}
}

