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
}