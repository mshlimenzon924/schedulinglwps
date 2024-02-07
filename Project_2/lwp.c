#include "/Asgn2/include/lwp.h"

scheduler current_scheduler;

// global variable scheduler 
// init and shutdown functions inside the scheduler are done in lwp.h
// keep track of what's terminated
// what's waiting 


// Creating new lightweight process (thread), adds to Scheduler, But does not run it 
tid_t lwp_create(lwmpfun function, void *argument) {

    // allocate stack

    // getting page_size
    long page_size = sysconf(_SC_PAGE_SIZE);
    if (page_size == -1) {
        fprintf(stderr, "sysconf issue\n");
        return NO_THREAD;
    }

    // getting stack size
    struct rlimit rlim;
    if (getrlimit(RLIMIT_STACK, &rlim) == -1) {
        printf(stderr, "getrlimit issue\n");
        return NO_THREAD;
    } 
    long stack_size;
    if (rlim.rlim_cur == RLIM_INFINITY) || (rlim.rlim_cur > 0) {
        stack_size = DEFAULT_STACK_SIZE;
    } else {
        stack_size = rlim.rlim_cur;
    }
    // rounding up to nearest multiple of page size Round up to the nearest multiple of page size
    long stack_size = (long)(ceil((double)stack_size / page_size) * page_size);


    stack_size = 16*100;  

    void * stack = mmap(NULL, stack_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (stack == MAP_FAILED) {
        fprintf(stderr, "mmap issue\n");
        return NO_THREAD;
    }

    // allocate space for registers 
     // mmap + size = jsut put return at the top of stack

    /// allocate context
    thread *thread_new = (thread)malloc(sizeof(thread));
    context thread_info = (context)malloc(sizeof(context));
    thread_new = &thread_info   //set up thread_new pointer
    thread_info.


    // manual go back call at top (main call) - exit call 
    // index with functions - 16 
    // stack - set top as exit of stack
    // actual registers - atomic switch 
    // empties 
    // Fills up stack correctly 

    // admit thread to scheduler
    current_scheduler->admit(thread_new);


    // 1. malloc context of thread (arrow to dereference pointer . for rfile cause it is struct not pointer)
    // new_thread->state.rsp;
    // pg.4 for mmap 

    // pg.8/ 9 for our stack
    // draw this out and really know where you need to go! 
    // know where to slide in return address
    // KEY: Stack be aligned = what that means; needs to be divisible by 16; can get there because unsigned longs
    // count how many bytes to get 16 due to the unsigned longs in our stack - not divisible by 16
    // confirm that stack is perfectly aligned 

    // returns what you want and not what's there 
    // it's gonna have its own our stack we are making stack - don't use the stack you have, use my stack 
    // manipulate our stack to return somewhere else 
    // Remember, the ret instruction, while called “return”, really means “pop the top of the stack into the program counter.”

    // 2. malloc stack  == mmap();
    // gonna put the address in LWP rap/ address of function you are placing, return address different can trick it  

    // 3. fill in members of context where stack points, process id, all the other variables lib1, lib2
    // lib_one = keep track of what threads are terminated, lib_two = design_choice 
    //lib_one and lib_two are reserved for the use of the library internally, for any purpose or no purpose at all. 
    // lib_one and lib_two 
    // (Many people find these useful to maintain a global linked list of all threads for implementing tid2thread() 
    // or perhaps for keeping track of threads that are waiting.)
    // intiliaze stack and prepare it 
    // intialize state r file rfile ==> initiliaze stuff in rfile 
    // thread ready to go 

    // set up stack (hardest part)
    // preparing stack - trick 
    // trick c to return to address you want 

    // admit to scheduler 

    // make global list of threads ALSO
    // return process ID or NO_THREAD if thread cannot be created 







    // allocate thread to make space for context (use context struct)
    // create some structure with members you think would be part of the thread = malloc 
    // process id, stack pointer currently, program counter can return to where the code is running, 



    // parameter function = has code to be executed by thread 
    // when function is called, the code will be executed until it either calls lwp_exit()
    // or function terminates with termination status 
    // thread function takes single arg (pointer to anything) aka parameter argument 

    // Creates new thread 
    // Admits it to the current scheduler 
    // Creates resources for it: context and stack (intilaize both and connect with scheduler)
    // DO NOT RUN THIS PROCESS! up to scheduler to do that 

    // returns LWP id of new thread or NO_THREAD if thread cannot be created 

    // you have pointer to base of mmap take that and add size of stack to jump to top put exit to main function
    // stack will populate and then return will be set at top 
}


// Starts LWP system
void lwp_start(void){
    // Starts LWP system by converting calling thread into LWP
    // allocates context for main thread and admits to scheduler 
    // yields control to whichever thread scheduler indicates 

    // how to get data from calling thread in order to convert to LWP 
    // take struct declare object and give it threadid and status and admit to scheduler 
    // 
    // create thread give very basic data 
    // give it thread id and admit to scheduler and then back on main process 
    // context of calling thread?
    // 

    // to do
    // give thread id
    // admit it (stacks already created etc context swap happens in lwp yield )

    // NOTE: no need to make stack for this thread, it already has one! 
}

// Yields control to another thread 
void lwp_yield(void){
    // Yields control to another thread depending on scheduler 
    // Here is all the stack moving stuff happening
    // Saving current thread's context, restoring next thread's context

    // If no next thread, end entire program = call exit with termination status of calling thread

    //scheduler determines what comes next once scheduler is written yield is just call to scheduler just ask next 
    // swap out contexts 4-5 lines 

    //
    //determine thread to yield too scheduler->next()
    //swap out context

}

// Terminates current thread and yields to whichever thread the scheduler choose
void lwp_exit(int exitval) {
    // terminates calling thread!! 
    // termination status becomes low 8 bits of passed integer (exitval)
    

    // don't deallocate or anything - the next thread will deallocate it for us in lwp_wait()

    // yields control to next thread using lwp_yield()

    // removes stack 
    if (munmap(stack, size) == -1) {
        perror("munmap");
        return EXIT_FAILURE;
    }
}


// Waiting for thread to terminate 
tid_t lwp_wait(int *status){
    // waiting for thread to terminate
    // If multiple terminated threads, grab in FIFO terminated threads (aka oldest thread first)
    // If there are no terminated threads and still running ones, caller of lwp_wait() needs to block
    // We will deschedule it, sched->remove() , and place it on queue of waiting threads 
    // Once a thread FINALLY calls lwp_exit() - make it be known its the oldest thread somehow (us to decide)
    // and then remove the blocking process from our waiting queue and readmit it back into the schedule
    // with sched->admit() command so it can finish its lwp_wait() command


    // NOTE: if there are literally no threads left that could block, then we just return
    // NO_THREAD; we can tell this by using qlen(): check if qlen() > 1? (is this correct?)

    // deallocates resources of terminated LWP
    // NOTE: be careful don't deallocate stack of main thread

    // if status is non-NULL (status has termination status)? weird phrasing (need to do something with status)
    // reports termination status if not NULL

    // returns either tid of terminated threat or NO_THREAD 
    // if blocking forever (since no more runnable threads that could terminate)
}

// Getting tid of called LWP 
tid_t lwp_gettid(void){
    // returns either tid of thread or NO_THREAD if not called by LWP
}

// given a thread ID, return thread 
thread tid2thread(tid_t tid){
    // given a thread ID, return thread or NO_THREAD if ID is invalid
}

// LWP package uses scheduler to choose next process to run
void lwp_set_scheduler(scheduler sched) {
    if (!scheduler) {   // if scheduler is NULL, return back to round-robin scheduling
        // set up round robin
        // struct scheduler roundrobin = {NULL, NULL, rr_admit, rr_remove, rr_next, rr_qlen};
        current_scheduler = &roundrobin;
        roundrobin.init = rr_init();  // defined in lwp.h
        roundrobin.shutdown = rr_shutdown();
    } else {    // if not null, then use new scheduler 
        current_scheduler = sched;
    }
}

// Helps us get current scheduler 
// return pointer to current scheduler
scheduler lwp_get_scheduler(void){
    return scheduler;
}

// current process we are running 
int main(void) {


    lwp_start();

}