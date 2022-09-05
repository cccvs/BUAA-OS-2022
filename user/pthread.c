#include "lib.h"
#include <error.h>
#include <mmu.h>

int pthread_create(pthread_t *threadid, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg) {
    return syscall_thread_create(threadid, attr, start_routine, arg, (void *)exit);
}

void pthread_exit(void *retval) {
    syscall_thread_exit(retval);
}

int pthread_cancel(pthread_t threadid) {
    return syscall_thread_cancel(threadid);
}

int	pthread_setcancelstate(int state, int *oldstate) {
    return syscall_thread_setcancelstate(state, oldstate);
}

int	pthread_setcanceltype(int type, int *oldtype) {
    return syscall_thread_setcanceltype(type, oldtype);
}

void pthread_testcancel(void) {
    syscall_thread_testcancel();
}

int pthread_detach(pthread_t threadid) {
    return syscall_thread_detach(threadid);
}

int	pthread_join(pthread_t threadid, void **retval) {
    return syscall_thread_join(threadid, retval);
}

