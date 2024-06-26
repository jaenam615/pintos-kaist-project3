#ifndef VM_FILE_H
#define VM_FILE_H
#include "filesys/file.h"
#include "vm/vm.h"

struct page;
enum vm_type;

struct file_page {
	//IMPLEMENTATION
	struct file *file;
	bool writable;
	off_t ofs; 
	int64_t data_size;
	size_t read_bytes;
	size_t zero_bytes;
};

void vm_file_init (void);
bool file_backed_initializer (struct page *page, enum vm_type type, void *kva);
void *do_mmap(void *addr, size_t length, int writable,
		struct file *file, off_t offset);
void do_munmap (void *va);
bool lazy_load_segment_for_file (struct page *page, void *aux);
#endif
