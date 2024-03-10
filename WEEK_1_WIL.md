Krafton Jungle 
Week 7 Team 6 WIL

Project 1 Thread:
Alarm Clock ~ Priority Scheduling(WIP)

---

alarm-single

문제: timer_sleep이 기존에 구현되어 있었으나, Busy-wait을 하는 방식으로 되어있었기 때문에 효율이 좋지 않았다. 

기존 함수:
```c
void
timer_sleep (int64_t ticks) {
	int64_t start = timer_ticks ();

	ASSERT (intr_get_level () == INTR_ON);
	while (timer_elapsed (start) < ticks)
		thread_yield ();
}
```
기존 함수는 while문을 돌면서 시간을 확인하고, 충분한 시간이 되기 전까지 계속 yield를 시도한다.  

수정된 함수:
```c
void
timer_sleep (int64_t ticks) {
	
	int64_t start = timer_ticks ();

	ASSERT(intr_get_level() == INTR_ON);

	thread_sleep(ticks+start);
```
수정된 함수는 그저 쓰레드를 재울 뿐, 이후 깨우는 과정은 timer_interrupt함수를 통해 진행된다. 
쓰레드가 양보할 필요는 없이, 시간이 지났다면 ready list에 넣기만 해주면 된다. 

---

문제 해결  

<b>전역변수:</b>
```c
static struct list sleep_list;
```
Sleep list를 만들어 '잠든' 쓰레드를 모두 보관하고 관리할 수 있도록 하였다.  
해당 리스트에는 추후 나올 함수인 'thread_sleep'를 통해 재운 함수들이 들어간다.  

구조체 변경:  
```c
struct thread {
	tid_t tid;                        
	enum thread_status status;         
	char name[16];                     
	int priority;                       
    int64_t sleep_ticks; // 해당 변수 추가

	struct list_elem elem;     

    //등등
}
```
쓰레드가 'thread_sleep'를 통해 잠들 때, 잠들어있을 시간을 보관할 변수를 추가한다. 

함수:
```c
void thread_sleep(int64_t ticks){

	struct thread *t = thread_current();
	enum intr_level old_level;
	t->sleep_ticks = ticks;

	old_level = intr_disable();
	list_insert_ordered(&sleep_list, &t->elem, sleep_list_order, NULL);
	thread_block();
	intr_set_level(old_level);
}


void thread_wakeup(int64_t ticks) {
  enum intr_level old_level;
  old_level = intr_disable();

    struct list_elem *waking_up = list_front(&sleep_list);
    struct thread *checker = list_entry(waking_up, struct thread, elem);

    if (checker->sleep_ticks <= (ticks)) {
      waking_up = list_pop_front(&sleep_list);
      list_push_back(&ready_list, waking_up);
    } else {
      break;
    }


  intr_set_level(old_level);
}

static bool sleep_list_order(const struct list_elem *a_, const struct list_elem *b_,
            void *aux UNUSED){
  const struct thread *a = list_entry (a_, struct thread, elem);
  const struct thread *b = list_entry (b_, struct thread, elem);

  return a->sleep_ticks < b->sleep_ticks;
}
```
thread_sleep은 쓰레드를 재우며, 이는 특정 정책(policy)에 따라 sleep list에 정렬된 위치로 삽입하고 block하는 방식으로 실행된다.  
이 때 사용된 정책은 sleep_list_order로, sleep_tick의 대소관계에 따라 오름차순으로 정렬 및 삽입된다. 

이 때 주의해야 할 점은, block을 하기 전에 interrupt가 비활성화 되어있어야 한다는 점이다. 
활성화가 되어있다면, 지속되는 interrupt로 인해 효율이 감소하며, 이는 원래 취지인 효율성의 상승과 상반된다. 

thread_wakeup은 일정한 주기로 실행되는 timer_interrupt함수로부터 호출된다.

thread_wakeup은 sleep_list의 front원소를 확인하며, OS가 부팅된 시점부터의 시간이 쓰레드가 일어나야 할 시간을  
초과했을 때 해당 쓰레드는 list_pop_front를 통해 sleep list를 탈출하고 ready list로 들어간다.

이렇게 구현해두면 alarm-multiple도 성공된다.   

--- 
 alarm-simultaneous


문제: 
두 개 이상의 쓰레드가 깨워지는 시간이 동일할 때를 다루는 과제이다. 

