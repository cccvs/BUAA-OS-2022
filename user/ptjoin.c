#include "lib.h"
pthread_t t1, t2;
pthread_t t;
int begin = 0, join_count = 0;
void ptjoin(void *name)
{
    void *retval;
    int r;
    while (!begin) {
        writef("");
    }
    
    if(syscall_getthreadid() == t1) {
        // writef("%08x\n", t2);
        if ((r = pthread_join(t2, &retval)) < 0)
            user_panic("waiting failed: %d!\n", r);
        writef("got retval from t2: %s\n", (char *)retval);
    }
    pthread_exit(name);
}

void too_many_thread_join() {
    int r;
    if ((r = pthread_join(t, 0)) < 0)
        user_panic("waiting failed: %d!\n", r);
    join_count++;
    while(join_count != 2) {
        writef("");
    }
}

void umain() {
    int r;
    void *retval;
    // basic test
    if ((r = pthread_create(&t1, NULL, ptjoin, "t1")) < 0) 
		user_panic("thread create failed\n");
    if ((r = pthread_create(&t2, NULL, ptjoin, "t2")) < 0) 
		user_panic("thread create failed\n");
    begin = 1;
    if ((r = pthread_join(t1, &retval)) < 0)
        user_panic("waiting failed: %d!\n", r);
    writef("got retval from t1: %s\n", (char *)retval);
    // panic test
    writef("-------NOW BEGIN ANOTHER TEST!-------\n");
    t = syscall_getthreadid();
    if ((r = pthread_create(&t1, NULL, too_many_thread_join, "t1")) < 0) 
		user_panic("thread create failed\n");
    if ((r = pthread_create(&t2, NULL, too_many_thread_join, "t2")) < 0) 
		user_panic("thread create failed\n");
    while(join_count != 2) {
        writef("");
    }
    user_panic("SHOULD HAVE PANIC BEFORE!!!\n");
    return 0;
}
