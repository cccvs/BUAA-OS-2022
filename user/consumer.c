#include "lib.h"
int n = 4;
int m = 15;
int in_no = 0;
int out_no = 0;
int start = 0;
pthread_t pro, con;
sem_t mutex, empty, full; 
void producer() {
	while(!start) {
		writef("");
	}
	while (1) {
		sem_wait(&empty);
		sem_wait(&mutex);
		if(in_no < m) {
			in_no++;
			writef("producer: %08x put message %d in buffer\n", syscall_getthreadid(), in_no);
		}
		sem_post(&mutex);
		sem_post(&full);
		syscall_yield();	//时间片过长
		if(in_no >= m && out_no >= m)
			return;
	}
	
}

void consumer() {
	while(!start) {
		writef("");
	}
	while (1) {
		sem_wait(&full);
		sem_wait(&mutex);
		out_no++;
		writef("consumer: %08x get message %d from buffer\n", syscall_getthreadid(), out_no);
		sem_post(&mutex);
		sem_post(&empty);
		syscall_yield();
		if(in_no >= m && out_no >=m)
			return;
	}
}

void umain() {
	sem_init(&mutex, 0, 1);
	sem_init(&empty, 0, n);
	sem_init(&full, 0, 0);
	pthread_create(&pro, NULL, producer, NULL);
	pthread_create(&pro, NULL, producer, NULL);
	pthread_create(&pro, NULL, producer, NULL);
	pthread_create(&con, NULL, consumer, NULL);
	pthread_create(&con, NULL, consumer, NULL);
	start = 1;
	while (1) {
		if(in_no >= m && out_no >=m){
			writef("completed!\n");
			pthread_exit(0);
		}
	}
}
