#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

#include "threads/synch.h"
#include "threads/palloc.h"
#include "lib/string.h"
#include "filesys/filesys.h"
// #include "filesys/file.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);
void halt (void);
void exit (int status);
// pid_t fork (const char *thread_name);
int exec (const char *file);
int wait (pid_t);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
// int filesize (int fd);
int read (int fd, void *buffer, unsigned length);
int write (int fd, const void *buffer, unsigned length);
// void seek (int fd, unsigned position);
// unsigned tell (int fd);
void close (int fd);

int insert_file_fdt(struct file *file);
static struct file *find_file_by_fd(int fd);
/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */



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
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);

	
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f) {
	// TODO: Your implementation goes here.

	struct thread *t = thread_current();
	t->tf = *f;

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

	// struct file_descriptor fd;

	addr = f->R.rsi;
	
	switch(f->R.rax){
	case SYS_HALT:
		halt();		
		break;

	case SYS_EXIT:
		exit(f->R.rdi);
		break;

	case SYS_FORK:
		fork(thread_current()->name);
		break;

	case SYS_EXEC:
		f->R.rax = exec(f->R.rsi);
		break;

	case SYS_WAIT:
		wait(t);
		break;
	
	case SYS_CREATE:
		create(*(char*)(addr), PGSIZE);
		break;

	case SYS_REMOVE:
		remove(*(char*)(addr));
		break;
		
	case SYS_OPEN:
		f->R.rax = open((char*)addr);
		break;
	
	// case SYS_FILESIZE:
	// 	filesize();

	case SYS_READ:
		f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
		
	case SYS_WRITE:
		write(f->R.rdi, f->R.rsi, f->R.rdx);
		break;

	// case SYS_SEEK:
	// 	seek();

	// case SYS_TELL:
	// 	tell();

	case SYS_CLOSE:
		close(f->R.rdi);
		break;
	}


	// thread_exit ();
}

void halt(void)
{
	power_off();
}

void exit (int status)
{
	thread_current()->exit_status = status;
	printf ("%s: exit(%d)\n", thread_current()->name, status);
	thread_exit();
}

tid_t fork (const char *thread_name){
	
	process_fork(thread_name, thread_current()->tf);
}

int exec (const char *file){
	
	char* file_in_kernel;
	file_in_kernel = palloc_get_page(PAL_ZERO);

	if (file_in_kernel == NULL)
		exit(-1);
	strlcpy(file_in_kernel, file, PGSIZE);
	
	if (process_exec(file_in_kernel) == -1)
		return -1;
}

int wait (tid_t t)
{

	process_wait(t); 
}

bool create (const char *file, unsigned initial_size)
{
	if(filesys_create(file, initial_size)== false)
		return false;
	return true;
}

bool remove (const char *file){
	if(filesys_remove(file)==false)
		return false;
	return true;
}

int open (const char *file){
	
	struct file *open_file = filesys_open(file);
	
	if (file == NULL){
		return -1;
	}

	int fd = insert_file_fdt(open_file);

	if (fd == -1){
		file_close(open_file);
	}
	return fd;
	
}

// int filesize (int fd){

// }
int read (int fd, void *buffer, unsigned length)
{
	int byte = 0;
	char* _buffer = buffer;
	if(fd == 0)
	{
		while(byte < length)
		{
			_buffer[byte++]=input_getc();
		}
		return byte;
	}
	else if(fd == 1)
	{
		return -1;
	}
	else{

	}
}

int write (int fd, const void *buffer, unsigned length){
	char * _buffer = buffer;
	if (fd == 0)
	{
		return -1;
	}
	else if (fd == 1){
		putbuf(_buffer, length);
		return length;
	}
}
// void seek (int fd, unsigned position){

// }
// unsigned tell (int fd){

// }
void close (int fd){
	
	struct file *file;

	file = find_file_by_fd(fd);
	if (file== NULL){
		return;
	}

	file_close(&file);
}


int insert_file_fdt(struct file *file)
{
    struct thread *cur = thread_current();
    struct file **fdt = cur->fd_table;

    // Find open spot from the front
    //  fd 위치가 제한 범위 넘지않고, fd table의 인덱스 위치와 일치한다면
    while (cur->fd_idx < 10 && fdt[cur->fd_idx])
    {
        cur->fd_idx++;
    }

    // error - fd table full
    if (cur->fd_idx >= 10)
        return -1;

    fdt[cur->fd_idx] = file;
    return cur->fd_idx;
}

static struct file *find_file_by_fd(int fd)
{
    struct thread *cur = thread_current();
    if (fd < 0 || fd >= 10)
    {
        return NULL;
    }
    return cur->fd_table[fd];
}