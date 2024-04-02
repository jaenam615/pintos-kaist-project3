/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "list.h"
#include "include/threads/vaddr.h"
#include "include/lib/kernel/hash.h"
#include "include/threads/mmu.h"

/* implementation - pongpongie */
static unsigned page_hash (const struct hash_elem *p_, void *aux UNUSED);
static bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);
void page_table_kill(struct hash_elem *h, void* aux UNUSED);
void hash_free (struct hash_elem *e, void *aux);
struct list frame_table;

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
    list_init(&frame_table);
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
        bool (*initializer)(struct page *, enum vm_type, void *);
        p = (struct page *)malloc(sizeof(struct page));
        if (p == NULL)
            return false;
        switch (type)
        {
        case VM_ANON:
            initializer = anon_initializer;
            break;
        case VM_FILE:
            initializer = file_backed_initializer;
            break;
        }
        uninit_new(p, upage, init, type, aux, initializer);
        p->writable = writable;
        return spt_insert_page(spt, p);
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
    page = (struct page *)malloc(sizeof(struct page));
    page->va = pg_round_down(va);  // 왜 round_down?
    element = hash_find(&spt->spt_hash, &page->hash_elem);
    // element 없으면 null처리
    return element != NULL ? hash_entry(element, struct page, hash_elem) : NULL;
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
    
    element = hash_insert(&spt->spt_hash, &page->hash_elem);
    if (element != NULL)
    {
        succ = false;
    }
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
    struct list_elem *element;
    /* TODO: The policy for eviction is up to you. */
    // implementation - pongpongie
    element = list_pop_front(&frame_table);
    victim = list_entry(element, struct frame, frame_elem);
    return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim = vm_get_victim ();

	/* TODO: swap out the victim and return the evicted frame. */
    // implementation - pongpongie
    if (!list_empty(&frame_table))
    {
        if (swap_out(victim->page) == true);
        {
            return victim;
        }    
    }
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

    // frame->kva = NULL; // 구조체 멤버 초기화할 필요 없음
    // frame->page = NULL;

    frame = (struct frame *)malloc(sizeof(struct frame));
    frame->kva = palloc_get_page(PAL_USER);  // palloc으로 가져온 페이지에 프레임 할당
    if (frame->kva == NULL)
    {
        frame = vm_evict_frame();  // 페이지 쫓아내기
        frame->page = NULL;
        return frame;
        // PANIC("todo");
    }

    list_push_back(&frame_table, &frame->frame_elem);  // 프레임 테이블에 프레임 추가
    frame->page = NULL;
    
    ASSERT (frame != NULL);
    ASSERT (frame->page == NULL);
    return frame;
}

/* Growing the stack. */
// 매개변수로 들어온 addr에 대한 조건들을 다 체크하고 호출될 것이기 때문에, 늘려주기만 하면 될듯?
// rsp를 어떻게 가져올 것인가? - stack_pointer를 thread 구조체에 추가함
static void
vm_stack_growth (void *addr) {
    // implementation - pongpongie

    vm_alloc_page(VM_ANON, pg_round_down(addr), true);
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f, void *addr,
        bool user, bool write, bool not_present) {
    struct supplemental_page_table *spt = &thread_current ()->spt;
    struct page *page = NULL;
    /* TODO: Validate the fault */
    // implementation - pongpongie
    // TODO: 스택 증가를 확인하는 로직 추가
    // TODO: 스택을 증가시켜도 되는 경우인지 아닌지 확인
    // TODO: 만약 스택을 증가시켜도 되는 경우라면, addr로 vm_stack_growth 호출
    if (addr == NULL)
    {
        return false;
    }

    if (is_kernel_vaddr(addr))
    {
        return false;
    }
    
    // physical page가 존재하지 않는 경우
    if (not_present)
    {
        struct thread *t = thread_current();
        void *rsp;
        if (user)  // page fault가 user mode에서 발생한 경우
        {
            rsp = f->rsp; // rsp는 매개변수인 intr_frame 구조체에서 호출하면 됨
        }

        if (!user) // page fault가 kernel mode에서 발생한 경우
        {
            rsp = t->stack_pointer; // rsp는 유저 모드에서 커널 모드로 전환 이전의 값을 저장한 stack_pointer을 사용해야 유저 스택의 값을 가리킬 수 있음
        }

        if (addr <= USER_STACK && USER_STACK - (1 << 20) <= addr)
        {
            if (addr >= rsp)
            {
                vm_stack_growth(addr);
                return true;
            }
            if (rsp - 8 == addr)
            {
                vm_stack_growth(addr);
                return true;
            }
            if (addr < rsp)
            {
                return false;
            }
        }
        else if (addr <= USER_STACK - (1 << 20))
        {
            return false;
        }
        page = spt_find_page(spt, addr);
        if (page == NULL)
        {
            return false;
        }
        if (!page->writable && write)   // 쓰기가능하지 않은데 쓰려고 한 경우
        {
            return false;
        }
        return vm_do_claim_page(page) ? true : false;
    }
    return false;
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
    page = spt_find_page(&thread_current()->spt, va);
    if (page == NULL)
        return false;
    return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
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
    
    if (pml4_get_page(t->pml4, page->va) != NULL)
        return false;
    if (!pml4_set_page(t->pml4, pg_round_down(page->va), frame->kva, page->writable))
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
    struct hash_iterator i; // *i 아닌 그냥 i
    hash_first (&i, &src->spt_hash);
    while (hash_next (&i))
    { 
        struct page *src_page = hash_entry (hash_cur (&i), struct page, hash_elem);
        enum vm_type src_type = src_page->operations->type; // page_get_type 왜 안될까? - 현재 페이지의 타입이 아닌 초기화 되고 난 후의 타입을 반환하기 때문인듯.
        // enum vm_type src_type = page_get_type(src_page);
        // printf("page get : %d \n\n", page_get_type(src_page));

        // printf("type = %d \n\n", src_page->operations->type);
        bool src_writable = src_page->writable;
        void *upage_va = src_page->va;
        
        if (src_type == VM_UNINIT)
        {
            vm_initializer *src_init = src_page->uninit.init;
            void *src_aux = src_page->uninit.aux;
            vm_alloc_page_with_initializer(VM_ANON, upage_va, src_writable, src_init, src_aux);
            continue;
        }
        if (!vm_alloc_page(src_type, upage_va, src_writable))
        {
            return false;
        }
        if (!vm_claim_page(upage_va))
        {
            return false;
        }
        struct page *dst_page = spt_find_page(dst, upage_va);
        memcpy(dst_page->frame->kva, src_page->frame->kva, PGSIZE);
    }
    return true;
}

void
supplemental_page_table_kill (struct supplemental_page_table *spt) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */

    // implementation - pongpongie
    hash_clear(&spt->spt_hash, page_table_kill);
}

/* implementation - pongpongie */

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

/* vm.c: Generic interface for virtual memory objects. */