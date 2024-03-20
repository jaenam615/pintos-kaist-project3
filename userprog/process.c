#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/fsutil.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#include "lib/string.h"
#ifdef VM
#include "vm/vm.h"
#endif

#include "userprog/syscall.h"


static void process_cleanup (void);
static bool load (const char *file_name, struct intr_frame *if_);
static void initd (void *f_name);

void argument_stack(char** argv, int argc, struct intr_frame *if_);
struct thread *get_thread_from_tid(tid_t thread_id);

struct fork_data
{
	struct thread *parent;
	struct intr_frame *user_level_f;
};
static void __do_fork (struct fork_data *aux);
// //구현
// static char parse_options (char **argv);

/* General process initializer for initd and other process. 
	initd 및 기타 프로세스에 대한 일반 프로세스 초기화*/
static void
process_init (void) {
	struct thread *current = thread_current ();
}

/* Starts the first userland program, called "initd", loaded from FILE_NAME.
 * The new thread may be scheduled (and may even exit)
 * before process_create_initd() returns. Returns the initd's
 * thread id, or TID_ERROR if the thread cannot be created.
 * Notice that THIS SHOULD BE CALLED ONCE. 
 
 * ELF 바이너리를 로드하고 프로세스를 시작합니다.
 * FILE_NAME에서 로드된 "initd"라는 첫 번째 사용자 랜드 프로그램을 시작합니다.
 * 새 스레드가 예약될 수 있으며 종료될 수도 있습니다
 * process_create_initd()가 반환되기 전에 initd를 반환합니다
 * 스레드 ID 또는 스레드를 만들 수 없는 경우 TID_ERROR.
 * 한 번 호출해야 합니다
 * */

void argument_stack (char **argv, int argc, struct intr_frame *if_){
	
	int minus_addr;
	int address = if_->rsp;
	for (int i = argc-1; i >= 0;i-- ){
		minus_addr = strlen(argv[i]) + 1; //if onearg, value = 7 
		address -= minus_addr;
		memcpy(address, argv[i], minus_addr);
		argv[i] = (char *)address;
	}

	if (address % 8){
		int word_align = address % 8;
		address -= word_align;
		memset(address, 0, word_align);
	}

	address -= 8;
	memset(address, 0, sizeof(char*));

	// address -= 8*argc;
	// memcpy(address, &argv, 8*argc);

	address -= (sizeof(char*) * argc);
	memcpy(address, argv, sizeof(char*) * argc);


	address -= 8;
	memset(address, 0, 8);
	if_->rsp = address;
}


tid_t
process_create_initd (const char *file_name) {
	char *fn_copy;
	tid_t tid;

	/* Make a copy of FILE_NAME.
	 * Otherwise there's a race between the caller and load(). 
	 * FILE_NAME의 복사본을 만듭니다.
	 * 그렇지 않으면 발신자와 load() 사이에 경합이 발생합니다.
	 */
	fn_copy = palloc_get_page (0);
	if (fn_copy == NULL)
		return TID_ERROR;
	strlcpy (fn_copy, file_name, PGSIZE);

	// // 스페이스 전 첫 부분을 실행하고자 하는 파일의 이름으로 지정
	// // argument passing
	char *save_ptr;
	strtok_r (file_name, " ", &save_ptr);

	/* Create a new thread to execute FILE_NAME. */
	tid = thread_create (file_name, PRI_DEFAULT, initd, fn_copy);

	if (tid == TID_ERROR)
		palloc_free_page (fn_copy);
	return 	tid;
}


/* A thread function that launches first user process. */
static void
initd (void *f_name) {
#ifdef VM
	supplemental_page_table_init (&thread_current ()->spt);
#endif
	process_init ();
	if (process_exec (f_name) < 0)
		PANIC("Fail to launch initd\n");
	NOT_REACHED ();
}

/* Clones the current process as `name`. Returns the new process's thread id, or
 * TID_ERROR if the thread cannot be created. 
 * 현재 프로세스를 'name'으로 복제합니다. 새 프로세스의 스레드 ID를 반환하거나
 * 스레드를 만들 수 없는 경우 TID_ERROR입니다.
 */
