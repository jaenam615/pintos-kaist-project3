Krafton Jungle 
Week 7 Team 6 WIL

Project 2 User Programs:

---

Argument Passing

---

시스템 콜을 만들기에 앞서 Argument Passing을 구현해야 했다. 
Argument Passing이란, main함수로 들어오는 명령어를 구분하여 인자로 전달하는 것이다.  
우선, init.c main()함수의 read_command_line()함수가 커맨드라인으로 들어오는 인자를 받아와 저장한다.  
(예: -q -f put args-single run 'args-single onearg')
이후 일차적으로 파싱을 통해 -q와 -f등의 플래그들을 제거한 후 남은 부분을 run_actions로 전달해준다.  
run_actions -> run taks 등을 거쳐 args-single onearg를 process_create_initd로 전달한다.  
process_create_initd에서는 미리 fn_copy를 만들어 전체 인자(args-single onearg)를 복사해두고 파일의 이름을 구하기 위해 strktok_r 함수로 파싱한다. 
thread_create로 file_name을 전달해 쓰레드를 만들고, process_exec에는 fn_copy를 전달하여 프로세스(쓰레드)를 실행한다. 

Load함수에서 다시 한 번 strtok_r함수로 들어온 인자를 delimiter(여기의 경우에서는 스페이스)를 기준으로 파싱하여 배열(스택)에 저장한다.  
argument_stack이라는 함수를 만들어 배열에 깃북에서 설명한 방식으로 인자를 쌓는다. 

이 모든 과정에서 중간중간 hex_dump를 통해 어느 시점에 argv가 어떤 형태로 전달되는지 확인하는 것이 흐름 파악에 도움이 되었다.   

---
| Address	 |  Name	      |  Data	   | Type       |  
|---|---|---|---|
|0x4747fffc	 |  argv[3][...]  | 'bar\0'	   |char[4]     |     
|0x4747fff8	 |   argv[2][...] | 'foo\0'	   |char[4]     |     
|0x4747fff5	 |   argv[1][...] | '-l\0'	   |char[3]     |      
|0x4747ffed	 |   argv[0][..|.]|	'/bin/ls\0'|char[8]     |    
|0x4747ffe8	 |   word-align	  | 0	       |uint8_t[]   |      
|0x4747ffe0	 |   argv[4]	  | 0	       |char *      |     
|0x4747ffd8	 |   argv[3]	  | 0x4747fffc |char *      |      
|0x4747ffd0	 |   argv[2]	  | 0x4747fff8 |char *      |    
|0x4747ffc8	 |   argv[1]	  | 0x4747fff5 |char *      |     
|0x4747ffc0	 |   argv[0]	  | 0x4747ffed |char *      |     
|0x4747ffb8	 |  return address|	0	       |void (*) () |     
---

```c
void argument_stack (char **argv, int argc, struct intr_frame *if_){
	
    //포인터를 이동시킬 단위
	int minus_addr;

    //포인터
	int address = if_->rsp;
	
    //인자를 쌓는 반복문 (이 때, 스택은 위에서 아래로 자라기 떄문에 i--로 이동시켜준다)
    for (int i = argc-1; i >= 0;i-- ){
		minus_addr = strlen(argv[i]) + 1; 
		address -= minus_addr;
		memcpy(address, argv[i], minus_addr);
		argv[i] = (char *)address;
	}

    //패딩(word-align)을 통해 8의 배수로 맞춰준다 
    //정렬된 접근은 정렬 안된 접근보다 빠르기 때문이다
	if (address % 8){
		int word_align = address % 8;
		address -= word_align;
		memset(address, 0, word_align);
	}

    //주소의 끝을 알려주는 부분 (위 표에서는 argv[4] / 0 부분에 해당)
	address -= 8;
	memset(address, 0, sizeof(char*));

    //인자의 개수만큼 포인터를 앞으로 당긴 후 모든 주소를 한 번에 넣어준다
	address -= (sizeof(char*) * argc);
	memcpy(address, argv, sizeof(char*) * argc);

    //리턴포인트
	address -= 8;
	memset(address, 0, 8);
	if_->rsp = address;
}
```

---

User Memory Access

---

시스템콜을 구현하기 전에 가상 주소 공간의 데이터를 읽고 쓸 수 있는 방법을 제공해야 한다. 이는 인자를 전달받는 시점에는 필요 없지만, 시스템콜의 인자로 제공받는 포인터로부터 데이터를 읽으려면 몇가지 예외상황 처리가 필요하다.  

