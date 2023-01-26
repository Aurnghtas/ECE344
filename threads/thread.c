#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include "thread.h"
#include "interrupt.h"

#define EXITED 0
#define READY 1
#define RUNNING 2
#define BLOCKED 3
#define NO_STATE -10

/* This is the thread control block */
struct thread {
    Tid thread_id;
    int thread_state;
    void* stack_pointer;
    ucontext_t thread_context;
    struct wait_queue* self_wait_queue;
};

/* Global array of thread structures */
struct thread thread_array[THREAD_MAX_THREADS];

/* Global queues */
struct queue_node {
    struct thread* node;
    struct queue_node* next_node;
};

struct queue {
    struct queue_node* head;
};

/* This is the wait queue structure */
struct wait_queue {
    struct queue* wait_queue;
};

/* global queues */
struct queue* ready_queue = NULL;
struct queue* exited_queue = NULL;

/* free the threads that are in the exited queue*/
void free_exited_queue();

/* current running thread */
Tid curr_running_thread;

/* function to manipulate queues
 * only be able to append at the beginning and at the end */
void enqueue(struct queue* queue, struct thread *thread) {
    if(queue == NULL) {
        return;
    }
    
    struct queue_node* insert_node = (struct queue_node *)malloc(sizeof(struct queue_node));
    insert_node->node = thread;
    insert_node->next_node = NULL;
    
    if(queue->head == NULL) {
        queue->head = insert_node;
    } else {
        struct queue_node* temp = queue->head;
        while (temp->next_node != NULL) {
            temp = temp->next_node;
        }
        temp->next_node = insert_node;
    }
}

/* function to manipulate queues 
 * only be able to remove the head of the queue*/
struct thread* dequeue_head(struct queue* queue) {

    if(queue == NULL) {
        return NULL;
    }
    
    if(queue->head == NULL) {
        return NULL;
    } else {
        struct queue_node* temp = queue->head;
        struct thread* thread = temp->node;
        queue->head = queue->head->next_node;
        free(temp);
        return thread;
    }
}

/* function to manipulate queues
 * only be able to remove the desired thread */
struct thread* dequeue_tid(struct queue* queue, Tid thread_id) {
    if(queue == NULL) {
        return NULL;
    }
    
    if(queue->head == NULL) {
        return NULL;
    } else {
        /* previous thread node and current thread node */
        struct queue_node* prev = NULL;
        struct queue_node* curr = queue->head;
        struct thread* thread = NULL;
        
        /* look for the desired thread */
        while (curr != NULL) {
            if(curr->node->thread_id == thread_id) {
                break; //find the desired thread
            }
            prev = curr;
            curr = curr->next_node;
        }
        
        if(curr == NULL) {
           return NULL;
        }
        
        /* it's the first thread in the queue */
        if(curr == queue->head) {
            queue->head = queue->head->next_node;
            thread = curr->node;
            free(curr);
            return thread;
        }
        
        /* other cases */
        prev->next_node = curr->next_node;
        thread = curr->node;
        free(curr);
        return thread;
    }
}

void thread_init(void) {
    /* set tid and state of each thread */
    for (int i=0; i<THREAD_MAX_THREADS; i++) {
        thread_array[i].thread_id = i;
        thread_array[i].thread_state = NO_STATE;
    }
    
    /* set up the ready queue and exited queue */
    ready_queue = (struct queue*)malloc(sizeof(struct queue));
    ready_queue->head = NULL;
    exited_queue = (struct queue*)malloc(sizeof(struct queue));
    exited_queue->head = NULL;
    
    /* set up the main thread */
    thread_array[0].self_wait_queue = wait_queue_create();
    thread_array[0].thread_state = RUNNING;
    curr_running_thread = 0;
    getcontext(&thread_array[0].thread_context);
}

Tid thread_id() {
    if(curr_running_thread<0 || curr_running_thread>THREAD_MAX_THREADS-1){
        return THREAD_INVALID;
    } else {
        return curr_running_thread;
    }
}

/* thread starts by calling thread_stub. The arguments to thread_stub are the
 * thread_main() function, and one argument to the thread_main() function. */
void thread_stub(void (*thread_main)(void *), void *arg) {
    interrupts_on();
    
    thread_main(arg); // call thread_main() function with arg
    thread_exit();
}

