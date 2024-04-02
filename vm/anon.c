/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"

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

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get(1,1); //swapdisk 로 쓸 때 1:1

	// disk_sector_t *disk_capacity = (disk_sector_t*)malloc(4096);
	// swap_disk->disk_capacity = disk_capacity;
	// disk_sector_t size = disk_size(swap_disk);
}	

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;
	// page->anon.page = page; 

	struct anon_page *anon_page = &page->anon;
	// anon_page->page = page; 
	// anon_page->is_swapped = 0; 
	// list_push_back(&swap_table, &anon_page->swap_elem);
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;

	// disk_write(&swap_disk, );
	// struct list_elem *e;
	// for (e = list_front(&swap_table); e != list_end(&swap_table); e = list_next(e)){
	// 	if (anon_page == list_entry(e, struct anon_page, swap_elem)){
	// 		anon_page->is_swapped = 1;
	// 		// disk_write(&swap_disk, )
	// 	}
	// }
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}