tid_t
process_fork (const char *name, struct intr_frame *if_) {
	/* Clone current thread to new thread.*/
	// thread_current()->tf = if_;
	struct fork_data my_data;
	my_data.parent = thread_current();
	my_data.user_level_f = if_;

	struct thread *cur = thread_current();
	memcpy(&cur->parent_tf, if_, sizeof(struct intr_frame));

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
	// return thread_create (name, PRI_DEFAULT, __do_fork, thread_current ());
}

#ifndef VM
/* Duplicate the parent's address space by passing this function to the
 * pml4_for_each. This is only for the project 2. 
 * 이 함수를 다음에 전달하여 부모 주소 공간을 복제합니다
 * pml4_for_each. 이것은 오직 프로젝트 2만을 위한 것입니다.
 */
static bool
duplicate_pte (uint64_t *pte, void *va, void *aux) {
	struct thread *current = thread_current ();
	struct thread *parent = (struct thread *) aux;
	void *parent_page;
	void *newpage;
	bool writable;

	/* 1. TODO: If the parent_page is kernel page, then return immediately. */
	if (is_kernel_vaddr(va)){
		return true;
	}

	/* 2. Resolve VA from the parent's page map level 4. */
	parent_page = pml4_get_page (parent->pml4, va);
	if (parent_page == NULL){
		return false;
	}

	/* 3. TODO: Allocate new PAL_USER page for the child and set result to
	 *    TODO: NEWPAGE. */
	newpage = palloc_get_page(PAL_USER);
	if (newpage == NULL){
		return false;
	}
	// newpage = pte;
	/* 4. TODO: Duplicate parent's page to the new page and
	 *    TODO: check whether parent's page is writable or not (set WRITABLE
	 *    TODO: according to the result). */
	memcpy(newpage, parent_page, PGSIZE);
	writable = is_writable(pte);

	/* 5. Add new page to child's page table at address VA with WRITABLE
	 *    permission. 
	 * 5. 쓰기 가능한 권한으로 주소 VA의 어린이 페이지 테이블에 새 페이지를 추가합니다.
	 */
	if (!pml4_set_page (current->pml4, va, newpage, writable)) {
		/* 6. TODO: if fail to insert page, do error handling. 
		 * 6. 작업: 페이지를 삽입하지 못할 경우 오류 처리를 수행합니다.
		 */
		return false;

	}
	return true;
}
#endif

/* A thread function that copies parent's execution context.
 * Hint) parent->tf does not hold the userland context of the process.
 *       That is, you are required to pass second argument of process_fork to
 *       this function. 
 * 부모의 실행 컨텍스트를 복사하는 스레드 기능입니다.
 * 힌트) parent->tf는 프로세스의 사용자 및 컨텍스트를 유지하지 않습니다.
 * 즉, process_fork의 두 번째 인수를 이 함수에 전달해야 합니다.
 */
