#include "swap.h"

struct lock sl;
struct block *sb;
struct bitmap *st;
int SECTORS = 8;
void swap_init(void){
    lock_init(&sl);
    sb = block_get_role(BLOCK_SWAP);
    int size = block_size(sb);
    size /= SECTORS;
    st = bitmap_create(size);
    bitmap_set_all(st, 0);
}

int memtswap(void* frame){
    lock_acquire(&sl);
    int freeIndex = bitmap_scan_and_flip(st, 0, 1, 0);
    if(freeIndex == BITMAP_ERROR){
        PANIC("SWAP FULL");
    }
    for(int i = 0; i < SECTORS; i++){
        block_write(sb, freeIndex * SECTORS + i, (uint8_t*)frame + SECTORS * i);
    }
    lock_release(&sl);
    return freeIndex;
}

void swaptmem(void* frame, int index){
    lock_acquire(&sl);
    for(int i = 0; i < SECTORS; i++){
        block_read(sb, index * SECTORS + i, (uint8_t)frame + i * BLOCK_SECTOR_SIZE);
    }
    lock_release(&sl);
    return true;
}
