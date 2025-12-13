
#include "queue.h"
#include "sched.h"
#include <pthread.h>

#include <stdlib.h>
#include <stdio.h>
static struct queue_t ready_queue;
static struct queue_t run_queue;
static pthread_mutex_t queue_lock;


#ifdef MLQ_SCHED
static struct queue_t mlq_ready_queue[MAX_PRIO];
static int slot[MAX_PRIO];
#endif

int queue_empty(void) {
#ifdef MLQ_SCHED
	unsigned long prio;
	for (prio = 0; prio < MAX_PRIO; prio++)
		if(!empty(&mlq_ready_queue[prio])) 
			return -1;
#endif
	return (empty(&ready_queue) && empty(&run_queue));
}

void init_scheduler(void) {
#ifdef MLQ_SCHED
    int i ;

	for (i = 0; i < MAX_PRIO; i ++) {
		mlq_ready_queue[i].size = 0;
		slot[i] = MAX_PRIO - i; 
	}
#endif
	ready_queue.size = 0;
	run_queue.size = 0;
	pthread_mutex_init(&queue_lock, NULL);
}

#ifdef MLQ_SCHED
/* 
 *  Stateful design for routine calling
 *  based on the priority and our MLQ policy
 *  We implement stateful here using transition technique
 *  State representation   prio = 0 .. MAX_PRIO, curr_slot = 0..(MAX_PRIO - prio)
 */
struct pcb_t *get_mlq_proc(void) {
    struct pcb_t *proc = NULL;
    /*
     * TODO: Gatekeeper
     * Iterate through priorities from high to low
     */
    pthread_mutex_lock(&queue_lock);
    
    for (int i = 0; i < MAX_PRIO; i++) {
        if (mlq_ready_queue[i].size > 0) {
            /* Check if the current priority has slots left */
            if (slot[i] > 0) {
                proc = dequeue(&mlq_ready_queue[i]);
                slot[i]--;
                break;
            } else {
                /* Slot expired, reset slot and move to next priority or rotate? 
                 * MLQ policy: If slot expired, we might still pick it if no higher prio? 
                 * Usually, we reset slot when we pick a new queue or when it's empty.
                 * Let's implement: Reset slot if we pick from this queue.
                 * Wait, if slot == 0, we should skip? Or reset?
                 * Simple MLQ: Fixed priority. If slot > 0, run. If slot == 0, reset?
                 * Let's assume: Reset slot when queue becomes empty or we switch.
                 * Actually, standard MLQ: High prio always wins. Slot is for Round Robin WITHIN same prio.
                 */
                 slot[i] = MAX_PRIO - i; // Reset slot
                 proc = dequeue(&mlq_ready_queue[i]);
                 slot[i]--;
                 break;
            }
        }
    }
    
    pthread_mutex_unlock(&queue_lock);
    return proc;
}

void put_mlq_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&mlq_ready_queue[proc->prio], proc);
	pthread_mutex_unlock(&queue_lock);
}

void add_mlq_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&mlq_ready_queue[proc->prio], proc);
	pthread_mutex_unlock(&queue_lock);	
}

struct pcb_t * get_proc(void) {
	return get_mlq_proc();
}

void put_proc(struct pcb_t *proc) {
    return put_mlq_proc(proc);
}

void add_proc(struct pcb_t *proc) {
    return add_mlq_proc(proc);
}
#else
struct pcb_t * get_proc(void) {
    struct pcb_t *proc = NULL;
    pthread_mutex_lock(&queue_lock);
    if (ready_queue.size > 0) {
        proc = dequeue(&ready_queue);
    }
    pthread_mutex_unlock(&queue_lock);
    return proc;
}


void put_proc(struct pcb_t * proc) {
    proc->ready_queue = &ready_queue;
    proc->running_list = &running_list;

    pthread_mutex_lock(&queue_lock);
    enqueue(&run_queue, proc);
    enqueue(&running_list, proc);
    pthread_mutex_unlock(&queue_lock);
}

void add_proc(struct pcb_t * proc) {
    proc->ready_queue = &ready_queue;
    proc->running_list = &running_list;

    pthread_mutex_lock(&queue_lock);
    enqueue(&ready_queue, proc);
    enqueue(&running_list, proc);
    pthread_mutex_unlock(&queue_lock);    
}


#endif