해결: 
해당 과제는 thread_wakeup에 조그마한 수정을 통해 해결되었다. 
```c
void thread_wakeup(int64_t ticks) {
  enum intr_level old_level;
  old_level = intr_disable();

  while (!list_empty(&sleep_list)) {
    struct list_elem *waking_up = list_front(&sleep_list);
    struct thread *checker = list_entry(waking_up, struct thread, elem);

    if (checker->sleep_ticks <= (ticks)) {
      waking_up = list_pop_front(&sleep_list);
      list_push_back(&ready_list, waking_up);
    } else {
      break;
    }
  }
}
```

기존에 있던 함수의 깨우는 부분을 while문으로 감싸주면 된다.  
이렇게 하면 여러 쓰레드가 일어날 시간이 되었다면 모두 ready list에 삽입하고 나서야 while문을 탈출한다. 

---

alarm-priority

문제: 
쓰레드에 우선도가 추가되어, 꺠어난 후 ready list에 들어갈 때 우선도에 따라 삽입 및 정렬되어야 한다.  

해결:
위의 sleep list에 넣어줄 때와 비슷하게, ready list로 들어갈 때의 정책을 만들어줬다.  

```c
static bool priority_scheduling(const struct list_elem *a_, const struct list_elem *b_,
            void *aux UNUSED){
	const struct thread *a = list_entry (a_, struct thread, elem);
	const struct thread *b = list_entry (b_, struct thread, elem);

	return a->priority > b->priority;
}
```
두 정책의 주요 차이점은 sleep_tick은 오름차순으로 정렬되는 것에 비해, priority는 내림차순으로 정렬된다는 점이다.  

해당 정책은 sleep_list에서 Unblock되어 ready_list로 갈 때 사용되기 떄문에,  
thread_unblock함수 안에 list_push_back 대신 list_insert_ordered로 변경하여 사용되었다.  

```c
void
thread_unblock (struct thread *t) {
	enum intr_level old_level;

	ASSERT (is_thread (t));

	old_level = intr_disable ();
	ASSERT (t->status == THREAD_BLOCKED);

	/* 쓰레드가 언블락되면 ready_list에 우선순위에 따라서 넣는 부분 */
	list_insert_ordered(&ready_list, &t->elem, priority_scheduling, NULL);
	//list_push_back (&ready_list, &t->elem);
	t->status = THREAD_READY;
	intr_set_level (old_level);
}
```

---

alarm-zero & alarm-negative

사실 기존에 만든 것으로도 통과되긴 했지만,  
코드의 효율을 위해 아래 줄을 추가해줬다.  
```c
timer_sleep (int64_t ticks) {
	
	int64_t start = timer_ticks ();

	if(ticks <= 0)  //이 부분 추가
		return;
}
```

priority-change 

문제:
1) 더 높은 우선순위의 쓰레드가 ready list에 들어오면 즉시 양보해야 한다.
2) 언제든 자신의 우선순위를 바꿀 수 있지만, 이로 인해 다른 쓰레드보다 우선순위가 낮아지게 되면 즉시 양보해야 한다. 

해결: 
두 개의 함수를 수정하여 해결하였다. 
```c
tid_t
thread_create (const char *name, int priority,
		thread_func *function, void *aux) {
	struct thread *t;
	tid_t tid;

	ASSERT (function != NULL);

	/* Allocate thread. */
	t = palloc_get_page (PAL_ZERO);
	if (t == NULL)
		return TID_ERROR;

	/* Initialize thread. */
	init_thread (t, name, priority);
	tid = t->tid = allocate_tid ();

	/* Call the kernel_thread if it scheduled.
	 * Note) rdi is 1st argument, and rsi is 2nd argument. */
	t->tf.rip = (uintptr_t) kernel_thread;
	t->tf.R.rdi = (uint64_t) function;
	t->tf.R.rsi = (uint64_t) aux;
	t->tf.ds = SEL_KDSEG;
	t->tf.es = SEL_KDSEG;
	t->tf.ss = SEL_KDSEG;
	t->tf.cs = SEL_KCSEG;
	t->tf.eflags = FLAG_IF;

	/* Add to run queue. */
	thread_unblock (t);

	if (thread_get_priority() < priority){ //이 부분 추가

		thread_yield();		
	}

	return tid;
}

void
thread_set_priority (int new_priority) {
	thread_current ()->priority = new_priority;
	// printf("%d\n", new_priority);	
	//새 priority가 더 낮은지 확인
	//Ready List에 더 높은 우선순위가 있다면 양보
	struct list_elem *max_elem = list_max(&ready_list, priority_scheduling, NULL);
	struct thread *next = list_entry(max_elem, struct thread, elem);
	if (thread_get_priority() < next->priority){

		thread_yield();	
	}
}

```
thread_create함수에 새로 생성된 쓰레드의 우선도가 현재 실행되는 쓰레드의 우선도보다 높으면 thread_yield함수를 통해 양보하도록 했다.

