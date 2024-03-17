#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <list.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "filesys/file.h"
#include "lib/user/syscall.h"
#include "filesys/filesys.h"
#include "threads/synch.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* System call.
 *
 * 사용자 프로세스가 커널 기능에 액세스하기를 원할 때마다 시스템 호출을 호출합니다. 
 * 스켈레톤 시스템 호출 핸들러입니다. 현재는 메시지만 출력하고 사용자 프로세스를 종료합니다. 
 * 이 프로젝트의 2부에서는 시스템 호출에 필요한 다른 모든 작업을 수행하기 위해 코드를 추가합니다.

 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * 이전 시스템 호출 서비스는 인터럽트 핸들러에서 처리했습니다
 * (예: 리눅스의 경우 int 0x80). 그러나 x86-64에서는 시스템 호출을 
 * 요청하기 위한 효율적인 경로, 즉 'syscall' 명령을 제공한다.
 * 
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. 
 * 
 * syscall 명령은 모델에서 값을 읽음으로써 작동합니다
 * MSR(Specific Register). 자세한 내용은 설명서 참조 */
 

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

int open (const char *file);
bool create (const char *file, unsigned initial_size);
void exit(int status);
void halt(void);
void close (int fd);
bool remove (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
// pid_t fork (const char *thread_name, struct intr_frame *f);
int exec (const char *file);
int wait (pid_t pid);

void check_address(const uint64_t *uaddr);
static struct file *find_file_by_fd(int fd);

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. 
	 * 인터럽트 서비스 반올림은 인터럽트 서비스를 제공해서는 안 됩니다
	 * syscall_entry가 userland 스택을 커널 모드 스택으로 스왑할 때까지. 따라서 FLAG_FL을 마스킹했습니다.*/
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);

	lock_init(&thread_current()->wait_on_lock);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f) {
	// TODO: Your implementation goes here.

	char *fn_copy;

	// if(!is_user_vaddr(f->rsp)){
	// 	printf("isnotvaddr\n");
	// 	thread_exit();
	// }

	// else if(f->rsp > KERN_BASE || f->rsp < 0){
	// 	printf("smaller\n");
	// 	thread_exit();
	// }
	
	// int addr = (f->rsp + 8);
	// if (!is_user_vaddr(addr) || (addr > KERN_BASE || addr<0)) {
	// 	printf ("third condition\n");
	// 	thread_exit();
	// }
	
	// printf ("system call!\n");
	switch(f->R.rax) {
		case SYS_HALT:
			halt();
			break;		

		case SYS_EXIT:
			exit(f->R.rdi);
			break;
		
		// case SYS_FORK:
		// 	f->R.rax = fork(f->R.rdi, f);
		// 	break;

		// case SYS_EXEC:
		// 	if(exec(f->R.rdi) == -1)
		// 		exit(-1);
		// 	break;

		// case SYS_WAIT:
		// 	f->R.rax = process_wait(f->R.rdi);
		// 	break;

		case SYS_CREATE:
			f->R.rax = create(f->R.rdi,f->R.rsi);
			break;
		
		// case SYS_REMOVE:
		// 	remove(f->R.rdi);
		// 	break;

		// case SYS_OPEN:
		// 	open(f->R.rdi);
		// 	break;

		// case SYS_FILESIZE:
		// 	filesize(f->R.rdi);
		// 	break;

		// case SYS_READ:
		// 	read(f->R.rdi,f->R.rsi,f->R.rdx);
		// 	break;

		case SYS_WRITE:
			f->R.rax = write(f->R.rdi,f->R.rsi,f->R.rdx);
			break;

		// case SYS_SEEK:
		// 	seek(f->R.rdi,f->R.rsi);
		// 	break;

		// case SYS_TELL:
		// 	tell(f->R.rdi);
		// 	break;

		// case SYS_CLOSE:
		// 	close(f->R.rdi);
		// 	break;
		// default:
		// 	break;
	}

}

void check_address(const uint64_t *uaddr) {
	struct thread *cur = thread_current();
	if (uaddr == NULL || is_kernel_vaddr(uaddr) || pml4_get_page(cur->pml4, uaddr) == NULL) {
		exit(-1);
	}
}

void halt(void){
	power_off();
}

void exit(int status){
	struct thread *t = thread_current ();
	t->status = status;
	printf ("%s: exit(%d)\n", thread_name(), status);
	thread_exit();
}

// tid_t fork(const char *therad_name, struct intr_frame *f) {
// 	return process_fork
// }

bool create (const char *file, unsigned initial_size) {
	check_address(file);
	return filesys_create(file,initial_size);
}

int write (int fd, const void *buffer, unsigned size) {
	check_address(buffer);
	int write_result;
	lock_acquire(&thread_current()->wait_on_lock);
	if (fd == 1){
		putbuf(buffer, size);
		write_result = size;
	}
	else if(fd<2){
		write_result = -1;
	}
	else
	{	
		struct file *file = find_file_by_fd(fd);
		if(file == NULL){
			write_result = -1;
		}
		else{
			write_result = file_write(file, buffer, size);
		}
	}
	lock_release(&thread_current()->wait_on_lock);
	return write_result;
}

static struct file *find_file_by_fd(int fd) {
	struct thread *cur = thread_current();

	if (fd < 0 || fd >= FDCOUNT_LIMIT) {
		return NULL;
	}
	return cur->fd_table[fd];
}