Tid thread_create(void (*fn) (void *), void *parg) {
    int enabled = interrupts_off();
    
    /* the thread id that is available to be created */
    Tid thread_num=0;
    
    /* find a available thread that is not being used or is exited */
    while(thread_num<THREAD_MAX_THREADS) {
        if(thread_array[thread_num].thread_state==NO_STATE || thread_array[thread_num].thread_state==EXITED) {
            break;
        }
        thread_num++;
    }
    
    /* exceed the THREAD_MAX_THREADS */
    if(thread_num == THREAD_MAX_THREADS) {
        interrupts_set(enabled);
        
        return THREAD_NOMORE;
    }
    
    /* allocate the stack */
    void* stack = malloc(THREAD_MIN_STACK);
    
    /* if no more stack is allocated */
    if(stack == NULL) {
        interrupts_set(enabled);
        
        return THREAD_NOMEMORY;
    }
    
    thread_array[thread_num].stack_pointer = stack;
    thread_array[thread_num].thread_state = READY;
    getcontext(&thread_array[thread_num].thread_context);

    /* the size of a void pointer in c is 8 bytes */
    thread_array[thread_num].thread_context.uc_mcontext.gregs[REG_RSP] = (long long int) stack + THREAD_MIN_STACK - 8;
    thread_array[thread_num].thread_context.uc_mcontext.gregs[REG_RIP] = (long long int) thread_stub;
    thread_array[thread_num].thread_context.uc_mcontext.gregs[REG_RDI] = (long long int) fn;    //first argument
    thread_array[thread_num].thread_context.uc_mcontext.gregs[REG_RSI] = (long long int) parg;  //second argument
    
    //wait_queue_array[thread_num] = wait_queue_create();
    thread_array[thread_num].self_wait_queue = wait_queue_create();
    
    /* now this thread is ready*/
    enqueue(ready_queue, &thread_array[thread_num]);
    
    interrupts_set(enabled);
    
    return thread_num;
}

Tid thread_yield(Tid want_tid) {
    int enabled = interrupts_set(0);
    
    free_exited_queue();
    
    /* pass in a invalid thread id */
    if(want_tid<-2 || want_tid >THREAD_MAX_THREADS-1) {
        interrupts_set(enabled);
        return THREAD_INVALID;
        /* now it is a valid thread id*/
    } else {
        /* no threads in the ready queue*/
        if(want_tid == THREAD_ANY && ready_queue->head==NULL) {
            interrupts_set(enabled);
            return THREAD_NONE;
        }
        
        /* if the running thread is not yielding itself or the called thread is not in the ready state*/
        if(0 <= want_tid && want_tid <= THREAD_MAX_THREADS-1){
            if(want_tid != THREAD_SELF && thread_array[want_tid].thread_state != READY && 
                    want_tid != curr_running_thread && thread_array[want_tid].thread_state != RUNNING) {
                interrupts_set(enabled);
                return THREAD_INVALID;
            }
        }
    }
    
    /* following are the scenario that the given want_tid is valid and is in the ready queue */
    
    /* if the running thread yields itself */
    if(want_tid == THREAD_SELF || want_tid == curr_running_thread) {
        interrupts_set(enabled);
        
        return curr_running_thread;
    }
     
    /* the switched/called thread */
    struct thread* want_thread = NULL;
    if(want_tid == THREAD_ANY) {
       want_thread = dequeue_head(ready_queue);
    } else {
        want_thread = dequeue_tid(ready_queue, want_tid);
    }
    
    /* use volatile to avoid compiler to optimize it
     * e.g. to put it on the stack */
    volatile int first_time = 0;
    
    getcontext(&thread_array[curr_running_thread].thread_context);
    
    if(first_time) {
        first_time = 0;
        free_exited_queue();
        interrupts_set(enabled);
        
        return want_thread->thread_id;
    } else {
        first_time=1;
        
        /* put the current running thread in the ready queue*/
        thread_array[curr_running_thread].thread_state = READY;
        enqueue(ready_queue, &thread_array[curr_running_thread]);
        
        /* switch to the called thread to run */
        curr_running_thread = want_thread->thread_id;
        want_thread->thread_state = RUNNING;
        setcontext(&want_thread->thread_context);
    }
    
    /* should not go here
     * but return thread_none for debug purpose */
    interrupts_set(enabled);
    return THREAD_NONE;
}

