/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/mmu.h"
#include "userprog/process.h"
#include "userprog/syscall.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);
// bool lazy_load_segment_for_file (struct page *page, void *aux);
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
	
	// file-backed page에 있는 파일 페이지 구조체에 값들을 넣어주면 off가 통과한다. 
	file_page->file = ((struct lazy*)(page->uninit.aux))->file;
	file_page->ofs =((struct lazy*)(page->uninit.aux))->ofs;
	file_page->read_bytes = ((struct lazy*)(page->uninit.aux))->read_bytes;
	file_page->zero_bytes = ((struct lazy*)(page->uninit.aux))->zero_bytes;
	
	// return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page = &page->file;

	return lazy_load_segment(page, file_page);
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
// static void

static void
file_backed_destroy (struct page *page) 
{
	struct file_page *file_page = &page->file;
	 if (pml4_is_dirty(thread_current()->pml4, page->va))
    {
        file_write_at(file_page->file, page->va, file_page->read_bytes, file_page->ofs);
        pml4_set_dirty(thread_current()->pml4, page->va, 0);
    }
    pml4_clear_page(thread_current()->pml4, page->va);
}


/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {

	//똑같은 파일을 그대로 쓰면 외부에서 해당 파일을 close하게 될 수도 있는데, 이 때 문제가 생긴다
	struct file* new_file = file_reopen(file);
	if(new_file == NULL){
		return NULL;
	}

	void* return_address = addr;
	
	// 파일의 길이에 따라서 읽을 바이트 수 결정 
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

	//이 부분이 length로 되어있었는데, 아래를 load_segment와 같게 했기 때문에 page_read_bytes만 바뀌고 length가 안바뀌었다 - 무한루프를 돌았는데 해결
	while (read_bytes > 0 || zero_bytes > 0) {
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

		if (!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_segment, file_lazy)){
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
// void
// do_munmap (void *addr) {
// 	// for (int fd = 2; fd <= thread_current()->last_created_fd ; fd++){
// 	while(1){
// 		// struct file_descriptor *file_desc = find_file_descriptor(fd);
// 		struct page * munmap_page = spt_find_page(&thread_current()->spt, addr);
// 		if (munmap_page == NULL){
// 			return NULL;
// 		}
// 		if (page_get_type(munmap_page) != VM_FILE){
// 			PANIC("page is not file-backed\n");
// 		}
// 		struct lazy *munmap_aux = ((struct lazy*)munmap_page->uninit.aux);
// 		// int checker = strcmp(&file_desc->file, &munmap_page->file.file);
// 		// if (checker != file_length(munmap_page))
// 		// 	goto err;
// 		if(pml4_is_dirty(thread_current()->pml4, munmap_page->va)){
// 			file_write_at(munmap_aux->file, addr, munmap_aux->read_bytes , munmap_aux->ofs);
// 			pml4_set_dirty(thread_current()->pml4, munmap_page->va, false);
// 		}
// 		pml4_clear_page(thread_current()->pml4, munmap_page->va);	
// 		addr += PGSIZE;
// 	}
// }
void
do_munmap (void *addr)
{

    struct supplemental_page_table *spt = &thread_current()->spt;
    struct page *page = spt_find_page(spt, addr);
	//페이지 개수는 마지막 + PGSIZE를 해주어야 하는데, 이 때 중요한 건 file_length 자체가 PGSIZE단위이면 더해주면 안된다. 
	
	int page_count;
	if (file_length(&page->file)%PGSIZE != 0){
 	    page_count = file_length(&page->file) + PGSIZE;
	} else {
		page_count = file_length(&page->file);
	}
	
    for (int i = 0; i < page_count/PGSIZE; i++)
    {
        if (page)
            destroy(page);
        addr += PGSIZE;
        page = spt_find_page(spt, addr);
    }
}

