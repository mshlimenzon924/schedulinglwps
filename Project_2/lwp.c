#include "Asgn2/include/lwp.h"
#include "rr.h"

//Running command: gcc -g lwp.c rr.c Asgn2/include/lwp.h rr.h -lm Asgn2/src/magic64.S

struct scheduler roundrobin = {NULL, NULL, rr_admit, rr_remove, rr_next, rr_qlen};
thread list_of_all_threads = NULL;  // list of all threads at the beginning of program (linked list)
thread list_of_terminated_threads = NULL;
thread list_of_waiting_threads = NULL;
scheduler current_scheduler = &roundrobin; // set up round robin
// roundrobin.init = rr_init;  // defined in lwp.h
// roundrobin.shutdown = rr_shutdown;

int current_tid = PID_START; // used to track current thread id

/* 
Calls given lwpfunction with given argument
then lwp_exit with return value 
*/
static void lwp_wrap(lwpfun fun, void *arg){
    int rval;
    printf("current_func %d\n", current_thread->tid);
    rval = fun(arg);
    printf("rval %d\n", rval);
    lwp_exit(rval);
}

/* 
Creating new lightweight process (thread), adds to Scheduler, But does not run it 
*/
tid_t lwp_create(lwpfun function, void *argument) {
    
    thread new_thread = (thread)malloc(sizeof(context)); // allocate context
    if(!new_thread) {   // malloc issue
        fprintf(stderr, "thread malloc issue\n");
        return NO_THREAD;
    }

    // ALLOCATE STACK
    // getting page_size
    long page_size = sysconf(_SC_PAGE_SIZE);
    if (page_size == -1) {
        fprintf(stderr, "sysconf issue\n");
        return NO_THREAD;
    }

    // getting stack size
    struct rlimit rlim;
    if (getrlimit(RLIMIT_STACK, &rlim) == -1) {
        fprintf(stderr, "getrlimit issue\n");
        return NO_THREAD;
    } 
    // calculating stack_size
    long stack_size;
    if ((rlim.rlim_cur == RLIM_INFINITY) || (rlim.rlim_cur > 0)) {
        stack_size = DEFAULT_STACK_SIZE;
    } else {
        stack_size = rlim.rlim_cur;
    }
    stack_size = ceil((long)stack_size / page_size) * page_size; // rounding up 
    //printf("%d\n", stack_size);

    // stack points to top
    unsigned long *stack = (unsigned long *)mmap(NULL, stack_size, (PROT_READ | PROT_WRITE), (MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK), -1, 0);
    new_thread->stack = (unsigned long *)stack; 
    stack += stack_size / sizeof(unsigned long);

    if (stack == MAP_FAILED) {
        fprintf(stderr, "mmap issue\n");
        return NO_THREAD;
    }

    new_thread->tid = ++current_tid;
    new_thread->status =  MKTERMSTAT(LWP_LIVE, 0);  // thread is live
    new_thread->stacksize = (size_t)stack_size;
    new_thread->state.fxsave = FPU_INIT;

    // set up thread
    new_thread->state.rdi = (unsigned long)function;
    new_thread->state.rsi = (unsigned long)argument;

    // did -1 becasue unsigned longs are multiples of 16
    stack--;  // 8 bits
    stack--;  // 8 bits
    *stack = (unsigned long)lwp_wrap;         // return address where we want to return to
    stack--;


    new_thread->state.rsp = (unsigned long)stack;  //pointing to top of stack
    new_thread->state.rbp = (unsigned long)stack;  // bottom of stack

    // intialize threads to NULL 
    new_thread->lib_one = NULL;
    new_thread->lib_two = NULL; 
    new_thread->sched_one = NULL;
    new_thread->sched_two = NULL;
    new_thread->exited = NULL;

    current_scheduler->admit(new_thread); // scheduler lwp to be run

    // Add thread to list of threads 
    if(list_of_all_threads){    // if already has at least one thread
        thread temp = new_thread;  
        new_thread->lib_one = list_of_all_threads;   
        list_of_all_threads = temp;  // then just add it to the front
    } else {
        list_of_all_threads = new_thread;
    }
    
    return new_thread->tid;
}