/* this function free the thread in the exited queue */
void free_exited_queue() {
    struct queue_node* thread = NULL;
    while(exited_queue->head != NULL) {
        thread = exited_queue->head;
        exited_queue->head = exited_queue->head->next_node;
        
        thread->next_node = NULL;
        free(thread->node->stack_pointer);
        thread->node->stack_pointer = NULL;
        free(thread);
    }
}

void thread_exit() {
    int enabled = interrupts_set(0);

    thread_wakeup(thread_array[curr_running_thread].self_wait_queue, 1);
    
    /* this is the last running thread and no more threads in the ready queue */
    if(ready_queue->head == NULL) {
        free(ready_queue);
        free(exited_queue);
        exit(0);
    } else {  
        /* set current running thread as exited and put it in the exited queue*/
        thread_array[curr_running_thread].thread_state = EXITED;
        enqueue(exited_queue, &thread_array[curr_running_thread]);
        
        /* dequeue any available ready thread */
        struct thread* want_thread = dequeue_head(ready_queue);
        curr_running_thread = want_thread->thread_id;
        //want_thread->thread_state = RUNNING;
          
        setcontext(&want_thread->thread_context);
        interrupts_set(enabled);
    }
 
}

Tid thread_kill(Tid tid) {
    int enabled = interrupts_set(0);
    
    /* if the given thread id is not valid or it's the running thread or it's not in the ready state */
    if(tid<0 || tid>THREAD_MAX_THREADS-1 || tid == curr_running_thread || 
            thread_array[tid].thread_state == EXITED/*|| thread_array[tid].thread_state != READY*/) {
        interrupts_set(enabled);
        return THREAD_INVALID;
    } 
    
    /* mark the called thread to exited and put in the exited queue */
    thread_array[tid].thread_state = EXITED;
    
    struct thread* killed_thread = dequeue_tid(ready_queue, tid);
    if(killed_thread != NULL) {
        enqueue(exited_queue, killed_thread);
    }
    
    interrupts_set(enabled);
    return tid;
}

/*******************************************************************
 * Important: The rest of the code should be implemented in Lab 3. *
 *******************************************************************/

/* make sure to fill the wait_queue structure defined above */
struct wait_queue * wait_queue_create() {
    int enabled = interrupts_set(0);
    
    struct wait_queue *wq;
    wq = malloc(sizeof(struct wait_queue));
    assert(wq);
    
    wq->wait_queue = (struct queue*)malloc(sizeof(struct queue));
    wq->wait_queue->head = NULL;
 
    interrupts_set(enabled);
    return wq;
}

void wait_queue_destroy(struct wait_queue *wq) {
    while(wq->wait_queue->head != NULL) {
        dequeue_head(wq->wait_queue);
    }
    free(wq->wait_queue);
    wq->wait_queue = NULL;
    free(wq);
    wq = NULL;
}

Tid thread_sleep(struct wait_queue *queue) {
    
    int enabled = interrupts_set(0);
    
    /* the wait queue is empty */
    if(queue == NULL) {
        interrupts_set(enabled);
        
        return THREAD_INVALID;
    } 
    
    struct thread* want_thread = dequeue_head(ready_queue);
    
    /* no available threads to run other than the caller */
    if(want_thread == NULL) {
        interrupts_set(enabled);
        
        return THREAD_NONE;
    }
    
    /* use volatile to avoid compiler to optimize it
     * e.g. to put it on the stack */
    volatile int first_time = 0;
    
    getcontext(&thread_array[curr_running_thread].thread_context);
    
    if(first_time) {
        first_time = 0;

        if(thread_array[curr_running_thread].thread_state == EXITED) {
            thread_exit();
        }
        
        interrupts_set(enabled);
        return want_thread->thread_id;
    } else {
        first_time=1;
        
        /* put the current running thread in the wait queue*/
        thread_array[curr_running_thread].thread_state = BLOCKED;
        enqueue(queue->wait_queue, &thread_array[curr_running_thread]);
        
        /* switch to the called thread to run */
        curr_running_thread = want_thread->thread_id;
        want_thread->thread_state = RUNNING;
        interrupts_set(enabled);
        setcontext(&want_thread->thread_context);
    }
    
    interrupts_set(enabled);
    return THREAD_FAILED;
}

/* when the 'all' parameter is 1, wakeup all threads waiting in the queue.
 * returns whether a thread was woken up on not. */
