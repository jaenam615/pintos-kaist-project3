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
static unsigned page_hash (const struct hash_elem *p_, void *aux UNUSED);
static bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);
struct list frame_table; // TODO: frame table 초기화 해주어야 함

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
/* 
 * initializer를 사용하여 보류 중인 페이지 객체를 만든다.
 * 페이지를 만들고 싶으면, 이 함수를 거쳐서 만들거나 'vm_alloc_page'를 통해 만들어야 한다.
 */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
        
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */

		/* TODO: Insert the page into the spt. */

        /* 페이지를 만들고, VM type에 따른 initializer를 가져온다.
         * uninit_new 함수를 통해 uninit page 구조체를 만든다.
         * uninit_new를 호출하고 난 다음에 필드를 수정해야 한다.
         * 초기화되지 않은 주어진 type의 페이지를 생성한다.
         */ 

        /* 페이지를 spt에 삽입하라 */
        struct page *p;
        p = palloc_get_page(PAL_USER);
        if (p == NULL)
            return false;
        switch (type)
        {
        case VM_ANON:
            uninit_new(p, upage, init, type, aux, anon_initializer);
            break;
        case VM_FILE:
            uninit_new(p, upage, init, type, aux, file_backed_initializer);
            break;
        default:
            return false;
        }
        spt_insert_page(spt, p);
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	struct page *page = NULL;
    struct hash_elem *element;

    // implementation - pongpongie
    page->va = va;
    element = hash_find(&spt->spt_hash, &page->hash_elem);
    if (element != NULL){
        page = hash_entry(element, struct page, hash_elem);
    }
    return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt,
		struct page *page) {
    struct page *p;
    struct page_elem *element;
    bool succ = true;

    // implementation - pongpongie    
    // TODO: 인자로 주어진 spt에 페이지 구조체를 삽입한다.
    // TODO: 가상 주소가 spt에 존재하는지 확인해야한다.
    if (!spt_find_page(&spt->spt_hash, page->va))
    {
        succ = false;
    }
    hash_insert(&spt->spt_hash, &page->hash_elem);
	return succ;
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
    // implementation - pongpongie
    list_pop_front(&frame_table);
	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim = vm_get_victim ();

	/* TODO: swap out the victim and return the evicted frame. */
    // implementation - pongpongie
    if (swap_out(victim->page) == true);
        return victim;
	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
/*
 * palloc()을 하고 프레임을 가져온다. 만약 사용 가능한 페이지가 없으면, 페이지를 쫓아내고 반환한다.
 * 항상 유효한 주소를 리턴해야한다. 그 말은 곧, user pool이 꽉 차있으면 이 함수는 프레임을 쫓아내고
 * 사용 가능한 메모리 공간을 가져온다는 말이다.
 */
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	// implementation - pongpongie

    frame->kva = NULL;  // 프레임 구조체 멤버 초기화
    frame->page = NULL;

    frame->kva = palloc_get_page(PAL_USER);  // palloc으로 가져온 페이지에 프레임 할당
    if (frame->kva == NULL)
    {
        vm_evict_frame();  // 페이지 쫓아내기
    }
    list_push_back(&frame_table, frame);  // 프레임 테이블에 프레임 추가
    
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
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */

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
    
    // implementation - pongpongi
    // 인자로 주어진 va에 페이지 할당
    // 그 페이지를 인자로 갖는 vm_do_claim_page 호출
    va = palloc_get_page(PAL_USER);
    page->va = va;
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
/* 페이지를 요정하고 mmu를 세팅한다. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
    // implementation - pongpongie
    // MMU 세팅; 가상 주소와 물리 주소를 매핑한 정보를 페이지 테이블에 추가하기
    struct thread *t = thread_current();
    frame->kva = pml4_get_page(t->pml4, page);
    if (frame->kva == NULL)
        return false;
    if (!pml4_set_page(t->pml4, page, frame->kva, page->writable))
        return false;
	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt /* UNUSED */) {
    // implementation - pongpongie
    hash_init(&spt->spt_hash, page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst,
	struct supplemental_page_table *src) {
    // implementation - pongpongie

    struct hash_iterator *i;
    struct page *new_page;
    hash_first (&i, &src->spt_hash);
    while (hash_next (&i))
    {
        struct page *page = hash_entry (hash_cur (&i), struct page, hash_elem);

        if (page_get_type(page) == VM_FILE)
        {
            new_page->va = palloc_get_page(PAl_USER);
            vm_alloc_page(VM_FILE, &new_page->va, true);

        }
        if (page_get_type(page) == VM_ANON)
        {
            new_page->va = palloc_get_page(PAl_USER);
            vm_alloc_page(VM_ANON, &new_page->va, true);
        }
        else {
            new_page = palloc_get_page(PAl_USER);
            page->writable = new_page->writable;
            page->uninit.init = new_page->uninit.init;
            page->uninit.type = new_page->uninit.type;
            page->uninit.aux = new_page->uninit.aux;
            page->uninit.page_initializer = new_page->uninit.page_initializer;
        }
        
    }
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */

    // implementation - pongpongie
    hash_destroy(&spt->spt_hash, hash_free);
}

/* implementation - pongpongie */

hash_action_func hash_free (struct hash_elem *e, void *aux){
    
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

// /* Returns the page containing the given virtual address, or a null pointer if no such page exists. */
// struct page *
// page_lookup (const void *address) {
//   struct page p;
//   struct hash_elem *e;

//   p.addr = address;
//   e = hash_find (&pages, &p.hash_elem);
//   return e != NULL ? hash_entry (e, struct page, hash_elem) : NULL;
// }