- 사용자가 유효하지 않은 포인터를 제공  
- 커널 메모리 영역으로의 포인터를 제공  
- 블록의 일부가 위의 두 영역에 걸쳐있음  

위의 조건들을 항시 검사하여 유효한 주소가 제공되었는지 확인해야한다.  

```c
.
.
.
	if(!is_user_vaddr(f->rsp)){
		printf("isnotvaddr\n");
		thread_exit();
	}

	else if(f->rsp > KERN_BASE || f->rsp < 0){
		printf("smaller\n");
		thread_exit();
	}
	
	int addr = (f->rsp + 8);
	if (!is_user_vaddr(addr) || (addr > KERN_BASE || addr<0)) {
		printf ("third condition\n");
		thread_exit();
	}
.
.
.

syscall_function(){
	if(pml4_get_page(thread_current()->pml4, buffer) == NULL || buffer == NULL || !is_user_vaddr(buffer) || fd < 0)
		exit(-1);
}
```

위와 같이 syscall_handler에 우선 조건을 넣어주고, 일부 시스템콜에 아래의 syscall_function에 들어간 조건문을 넣어줌으로 유효하지 않은 주소가 들어오는 경우를 처리하도록 했다. 

---

<b>시스템 콜</b>

---

시스템 콜 넘버는 %rax로 들어온다. 
인자들은 %rdi %rsi %rdx %r10 %r8 %r8 순서로 들어오며, 이는 전달된 intr_frame을 통해 접근 가능하다.  

`lib/user/syscall.c`에서 확인할 수 있다. 

```c
__attribute__((always_inline))
static __inline int64_t syscall (uint64_t num_, uint64_t a1_, uint64_t a2_,
		uint64_t a3_, uint64_t a4_, uint64_t a5_, uint64_t a6_) {
	int64_t ret;
	register uint64_t *num asm ("rax") = (uint64_t *) num_;
	register uint64_t *a1 asm ("rdi") = (uint64_t *) a1_;
	register uint64_t *a2 asm ("rsi") = (uint64_t *) a2_;
	register uint64_t *a3 asm ("rdx") = (uint64_t *) a3_;
	register uint64_t *a4 asm ("r10") = (uint64_t *) a4_;
	register uint64_t *a5 asm ("r8") = (uint64_t *) a5_;
	register uint64_t *a6 asm ("r9") = (uint64_t *) a6_;

```
---

`void halt(void)`

---
power_off()를 호출함으로 핀토스를 종료시킨다. (`src/include/threads/init.h`에서 확인 가능하다)

---

`void exit(int status)`

---
현재 실행되고 있는 프로세스를 종료시키며, status를 커널로 반환한다. 

> <b>Process Termination Message</b>
> 핀토스에서 프로세스 종료시 아래 메시지를 출력해야 한다. 
> `printf("%s: exit(%d)\n", ...)`
{: .prompt-info}


`process_exit`에서는 위 출력 문구를 출력해주고, fd_table을 탐색하며 fd들을 모두 하나씩 닫아주고 메모리를 반환해준다. 

---

`bool create(const char *file, unsigned initial_size)`

---
`file`이라는 파일을 `initial_size`의 크기로 생성한다.  
`filesys/filesys.c`에 있는 `filesys_create()`함수를 이용해서 생성해준다.  
한 번에 생성은 한 파일만 될 수 있도록 `filesys_create`를 `lock_acquire`과 `lock_release`로 감싸주었다.  
(`filesys_create()`함수는 별도의 수정이 필요 없었다)

```c

	lock_acquire(&filesys_lock);
	bool success = filesys_create(file, initial_size);
	lock_release(&filesys_lock);
	return success;

```

---

`bool_remove(const char *file)`

---
`file`이라는 파일을 삭제한다. 열려있는지에 대한 여부와 별개로 삭제가 가능하며, 열려있는 파일을 삭제한다고 하더라도 이는 닫히지 않는다.  

---

`int open(const char *file)`

---

`file`이라는 파일을 열고, '파일 디스크립터'라는 정수형 핸들을 반환한다. (실패 시 -1을 반환한다.)

모든 프로세스는 별도의 파일 디스크립터 테이블이 있으며, 자식 프로세스가 생성이 된다면 부모 프로세스의 파일 디스크립터 테이블 역시 상속받는다. 

