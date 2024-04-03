Krafton Jungle 
Week 10 Team 5 WIL

Project 3 VM:

---

기억해야 할 것:

 - 페이지는 page-aligned되어있어야 한다 - 즉, PGSIZE로 나누어 떨어져야 한다.
 - pml4안에는 page와 frame이 연결되어 있는 시점에만 그들의 연결 정보가 저장된다. 
 - 연결된 frame이 없는 page는 Supplemental Page Table에 저장함으로 관리한다. 
 - 특정 페이지에 대한 접근은 accessed-bit로 확인한다.
 - 특정 페이지에 대한 수정 여부는 dirty-bit로 확인한다. 
 

--- 

Memory Management 

---

페이지 구조체

- 여기서 페이지 구조체는 실제 pml4에 들어가는 것이 아닌, SPT에 넣어 관리할 페이지 구조체이다.

```c
struct page {
	const struct page_operations *operations;
	void *va;              /* Address in terms of user space */
	struct frame *frame;   /* Back reference for frame */

	struct hash_elem hash_elem;
	bool writable;

	union {
		struct uninit_page uninit;
		struct anon_page anon;
		struct file_page file;
#ifdef EFILESYS
		struct page_cache page_cache;
#endif
	};
};
```
SPT를 해시로 구현하기로 했기 때문에 hash_elem 을 페이지 구조체에 넣어주었다. 
추가적으로, 추후 해당 구조체가 Read-only 인지 Writable인지 확인하기 위해 boolean 형태의 writable또한 추가했다. 

페이지 구조체에는 page_operations라는 구조체가 있는데, 해당 구조체는 매크로로 이루어져있으며, 이 매크로들이 지정하는 함수는 페이지의 Type에 따라 변경된다. 

---

Supplemental Page Table을 Hash로 구현하기로 했기 떄문에, Thread구조체 내 spt에 hash를 추가해주었다. 

```c
struct supplemental_page_table {
	struct hash spt_hash;
};
```

직전 프로젝트들에서 사용했던 list와 비슷하게 해당 자료구조를 다루는 함수들이 있을것이라 생각하여 `hash.h`를 검토하여 기본 헬퍼 함수들을 구현했다. 

`hash_init`, `hash_find`, `hash_insert`를 사용하여 구현하였다. 

```c

void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	

	hash_init(&spt->spt_hash, page_hash, page_less, NULL);
}

struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	struct page *_page;

	_page = (struct page*)malloc(sizeof(struct page));
	_page->va = pg_round_down(va);

	struct hash_elem* e = NULL;

	if (!hash_empty(&spt->spt_hash)){
		e =	hash_find(&spt->spt_hash, &_page->hash_elem);
	}
	free(_page);

	if (e == NULL){
		return NULL;
	}
 
	_page = hash_entry(e, struct page, hash_elem);

	return _page;
}

bool
spt_insert_page (struct supplemental_page_table *spt,
		struct page *page) {

	struct hash_elem *e = hash_insert(&spt->spt_hash, &page->hash_elem);
	
	if (e == NULL){
		return true;
	} 
	return false;
}
```

---

Frame Managment

--- 

해당 부분에서 구현하는 함수들 또한 추후 프레임들을 효율적으로 적절하게 사용하기 위함이다. 

우선 전역으로 list `frame_list`를 선언하여 `vm_init`함수에서 `list_init`해주었다. 
이후, 리스트로 관리하기 위해 frame 구조체에도 list_elem `frame_elem`을 추가해주었다. 

```c 
struct frame {
	void *kva;
	struct page *page;

    struct list_elem frame_elem;
};
```
이 부분에서도 추후 사용할 
- `get_frame` - 빈 (사용 가능한) 프레임을 가져와주는 함수
- `vm_do_claim_page` - 페이지와 프레임을 서로 link해주는 함수
- `vm_claim_page` - vm_do_claim_page의 wrapper함수
들을 구현해주었다. 

```c
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;

	void* kva = palloc_get_page(PAL_USER);
	if (kva == NULL){	
		PANIC("todo");
	} 
	frame = (struct frame*)malloc(sizeof(struct frame));
	frame->kva = kva; 
	frame->page = NULL; 

	lock_acquire(&page_lock);
	list_push_back(&frame_list, &frame->frame_elem);
	lock_release(&page_lock);


	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}
```
`vm_get_frame`함수를 살펴보면, palloc_get_page로 커널 가상 메모리의 USER POOL에서 물리 메모리로 매핑된 페이지 크기의 메모리 공간의 주소를 받아오고
해당 프레임을 추후 관리를 위해 이전에 만든 frame_list에 삽입해준다. 

---

Anonymous Page 

--- 



