#ifndef PAGE_H
#define PAGE_H

#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "filesys/file.h"
#include "../lib/kernel/hash.h"

struct data{
    struct file *file; 
    uint32_t ofs;
    uint32_t upage; 
    uint32_t page_read_bytes; 
    uint32_t page_zero_bytes;
    bool loaded;
    bool writable;
    uint8_t * kpage;
    bool inSwap;
    int swapIndex;
};

struct spte {   
    struct hash_elem hash_elem;   
    int key; /**< the key for ordering, can use any "comparable" type 
		a pointer for example (virtual addr of start of a page
		which can be thought of as the page number left 
		shifted by 12 bits) 
	     */

    struct data *d; ///< The payload - could be a struct
};
bool spt_init(struct hash *spt);
bool spt_put(struct hash *spt,int page_number, struct data *d);
struct data* spt_get(struct hash *spt,int page_number);
bool add_data(struct file *file, int32_t ofs, uint32_t upage, uint32_t page_read_bytes, uint32_t page_zero_bytes, bool writable, bool loaded);

#endif