```c
int open (const char *file) 
{
	if(pml4_get_page(thread_current()->pml4, file) == NULL || file == NULL || !is_user_vaddr(file)) 
		exit(-1);
	lock_acquire(&filesys_lock);
	struct file *open_file = filesys_open(file);
	int fd = -1;
	if(open_file == NULL){
		lock_release(&filesys_lock);
		return fd;
	}
	// fd = allocate_fd(open_file, &thread_current()->fd_table);
	fd = process_add_file(open_file);
	if (fd == -1)
		file_close(open_file);
	lock_release(&filesys_lock);
	return fd;
}

int process_add_file(struct file *f)
{
	struct thread *curr = thread_current();
	struct file_descriptor *new_fd = malloc(sizeof(struct file_descriptor));

	curr->last_created_fd += 1;
	new_fd->fd = curr->last_created_fd;
	new_fd->file = f;
	list_push_back(&curr->fd_table, &new_fd->fd_elem);

	return new_fd->fd;
}
```
create랑 remove할때와 동시성 문제를 해결하기 위해 `filesys_open` 전후로 lock을 acquire및 release 해주었다.
`process_add_file`함수는 `filesys_open`을 통해 열린 파일에 대해 파일 디스크립터를 부여하고 파일 디스크립터 테이블에 해당 파일을 넣어준다. 

---

`void close(int fd)`

---
이제부터 인자에 파일 디스크립터를 받기 때문에, 이 시점 전에 open이 구현되어 있는 것이 중요하다. 

`file_close`함수를 통해 파일 디스크립터 fd에 해당하는 파일을 닫는다.  
프로세스를 종료하면 해당 프로세스에 열려있는 모든 파일 디스크립터를 닫아준다. 

```c
void close (int fd) {

	struct thread *curr = thread_current();
	struct list_elem *start;
	for (start = list_begin(&curr->fd_table); start != list_end(&curr->fd_table); start = list_next(start))
	{
		struct file_descriptor *close_fd = list_entry(start, struct file_descriptor, fd_elem);
		if (close_fd->fd == fd)
		{
			file_close(close_fd->file);
			list_remove(&close_fd->fd_elem);
		}
	}
	return;
}
```
---

`int read (int fd, void *buffer, unsigned size)`

---

열려있는 파일 fd로부터 size만큼의 바이트를 읽어온다(buffer로 넣어준다). 
커맨드라인을 통해 전달된 텍스트(인자)를 `input_getc`을 통해 읽어오고,  
반복문을 통해 일치하는 fd를 찾아 해당 fd로부터 전달된 텍스트(인자)를 buffer에 넣어준다.  

```c
int read (int fd, void *buffer, unsigned size)
{
	if(pml4_get_page(thread_current()->pml4, buffer) == NULL || buffer == NULL || !is_user_vaddr(buffer) || fd < 0)
		exit(-1);

	struct thread *curr = thread_current();
	struct list_elem *start;
	off_t buff_size;

	if (fd == 0)
	{
		return input_getc();
	}
	else if (fd < 0 || fd == NULL || fd == 1)
	{
		exit(-1);
	}
	else
	{
		for (start = list_begin(&curr->fd_table); start != list_end(&curr->fd_table); start = list_next(start))
		{
			struct file_descriptor *read_fd = list_entry(start, struct file_descriptor, fd_elem);
            //탐색 중 fd가 일치하는지 확인
			if (read_fd->fd == fd)
			{
				lock_acquire(&filesys_lock);
                //텍스트를 읽고 버퍼에 넣어준다. 
				buff_size = file_read(read_fd->file, buffer, size);
				lock_release(&filesys_lock);
			}
		}
	}
	return buff_size;
}
```
---

`int write(int fd, const void *buffer, unsigned size)`

---

read와 비슷하지만, 데이터의 이동이 반대 방향이다.  
버퍼에 있는 데이터를 size바이트만큼 열려있는 파일 fd로 기록한다. 

