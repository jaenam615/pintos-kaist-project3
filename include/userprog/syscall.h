#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "threads/synch.h"

void syscall_init (void);
void close(int fd);
struct lock filesys_lock;
// tid_t fork (const char *thread_name, struct intr_frame *f)
// 구현
#endif /* userprog/syscall.h */
