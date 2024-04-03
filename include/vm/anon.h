#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"
#include "devices/disk.h"

struct page;
enum vm_type;



struct anon_page {
    // struct list_elem swap_elem;
    // bool is_swapped;
    // void* va;
    // struct page* page;
    uint32_t slot; 
};

void vm_anon_init (void);
bool anon_initializer (struct page *page, enum vm_type type, void *kva);



#endif
