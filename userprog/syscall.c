#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

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
	
	printf ("system call!\n");
	switch(f->R.rax){
	case SYS_HALT:
		halt();		
	
	case SYS_EXIT:
		exit();
	
	case SYS_FORK:
		fork();

	case SYS_EXEC:
		exec();

	case SYS_WAIT:
		wait();

	case SYS_CREATE:
		create();
	
	case SYS_REMOVE:
		remove();

	case SYS_OPEN:
		open();

	case SYS_FILESIZE:
		filesize();

	case SYS_READ:
		read();

	case SYS_WRITE:
		write();

	case SYS_SEEK:
		seek();

	case SYS_TELL:
		tell();

	case SYS_CLOSE:
		close();
	}


	thread_exit ();
}

// void halt(void){
// 	power_off();
// }

// void exit(int status){

// }

// pid_t fork()