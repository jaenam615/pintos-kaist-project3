#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"
struct page;
enum vm_type;

//implementation
// struct list swap_table;

// struct swap_table_entry{
//     struct list_elem swap_elem;
//     bool is_swapped;
//     void* va;
// };

struct anon_page {
    // struct swap_table_entry *ste;
};

void vm_anon_init (void);
bool anon_initializer (struct page *page, enum vm_type type, void *kva);



#endif
