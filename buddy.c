#include "buddy.h"
 #include <stdlib.h>

 #define NULL ((void *)0)
 #define MAXRANK 16
 #define PAGE_SIZE (4 * 1024)
 
 typedef struct free_area {
     void *head;
     void *tail;
     int count;
 } free_area_t;
 
 typedef struct page_block {
     void *next;
     void *prev;
     int rank;
     int is_allocated;
 } page_block_t;
 
 static void *memory_base = NULL;
 static int total_pages = 0;
 static free_area_t free_areas[MAXRANK + 1];
 static page_block_t *page_blocks = NULL;
 
 static int get_page_index(void *p) {
     if (p < memory_base || p >= memory_base + total_pages * PAGE_SIZE) {
         return -1;
     }
     long offset = (long)p - (long)memory_base;
     if (offset % PAGE_SIZE != 0) {
         return -1;
     }
     return offset / PAGE_SIZE;
 }
 
 static void *get_page_addr(int idx) {
     return memory_base + idx * PAGE_SIZE;
 }
 
 static void list_add(free_area_t *area, int page_idx) {
     page_blocks[page_idx].next = NULL;
     page_blocks[page_idx].prev = (void *)(long)area->tail;
     
     if (area->tail != NULL) {
         int tail_idx = (int)(long)area->tail;
         page_blocks[tail_idx].next = (void *)(long)page_idx;
     }
     
     area->tail = (void *)(long)page_idx;
     if (area->head == NULL) {
         area->head = (void *)(long)page_idx;
     }
     area->count++;
 }
 
 static void list_remove(free_area_t *area, int page_idx) {
     void *prev = page_blocks[page_idx].prev;
     void *next = page_blocks[page_idx].next;
     
     if (prev != NULL) {
         int prev_idx = (int)(long)prev;
         page_blocks[prev_idx].next = next;
     } else {
         area->head = next;
     }
     
     if (next != NULL) {
         int next_idx = (int)(long)next;
         page_blocks[next_idx].prev = prev;
     } else {
         area->tail = prev;
     }
     
     page_blocks[page_idx].next = NULL;
     page_blocks[page_idx].prev = NULL;
     area->count--;
 }
 
 static int get_buddy_index(int page_idx, int rank) {
     int block_size = 1 << (rank - 1);
     int buddy_idx = page_idx ^ block_size;
     return buddy_idx;
 }

 int init_page(void *p, int pgcount){
     memory_base = p;
     total_pages = pgcount;
     
     for (int i = 0; i <= MAXRANK; i++) {
         free_areas[i].head = NULL;
         free_areas[i].tail = NULL;
         free_areas[i].count = 0;
     }
     
     page_blocks = (page_block_t *)malloc(pgcount * sizeof(page_block_t));
     for (int i = 0; i < pgcount; i++) {
         page_blocks[i].next = NULL;
         page_blocks[i].prev = NULL;
         page_blocks[i].rank = 0;
         page_blocks[i].is_allocated = 0;
     }
     
     int current_page = 0;
     for (int rank = MAXRANK; rank >= 1; rank--) {
         int block_size = 1 << (rank - 1);
         while (current_page + block_size <= pgcount) {
             page_blocks[current_page].rank = rank;
             list_add(&free_areas[rank], current_page);
             current_page += block_size;
         }
     }
     
     return OK;
 }

 void *alloc_pages(int rank){
     if (rank < 1 || rank > MAXRANK) {
         return ERR_PTR(-EINVAL);
     }
     
     int current_rank = rank;
     while (current_rank <= MAXRANK && free_areas[current_rank].count == 0) {
         current_rank++;
     }
     
     if (current_rank > MAXRANK) {
         return ERR_PTR(-ENOSPC);
     }
     
     int page_idx = (int)(long)free_areas[current_rank].head;
     list_remove(&free_areas[current_rank], page_idx);
     
     while (current_rank > rank) {
         current_rank--;
         int block_size = 1 << (current_rank - 1);
         int buddy_idx = page_idx + block_size;
         page_blocks[buddy_idx].rank = current_rank;
         list_add(&free_areas[current_rank], buddy_idx);
     }
     
     page_blocks[page_idx].rank = rank;
     page_blocks[page_idx].is_allocated = 1;
     
     return get_page_addr(page_idx);
 }

 int return_pages(void *p){
     int page_idx = get_page_index(p);
     if (page_idx < 0 || !page_blocks[page_idx].is_allocated) {
         return -EINVAL;
     }
     
     page_blocks[page_idx].is_allocated = 0;
     int rank = page_blocks[page_idx].rank;
     
     while (rank < MAXRANK) {
         int buddy_idx = get_buddy_index(page_idx, rank);
         
         if (buddy_idx < 0 || buddy_idx >= total_pages) {
             break;
         }
         
         if (page_blocks[buddy_idx].is_allocated || page_blocks[buddy_idx].rank != rank) {
             break;
         }
         
         list_remove(&free_areas[rank], buddy_idx);
         
         if (page_idx > buddy_idx) {
             page_idx = buddy_idx;
         }
         
         rank++;
         page_blocks[page_idx].rank = rank;
     }
     
     list_add(&free_areas[rank], page_idx);
     
     return OK;
 }

 int query_ranks(void *p){
     int page_idx = get_page_index(p);
     if (page_idx < 0) {
         return -EINVAL;
     }
     
     return page_blocks[page_idx].rank;
 }

 int query_page_counts(int rank){
     if (rank < 1 || rank > MAXRANK) {
         return -EINVAL;
     }
     
     return free_areas[rank].count;
 }