비슷하게, thread_set_priority함수에 현재 쓰레드(우선도를 바꾼 쓰레드)의 우선도가 ready list의 대기하는 가장 앞 쓰레드의 우선도보다 낮아지면 양보하도록 했다. 
(ready list의 가장 앞 쓰레드는 ready list의 모든 쓰레드 중 가장 높은 우선도를 갖게 정렬했다.)

---

priority-donate-one

문제: 
만약 현재 돌아가는 쓰레드의 우선도가 새로 생성된 쓰레드의 우선도보다 낮으면서,  
새로 생성된 쓰레드가 필요로 하는 Lock을 보유중이면 Deadlock상태가 발생한다.  

해결: 
해결을 위해서는, Lock을 보유한 기존의 쓰레드의 우선도를 새로 생성된 쓰레드의 우선도까지 올려주어야 한다. 이는 '기부'라는 방식으로 진행되는데 Lock을 필요로 하는 쓰레드의 우선도를 Lock을 보유한 쓰레드의 우선도에 맞춰주는 것이다.  
이후, Lock이 해제되면 기부받은 쓰레드의 우선도는 기존의 우선도로 원상복구된다.  

구조체:
```c
struct thread {
	/* Owned by thread.c. */
	tid_t tid;                          /* Thread identifier. */
	enum thread_status status;          /* Thread state. */
	char name[16];                      /* Name (for debugging purposes). */
	int priority;                       /* Priority. */
	int original_priority;				/* 원래의 우선도(priority)*/
	int64_t sleep_ticks; 				/* 자고 있는 시간*/
    bool has_lock;
}
```
다시 기존의 우선도로 돌릴 수 있도록 original_priority 변수를 구조체에 추가해 주었으며, 이는 쓰레드가 thread_create로 만들어지는 시점에 priority 값으로 맞춰진다. 쓰레드의 Lock 보유 상황을 관리할 수 있도록 bool has_lock 또한 추가해주었다. 

```c
tid_t
thread_create (const char *name, int priority,
		thread_func *function, void *aux) {
	struct thread *t;
	tid_t tid;

	ASSERT (function != NULL);

	/* Allocate thread. */
	t = palloc_get_page (PAL_ZERO);
	if (t == NULL)
		return TID_ERROR;

	/* Initialize thread. */
	init_thread (t, name, priority);
	tid = t->tid = allocate_tid ();

	/* Call the kernel_thread if it scheduled.
	 * Note) rdi is 1st argument, and rsi is 2nd argument. */
	t->tf.rip = (uintptr_t) kernel_thread;
	t->tf.R.rdi = (uint64_t) function;
	t->tf.R.rsi = (uint64_t) aux;
	t->tf.ds = SEL_KDSEG;
	t->tf.es = SEL_KDSEG;
	t->tf.ss = SEL_KDSEG;
	t->tf.cs = SEL_KCSEG;
	t->tf.eflags = FLAG_IF;

	/* Add to run queue. */
	thread_unblock (t);

	if (thread_get_priority() < priority){
        if(aux != NULL && thread_current()->has_lock){
            thread_current()->priority = priority;
        }
		thread_yield();		
	}

	return tid;
}

```
기존의 thread_create함수에 더 높은 우선순위가 들어오면 양보하는 부분이다.  
여기에 양보하기 전, Lock이 있고, 현재 실행되고 있는 낮은 우선도 쓰레드가 Lock을 보유했다면 우선도를 기부받는다.

이후, sema_up에서 현재 쓰레드의 priority가 기존의 original_priority와 다르면 `thread_current()->priority = thread_current()->original_priority;`를 통해  
우선도를 original priority로 낮춰주었다.  

---

priority-donate-multiple & priority-donate-multiple2

문제:
다수의 lock이 초기화되며 이는 모두 현재 실행되고 있는 main 쓰레드가 보유하고 있다. lock의 개수만큼 쓰레드가 생성되며, 이 때 생성되는 쓰레드는 모두 다른 lock을 필요로 한다. 

Lock a & b가 하나의 저우선도 쓰레드에 묶여있을 떄, a와 b를 필요로 하는 쓰레드 중 더 높은 우선도의 쓰레드의 우선도에 맞춰져 하나의 lock을 release하지만 우선도를 곧바로 원래의 original_priority우선도로 낮춰 두번쨰 lock은 release하지 못한다. 

