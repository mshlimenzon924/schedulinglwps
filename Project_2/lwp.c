#include "Asgn2/include/lwp.h"
#include "rr.h"

struct scheduler roundrobin = {NULL, NULL, rr_admit, rr_remove, rr_next, rr_qlen};
thread list_of_all_threads = NULL;  // list of all threads at the beginning of program (linked list)
thread list_of_terminated_threads = NULL;
scheduler current_scheduler;

int current_tid = PID_START; // used to track current thread id

/* 
Calls given lwpfunction with given argument
then lwp_exit with return value 
*/
static void lwp_wrap(lwpfun fun, void *arg){
    int rval;
    rval = fun(arg);
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

    // points to bottom aka starting point for stack operations; it grows upward
    unsigned long *stack = (unsigned long *)mmap(NULL, stack_size, (PROT_READ | PROT_WRITE), (MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK), -1, 0);
    if (stack == MAP_FAILED) {
        fprintf(stderr, "mmap issue\n");
        return NO_THREAD;
    }

    //new_thread->tid = PID_START++;  // tid setup // pid_start isnt modifiable 
    // Inside your code, assign the thread identifier to new_thread->tid
    new_thread->tid = ++current_tid;
    new_thread->status =  LWP_LIVE;  // thread is live
    new_thread->stacksize = (size_t)stack_size;
    new_thread->state.fxsave = FPU_INIT;

    // set up thread
    new_thread->state.rdi = (unsigned long)function;
    new_thread->state.rsi = (unsigned long)argument;
    new_thread->state.rbp = (unsigned long)stack;  // bottom of stack 
    // t->state.rbp = stack_addr + stack_addr % 16 - 1;  ?? (is our stack actually divisible by 16)

    // did -1 becasue unsigned longs are multiples of 16
    stack[0] = (unsigned long)lwp_wrap;         // return address where we want to return to
    stack[-1] = (unsigned long)new_thread->state.rbp;       

    new_thread->state.rsp = (unsigned long)*(stack - 1); //pointing to top of stack
    new_thread->stack = (unsigned long *)stack;   // is this correct?

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
    new_lwp->tid = current_tid;
    new_lwp->status = LWP_LIVE;

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

    thread next_scheduled = current_scheduler->next(); // get next thread to be scheduled

    if(!next_scheduled) // if no next thread, end program with call to lwp_exit(int status);
        lwp_exit(current_thread->status);

    swap_rfiles(&current_thread->state, NULL); // save current threads context

    // round 1 current thread = main, next_scheduled = main 
    swap_rfiles(&current_thread->state, &next_scheduled->state); // swap out current with next thread's context
    
    current_thread = next_scheduled; // mark next_scheduled as the current lwp 
}

/* 
Terminates current thread and yields to whichever thread the scheduler choose
termination status becomes low 8 bits of passed integer (exitval) 
*/
void lwp_exit(int exitval) {
    
    current_thread->status = MKTERMSTAT(LWP_TERM, exitval); // set up termination status

    // terminates current_thread
    if(current_thread){ 
        current_scheduler->remove(current_thread);
        thread temp_wait = list_of_waiting_threads;
        if(temp_wait){ // temp_wait exists then let's throw it in oldest wait node
            temp_wait->exited = current_thread;
            list_of_waiting_threads = temp_wait->lib_one;    // dequeue oldest waiting node
            current_scheduler->admit(temp_wait);     // add back to scheduler
        }
        else { // if no waiting nodes
            if(list_of_terminated_threads){  // add to the list of terminated nodes that need to be cleaned up

                thread temp = list_of_terminated_threads; // add terminated thread to the BACK of the linked list 
                thread next = temp->exited;

                while(next) {       // while there exists another thread in linked list
                    temp = next;    // move temp over 
                    next = next->exited;
                }
                // next is now NULL;
                temp->exited = current_thread; //sent current thread to exited!
            } else {
                list_of_terminated_threads = current_thread;
            }
        }
    }

    // don't deallocate, next thread will deallocate for us in lwp_wait()
    current_thread->status = LWP_TERM;

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

    if(current_thread){
        // CASE: current thread waited before no terminated threads so we have now come back after thread terminated
        thread thread_for_cleanup = current_thread->exited; 

        if(!thread_for_cleanup){ // if exited value does not exist

            if(list_of_terminated_threads){ // if terminated threads exist
                
                thread_for_cleanup = list_of_terminated_threads; // grab the oldest terminated thread (FIFO)
                thread_for_cleanup->lib_one = NULL;  // so no more linked list!

                thread temp = list_of_terminated_threads->lib_one;
                list_of_terminated_threads = temp; // move queue over
            }
            else { // no terminated threads exist
                if(current_scheduler->qlen() > 1){ // running threads still happening 
                    current_scheduler->remove(current_thread); // remove off scheduler 

                    if(list_of_waiting_threads) {
                        thread temp = list_of_waiting_threads; 
                        thread next = temp->lib_one;

                        while(next){
                            temp = next;
                            next = next->lib_one;
                        }

                        temp->lib_one = current_thread;
                    } else {
                        list_of_waiting_threads = current_thread;
                        return NO_THREAD;   // return no thread since we didn't clean up yet
                    }
                } 
                else { // then we do something else?? 
                    return NO_THREAD; // NO_THREAD because we aren't cleaning up any thread
                }
            }

        } else {
            // deallocates resources of terminated LWP
            // NOTE: be careful don't deallocate stack of main thread
            if(thread_for_cleanup->tid != PID_START){ // not main
                if (munmap(thread_for_cleanup->stack, thread_for_cleanup->stacksize) == -1) {
                    perror("munmap");
                    return EXIT_FAILURE;
                }
            }
            free(thread_for_cleanup);
        }
    }

    return NO_THREAD;
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

/* 
current process we are running 
*/
int main(void) {
    lwp_set_scheduler(current_scheduler);
    lwp_start();

    return 0;
}
