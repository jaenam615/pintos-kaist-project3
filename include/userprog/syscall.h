#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);


// 구현
void halt(void);
void exit(int status);
bool create (const char *file, unsigned initial_size);
#endif /* userprog/syscall.h */
