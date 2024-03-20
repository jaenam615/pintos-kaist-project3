#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "threads/init.h"
#include "userprog/gdt.h"
#include "userprog/process.h"
#include "threads/flags.h"
#include "intrinsic.h"

#include "threads/synch.h"
#include "threads/palloc.h"
#include "lib/string.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);
void halt (void);
void exit (int status);
tid_t fork (const char *thread_name, struct intr_frame *f);
int exec (const char *file);
int wait (tid_t);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned length);
int write (int fd, const void *buffer, unsigned length);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

int insert_file_fdt(struct file *file);
static struct file *find_file_by_fd(int fd);
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

struct file_descriptor *find_file_descriptor(int fd) {
	struct list *fd_table = &thread_current()->fd_table;
	ASSERT(fd_table != NULL);
	ASSERT(fd > 1);
	if (list_empty(fd_table)) 
		return NULL;
	struct file_descriptor *file_descriptor;
	struct list_elem *e = list_begin(fd_table);
	ASSERT(e != NULL);
	while (e != list_tail(fd_table)) {
		file_descriptor = list_entry(e, struct file_descriptor, fd_elem);
		if (file_descriptor->fd == fd) {
			return file_descriptor;
		}
		e = list_next(e);
	}
	return NULL;
}

/* An open file. */
struct file {
	struct inode *inode;        /* File's inode. */
	off_t pos;                  /* Current position. */
	bool deny_write;            /* Has file_deny_write() been called? */
};

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

	lock_init(&filesys_lock);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f) {
	// TODO: Your implementation goes here.

	// int size = palloc_init();
	
	//주소가 유호한지 확인
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

	// addr = f->R.rsi;
	
	switch(f->R.rax){
	case SYS_HALT:
		halt();
		break;		

	case SYS_EXIT:
		exit(f->R.rdi);
		break;
	
	case SYS_FORK:
		f->R.rax = fork(f->R.rdi,f);
		break;

	case SYS_EXEC:
		f->R.rax = exec(f->R.rdi);
		break;

	case SYS_WAIT:
		f->R.rax = wait(f->R.rdi);
		break;
	
	case SYS_CREATE:
		f->R.rax = create(f->R.rdi, f->R.rsi);
		break;

	case SYS_REMOVE:
		f->R.rax = remove(f->R.rdi);
		break;
		
	case SYS_OPEN:
		f->R.rax = open(f->R.rdi);
		break;

	case SYS_FILESIZE:
		f->R.rax = filesize(f->R.rdi);
		break;

	case SYS_READ:
		f->R.rax = read(f->R.rdi,f->R.rsi,f->R.rdx);
		break;

	case SYS_WRITE:
		f->R.rax = write(f->R.rdi,f->R.rsi,f->R.rdx);
		break;

	case SYS_SEEK:
		seek(f->R.rdi,f->R.rsi);
		break;

	case SYS_TELL:
		f->R.rax = tell(f->R.rdi);
		break;

	case SYS_CLOSE:
		close(f->R.rdi);
		break;

	default:
		break;
	}

}

//핀토스 종료
void halt(void)
{
	power_off();
}

//현재 유저 프로그램 종료 (status를 반환함)
void exit(int status)
{
	char* p_name = thread_current ()->name;
	char* p = "\0";
	thread_current()->exit_status = status;
	strtok_r(p_name," ",&p);
	printf ("%s: exit(%d)\n", p_name, status);
	thread_exit();
}

tid_t fork (const char *thread_name, struct intr_frame *f){
	
	return process_fork(thread_name, f);
}

//현재 프로세스를 file로 바꿈
int exec (const char *file){
	
	if(pml4_get_page(thread_current()->pml4, file) == NULL || file == NULL || !is_user_vaddr(file)) 
		exit(-1);

	char* file_in_kernel;
	file_in_kernel = palloc_get_page(0);

	if (file_in_kernel == NULL)
		exit(-1);
	strlcpy(file_in_kernel, file, PGSIZE);
	
	if (process_exec(file_in_kernel) == -1)
		exit(-1);
}