static void
__do_fork (struct fork_data *aux) {
	struct intr_frame if_;
	struct thread *parent = aux->parent;
	struct thread *current = thread_current ();
	/* TODO: somehow pass the parent_if. (i.e. process_fork()'s if_) 
	 * 어떻게든 parent_if를 전달해라.
	 */
	
	struct intr_frame *parent_if = aux->user_level_f;

	bool succ = true;

    /* 1. Read the cpu context to local stack. */
    memcpy(&if_, parent_if, sizeof(struct intr_frame));
    if_.R.rax = 0; // 자식 프로세스의 리턴값은 0

    /* 2. Duplicate PT */
    current->pml4 = pml4_create();
    if (current->pml4 == NULL)
        goto error;

    process_activate(current);
#ifdef VM
    supplemental_page_table_init(&current->spt);
    if (!supplemental_page_table_copy(&current->spt, &parent->spt))
        goto error;
#else
    if (!pml4_for_each(parent->pml4, duplicate_pte, parent))
        goto error;
#endif

    /* TODO: Your code goes here.
     * TODO: Hint) To duplicate the file object, use `file_duplicate`
     * TODO:       in include/filesys/file.h. Note that parent should not return
     * TODO:       from the fork() until this function successfully duplicates
     * TODO:       the resources of parent.*/
	// struct list_elem* e = list_begin(&parent->fd_table);
	// 	for(int i = 0; i< list_size(&parent->fd_table); ++i)
	// 	{
	// 		struct file_descriptor* file_desc =list_entry(e,struct file_descriptor, fd_elem);
	// 		struct file_descriptor* tmp_file_desc;
	// 		tmp_file_desc->fd = file_desc->fd;
	// 		tmp_file_desc->file = file_duplicate(file_desc->file);
	// 		list_push_back(&tmp_file_desc->fd_elem,&current->fd_table);
			
	// 	}
	// current->last_created_fd = parent->last_created_fd;

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

	if_.R.rax = 0;

    // 로드가 완료될 때까지 기다리고 있던 부모 대기 해제
    sema_up(&current->process_sema);
    process_init();

    /* Finally, switch to the newly created process. */
    if (succ)
        do_iret(&if_);
error:
    // sema_up(&current->process_sema);
    exit(TID_ERROR);
}

/* Switch the current execution context to the f_name.
 * Returns -1 on fail. 
 * 현재 실행 컨텍스트를 f_name으로 전환합니다.
 * 실패 시 -1을 반환합니다.
 */
 

int
process_exec (void *f_name) {

	char *file_name = f_name;
	bool success;
	
	/* We cannot use the intr_frame in the thread structure.
	 * This is because when current thread rescheduled,
	 * it stores the execution information to the member. 
	 * 스레드 구조에서 intr_frame을 사용할 수 없습니다.
	 * 현재 스레드가 다시 예약되면 실행 정보를 회원에게 저장하기 때문입니다.
	 */
	struct intr_frame _if;
	_if.ds = _if.es = _if.ss = SEL_UDSEG;
	_if.cs = SEL_UCSEG;
	_if.eflags = FLAG_IF | FLAG_MBS;
	
	/* We first kill the current context */
	process_cleanup ();

	// char *stk[64];
   	// char *token, *save_ptr;
	// int i = 0; 

   	// for (token = strtok_r (file_name, " ", &save_ptr); token != NULL; token = strtok_r (NULL, " ", &save_ptr)){
	// 	stk[i] = token;
	// 	i++;
	// }

	/* And then load the binary */
	lock_acquire(&filesys_lock);
	success = load (file_name, &_if);
	lock_release(&filesys_lock);

	// argument_stack(stk, i, &_if);
	// _if.R.rdi = i;
	// _if.R.rsi = (char*)_if.rsp + 8;


	// hex_dump(_if.rsp, _if.rsp, USER_STACK - (uint64_t)_if.rsp, true);

	/* If load failed, quit. */
	palloc_free_page (file_name);
	if (!success)
		return -1;

	/* Start switched process. */
	do_iret (&_if);
	NOT_REACHED ();
}


/* Waits for thread TID to die and returns its exit status.  If
 * it was terminated by the kernel (i.e. killed due to an
 * exception), returns -1.  If TID is invalid or if it was not a
 * child of the calling process, or if process_wait() has already
 * been successfully called for the given TID, returns -1
 * immediately, without waiting.
 *
 * This function will be implemented in problem 2-2.  For now, it
 * does nothing. */
int
process_wait (tid_t child_tid UNUSED) {
	/* XXX: Hint) The pintos exit if process_wait (initd), we recommend you
	 * XXX:       to add infinite loop here before
	 * XXX:       implementing the process_wait. */

	// for (uint64_t i; i < 40000000000; i++){

	// }
	// return -1;
	struct thread *t = get_thread_from_tid(child_tid);
	if (t == NULL) {
		return -1;
	}

	sema_down(&t->wait_sema);
	list_remove(&t->child_list_elem);

	sema_up(&t->exit_sema);

	return t->exit_status; 
}