```c
int write (int fd, const void *buffer, unsigned size)
{
	if(pml4_get_page(thread_current()->pml4, buffer) == NULL || buffer == NULL || !is_user_vaddr(buffer) || fd < 0)
		exit(-1);

	struct thread *curr = thread_current();
	struct list_elem *start;
	if (fd == 1)
	{
		putbuf(buffer, size);
		return size;
	}
	else if (fd < 0 || fd == NULL)
	{
		exit(-1);
	}
	for (start = list_begin(&curr->fd_table); start != list_end(&curr->fd_table); start = list_next(start))
	{
		struct file_descriptor *write_fd = list_entry(start, struct file_descriptor, fd_elem);
		if (write_fd->fd == fd)
		{
			lock_acquire(&filesys_lock);
			off_t write_size = file_write(write_fd->file, buffer, size);
			lock_release(&filesys_lock);
			return write_size;
		}
	}
}
```
---

`int filesize(int fd)`

---

현재 열려있는 파일 `fd`의 크기를 반환해주는 함수이다.  
기존에 있던 `file_length`함수에 파일을 인자로 전달함으로 반환받는다.  

---

`void seek(int fd, unsigned position)`

---

열려있는 파일 `fd`에서 다음에 읽고 쓸 바이트 위치(포인터)를 `position`으로 변경해주는 함수이다. 
이미 구현되어 있는 `file_seek`함수를 사용했다. 

---

`unsigned tell(int fd)`

---

열린 파일 `fd`에서 다음에 읽고 쓸 바이트 위치(포인터)를 반환하는 함수이다. 

이미 구현되어 있는 `file_tell`함수를 사용했다. 

---

`tid_t fork(const char *thread_name, struct intr_frame *f)`

---

현재 프로세스의 복제본을 생성하는 시스템 콜로, 레지스터 값들의 일부를 복사해온다. 
자식 프로세스는 파일 디스크립터와 가상 메모리 공간 등의 자원을 복제해야 한다.  

- 자식 프로세스의 PID를 반환함
- 자식 프로세스에서는 0을 반환함

스켈레톤으로 존재하는 `process_fork`함수를 완성해야 한다. 

우선 `parent_info`라는 구조체를 만들어 부모 프로세스의 정보를 저장한다.  
이 구조체에는 부모 쓰레드와 부모 쓰레드의 레지스터 값들이 저장된다.  

```c
struct parent_info
{
	struct thread *parent;
	struct intr_frame *parent_f;
};
```

`process_fork`안에 부모 쓰레드의 정보를 저장할 구조체를 선언하고 값을 넣어준다.  
이후, 인자로 전달받은 레지스터 값을 자식이 보관하는 부모의 레지스터 값에 넣어준다. 
이는 이후 __do_fork에서 부모의 값을 복제하는 과정에서 사용될 예정이다.  

`get_thread_from_tid`함수는 tid를 인자로 받아 해당하는 쓰레드를 반환하는 함수이다.
`list.h`안에 있는 주석 중 아래 내용을 확인하여 elem으로부터 속해있는 구조체를 반환받는 형식의 함수를 구현했다. 특정 리스트를 탐색하여 일치하는 tid가 있는 쓰레드를 반환한다. 

이를 위해 thread 구조체 안에 자식 프로세스를 보관하는 child_list와 이에 들어가는 child_list_elem을 추가해주었다. 

thread 구조체에는 세마포어 또한 추가해주었는데, `process_sema`는 자식 프로세스에 대한 복사가 완료될 때에만 `sema_up`이 호출되게 하여 함수가 끝나지 않도록 설계했다.  


```c
 /* Iteration is a typical situation where it is necessary to
 * convert from a struct list_elem back to its enclosing
 * structure.  Here's an example using foo_list:

 * struct list_elem *e;

 * for (e = list_begin (&foo_list); e != list_end (&foo_list);
 * e = list_next (e)) {
 *   struct foo *f = list_entry (e, struct foo, elem);
 *   ...do something with f...
 * } */

struct thread{
.
.
.

	struct list child_list;
	struct list_elem child_list_elem;

    /* 자식 프로세스의 fork가 완료될 때까지 기다리도록 하기 위한 세마포어 */
    struct semaphore process_sema;
.
.
};
 ```

위의 과정을 따라 `get_thread_from_tid`를 구현하면 이를 `process_fork`에서 호출해 tid에 해당하는 쓰레드를 구하고 `sema_down`을 하여 대기상태로 진입, 자식 프로세스가 `__do_fork`를 실행한다. 