과정: 
처음 고안해낸 해결법은 전역으로 선언한 lock_list에 모든 보유중인 lock을 넣고, 구조체 lock에 lock_elem과 lock_priority라는 변수를 만드는 것이었다.  
lock_acquire에서 lock->lock_priority를 해당 lock을 필요로 하는 모든 쓰레드들 중 가장 높은 우선도로 맞춰주고, 
lock_list에 우리가 원하는 lock이 있다면 그 lock의 holder 쓰레드의 우선도를 lock_priority로 올려주는 것이었다.  

여기에 기반해서 매 번 조금씩 사소한 변경들을 했지만 Kernel Panic과 Pintos Booting/Boot Complete메시지 누락 등의 오류에 부딛혔다. 


해결:
쓰레드 구조체에 기부자 명단 리스트 donors와 현재 필요한(하지만 다른 쓰레드갑 보유하고 있는) lock wait_on_lock을 추가했다. 

```c
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
}
```
우선, thread_create함수에서 lock정보를 aux를 사용해 받아오는 부분을 삭제했다. 
```c
	if (thread_get_priority() < priority){
		thread_yield();		
	}
```

이후, lock_acquire함수와 lock_release함수를 변경했다. 

```c
void
lock_acquire (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (!lock_held_by_current_thread (lock));

	struct thread *lock_holder;
	struct thread *now = thread_current();
	
	/* 추가 구현한 부분 */
	if (lock->holder != NULL)
	{
		lock_holder = lock->holder;
		now->wait_on_lock = lock;
		if (now->priority> lock_holder->priority)
		{
			list_insert_ordered(&lock_holder->donors, &now->donor_elem, lock_priority, NULL);
			now->wait_on_lock->holder->priority = now->priority;
		}
	}
	/*                   */

	sema_down (&lock->semaphore);
	thread_current()->wait_on_lock = NULL;
	lock->holder = thread_current ();
}
```
기존 함수에서 새로운 부분을 추가했는데, 이 부분에서 현재 쓰레드의 우선도와 lock holder의 우선도를 비교하여, 현재 쓰레드의 우선도가 적을 시 lock holder를 현재 쓰레드의 donors 명단에 삽입해주고, lock holder의 우선도를 현재 쓰레드(lock이 필요한 쓰레드)의 우선도로 올려주었다.  

```c
void
lock_release (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (lock_held_by_current_thread (lock));

	/* 추가 구현한 부분 */
	if (!list_empty(&lock->holder->donors)){

		struct list_elem *element;
		element = list_front(&lock->holder->donors);
		
		while(element != NULL){	
			struct thread *t = list_entry(element, struct thread, donor_elem);
			if (t->wait_on_lock == lock){
				list_remove(element);
				break;
			}

			element = element->next;
			if (element->next == NULL){
				break;
			}
		}
	} 
	
	if (!list_empty(&thread_current()->donors))
	{
		struct thread* foremost_thread = list_entry(list_front(&thread_current()->donors), struct thread, donor_elem);
		thread_current()->priority = foremost_thread->priority;
	}
	else {
		thread_current()->priority = thread_current()->original_priority;
	}
	/*                   */

	lock->holder = NULL;
	thread_current()->wait_on_lock = NULL;
	sema_up (&lock->semaphore);

}
```
lock_release에서는 release하려는 lock holder의 donors리스트를 확인 후 이를 탐색하며 현재 쓰레드가 필요로 하는 lock의 보유 쓰레드인지 찾고 이를 리스트에서 제거한다.  

이후 반복문을 탈출해 조건문에 따라 추가 기부가 있다면 그 중 가장 우선도가 높은 기부자의 우선도로 현재 쓰레드의 우선도를 조정하고, 없을 시 쓰레드의 원래의 우선도로 조정한다. 

마지막으로, wait_on_lock을 NULL로 변경해주면서 더 이상 기다리고 있는 lock이 없다는 것을 명확하게 해준다. 

---

priority-donate-nested & priority-donate-chain

문제: 
가장 높은 우선도의 쓰레드가 필요로 하는 lock의 보유자가 다른 lock을 기다리고 있을 때  
이를 순차적으로 release하여 deadlock 상태를 예방해야 한다. 