/* Exit the process. This function is called by thread_exit (). */
void
process_exit (void) {

	/* TODO: Your code goes here.
	 * TODO: Implement process termination message (see
	 * TODO: project2/process_termination.html).
	 * TODO: We recommend you to implement process resource cleanup here. */
	struct thread *t = thread_current();


	if (t->pml4 != NULL){
		printf("%s: exit(%d)\n", t->name, t->exit_status);
		file_close(t->running);
		t->running = NULL;
	}

	struct list *exit_list = &t->fd_table;
	struct list_elem *e = list_begin(&exit_list);
	for(int i = 2; i< t->last_created_fd; ++i)
		close(i);


	file_close(t->running);
	process_cleanup();
	sema_up(&t->wait_sema);
	sema_down(&t->exit_sema);

}

/* Free the current process's resources. */
/* 현재 프로세스의 리소스를 확보합니다.*/
static void
process_cleanup (void) {
	struct thread *curr = thread_current ();

#ifdef VM
	supplemental_page_table_kill (&curr->spt);
#endif

	uint64_t *pml4;
	/* Destroy the current process's page directory and switch back
	 * to the kernel-only page directory. */
	pml4 = curr->pml4;
	if (pml4 != NULL) {
		/* Correct ordering here is crucial.  We must set
		 * cur->pagedir to NULL before switching page directories,
		 * so that a timer interrupt can't switch back to the
		 * process page directory.  We must activate the base page
		 * directory before destroying the process's page
		 * directory, or our active page directory will be one
		 * that's been freed (and cleared). 
		 * 여기서 올바른 순서는 매우 중요합니다. 
		 * 페이지 디렉토리를 전환하기 전에 cur->pagedir를 NULL로 설정해야 
		 * 타이머 인터럽트가 프로세스 페이지 디렉토리로 다시 전환할 수 없습니다.
		 * 프로세스의 페이지 디렉토리를 파기하기 전에 기본 페이지 디렉토리를 활성화해야 합니다. 
		 * 그렇지 않으면 활성화된 페이지 디렉토리가 해제(및 삭제)된 디렉토리가 됩니다
		 */
		curr->pml4 = NULL;
		pml4_activate (NULL);
		pml4_destroy (pml4);
	}
}

/* Sets up the CPU for running user code in the nest thread.
 * This function is called on every context switch. 
 * 네스트 스레드에서 사용자 코드를 실행할 CPU를 설정합니다.
 * 이 기능은 모든 컨텍스트 스위치에서 호출됩니다.
 */
void
process_activate (struct thread *next) {
	/* Activate thread's page tables. 
	 * 스레드의 페이지 테이블을 활성화합니다.
	 */
	pml4_activate (next->pml4);

	/* Set thread's kernel stack for use in processing interrupts. 
	 * 인터럽트 처리에 사용할 스레드의 커널 스택을 설정합니다.
	 */
	tss_update (next);
}

/* We load ELF binaries.  The following definitions are taken
 * from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
#define EI_NIDENT 16

#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
 * This appears at the very beginning of an ELF binary. */
struct ELF64_hdr {
	unsigned char e_ident[EI_NIDENT];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
};

struct ELF64_PHDR {
	uint32_t p_type;
	uint32_t p_flags;
	uint64_t p_offset;
	uint64_t p_vaddr;
	uint64_t p_paddr;
	uint64_t p_filesz;
	uint64_t p_memsz;
	uint64_t p_align;
};

/* Abbreviations */
#define ELF ELF64_hdr
#define Phdr ELF64_PHDR

