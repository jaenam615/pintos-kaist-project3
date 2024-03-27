/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "list.h"
#include "include/threads/vaddr.h"
#include "include/lib/kernel/hash.h"
#include "include/threads/mmu.h"

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
	list_init(&frame_list);
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);
/* implementation - pongpongie */
void page_table_kill(struct hash_elem *h, void* aux UNUSED);
static unsigned page_hash (const struct hash_elem *p_, void *aux UNUSED);
static bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	//upage라는 주소를 갖는 페이지는 존재하지 않아야 한다 - 여기서 upage는 새로 만들 페이지의 user virtual address이다
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		//IMPLEMENTATION
		//빈 페이지 동적 할당
		struct page *_page = (struct page*)malloc(PGSIZE);

		if (_page == NULL){
			return false;
		}
		
		//이후 사용될 페이지 이니셜라이저 만들기
		typedef bool (*page_initializer)(struct page *, enum vm_type, void *kva);
		page_initializer init_func = NULL; 
		//페이지 타입에 맞게 이니셜라이져 init_func를 설정해준다
		if (type == VM_ANON){
			init_func = anon_initializer;
		} else if (type == VM_FILE){
			init_func = file_backed_initializer;
		} 
		uninit_new(_page, upage, init, type, aux, init_func);
		_page->writable = writable;
		/* TODO: Insert the page into the spt. */
		// lock_acquire(&page_lock);
		bool result = spt_insert_page(&spt,&_page);
		// lock_release(&page_lock);
		return result;
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	struct page *_page = NULL;
	// /* TODO: Fill this function. */
	
	//SPT에 존재하는 페이지를 찾아줌
	//더미 페이지를 할당하고 해당 페이지의 VA를 우리가 찾고자 하는 VA로 설정
	_page = (struct page*)malloc(PGSIZE);
	_page = NULL;
	_page->va = pg_round_down(va);

	struct hash_elem* e;

	//더미 페이지의 hash_elem을 사용해서 이와 같은 element를 spt에서 받아온다
	e =	hash_find(&spt->spt_hash, &_page->hash_elem);
	//실패 시 NULL을 반환하며, 이 경우에는 이전에 할당한 페이지의 메모리를 반환하고 NULL을 반환한다
	if (e == NULL){
		free(_page);
		return NULL;
	}
	// 성공시에도 우선 더미페이지의 메모리는 반환해야 한다(이미 이전에 할당하여 SPT에 넣어준 페이지가 대입되기 때문이다)
	free(_page);
	//LIST_ENTRY와 비슷한 HASH_ENTRY를 사용해 우리가 찾은 hash_elem이 속해있는 page를 얻고, 이를 반환한다. 
	_page = hash_entry(e, struct page, hash_elem);

	return _page;
}


/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt,
		struct page *page) {
	/* TODO: Fill this function. */
	// IMPLEMENTATION
	//validation	
	//hash_insert가 성공적으로 삽입에 성공하면 NULL을 반환하기 떄문에, 반환값이 NULL이면 true를 반환한다
	if (hash_insert(&spt->spt_hash, &page->hash_elem) == NULL){
		return true;
	}
	//실패시에는 false를 반환한다
	return false;
}


void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */
	//퇴출정책에 의거하여 퇴출시킬 프레임을 반환한다
	//일단은 FIFO로 구현
	struct list_elem *e = list_pop_front(&frame_list);
	victim = list_entry(e, struct frame, frame_elem);
	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	//vm_get_victim을 통해 받아온 퇴출 프레임이 NULL이 아니라면 (정상적으로 구해왔다면) 해당 프레임을 swap_out하고 반환
	if (victim != NULL){
		//swap_out의 대상은 프레임이 아니라 페이지이다
		swap_out(victim->page);
		return victim;
	}
	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */
	//IMPLEMENTATION

	//allocate frame
	frame = (struct frame*)malloc(sizeof(struct frame));
	//PAL_USER를 사용해서 실제 물리메모리의 USER_POOL에서부터 물리 프레임으로 정의된 Kernel Virtual Address를 반환받는다 
	void* _kva = palloc_get_page(PAL_USER);
	//실패 시에는 SWAP을 해야하기 때문에 evict frame으로 일단 넣어둔다
	if (_kva == NULL){	
		PANIC("todo");
		frame = vm_evict_frame();
	}

	//initialize its members
	//프레임의 원소들을 초기화시켜주고, frame_list에 넣어서 이후 관리가 수월하도록 한다
	frame->kva = _kva;
	frame->page = NULL; 
	list_push_back(&frame_list, &frame->frame_elem);

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}


/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr,
		bool user, bool write, bool not_present UNUSED) {

	struct supplemental_page_table *spt = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	if (is_kernel_vaddr(addr) || !is_user_vaddr(addr)){
		return false;
	}
	// page = pml4_get_page(addr);

	page = (struct page*)malloc(PGSIZE);
	// if (write == true && !is_writable(pml4_get_page(thread_current()->pml4, page))){
	// 	return false;
	// }
	if (page == NULL){
		return false; 
	}

	//round down 안할 시 ofs오류 
	page->va = pg_round_down(addr);
	// page->va = palloc_get_page(PAL_USER);
	// page->va = addr;

	return vm_do_claim_page (page);
}


