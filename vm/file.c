/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/mmu.h"
#include "userprog/process.h"
#include "userprog/syscall.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);
bool lazy_load_segment_for_file (struct page *page, void *aux);
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

	struct file_page *file_page = &page->file;
	
	// file_page->file = ((struct lazy*)(page->uninit.aux))->file;
	// file_page->ofs =((struct lazy*)(page->uninit.aux))->ofs;
	// file_page->read_bytes = ((struct lazy*)(page->uninit.aux))->read_bytes;
	// file_page->zero_bytes = ((struct lazy*)(page->uninit.aux))->zero_bytes;
	
	// return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page = &page->file;

	if(page==NULL){
		return false;
	}

	struct lazy* aux = (struct lazy*)page->uninit.aux;
	
	struct file * file = aux->file;

	off_t offset = aux->ofs;
	size_t page_read_bytes = aux->read_bytes;
	size_t page_zero_bytes = PGSIZE - page_read_bytes;

	file_seek(file,offset);

	if(file_read(file, kva, page_read_bytes)!=(int)page_read_bytes){
		return false;
	}

	memset(kva+page_read_bytes,0,page_zero_bytes);

	return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	if(page==NULL){
		return false;
	}
	struct lazy* aux = (struct lazy*)page->uninit.aux;
	struct file * file = aux->file;

	if(pml4_is_dirty(thread_current()->pml4,page->va)){
		file_write_at(file,page->va, aux->read_bytes, aux->ofs);
		// page->va == frame->page->kva (page->va 있는 정보나 frame에 있는 정보나 같다.)
		pml4_set_dirty(thread_current()->pml4, page->va, false);
	}
	pml4_clear_page(thread_current()->pml4, page->va);
	return true;
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

	struct file* new_file = file_reopen(file);
	if(new_file == NULL){
		return NULL;
	}

	void* return_address = addr;

	size_t read_bytes;
	if (file_length(new_file) < length){
		read_bytes = file_length(new_file);
	} else {
		read_bytes = length;
	}

	size_t zero_bytes = PGSIZE - (read_bytes%PGSIZE);
	
	ASSERT (pg_ofs (addr) == 0);
	ASSERT (offset % PGSIZE == 0);
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);

	while (length > 0) {
			/* Do calculate how to fill this page.
			* We will read PAGE_READ_BYTES bytes from FILE
			* and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		struct lazy* file_lazy = (struct lazy*)malloc(sizeof(struct lazy));
		file_lazy->file = new_file;
		file_lazy->ofs = offset;
		file_lazy->read_bytes = page_read_bytes;
		file_lazy->zero_bytes = page_zero_bytes;

		if (!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_segment_for_file, file_lazy)){
			return NULL;
		}		
		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		addr += PGSIZE;
		//이 부분을 추가해주어 project 3에서 지난 프로젝트 테스트 케이스 성공
		offset += page_read_bytes;
	}
	return return_address;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	// for (int fd = 2; fd <= thread_current()->last_created_fd ; fd++){
	while(1){
		// struct file_descriptor *file_desc = find_file_descriptor(fd);
		struct page * munmap_page = spt_find_page(&thread_current()->spt, addr);
		if (munmap_page == NULL){
			return NULL;
		}
		if (page_get_type(munmap_page) != VM_FILE){
			PANIC("page is not file-backed\n");
		}
		struct lazy *munmap_aux = ((struct lazy*)munmap_page->uninit.aux);
		// int checker = strcmp(&file_desc->file, &munmap_page->file.file);
		// if (checker != file_length(munmap_page))
		// 	goto err;
		if(pml4_is_dirty(thread_current()->pml4, munmap_page->va)){
			file_write_at(munmap_aux->file, addr, munmap_aux->read_bytes , munmap_aux->ofs);
			pml4_set_dirty(thread_current()->pml4, munmap_page->va, false);
		}
		pml4_clear_page(thread_current()->pml4, munmap_page->va);	
		addr += PGSIZE;
	}
}

bool
lazy_load_segment_for_file (struct page *page, void *aux) {
	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. */
	
	struct lazy *lazy = (struct lazy*)aux;

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