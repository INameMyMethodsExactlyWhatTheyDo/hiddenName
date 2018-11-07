#include "page.h"

/** 
 * @brief We pass this as a function pointer to routines in the hash api that work with ordering
 * @return Returns a hash value for page p.
 */
unsigned
page_hash (const struct hash_elem *p_, void *aux){
  const struct spte *p = hash_entry (p_, struct spte, hash_elem);
  return hash_bytes (&p->key, sizeof(p->key));
}

/**
 * @brief  Returns true if foo a precedes foo b. 
 */
bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux){
  const struct spte *a = hash_entry (a_, struct spte, hash_elem);
  const struct spte *b = hash_entry (b_, struct spte, hash_elem);

  return a->key < b->key;
}

bool spt_init(struct hash *spt){
    hash_init (spt, page_hash, page_less, NULL);
    return 1;
}

bool spt_put(struct hash *spt, int page_number, struct data *d){
    struct spte *e = calloc(1, sizeof(struct spte));
    e ->key = page_number;
    e ->d = d;
    if (hash_insert(spt, &e ->hash_elem) == NULL){
        return 1;
    }
    return 0;
}
struct data* spt_get(struct hash *spt, int page_number){
    static test;
    if(test == NULL){
        test = 0;
    }else{
        test += 1;
    }
    struct hash_elem *e;
    struct spte scratch;
    struct hash_iterator i;
    volatile struct data *d;
    volatile int key;
    scratch.key = pg_round_down(page_number);
    hash_first (&i, spt);
    while (hash_next (&i))
    {
        struct spte *f = hash_entry(hash_cur (&i), struct spte, hash_elem);
        key = f ->key;
        d = f ->d;
    }
    e = hash_find(spt, &scratch.hash_elem);
    if (e != NULL){
	    struct spte *result = hash_entry(e, struct spte, hash_elem);
	    // printf("Value for key(%d) is %d\n",result->key, result->value);
        return result ->d;
    }else{
        return NULL;
    }
}

bool add_data(struct file *file, int32_t ofs, uint32_t upage, uint32_t page_read_bytes, uint32_t page_zero_bytes, bool writable, bool loaded){
    struct data *d = (struct data*)malloc(sizeof(struct data));
    d ->file = file;
    d ->ofs = ofs;
    d ->upage = upage;
    d ->page_read_bytes = page_read_bytes;
    d ->page_zero_bytes = page_zero_bytes;
    d ->writable = writable;
    d ->loaded = loaded;
    d ->inSwap = false;

    return(spt_put(&thread_current() -> spt, upage, d));
}