해결: 
```c
/* 재귀형태로 구현한 함수 for nested & chain */
void donate_recursion(struct thread *t){
	if(t->priority > t->wait_on_lock->holder->priority)
		t->wait_on_lock->holder->priority = t->priority;
	
	if(t->wait_on_lock->holder->wait_on_lock != NULL){
		donate_recursion(t->wait_on_lock->holder);
	}
}

void
lock_acquire (struct lock *lock){
    //
    //
	//
	//
   
   	if (lock->holder != NULL)
	{
		lock_holder = lock->holder;
		now->wait_on_lock = lock;
		/* check 2 */
		if (now->priority> lock_holder->priority)
		{
			list_insert_ordered(&lock_holder->donors, &now->donor_elem, lock_priority, NULL);
			donate_recursion(now);
		}
	}


}
```
해당 문제는 기존에 구현한 lock_acquire에서의 우선도의 조정 과정을 재귀함수로 해석하여 여러 비슷한 문제를 한 번에 해결하였다.  

donate_recursion 함수에서는 현재 쓰레드가 필요로 하는 lock이 다른 lock을 필요로 하면 연속하여 가장 높은 우선도의 쓰레드로 우선도를 조정하도록 한다. 

---

priority-donate-sema

문제:
가장 낮은 우선도의 쓰레드가 lock을 갖고 있고, 중간에 lock이 필요 없는 중간 우선도의 쓰레드가 생성, 이후 바로 lock을 필요로 하는 가장 높은 우선도의 쓰레드가 생성이 될 때 순차적으로 해결해야 한다. 

해결: 
기존에 구현했던 방식들이 유기적으로 작동하여 해결되었다.
더 높은 우선도의 쓰레드가 들어올 시 양보하는 구조, lock이 필요할 시 lock holder의 우선도를 높이는 것과 release이후 도로 낮추는 구조 등이 해당 문제의 해결에 작용하였다.  

---

priority-donate-lower

문제: 
lock holder 쓰레드가 기부를 받아 우선도가 올라간 상태에서 우선도가 thread_set_priority를 통해 설정되면, 기부가 사라진 상태에서 이 set된 우선도로 돌아가도록 해야한다.  

해결: 
현재 기부 받은 상태면 priority를 변경하지 않도록 조건문을 추가하였고, 함수를 통해 설정된 우선도를 저장하기 위해 기존에 thread 구조체에 만들었던 original_priority 변수를 설정해주었다.   

```c
void
thread_set_priority (int new_priority) {
	/* priority-lower */
	if (thread_current()->priority == thread_current()->original_priority){
		thread_current ()->priority = new_priority;
	} 

	thread_current()->original_priority = new_priority;

	//새 priority가 더 낮은지 확인
	//Ready List에 더 높은 우선순위가 있다면 양보
	struct list_elem *max_elem = list_max(&ready_list, priority_scheduling, NULL);
	struct thread *next = list_entry(max_elem, struct thread, elem);
	if (thread_get_priority() < next->priority){

		thread_yield();	
	}
}
```
---

priority-fifo & priority-preempt & priority-sema

문제: 
FIFO: 같은 우선순위일 때 들어온 순서로 해결되도록 해야한다. 
PREEMPT: 높은 우선도의 쓰레드가 실제로 선점하는지 확인해야한다. 
SEMA: sema->waiters리스트에서 높은 우선도의 순서로 나오도록 해야한다. 

해결: 
기존에 구현했던 개념들이 유기적으로 작용하여 위 세 문제는 이미 처리되었다. 

---

priority-condvar

문제:


해결:
synch.c 중간에 숨어있던 semaphore_elem이라는 구조체를 활용하고자 했다.  
여기에 우선도를 저장해줄 변수 sema_priority를 추가하여, 이를 사용해 policy에 따라 리스트에 정렬되도록 했다. 

```c
struct semaphore_elem {
	struct list_elem elem;              /* List element. */
	struct semaphore semaphore;         /* This semaphore. */
	int sema_priority;
};

static bool sema_elem_priority(const struct list_elem *a_, const struct list_elem *b_,
            void *aux UNUSED){
	struct semaphore_elem *a = list_entry (a_, struct semaphore_elem, elem);
	struct semaphore_elem *b = list_entry (b_, struct semaphore_elem, elem);

	return (a->sema_priority > b->sema_priority);
}

void
cond_wait (struct condition *cond, struct lock *lock) {
	struct semaphore_elem waiter;
	waiter.sema_priority = NULL;

	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	sema_init (&waiter.semaphore, 0);
	/* 추가 구현한 부분 */
	waiter.sema_priority = thread_current()->priority;
	list_insert_ordered (&cond->waiters, &waiter.elem, sema_elem_priority, NULL);
	/*                 */
	lock_release (lock);
	sema_down (&waiter.semaphore);
	lock_acquire (lock);

}


```
기존에 있던 정렬 정책들과 비슷하지만, elem이 속해있는 원 구조체가 semaphore_elem이기 때문에 이 형태로 찾아주고, 앞서 만든 sema_priority를 비교하여 cond->waiters리스트에 정렬된 상태로 삽입했다. 

