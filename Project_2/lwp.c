#include "Asgn2/include/lwp.h"
#include "rr.h"

struct scheduler roundrobin = {NULL, NULL, rr_admit, rr_remove, rr_next, rr_qlen};
thread list_of_all_threads = NULL;         // list of all threads at the beginning of program (linked list)
thread list_of_terminated_threads = NULL;
scheduler current_scheduler;

// Define a global variable to track the current thread identifier
int current_tid = PID_START;

// Creating new lightweight process (thread), adds to Scheduler, But does not run it 
tid_t lwp_create(lwpfun function, void *argument) {
    /// ALLOCATE CONTEXT
    thread new_thread = (thread)malloc(sizeof(context));
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
    new_thread->tid = current_tid++;
    new_thread->status =  LWP_LIVE;  // thread is live
    new_thread->stacksize = (size_t)stack_size;
    new_thread->state.fxsave = FPU_INIT;

    // set up thread
    new_thread->state.rdi = (unsigned long)&argument; 
    new_thread->state.rbp = (unsigned long)&stack;  // bottom of stack 
    
    // Stack Setup
        // lower address 
    // local variables 
    // function arguments
    // caller-saved registers 
    // old rbp (current stack pointer)
    // instructions where? function we want to call (function)
    // return adress/ exit function (lwp_exit)
        // higher address

    // should i push args?
    stack[0] = (unsigned long)&lwp_exit;         // address where to go back 
    stack[-1] = (unsigned long)&function;       // return address = main function
    stack[-2] = (unsigned long)new_thread->state.rbp;       
    // should i push registers?

    new_thread->state.rsp = (unsigned long)*(stack - 2); //pointing to top of stack
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

static void lwp_wrap(lwpfun fun, void *arg){
    // Call given lwpfunciton with given argument
    //calls lwp_exit with its return value 

    int rval;
    rval = fun(arg);
    lwp_exit(rval);
}

// Starts LWP system
void lwp_start(void){
    // Starts LWP system by converting calling thread into LWP
    // allocates context for main thread and admits to scheduler 
    // yields control to whichever thread scheduler indicates 

    // create thread give very basic data (tid, alive status)
    // admit to scheduler (stacks already created etc context swap happens in lwp yield)
    // NOTE: no need to make stack for this thread, it already has one! 

    // create new thread with threadID and alive status 
    thread new_lwp = (thread)malloc(sizeof(struct threadinfo_st));
    //new_lwp = &new_lwp;
    new_lwp->tid = 1;
    new_lwp->status = LWP_LIVE;

    // schedule it 
    current_scheduler->admit(new_lwp);

    // yield
    lwp_yield();
}

// Yields control to another thread 
void lwp_yield(void){
    // Yields control to another thread depending on scheduler 
    // Here is all the stack moving stuff happening
    // Saving current thread's context, restoring next thread's context

    // If no next thread, end entire program = call exit with termination status of calling thread
    // scheduler determines what comes next once scheduler is written yield is just call to scheduler
    // swap out contexts
    // determine thread to yield too scheduler->next()
    // lwp_exit() terminates the calling thread and switches to another, if any.

    // thread new_lwp = (thread)malloc(sizeof(struct threadinfo_st));
    // new_lwp = &new_lwp;
    // new_lwp->tid = 2;
    // new_lwp->status = LWP_LIVE;
    // current_lwp 

    /* Test Code */
    // get next thread to be scheduled
    thread next_scheduled = current_scheduler->next();

    // if no next thread end program with call to lwp_exit(int status);
    if(!next_scheduled)
        lwp_exit(current_thread->status);

    // swap out current threads context with next threads context
    // loads in new context, saves old context 
    swap_rfiles(&current_thread->state, &next_scheduled->state); // takes in 2 arg stores current reg values, loads to register 
    
    // mark next_scheduled as the current lwp 
    current_thread = next_scheduled;
}

// Terminates current thread and yields to whichever thread the scheduler choose
 // termination status becomes low 8 bits of passed integer (exitval)
void lwp_exit(int exitval) {
    // set up termination status
    current_thread->status = MKTERMSTAT(LWP_TERM, exitval);

    // terminates current_thread
    if(current_thread){
        current_scheduler->remove(current_thread);
        thread temp_wait = list_of_waiting_threads;
        if(temp_wait){ //temp_wait exists then let's throw it in oldest wait node
            temp_wait->exited = current_thread;
            list_of_waiting_threads = temp_wait->lib_one;    // dequeue oldest waiting node
            current_scheduler->admit(temp_wait);     // add back to scheduler
        }
        else { // if no waiting nodes
            // great let's just add to the list of terminated nodes that need to be cleaned up
            if(list_of_terminated_threads){
            // add terminated thread to the BACK of the linked list 
                thread temp = list_of_terminated_threads;
                thread next = temp->exited;
                while(next) { // while there exists another thread in linked list
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

    // don't deallocate or anything - the next thread will deallocate it for us in lwp_wait()
    current_thread->status = LWP_TERM;

    // yields control to next thread using lwp_yield() 
    lwp_yield();

    // Terminates the calling thread. Its termination status becomes the low 8 bits of the passed integer. The thread’s resources will be deallocated once it is waited for in lwp_wait(). Yields control to the next thread using lwp_yield().

//     A thread’s termination value is the low 8 bits either of the argument to lwp_exit() or of the return value of the thread function. These are combined into a single integer using the macro MKTERMSTAT() which is what is passed back by lwp_wait().
//     MKTERMSTAT(a,b)  // combine status and exit code into an int 
//     LWP_LIVE // status of live thread
//     LWP_TERM //status of terminated thread
//     LWPTERMINATED(s) // true if status represnets tewrminateed thread
//     LWPTERMSTAT(s)  // extracts exit code from status 
}


// Waiting for thread to terminate 
// Waiting for thread to terminate 
tid_t lwp_wait(int *status){
    // waiting for thread to terminate
    // If multiple terminated threads, grab in FIFO terminated threads (aka oldest thread first)
    // If there are no terminated threads and still running ones, caller of lwp_wait() needs to block
    // We will deschedule it, sched->remove() , and place it on queue of waiting threads 
    // Once a thread FINALLY calls lwp_exit() - make it be known its the oldest thread somehow (us to decide)
    // and then remove the blocking process from our waiting queue and readmit it back into the schedule
    // with sched->admit() command so it can finish its lwp_wait() command

    if(current_thread){
        thread thread_for_cleanup = current_thread->exited; 
        // CASE: current thread waited before no terminated threads so we have now come back after thread terminated

        if(!thread_for_cleanup){ // if exited value does not exist
            // check if any terminated threads exist
            if(list_of_terminated_threads){ // if threads exist
                // let's grab the oldest terminated friend (FIFO)
                thread_for_cleanup = list_of_terminated_threads;
                thread_for_cleanup->lib_one = NULL;  // so no more linked list!

                thread temp = list_of_terminated_threads->lib_one;
                list_of_terminated_threads = temp; // moving queue over
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
                } {
                    // then we do something else?? 
                    return NO_THREAD;       // because we aren't cleaning up any thread
                }
            }

        } else {
            // deallocates resources of terminated LWP
            // NOTE: be careful don't deallocate stack of main thread
            if(thread_for_cleanup->tid != 1){ // not main
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


// Getting tid of Current LWP (either it is the LWP)
// returns either tid of thread or NO_THREAD if not called by LWP
tid_t lwp_gettid(void){
    if(current_thread) {
        return current_thread->tid; 
    }
    return NO_THREAD; 
}

// Given a thread ID, return thread or NO_THREAD if ID is invalid
thread tid2thread(tid_t tid){
    thread temp = list_of_all_threads;
    while(temp){
        if(tid == temp->tid) {
            return temp;
        } 
        temp = temp->lib_one;   // if no match skips to next thread
    } 
}

// LWP package uses scheduler to choose next process to run
void lwp_set_scheduler(scheduler sched) {
    if (!sched) {   // if scheduler is NULL, return back to round-robin scheduling
        // set up round robin
        current_scheduler = &roundrobin;
        roundrobin.init = rr_init;  // defined in lwp.h
        roundrobin.shutdown = rr_shutdown;
    } else {    // if not null, then use new scheduler 
        current_scheduler = sched;
    }
}

void rr_init(){

}

void rr_shutdown(){
    while(current_scheduler->qlen() >= 1){
        current_scheduler->remove(current_thread);
    }
    free(queue);
}

// Helps us get current scheduler 
// Return pointer to current scheduler
scheduler lwp_get_scheduler(void){
    return current_scheduler;
}

// current process we are running 
int main(void) {
    lwp_start();

    return 0;
}
