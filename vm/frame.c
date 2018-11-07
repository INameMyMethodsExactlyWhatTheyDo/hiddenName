#include "swap.h"
#include "frame.h"


struct list ft;
struct lock fl;
void frame_init(){
    list_init(&ft);
    lock_init(&fl);
}

void * get_frame(enum palloc_flags flags, struct data *d){
    lock_acquire(&fl);
    struct fte *fe = malloc(sizeof(struct fte));
    void * frame = palloc_get_page(flags);
    if(frame != NULL){
        fe ->frame = frame;
        fe ->d = d;
        add_frame(fe);
    }else{
        evict(flags);
        frame = palloc_get_page(flags);
    }
    lock_release(&fl);
    return frame;
}

void add_frame(struct fte *e){
    list_push_back(&ft, &e->elem);
}

void frame_free(void *frame){
    lock_acquire(&fl);
    struct list_elem *le;
    struct fte *e;
    for(le = list_begin(&ft); le != list_end(&ft); le = list_next(le)){
        e = list_entry(le, struct fte, elem);
        if(e ->frame == frame){
            list_remove(e);
            palloc_free_page(frame);
            free(e);
            break;
        }
    }
    lock_release(&fl);
}

void evict(){
    struct list_elem *le = list_begin(&ft);
    struct fte *fe;
    int pageNo;
    struct thread *t = thread_current();
    
    // for(le = list_begin(&ft); le != list_end(&ft); le = list_next(le)){
    //     fe = list_entry(le, struct fte, elem);
    //     if(!fe ->d -> inSwap){
    //         break;
    //     }
    // }
    le = list_next(le);
    fe = list_entry(le, struct fte, elem);

    int place = memtswap(fe -> frame);
    fe -> d -> inSwap = true;
    fe -> d -> swapIndex = place;
    fe -> d -> loaded = false;

    list_remove(&fe ->elem);
    pagedir_clear_page(t->pagedir, fe-> d -> upage);
    palloc_free_page(fe->frame);
    free(fe);
    return;

    //     struct fte *fe = list_entry(e, struct fte, elem);
    //     //if (!fte->spte->pinned){
    //     struct thread *t = thread_current();
    //     uint32_t pageNo = fe -> d ->upage;
    //     // if (pagedir_is_accessed(t->pagedir, pageNo)){
    //     //     pagedir_set_accessed(t->pagedir, pageNo, false);
    //     // }else{
    //         if (pagedir_is_dirty(t->pagedir, pageNo) || fte->spte->type == SWAP){
    //             if (fte->spte->type == MMAP) {
    //                 lock_acquire(&filesys_lock);
    //                 file_write_at(fte->spte->file, fte->frame,
    //                     fte->spte->read_bytes,
    //                     fte->spte->offset);
    //                 lock_release(&filesys_lock);
    //             }else{
    //                 fte->spte->type = SWAP;
    //                 fte->spte->swap_index = swap_out(fte->frame);
    //             }
    //         }
    //         fte->spte->is_loaded = false;
    //         list_remove(&fte->elem);
    //         pagedir_clear_page(t->pagedir, fte->spte->uva);
    //         palloc_free_page(fte->frame);
    //         free(fte);
    //         return palloc_get_page(flags);
    //     }
    // for(le = list_begin(&ft); le != list_end(&ft); le = list_next(le)){
    //     fe = list_entry(le, struct fte, elem);
    //     // pageNo = fe ->d ->upage;
    //     // if(pagedir_is_dirty(t -> pagedir, pageNo)){
    //     //     // list_remove(e);
    //     //     // palloc_free_page(frame);
    //     //     // free(e);
    //     //     // break;
            
    //     //     //skipp
    //     // }else{
            
    //     // }

    // }
    
}