---

mlfqs-load-1

문제: 
mlfqs에 대한 함수들의 프레임만 존재해서 함수들을 구현해야 했고, 
부동소수점을 사용할 수 없기에, 실수연산을 모두 고정소수점을 사용해서 해결해 주어야 했다.


해결:
부동소수점이 들어가는 연산을 고정소수점으로 바꾸는 #define 매크로를 사용해서 해결해주었다.
시스템이 얼마나 바쁜지 알려주는 thread_get_load_avg() 과 update_load_avg() 함수를 구현해주었다.

```c
#define P 17
#define Q 14
#define F (1 << (Q))

#define FIXED_POINT(x) (x) * (F)
#define REAL_NUMBER(x) (x) / (F)

#define ROUND_TO_INT(x) (x >= 0 ? ((x + F / 2) /F) : ((x - F / 2 ) /F))

//고정소수점 기본 연산
#define ADD_FIXED(x,y) (x) + (y)
#define SUB_FIXED(x,y) (x) - (y)
#define MUL_FIXED(x,y) ((int64_t)(x)) * (y) / (F)
#define DIV_FIXED(x,y) ((int64_t)(x)) * (F) / (y)

#define ADD_INT(x, n) (x) + (n) * (F)
#define SUB_INT(x, n) (x) - (n) * (F)
#define MUL_INT(x, n) (x) * (n)
#define DIV_INT(x, n) (x) / (n)

void 
update_load_avg(){
	ASSERT(thread_mlfqs == true)
	int ready = list_size(&ready_list);;
	struct thread* t = thread_current();

	if (t != idle_thread)
		ready ++;

	load_avg = ADD_FIXED(MUL_FIXED(DIV_INT(FIXED_POINT(59), 60), load_avg),MUL_INT(DIV_INT(F,60),ready));
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) {
	ASSERT(thread_mlfqs == true)
	
	int return_load_avg = ROUND_TO_INT(MUL_INT((load_avg), 100));
	
	return return_load_avg;
}

```

---

mlfqs-load-60 & mlfqs-load-avg

문제:
여러개의 쓰레드가 생성되고, nice값과 recent_cpu 값을 통해서 
쓰레드들의 load-avg값이 바뀌어야 했다.

사건?:
mlfqs-load-60의 경우 큰 문제 없이 해결이 되었는데, 
mlfqs-load-avg의 경우 5시간 이상의 많은 시간을 잡아먹었다.
문제를 못찾겠어서 여러 코드를 다 뜯어보다가, 
결국 #define 매크로에서 문제가 생겼다는 것을 찾아낼 수 있었다.

해결:
전역변수 all_list와 쓰레드 구조체에 nice_vale와 recent_cpu, all_elem을 만들어주고,
nice 갱신 함수를 구현하여 해결하였다.
```c
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
	/* 추가 구현한 부분 */
	int nice_value;
	int recent_cpu;
	struct list_elem all_elem;

void
thread_set_nice (int nice) {
	ASSERT(thread_mlfqs == true)

	thread_current()->nice_value = nice;
	int new_priority = calculate_advanced_priority(thread_current());

	thread_set_priority(new_priority);

}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) {
	ASSERT(thread_mlfqs == true)

	return thread_current()->nice_value;
}


int
calculating_recent_cpu(struct thread* t){
	
	ASSERT(thread_mlfqs == true);

	int recent = t->recent_cpu;
	recent = ADD_INT(MUL_FIXED(DIV_FIXED(MUL_INT(load_avg, 2), ADD_INT(MUL_INT(load_avg, 2),1)),recent),t->nice_value);

	t->recent_cpu = recent;

	return recent;
}

void calc_all_recent_cpu(){
	if (list_empty(&all_list))
		return;
	
	struct list_elem*e = list_front(&all_list);
	struct thread *t;
	for (e = list_begin (&all_list); e != list_end (&all_list); e = list_next (e)){
		t = list_entry(e, struct thread, all_elem);
		t->recent_cpu = calculating_recent_cpu(t);
	}	
}
```


