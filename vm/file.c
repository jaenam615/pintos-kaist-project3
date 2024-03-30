/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/mmu.h"
#include "userprog/process.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);
static bool lazy_load_segment (struct page *page, void *aux);
/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;
	page->file.file = ((struct lazy*)(page->uninit.aux))->file;
	page->file.ofs =((struct lazy*)(page->uninit.aux))->ofs;
	page->file.read_bytes = ((struct lazy*)(page->uninit.aux))->read_bytes;
	page->file.zero_bytes = ((struct lazy*)(page->uninit.aux))->zero_bytes;

	struct file_page *file_page = &page->file;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	
	if(file_reopen(file) != NULL){
		return NULL;
	}

	void* return_address = addr;

	ASSERT (pg_ofs (addr) == 0);
	ASSERT (offset % PGSIZE == 0);

	while (length > 0) {
			/* Do calculate how to fill this page.
			* We will read PAGE_READ_BYTES bytes from FILE
			* and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = length < PGSIZE ? length : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;
		
		struct lazy* file_lazy = (struct lazy*)malloc(sizeof(struct lazy));
		file_lazy->file = file;
		file_lazy->ofs = offset;
		file_lazy->read_bytes = page_read_bytes;
		file_lazy->zero_bytes = page_zero_bytes;

		if (!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_segment, file_lazy)){
			return NULL;
		}		
		/* Advance. */
		length -= page_read_bytes;
		addr += PGSIZE;
		//이 부분을 추가해주어 project 3에서 지난 프로젝트 테스트 케이스 성공
		offset += page_read_bytes;
	}
	return true;
}

/* Do the munmap */
void
do_munmap (void *addr) {
}

static bool
lazy_load_segment (struct page *page, void *aux) {
	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. */
	
	struct lazy *lazy = (struct lazy*)aux;


	//여기에서 오타가 있었나?
	//file에서의 위치를 시작에서부터 ofs만큼으로 설정
	file_seek(lazy->file, lazy->ofs);
	//file에서 read_bytes만큼 buf로 read한다
	/* Load this page. */
	// lock_try_acquire(&filesys_lock);
	if(file_read(lazy->file, page->frame->kva, lazy->read_bytes) != (int) lazy->read_bytes){
		palloc_free_page(page->frame->kva);	
		return false;
		
	}
	// lock_release(&filesys_lock);
	//read_bytes로 설정한 이후 부분부터 zero_bytes만큼 0으로 채운다
	void* start;
	start = page->frame->kva + lazy->read_bytes;
	memset(start,0,lazy->zero_bytes);

	file_seek(lazy->file,lazy->ofs);
	return true;

}