/* 
Starts LWP system by converting calling thread into LWP
allocates context for main thread and admits to scheduler 
yields control to whichever thread scheduler indicates 
create thread give very basic data (tid, alive status)
admit to scheduler (stacks already created etc context swap happens in lwp yield)
NOTE: no need to make stack for this thread, it already has one! 
*/
void lwp_start(void){

    // create new thread with threadID and alive status 
    thread new_lwp = (thread)malloc(sizeof(struct threadinfo_st));
    new_lwp->tid = PID_START;
    new_lwp->status = LWP_LIVE;
    // intialize threads to NULL 
    new_lwp->lib_one = NULL;
    new_lwp->lib_two = NULL; 
    new_lwp->sched_one = NULL;
    new_lwp->sched_two = NULL;
    new_lwp->exited = NULL;

    current_thread = new_lwp; // set current thread to main 

    current_scheduler->admit(new_lwp); // schedule thread

    lwp_yield();
}

/* 
Yields control to another thread depending on scheduler (if no next, end program, exit w. term status of calling thread)
Saving current thread's context, restoring next thread's context, swap out contexts
lwp_exit() terminates the calling thread and switches to another, if any.
*/
void lwp_yield(void){
    // issue with our next we are skipping our head

    thread next_scheduled = current_scheduler->next(); // get next thread to be scheduled

    if(!next_scheduled) // if no next thread, end program with call to lwp_exit(int status);
        exit(LWPTERMSTAT(current_thread->status));

    thread we_are_checking = current_thread;
    current_thread = next_scheduled; // mark next_scheduled as the current lwp

    // round 1 current thread = main, next_scheduled = main 
    swap_rfiles(&we_are_checking->state, &next_scheduled->state); // swap out current with next thread's context
}

/* 
Terminates current thread and yields to whichever thread the scheduler choose
termination status becomes low 8 bits of passed integer (exitval) 
*/
void lwp_exit(int exitval) {
    current_thread->status = MKTERMSTAT(LWP_TERM, exitval); // set up termination status
    printf("EXIT %d STATUS %d\n", exitval, current_thread->status);

    current_thread->lib_one = NULL;
    if(list_of_terminated_threads){  // add to the list of terminated nodes that need to be cleaned up
        thread temp = list_of_terminated_threads; // add terminated thread to the BACK of the linked list 
        thread next = temp->lib_one;

        while(next) {       // while there exists another thread in linked list
            temp = next;    // move temp over 
            next = next->lib_one;
        }
        // next is now NULL;
        temp->lib_one = current_thread; //sent current thread to exited!
    } else {
        list_of_terminated_threads = current_thread;
    }
    // terminates current_thread
    current_scheduler->remove(current_thread);
    thread temp_wait = list_of_waiting_threads;
    if(temp_wait) {
        list_of_waiting_threads = temp_wait->lib_one; 
        temp_wait->lib_one = NULL;
        current_scheduler->admit(temp_wait);  
    }

    lwp_yield(); // yield control to next thread
}


/*
Waiting for thread to terminate 
If multiple terminated threads, grab in FIFO terminated threads (aka oldest thread first)
If no terminated threads but still running ones, caller of lwp_wait() needs to block
deschedule it sched->remove(), place on queue of waiting threads 
Once a thread calls lwp_exit() - make it be known its the oldest thread remove the blocking process 
from waiting queue and re-admit to the scheduler so it can finish lwp_wait()
*/

