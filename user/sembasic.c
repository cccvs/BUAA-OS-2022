#include "lib.h"
int n = 5, x = 0;
int val, start = 0, end = 0;
sem_t count, zero;
void test() {
	int r;
	while(!start);
	while((r = sem_trywait(&count)) == 0) {
		sem_getvalue(&count, &val);
		writef("sem value is: %d\n", val);
	}
	sem_getvalue(&count, &val);
	writef("err: %d at sem value is: %d\n", r, val);
	end = 1;
}

void umain() {
	pthread_t t;
	sem_init(&count, 0, n); 
	sem_getvalue(&count, &val);
	writef("init sem value is: %d\n", val);
	pthread_create(&t, NULL, test, 0);
	start = 1;
	while(!end);
	sem_destroy(&count);
	if(count.sem_status == ENV_FREE)
		writef("destroy succeessfully!\n");
}

