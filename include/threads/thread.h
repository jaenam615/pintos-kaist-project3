#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#include "devices/timer.h"
#include "filesys/file.h"
#ifdef VM
#include "vm/vm.h"
#endif

#include "threads/synch.h"

/* States in a thread's life cycle. */
enum thread_status {
	THREAD_RUNNING,     /* Running thread. */
	THREAD_READY,       /* Not running but ready to run. */
	THREAD_BLOCKED,     /* Waiting for an event to trigger. */
	THREAD_DYING        /* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * The upshot of this is twofold:
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 *
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
 * the run queue (thread.c), or it can be an element in a
 * semaphore wait list (synch.c).  It can be used these two ways
 * only because they are mutually exclusive: only a thread in the
 * ready state is on the run queue, whereas only a thread in the
 * blocked state is on a semaphore wait list. */
struct thread {
	/* Owned by thread.c. */
	tid_t tid;                          /* Thread identifier. */
	enum thread_status status;          /* Thread state. */
	char name[16];                      /* Name (for debugging purposes). */
	int priority;                       /* Priority. */
	int original_priority;				/* 원래의 우선도(priority)*/
	int64_t sleep_ticks; 				/* 자고 있는 시간*/
	int has_lock;
	/* Shared between thread.c and synch.c. */
	struct list_elem elem;              /* List element. */
	struct list_elem donor_elem;

	struct list donors;					/* 해당 쓰레드에 기부한 목록*/
	struct lock *wait_on_lock;			/* 이 락이 없어서 못 가고 있을 때*/

	int nice_value;
	int recent_cpu;
	struct list_elem all_elem;
	
#ifdef USERPROG
	/* Owned by userprog/process.c. */
	uint64_t *pml4;                     /* Page map level 4 */
	// struct list fd_table;
	// unsigned last_created_fd;

#endif
#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	struct supplemental_page_table spt;
	// struct list frame_list;

#endif
	void* stack_pointer;
	void* stack_bottom; 
	/* Owned by thread.c. */
	struct intr_frame tf;               /* Information for switching */
	unsigned magic;                     /* Detects stack overflow. */

	//IMPLEMENTATION
	struct list child_list;
	struct list_elem child_list_elem;

	struct thread *parent;
	int exit_status; 
	struct intr_frame parent_tf;
	// struct file **descriptor_table;
	// int fd_idx;

	struct file *running;
	/* 자식 프로세스의 fork가 완료될 때까지 기다리도록 하기 위한 세마포어 */
	struct semaphore process_sema;
	struct semaphore wait_sema;
	struct semaphore exit_sema;

	struct list fd_table;
	unsigned last_created_fd;

};

struct file_descriptor
{
	unsigned fd;
	struct file* file;
	struct list_elem fd_elem;
};


/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

void do_iret (struct intr_frame *tf);

//구현목록
void thread_sleep(int64_t ticks);
void thread_wakeup(int64_t ticks);


void update_load_avg();
void apply_to_all();
int calculating_recent_cpu(struct thread* t);
struct list all_list;
struct list ready_list;

void calc_all_recent_cpu();
bool priority_scheduling(const struct list_elem *a_, const struct list_elem *b_,
            void *aux UNUSED);

void update_priority();
void calculate_all_priority();
void try_thread_yield();

#endif /* threads/thread.h */

#ifdef USERPROG
int allocate_fd(struct file *file, struct list *fd_table);
#endif