/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va) {
	struct page *page = NULL;
	/* TODO: Fill this function */
	//IMPLEMENTATION
	// do_claim_page의 wrapper함수, 
	// 물리 프레임과 매핑할 페이지를 spt에서 찾아 인자로 전달해준다.
	page = spt_find_page(&thread_current()->spt,va);
	if (page == NULL){
		return false; 
	}
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	//get frame
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;
	
	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	
	//IMPLEMENTATION
	//mapping from virtual address to a physical address in the current process' page table
	struct thread *t = thread_current();
	
	//미리 연결된 KVA(물리 주소)가 없는지 확인 - 연결되어 있을 시 NULL 반환
	if (pml4_get_page(t->pml4, page->va) != NULL){
		return false; 
	}
	//페이지와(User Virtual Page Address) 프레임(Physical Frame Address - Kernel Virtual Address)를 pml4에서 매핑한다
	if (!pml4_set_page(t->pml4 , pg_round_down(page->va) , pg_round_down(frame->kva), page->writable)){
		return false;
	}

	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	
	// IMPLEMENTATION
	//해시함수 사용하여 초기화
	hash_init(&spt->spt_hash, page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst */
// bool
// supplemental_page_table_copy (struct supplemental_page_table *dst,
// 		struct supplemental_page_table *src) {
	
// 	//IMPLEMENTATION
// 	//get first entry
// 	struct hash_iterator i;
// 	hash_first(&i, &src->spt_hash);
// 	bool final = false;
// 	//iterate through entries and copy them 
// 	while (hash_next (&i))
// 	{
// 		struct page *src_p = hash_entry (hash_cur (&i), struct page, hash_elem);
// 		struct page *dst_p = (struct page*)malloc(PGSIZE);
// 		dst_p->va = src_p->va;
	
// 		enum vm_type type = page_get_type(src_p);
// 		if (type == VM_UNINIT){
// 			final = vm_alloc_page_with_initializer(type, dst_p, src_p->writable, src_p->uninit.init, NULL);
// 		}else if (type == VM_ANON){
// 			final = vm_alloc_page_with_initializer(type, dst_p, src_p->writable, anon_initializer, NULL);
// 		} else if (type == VM_FILE) {
// 			struct lazy *aux = (struct lazy*)malloc(sizeof(struct lazy));
// 			aux->file = src_p->file.file;
// 			aux->ofs = src_p->file.ofs;
// 			aux->read_bytes = src_p->file.read_bytes; 
// 			aux->zero_bytes = src_p->file.zero_bytes;
// 			final = vm_alloc_page_with_initializer(type, dst_p, src_p->writable, file_backed_initializer, aux);
// 		}
// 	}
// 	return final; 
// }

bool supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED, struct supplemental_page_table *src UNUSED) {
    struct hash_iterator i;
    hash_first (&i, &src->spt_hash);
    while (hash_next (&i)) {	// src의 각각의 페이지를 반복문을 통해 복사
        struct page *parent_page = hash_entry (hash_cur (&i), struct page, hash_elem);   // 현재 해시 테이블의 element 리턴
        enum vm_type type = page_get_type(parent_page);		// 부모 페이지의 type
        void *upage = parent_page->va;						// 부모 페이지의 가상 주소
        bool writable = parent_page->writable;				// 부모 페이지의 쓰기 가능 여부
        vm_initializer *init = parent_page->uninit.init;	// 부모의 초기화되지 않은 페이지들 할당 위해 
        void* aux = parent_page->uninit.aux;

        if(parent_page->operations->type == VM_UNINIT) {	// 부모 타입이 uninit인 경우
            if(!vm_alloc_page_with_initializer(type, upage, writable, init, aux)) // 부모의 타입, 부모의 페이지 va, 부모의 writable, 부모의 uninit.init, 부모의 aux (container)
                return false;
        }
        else {
            if(!vm_alloc_page(type, upage, writable))
                return false;
            if(!vm_claim_page(upage))
                return false;
			struct page* child_page = spt_find_page(dst, upage);
            memcpy(child_page->frame->kva, parent_page->frame->kva, PGSIZE);
        }
    }
    return true;
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	hash_destroy(&spt->spt_hash, page_table_kill);
}

//IMPLEMENTATION

void page_table_kill(struct hash_elem *h, void* aux UNUSED){
	const struct page *_page = hash_entry(h, struct page, hash_elem);
	destroy(_page);
	free(_page);
}

unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED) {
  const struct page *p = hash_entry (p_, struct page, hash_elem);
  return hash_bytes (&p->va, sizeof p->va);
}

bool
page_less (const struct hash_elem *a_,
           const struct hash_elem *b_, void *aux UNUSED) {
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);

  return a->va < b->va;
}