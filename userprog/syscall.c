#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "threads/init.h"
#include "filesys/filesys.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);
bool create (const char *file, unsigned initial_size);

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

	char *fn_copy;

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
	
	// printf ("system call!\n");
	switch(f->R.rax){
		case SYS_HALT:
			halt();
			break;	
				
		case SYS_EXIT:
			exit(f->R.rdi);
			break;

		// case SYS_FORK:
		// 	fork();
		// 	break;

		// case SYS_EXEC:
		// 	exec();
		// 	break;

		// case SYS_WAIT:
		// 	wait();
		// 	break;

		case SYS_CREATE:
			f->R.rax = create(f->R.rdi, f->R.rsi);
			break;

		// case SYS_REMOVE:
		// 	f->R.rax = remove(f->R.rdi);
		// 	break;

		// case SYS_OPEN:
		// 	open();
		// 	break;

		// case SYS_FILESIZE:
		// 	filesize();
		// 	break;

		// case SYS_READ:
		// 	read();
		// 	break;

		// case SYS_WRITE:
		// 	write();
		// 	break;

		// case SYS_SEEK:
		// 	seek();
		// 	break;

		// case SYS_TELL:
		// 	tell();
		// 	break;

		// case SYS_CLOSE:
		// 	close();
		// 	break;
	}
  
	// thread_exit ();
}

void halt(void){
	power_off();
}

void exit(int status){
	struct thread *cur_thread = thread_current();
	cur_thread->status = THREAD_RUNNING;

	printf("%s: exit(%d)\n", thread_name(), cur_thread->status);
	thread_exit();
}

bool
create (const char *file, unsigned initial_size) {
	check_address(file);

	return filesys_create(file, initial_size);
}

// bool remove(const char *file) {
// 	check_address(file);
// 	return filesys_remove(file);
// }

// pid_t fork()

// int filesize (int fd) {
// 	return 
// }

// int write (int fd, const void *buffer, unsigned size) {
// 	if(!fd || !buffer) {
// 		return -1;
// 	}
// 	putbuf(buffer, size);

// 	int byteSize = size;

// 	return byteSize;
// }

void check_address(const uint64_t *addr) {
	struct thread *cur_thread = thread_current();

	if(addr == NULL || !(is_user_vaddr(addr)) || pml4_get_page(cur_thread, addr) == NULL) {
		exit(-1);
	}
}