/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "lib/kernel/bitmap.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};
struct list swap_table;
struct lock anon_lock;
/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	list_init(&swap_table);
	lock_init(&anon_lock);

	swap_disk = disk_get(1,1); //swapdisk 로 쓸 때 1:1


	//PGSIZE = 4096
	//DISK_SECTOR_SIZE = 512
	//페이지 단위로 관리하기 위해 
	disk_sector_t swap_size = disk_size(swap_disk) / (PGSIZE / DISK_SECTOR_SIZE);
	for (int i = 0; i < swap_size; i++)
	{
		//entry for swap_table
		struct sector *se = (struct sector *)malloc(sizeof(struct sector));
		
		se->page = NULL;
		se->slot = i;
		se->occupied = false;

		lock_acquire(&anon_lock);
		list_push_back(&swap_table, &se->swap_elem);
		lock_release(&anon_lock);
	}
}	

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
	anon_page->slot = -1;
	return true; 
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {

	struct anon_page *anon_page = &page->anon;
	// if (page == NULL){
	// 	return false; 
	// }
	struct list_elem*e; 
	struct sector *se; 

	lock_acquire(&anon_lock);
	for (e = list_begin(&swap_table); e != list_end(&swap_table); e = list_next(e)){
		se = list_entry(e, struct sector, swap_elem);
		if (se->slot == anon_page->slot){

			for (int i = 0; i < (PGSIZE / DISK_SECTOR_SIZE); i ++){
				disk_read(swap_disk, anon_page->slot * (PGSIZE / DISK_SECTOR_SIZE) + i, kva + (DISK_SECTOR_SIZE*i) );
			}

			//공간을 비어있도록 설정
			se->page = NULL;
			se->occupied = false;

			anon_page->slot= -1; 
			lock_release(&anon_lock);
			return true;
		}
	}
	lock_release(&anon_lock);
	return false; 
}

/* Swap out the page by writing contents to the swap disk. */
//메모리에서 디스크로 내용을 복사 - anon_page를 swap disk 로 옮김
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	// if (page == NULL){
	// 	return false;
	// }
	struct list_elem *e;
	struct sector *se;
;
	lock_acquire(&anon_lock);
	for (e = list_begin(&swap_table); e != list_end(&swap_table); e = list_next(e)){
		se = list_entry(e, struct sector, swap_elem);
		if (se->page == NULL && se->occupied == false){

			for (int i = 0 ; i < (PGSIZE / DISK_SECTOR_SIZE); i ++){
				disk_write(swap_disk, se->slot * (PGSIZE / DISK_SECTOR_SIZE) + i , page->va + DISK_SECTOR_SIZE * i );
			}

			anon_page->slot = se->slot;
			se->page = page;
			se->occupied = true;

			//unlink
			page->frame->page = NULL;
			page->frame = NULL; 
			
			pml4_clear_page(thread_current()->pml4, page->va);

			lock_release(&anon_lock);
			return true; 
		}
	}
	//디스크에 비어있는 공간이 없으면
	lock_release(&anon_lock);
	PANIC("No more free slot in disk!\n");
}
/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;

	// anonymous page에 의해 유지되던 리소스를 해제합니다.
	// page struct를 명시적으로 해제할 필요는 없으며, 호출자가 이를 수행해야 합니다.
	struct list_elem *e;
	struct sector *se;

	// 차지하던 slot 반환
	lock_acquire(&anon_lock);
	for (e = list_begin(&swap_table); e != list_end(&swap_table); e = list_next(e))
	{
		se = list_entry(e, struct sector, swap_elem);
		if (se->slot == anon_page->slot)
		{
			se->page = NULL;
			break;
		}
	}
	lock_release(&anon_lock);
}