static bool setup_stack (struct intr_frame *if_);
static bool validate_segment (const struct Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes,
		bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
 * Stores the executable's entry point into *RIP
 * and its initial stack pointer into *RSP.
 * Returns true if successful, false otherwise. 
 * FILE_NAME에서 ELF 실행 파일을 현재 스레드로 로드합니다.
 * 실행 파일의 진입 지점을 *RIP에 저장합니다
 * 초기 스택 포인터를 *RSP에 입력합니다.
 * 성공하면 true, 그렇지 않으면 false를 반환합니다.
 */
static bool
load (const char *file_name, struct intr_frame *if_) {
	struct thread *t = thread_current ();
	struct ELF ehdr;
	struct file *file = NULL;
	off_t file_ofs;
	bool success = false;
	int i, j;

	//스택에 전달받은 인자를 쌓아주는 작업
	char *stk[64];
	int argc = 0;
	char *token, *save_ptr;

   	for (token = strtok_r (file_name, " ", &save_ptr); token != NULL; token = strtok_r (NULL, " ", &save_ptr)){
		stk[argc] = token;
		argc++;
	}

	/* Allocate and activate page directory. */
	t->pml4 = pml4_create ();
	if (t->pml4 == NULL)
		goto done;
	process_activate (thread_current ());

	/* Open executable file. */
	file = filesys_open (file_name);
	if (file == NULL) {
		printf ("load: %s: open failed\n", file_name);
		goto done;
	}
	/* Read and verify executable header. */
	if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
			|| memcmp (ehdr.e_ident, "\177ELF\2\1\1", 7)
			|| ehdr.e_type != 2
			|| ehdr.e_machine != 0x3E // amd64
			|| ehdr.e_version != 1
			|| ehdr.e_phentsize != sizeof (struct Phdr)
			|| ehdr.e_phnum > 1024) {
		printf ("load: %s: error loading executable\n", file_name);
		goto done;
	}
	/* Read program headers. */
	file_ofs = ehdr.e_phoff;
	for (i = 0; i < ehdr.e_phnum; i++) {
		struct Phdr phdr;
		if (file_ofs < 0 || file_ofs > file_length (file))
			goto done;
		file_seek (file, file_ofs);
		if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
			goto done;
		file_ofs += sizeof phdr;
		switch (phdr.p_type) {
            case PT_NULL:
            case PT_NOTE:
            case PT_PHDR:
            case PT_STACK:
            default:
                /* Ignore this segment. */
                break;
            case PT_DYNAMIC:
            case PT_INTERP:
            case PT_SHLIB:
                goto done;
            case PT_LOAD:
                if (validate_segment (&phdr, file)) {
                    bool writable = (phdr.p_flags & PF_W) != 0;
                    uint64_t file_page = phdr.p_offset & ~PGMASK;
                    uint64_t mem_page = phdr.p_vaddr & ~PGMASK;
                    uint64_t page_offset = phdr.p_vaddr & PGMASK;
                    uint32_t read_bytes, zero_bytes;
                    if (phdr.p_filesz > 0) {
                        /* Normal segment.
                         * Read initial part from disk and zero the rest. */
                        read_bytes = page_offset + phdr.p_filesz;
                        zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                    } else {
                        /* Entirely zero.
                         * Don't read anything from disk. */
                        read_bytes = 0;
                        zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                    }
                    if (!load_segment (file, file_page, (void *) mem_page,
                                read_bytes, zero_bytes, writable))
                        goto done;
                }
                else
                    goto done;
                break;
        }
	}

	t->running = file; 

	file_deny_write(file);
	/* Set up stack. */
	if (!setup_stack (if_))
		goto done;

	/* Start address. */
	if_->rip = ehdr.e_entry;

	argument_stack(stk, argc, if_);

	/* TODO: Your code goes here.
	//  * TODO: Implement argument passing (see project2/argument_passing.html). */

	if_->R.rsi = if_->rsp + 8;
	if_->R.rdi = argc;	
	success = true;

done:
	/* We arrive here whether the load is successful or not. */
	// file_close (file);
	return success;
}


/* Checks whether PHDR describes a valid, loadable segment in
 * FILE and returns true if so, false otherwise. y
 * PHDR에서 로드 가능한 유효한 세그먼트를 설명하는지 확인합니다
 * 파일을 입력하면 true, 그렇지 않으면 false를 반환합니다.
 */
