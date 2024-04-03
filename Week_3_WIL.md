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


---

Stack Growth

---

VM파트에서의 사용자 가상 메모리는 가변 공간을 갖는다. 
스택을 늘릴 필요가 있는 상황에서만 스택을 늘려야 하며, 핀토스에는 스택이 늘어날 수 있는 공간의 제한을 1MB로 한정한다. 

```c
bool
vm_try_handle_fault (struct intr_frame *f, void *addr,
		bool user, bool write, bool not_present) {

	struct supplemental_page_table *spt = &thread_current ()->spt;
	struct page *page = NULL;

	if (is_kernel_vaddr(addr) || addr == NULL){

		return false;
	}

	if(not_present){
		struct thread* t = thread_current();
		
		void* ptr;
		if (user){
			ptr = f->rsp;
		}
		if (!user){
			ptr = t->stack_pointer;
		}

		if ((USER_STACK - (1 << 20) <= ptr && addr < USER_STACK && USER_STACK - (1 <<20) < addr)){
			if (ptr - sizeof(void*) == addr || ptr <= addr){
				vm_stack_growth(pg_round_down(addr));	
			}
		}

.
.
.
}

```
Page Fault가 발생하면, 핀토스는 fault가 일어난 주소로의 접근이 Stack Growth로 해결 가능한 fault인지를 판단해야 한다. 
이에 따라 조건을 설정하여, 해당 조건에 부합하면 `vm_stack_growth`를 호출하여 스택을 늘려준다. 

```c
static void
vm_stack_growth (void *addr) {
    vm_alloc_page(VM_ANON | VM_MARKER_0, addr, true)	
}
```

---

Memory Mapped Files

---

Memory-Mapped (File-Backed) 페이지를 다루는 부분이다. 
Anon 페이지와는 다르게, 해당 페이지들은 디스크에 매핑되어있는 파일이 존재하며, 페이지 폴트 발생 시 해당 파일의 내용이 메모리에 적재된다. 

```c
//System Call Handler
	// r10다음에 r9가 아니라 r8이다!
	case SYS_MMAP:
		f->R.rax = mmap(f->R.rdi, f->R.rsi, f->R.rdx, f->R.r10, f->R.r8);
		break;

	case SYS_MUNMAP:
		munmap(f->R.rdi);
		break;
```
`syscall_handler` 에 mmap과 munmap을 추가해주어 해당 시스템 콜들을 핸들링 할 수 있도록 해준다. 

```c
void *
mmap (void *addr, size_t length, int writable, int fd, off_t offset) {

	if(addr == 0 || addr == NULL) 
		return NULL;	
	
	if (addr != pg_round_down(addr) || offset != pg_round_down(offset) || is_kernel_vaddr(addr)){
		return NULL;
	}

	if(offset % PGSIZE != 0){
		return NULL; 
	}

	if (addr - length >= KERN_BASE ){
		return NULL; 
	}

	if(!is_user_vaddr(pg_round_down(addr)) || !is_user_vaddr(pg_round_up(addr))){
		return NULL;
	}

	if (length <= 0){
		return NULL;
	}

	if (spt_find_page(&thread_current()->spt, addr)){
		return NULL;
	}

	if (fd == 0 || fd == 1){
		exit(-1);
	}
	
	struct file_descriptor *mmap_fd= find_file_descriptor(fd);
	if (mmap_fd->file == NULL){
		return NULL;
	}
	return do_mmap(addr, length, writable, mmap_fd->file, offset);
}
```
위와 같이, gitbook에 명시되어있는 조건들을 예외처리 하고, 추가적으로 test case들을 확인하며 조건들을 추가해주었다. 

```c
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

	while (read_bytes > 0 || zero_bytes > 0) {

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

		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		addr += PGSIZE;
		offset += page_read_bytes;
	}
	return return_address;
}
```

`do_mmap`함수는 process.c에 있는 `load_segment`함수와 유사하다 (같은 기능을 수행하기 때문).

`do_munmap`함수는 `do_mmap`의 역연산이라고 생각하면 간단하다. 
주의해야 할 점은, 변경 사항이 있을 시 반복문을 돌아 file_backed 타입의 `destroy` 오퍼레이션을 호출해주어  `disk_write_at`을 통해 이를 다시 디스크로 write해준다. 

---

Swap In/Out

---

스왑 디스크를 사용하여 페이지들에 대한 일종의 버퍼를 만들어두는 것이 해당 부분에서의 과제이다. 

우선 Swap Table을 이 부분에서도 역시나 리스트로 구현하였다. 

```c
struct list swap_table;

void
vm_anon_init (void) {
	list_init(&swap_table);
	lock_init(&anon_lock);

	swap_disk = disk_get(1,1); 

	disk_sector_t swap_size = disk_size(swap_disk) / (PGSIZE / DISK_SECTOR_SIZE);

	for (int i = 0; i < swap_size; i++)
	{
		struct sector *se = (struct sector *)malloc(sizeof(struct sector));
		
		se->page = NULL;
		se->slot = i;
		se->occupied = false;

		lock_acquire(&anon_lock);
		list_push_back(&swap_table, &se->swap_elem);
		lock_release(&anon_lock);
	}
}	

```
우선 `disk_get`을 사용해서 스왑 디스크를 선언해준다. 
디스크는 Sector라는 512바이트 공간으로 분리되어 있는데, 섹터 단위로 읽기와 쓰기 등의 연산을 처리해야 한다. 
섹터들을 관리하기 위해 구조체를 만들어주었다. 

추가적으로, 특정 페이지가 어떤 섹터에 들어있는지 확인하기 위해 anon_page구조체에 slot항목을 추가해주었다. 

```c
struct sector{
	struct page *page;
	uint32_t slot;
	struct list_elem swap_elem; 
	bool occupied;
};

struct anon_page {
    uint32_t slot; 
};

```

```c
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;

	struct list_elem *e;
	struct sector *se;

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

	lock_release(&anon_lock);
	PANIC("No more free slot in disk!\n");
}

static bool
anon_swap_in (struct page *page, void *kva) {

	struct anon_page *anon_page = &page->anon;

	struct list_elem*e; 
	struct sector *se; 

	lock_acquire(&anon_lock);
	for (e = list_begin(&swap_table); e != list_end(&swap_table); e = list_next(e)){
		se = list_entry(e, struct sector, swap_elem);
		if (se->slot == anon_page->slot){

			for (int i = 0; i < (PGSIZE / DISK_SECTOR_SIZE); i ++){
				disk_read(swap_disk, anon_page->slot * (PGSIZE / DISK_SECTOR_SIZE) + i, kva + (DISK_SECTOR_SIZE*i) );
			}

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


```
`anon_swap_in`과 `anon_swap_out`은 swap disk를 사용하여 해당 공간을 저장소로 사용하여 페이지를 보관해두고 꺼내오는 함수들이다. 
앞서 말했듯이, 한 개의 섹터는 512바이트이기 떄문에, 디스크 안에서의 한 페이지는 8개의 섹터로 이루어져 있다. 
이런 이유로, for 문을 총 8번 돌아 각 섹터에 swap_out이면 write, swap_in 이면 read를 실행하여 스왑 디스크로 페이지를 쓰고, 읽는다. 

