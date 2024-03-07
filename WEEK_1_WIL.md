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




priority-sema

문제:
세마포어를 0으로 초기 설정한 후 다수의 쓰레드를 생성하는데, 이 때 이 쓰레드들은 세마포어의 대기 리스트인 sema->waiters에서 block된 상태로 세마포어의 값이 올라갈 때 까지 대기한다. 깨워질 때 우선도에 따라 풀려 ready list에 삽입되어야 한다.  

해결: 