//자식 프로세스 tid가 끝날때까지 기다림 & 자식프로세스의 status를 반환함
int wait (tid_t t)
{		
	return process_wait(t); 
}

//file이라는 파일을 만들고, 성공시 true 반환
//파일 생성은 해당 파일을 열지는 않음. 열기 위해서는 open을 사용
//메인 쓰레드의 fd_table 리스트에 할당
bool create (const char *file, unsigned initial_size) 
{
		//가상메모리 주소에 해당하는 물리메모리 주소를 확인하고, 커널의 가상메모리 주소를 반환함
	if(pml4_get_page(thread_current()->pml4, file) == NULL || file == NULL || !is_user_vaddr(file) || *file == '\0') 
		exit(-1);
	lock_acquire(&filesys_lock);
	bool success = filesys_create(file, initial_size);
	lock_release(&filesys_lock);
	return success;
}

//file이라는 파일을 삭제
//성공시 true 반환
bool remove (const char *file)
{
	//가상메모리 주소에 해당하는 물리메모리 주소를 확인하고, 커널의 가상메모리 주소를 반환함
	if(pml4_get_page(thread_current()->pml4, file) == NULL || file == NULL || !is_user_vaddr(file) || *file == '\0') 
		exit(-1);

	return filesys_remove(file);
}

//file이라는 파일을 연다
//fd반환
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
	fd = allocate_fd(open_file, &thread_current()->fd_table);
	if (fd == -1)
		file_close(open_file);
	lock_release(&filesys_lock);
	return fd;
}
void close (int fd) {
	struct file_descriptor *file_desc = find_file_descriptor(fd);
	if(file_desc == NULL)
		return;
	file_close(file_desc->file);
	list_remove(&file_desc->fd_elem);
	free(file_desc);

}
int filesize (int fd)
{
	struct file_descriptor *file_desc = find_file_descriptor(fd);
	if(file_desc == NULL)
		return -1;
	return file_length(file_desc->file);
}

int read (int fd, void *buffer, unsigned size)
{
	if(pml4_get_page(thread_current()->pml4, buffer) == NULL || buffer == NULL || !is_user_vaddr(buffer) || fd < 0)
		exit(-1);
	int byte = 0;
	char* _buffer = buffer;
	lock_acquire(&filesys_lock);
	if(fd == 0)
	{
		while(byte < size)
		{
			_buffer[byte++] = input_getc();
		}
		lock_release(&filesys_lock);
	}
	else if(fd == 1)
	{
		return -1;
	}
	else
	{
		struct file_descriptor *file_desc = find_file_descriptor(fd);
		if(file_desc == NULL)
		{
			lock_release(&filesys_lock);
			return -1;
		}
		byte = file_read(file_desc->file,buffer,size);
		lock_release(&filesys_lock);
	}
	return byte;
}

int write (int fd, const void *buffer, unsigned size)
{
	if(pml4_get_page(thread_current()->pml4, buffer) == NULL || buffer == NULL || !is_user_vaddr(buffer) || fd < 0)
		exit(-1);
	char* _buffer = buffer;
	if(fd == 0)
	{
		return -1;
	}
	else if(fd == 1)
	{
		putbuf(_buffer,size);
		return size;
	}
	else
	{
		struct file_descriptor *file_desc = find_file_descriptor(fd);
		if(file_desc == NULL)
			return -1;
		lock_acquire(&filesys_lock);
		file_write(file_desc->file,_buffer,size);
		lock_release(&filesys_lock);
		return size;
	}
}

void seek (int fd, unsigned position)
{
	struct file_descriptor *file_desc = find_file_descriptor(fd);
	if(file_desc == NULL)
		return;
	file_seek(file_desc->file, position);
}

unsigned tell (int fd)
{
	struct file_descriptor *file_desc = find_file_descriptor(fd);
	if(file_desc == NULL)
		return -1;
	return file_tell(&file_desc->file);
}