tid_t lwp_wait(int *status){
    
    current_thread->lib_one = NULL; // just checking case

    // CASE: current thread waited before no terminated threads so we have now come back after thread terminated
    if((!list_of_terminated_threads) && (current_scheduler->qlen() > 1)) {
    // if no terminated threads yet and we still have threads running 
    // then we have to yield
        current_scheduler->remove(current_thread); // remove off scheduler 
        if(list_of_waiting_threads) { 
            thread temp = list_of_waiting_threads; 
            thread next = temp->lib_one;

            while(next){    // if next is not NULL loop through 
                temp = next;
                next = next->lib_one;
            }

            temp->lib_one = current_thread;
            lwp_yield();
    
        } else {
            list_of_waiting_threads = current_thread;
            lwp_yield(); 
        }
    }

    thread thread_for_cleanup = NULL;
    unsigned int status_val = 0;
    tid_t thread_tid = NO_THREAD;

    if(list_of_terminated_threads){
        thread_for_cleanup = list_of_terminated_threads; // grab the oldest terminated thread (FIFO)
        thread temp = list_of_terminated_threads->lib_one;  // get the next thread on the linked list
        list_of_terminated_threads = temp; // move linked over
        thread_for_cleanup->lib_one = NULL; // no more linked list
    }

    if(thread_for_cleanup) {
        if (munmap((void *)thread_for_cleanup->stack, (size_t)thread_for_cleanup->stacksize) == -1) {
            perror("munmap");
            return EXIT_FAILURE;
        }
        status_val = thread_for_cleanup->status;
        printf("STATUS %d\n", status_val);
        thread_tid = thread_for_cleanup->tid;

        free(thread_for_cleanup);   // got rid of thread
        thread_for_cleanup = NULL;
    }

    if(status && status_val){
        *status = status_val;
    }
    else {
        status = NULL;
    }

    return thread_tid;
}  

/* 
Getting tid of Current LWP (either it is the LWP)
returns either tid of thread or NO_THREAD if not called by LWP 
*/
tid_t lwp_gettid(void){
    if(current_thread) {
        return current_thread->tid; 
    }
    return NO_THREAD; 
}

/* 
Given a thread ID, return thread or NO_THREAD if ID is invalid
*/
thread tid2thread(tid_t tid){
    thread temp = list_of_all_threads;
    while(temp){
        if(tid == temp->tid) {
            return temp;
        } 
        temp = temp->lib_one; // if no match skips to next thread
    } 
}

/* 
LWP package uses scheduler to choose next process to run
*/
void lwp_set_scheduler(scheduler sched) {
    if (!sched) {   // if scheduler is NULL, return back to round-robin scheduling
        current_scheduler = &roundrobin; // set up round robin
        roundrobin.init = rr_init;  // defined in lwp.h
        roundrobin.shutdown = rr_shutdown;
    } else {    // if not null, then use new scheduler 
        current_scheduler = sched;
    }
}

void rr_init(){

}

void rr_shutdown(){
    while(current_scheduler->qlen() > 1){
        current_scheduler->remove(current_thread);
    }
    free(queue);
}

/* 
Helps us get current scheduler 
Return pointer to current scheduler 
*/
scheduler lwp_get_scheduler(void){
    return current_scheduler;
}


// void printing_i(void *num) {
//     int value_num = (long)num;
//     printf("%*d\n", value_num);
// }

// /* 
// current process we are running 
// */
// int main(void) {
//     lwp_set_scheduler(current_scheduler);

//     /* spawn a number of individual LWPs */
//     long i = 1;
//     lwp_create((lwpfun)printing_i,(void*)i);
//     // for(i=1;i<=5;i++) {
//     //     lwp_create((lwpfun)printing_i,(void*)i);
//     // }

//     printf("Finished creating threads.\n");

//     lwp_start();


    // /* wait for the other LWPs */
    // for(i=1;i<=5;i++) {
    //     int status,num;
    //     tid_t t;
    //     t = lwp_wait(&status);
    //     num = LWPTERMSTAT(status);
    //     printf("Thread %ld exited with status %d\n",t,num);
    // }

    // printf("Back from LWPS.\n");
    // lwp_exit(0);
//     return 0;
// }