int thread_wakeup(struct wait_queue *queue, int all) {
    int enabled = interrupts_set(0);
    
    int num_threads = 0;
    
    /* queue is empty or no BLOCKED thread in the wait queue */
    if(queue == NULL || queue->wait_queue == NULL || queue->wait_queue->head == NULL) {
        interrupts_set(enabled);
        return 0;
    }
    
    /* queue is not empty AND there are BLOCKED threads in the wait queue*/
    
    /* wake up all the threads*/
    if(all) {
        struct thread* waked_thread = dequeue_head(queue->wait_queue);
        
        while (waked_thread != NULL) {
            if(waked_thread->thread_state == EXITED) {
                waked_thread = dequeue_head(queue->wait_queue);
            } else {
                num_threads++;
                //waked_thread->thread_state = READY;
                enqueue(ready_queue, &thread_array[waked_thread->thread_id]);
                waked_thread = dequeue_head(queue->wait_queue);
            }
        }
        
        interrupts_set(enabled);
        return num_threads;
        
    } else {
        struct thread* waked_thread = dequeue_head(queue->wait_queue);
        waked_thread->thread_state = READY;
        enqueue(ready_queue, &thread_array[waked_thread->thread_id]);
        
        interrupts_set(enabled);
        return 1;
    }
    
    /* function should not reach here as above 3 cases cover everything */
    interrupts_set(enabled);
    return 0;
}

/* suspend current thread until Thread tid exits */
Tid thread_wait(Tid tid) {
    
    int enabled = interrupts_set(0);
    
    if(tid<0 || tid>THREAD_MAX_THREADS-1 || tid == curr_running_thread || thread_array[tid].thread_state == NO_STATE) {
        interrupts_set(enabled);
        return THREAD_INVALID;
    }
    
    thread_sleep(thread_array[tid].self_wait_queue);
    
    interrupts_set(enabled);
    return tid;
}

struct lock {
    /* acquire the lock? 0 is not, 1 is yes */
    int acquire;
    struct wait_queue* wait_queue;
};

struct lock * lock_create() {
    int enabled = interrupts_set(0);
    
    struct lock *lock;

    lock = malloc(sizeof(struct lock));
    assert(lock);

    lock->acquire = 0;
    lock->wait_queue = wait_queue_create();
    
    interrupts_set(enabled);
    return lock;
}

void lock_destroy(struct lock *lock) {
    int enabled = interrupts_set(0);
    
    assert(lock != NULL);

    if(lock->acquire == 0) {
        wait_queue_destroy(lock->wait_queue);
        free(lock);
    }

    interrupts_set(enabled);
}

void lock_acquire(struct lock *lock) {
    int enabled = interrupts_set(0);
    
    assert(lock != NULL);
    
    while(lock->acquire == 1) {
        thread_sleep(lock->wait_queue);
    }
    
    lock->acquire = 1;
    interrupts_set(enabled);
    
}

void lock_release(struct lock *lock) {
    int enabled = interrupts_set(0);
    
    assert(lock != NULL);

    if(lock->acquire == 1) {
        lock->acquire = 0;
        thread_wakeup(lock->wait_queue, 1);
    }
    
    interrupts_set(enabled);
}

struct cv {
    struct wait_queue* wait_queue;
};

struct cv * cv_create() {
    int enabled = interrupts_set(0);
    
    struct cv *cv;

    cv = malloc(sizeof(struct cv));
    assert(cv);

    cv->wait_queue = wait_queue_create();

    interrupts_set(enabled);
    return cv;
}

void cv_destroy(struct cv *cv) {
    int enabled = interrupts_set(0);
    assert(cv != NULL);

    wait_queue_destroy(cv->wait_queue);

    free(cv);
    interrupts_set(enabled);
}

void cv_wait(struct cv *cv, struct lock *lock) {
    int enabled = interrupts_set(0);
    assert(cv != NULL);
    assert(lock != NULL);

    if(lock->acquire == 1) {
        lock_release(lock);
        thread_sleep(cv->wait_queue);
        lock_acquire(lock);
    }
    interrupts_set(enabled);
}

void cv_signal(struct cv *cv, struct lock *lock) {
    int enabled = interrupts_set(0);
    assert(cv != NULL);
    assert(lock != NULL);

    thread_wakeup(cv->wait_queue, 0);
    interrupts_set(enabled);
}

void cv_broadcast(struct cv *cv, struct lock *lock) {
    int enabled = interrupts_set(0);
    assert(cv != NULL);
    assert(lock != NULL);

    thread_wakeup(cv->wait_queue, 1);
    interrupts_set(enabled);
}