static bool
validate_segment (const struct Phdr *phdr, struct file *file) {
	/* p_offset and p_vaddr must have the same page offset. */
	if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
		return false;

	/* p_offset must point within FILE. */
	if (phdr->p_offset > (uint64_t) file_length (file))
		return false;

	/* p_memsz must be at least as big as p_filesz. */
	if (phdr->p_memsz < phdr->p_filesz)
		return false;

	/* The segment must not be empty. */
	if (phdr->p_memsz == 0)
		return false;

	/* The virtual memory region must both start and end within the
	   user address space range. */
	if (!is_user_vaddr ((void *) phdr->p_vaddr))
		return false;
	if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
		return false;

	/* The region cannot "wrap around" across the kernel virtual
	   address space. */
	if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
		return false;

	/* Disallow mapping page 0.
	   Not only is it a bad idea to map page 0, but if we allowed
	   it then user code that passed a null pointer to system calls
	   could quite likely panic the kernel by way of null pointer
	   assertions in memcpy(), etc. 
	 * 매핑 페이지 0을 허용하지 않습니다. 
	 * 페이지 0을 매핑하는 것은 나쁜 생각일 뿐만 아니라, 
	 * 이를 허용하면 시스템 호출에 널 포인터를 전달한 사용자 코드가 
	 * memcpy() 등에서 널 포인터 인수를 통해 커널을 패닉시킬 가능성이 높습니다.
	 */
	 
	if (phdr->p_vaddr < PGSIZE)
		return false;

	/* It's okay. */
	return true;
}

#ifndef VM
/* Codes of this block will be ONLY USED DURING project 2.
 * If you want to implement the function for whole project 2, implement it
 * outside of #ifndef macro. */

/* load() helpers. */
static bool install_page (void *upage, void *kpage, bool writable);

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	file_seek (file, ofs);
	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* Get a page of memory. */
		uint8_t *kpage = palloc_get_page (PAL_USER);
		if (kpage == NULL)
			return false;

		/* Load this page. */
		if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes) {
			palloc_free_page (kpage);
			return false;
		}
		memset (kpage + page_read_bytes, 0, page_zero_bytes);

		/* Add the page to the process's address space. */
		if (!install_page (upage, kpage, writable)) {
			printf("fail\n");
			palloc_free_page (kpage);
			return false;
		}

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Create a minimal stack by mapping a zeroed page at the USER_STACK */
static bool
setup_stack (struct intr_frame *if_) {
	uint8_t *kpage;
	bool success = false;

	kpage = palloc_get_page (PAL_USER | PAL_ZERO);
	if (kpage != NULL) {
		success = install_page (((uint8_t *) USER_STACK) - PGSIZE, kpage, true);
		if (success)
			if_->rsp = USER_STACK;
		else
			palloc_free_page (kpage);
	}
	return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
 * virtual address KPAGE to the page table.
 * If WRITABLE is true, the user process may modify the page;
 * otherwise, it is read-only.
 * UPAGE must not already be mapped.
 * KPAGE should probably be a page obtained from the user pool
 * with palloc_get_page().
 * Returns true on success, false if UPAGE is already mapped or
 * if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable) {
	struct thread *t = thread_current ();

	/* Verify that there's not already a page at that virtual
	 * address, then map our page there. */
	return (pml4_get_page (t->pml4, upage) == NULL
			&& pml4_set_page (t->pml4, upage, kpage, writable));
}
#else
/* From here, codes will be used after project 3.
 * If you want to implement the function for only project 2, implement it on the
 * upper block. */

static bool
lazy_load_segment (struct page *page, void *aux) {
	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. */
}

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		void *aux = NULL;
		if (!vm_alloc_page_with_initializer (VM_ANON, upage,
					writable, lazy_load_segment, aux))
			return false;

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Create a PAGE of stack at the USER_STACK. Return true on success. */
static bool
setup_stack (struct intr_frame *if_) {
	bool success = false;
	void *stack_bottom = (void *) (((uint8_t *) USER_STACK) - PGSIZE);

	/* TODO: Map the stack on stack_bottom and claim the page immediately.
	 * TODO: If success, set the rsp accordingly.
	 * TODO: You should mark the page is stack. */
	/* TODO: Your code goes here */

	return success;
}
#endif /* VM */

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