```c
tid_t
process_fork (const char *name, struct intr_frame *if_) {
	/* Clone current thread to new thread.*/
	struct parent_info my_data;
	my_data.parent = thread_current();
	my_data.parent_f = if_;

	struct thread *cur = thread_current();
	memcpy(&cur->parent_tf, my_data.parent_f, sizeof(struct intr_frame));

	tid_t tid = thread_create(name, PRI_DEFAULT, __do_fork, &my_data);
	if (tid == TID_ERROR){
		return TID_ERROR;
	}

	struct thread *child = get_thread_from_tid(tid);
	sema_down(&child->process_sema);
	if(child->exit_status == TID_ERROR)
	{
		sema_up(&child->exit_sema);
		
		return TID_ERROR;
	}

	return tid;
}

struct thread *get_thread_from_tid(tid_t thread_id){

	struct thread * t = thread_current();
	struct list* child_list = &t->child_list;
	struct list_elem* e;

	for (e = list_begin (child_list); e != list_end (child_list); e = list_next (e))
	{
		t = list_entry(e, struct thread, child_list_elem);
		if (t->tid == thread_id){
			return t;
		}
	}	
	return NULL; 
}

```

`__do_fork`에는 부모의 정보인 parent_info 구조체 `my_data`가 전달되고, 이를 사용해 자식 프로세스는 부모 프로세스의 레지스터 상태를 복사받는다.  
이후 반복문을 돌며 부모 프로세스의 `fd_table`내 파일들을 복제한다. 

모든 복제가 완료된 후, `sema_up`을 통해 부모 프로세스의 대기를 해제시킨다. 

```c
static void
__do_fork (struct parent_info *aux) {
	struct intr_frame if_;
	struct thread *parent = aux->parent;
	struct thread *current = thread_current ();

    //부모 프로세스의 레지스터값 복사
	struct intr_frame *parent_if = aux->parent_f;

	bool succ = true;
    //부모 프로세스의 레지스터값 복사
    memcpy(&if_, parent_if, sizeof(struct intr_frame));
    if_.R.rax = 0; // 자식 프로세스의 리턴값은 0

    current->pml4 = pml4_create();
    if (current->pml4 == NULL)
        goto error;
.
.
.
    //부모 프로세스의 파일 디스크립터 테이블 정보 복제
    struct list_elem* e = list_begin(&parent->fd_table);
        struct list *parent_list = &parent->fd_table;
        if(!list_empty(parent_list)){
            for (e ; e != list_end(parent_list) ; e = list_next(e)){
                struct file_descriptor* parent_fd =list_entry(e,struct file_descriptor, fd_elem);
                if(parent_fd->file != NULL){
                    struct file_descriptor *child_fd = malloc(sizeof(struct file_descriptor));
                    child_fd->file = file_duplicate(parent_fd->file);
                    child_fd->fd = parent_fd->fd;
                    list_push_back(&current->fd_table, & child_fd->fd_elem);
                }
                current->last_created_fd = parent->last_created_fd;
            }
            current->last_created_fd = parent->last_created_fd;
        } else {
            current->last_created_fd = parent->last_created_fd;
        }

    // 로드가 완료될 때까지 기다리고 있던 부모 대기 해제
    sema_up(&current->process_sema);
```

---

`int exec(const char *file)`

---

현재 프로세스를 주어진 이름 `file`을 갖는 실행 파일로 변경시켜주며, 주어진 모든 인자 또한 패싱한다.  
`exec`호출 시에는 열려있던 파일 디스크립터들은 열린 상태로 유지된다. 

파일을 커널로 복사해 `process_exec`을 사용해 실행한다. 

```c
int exec (const char *file){
	
	if(pml4_get_page(thread_current()->pml4, file) == NULL || file == NULL || !is_user_vaddr(file)) 
		exit(-1);

	char* file_in_kernel;
	file_in_kernel = palloc_get_page(PAL_ZERO);

	if (file_in_kernel == NULL)
		exit(-1);
	strlcpy(file_in_kernel, file, PGSIZE);
	
	if (process_exec(file_in_kernel) == -1)
		return -1;	
}
```

thread 구조체에 현재 실행되고 있는 파일을 표시하는 running을 넣어주었고, exit에서 현재 쓰레드에서 실행되고 있는 파일을 특정할 수 있도록 `t->running = file;`을 넣어주었다.  

```c
struct thread{
.
.
	struct file *running;
.
.
};
```

---

`int wait(tid_t t)`

---

자식 프로세스 t를 기다리며 자식 프로세스의 `exit_status`를 반환한다. 
이를 위해 thread 구조체에 `exit_status`를 추가해주었다.  




