#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "cache.h"

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;

int cache_create(int num_entries) {
  // Conditions in which cache cannot be created.
  if (cache_size!=0 || num_entries<2 || num_entries>4096) {
    return -1;
  }
  else {
    // Dynamically allocating memory for num_entries number of entries.
    cache = calloc(num_entries, sizeof(cache_entry_t));
    cache_size = num_entries;
    return 1;
  }
}

int cache_destroy(void) {
  // Cache can only be destroyed if it has content in it.
  if (cache_size==0) {
    return -1;
  }
  else {
    // Freeing the dynamically allocated memory.
    free(cache);
    cache = NULL;
    cache_size = 0;
    return 1;
  }
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
  // Conditions in which lookup to the cache should fail.
  if (cache_size==0 || buf==NULL) {
    return -1;
  }
  else {
    num_queries += 1;
    for (int i=0; i<cache_size; i++) {
      // Looking for the block identified by block_num and disk_num.
      if (cache[i].disk_num == disk_num && cache[i].block_num == block_num) {
	if (cache[i].valid == true) {
	  // Copying its contents into buf.
	  memcpy(buf, cache[i].block, JBOD_BLOCK_SIZE);
	  num_hits += 1;
	  clock += 1;
	  cache[i].access_time = clock;
	  return 1;
        }
      }
    }
  }
  return -1;
}

void cache_update(int disk_num, int block_num, const uint8_t *buf) {
  for (int i=0; i<cache_size; i++) {
    // Checking if the entry exists in the cache.
    if (cache[i].disk_num == disk_num && cache[i].block_num == block_num) {
      // Updating its block content with new data from buf.
      memcpy(cache[i].block, buf, JBOD_BLOCK_SIZE);
      clock += 1;
      cache[i].access_time = clock;
    }
  }
}

// Helper function to get the index of the LRU
// (least recently used) item from the cache.
int getLRU() {
  int j = 0;
  for (int i=0; i<cache_size; i++) {
    if (cache[j].access_time > cache[i].access_time) {
      j = i;
    }
  }
  return j;
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
  // Conditions in which inserting into cache should fail.
  if (cache_size==0 || buf==NULL || disk_num<0 || disk_num>15 || block_num<0 || block_num>255) {
    return -1;
  }
  // Checking if there is already an existing entry in the cache
  // with disk_num and block_num.
  for (int i=0; i<cache_size; i++) {
    if (cache[i].disk_num == disk_num && cache[i].block_num == block_num) {
      if (cache[i].valid == true) {
	return -1;
      }
    }
  }
  // Index of cache at which inserting will be done.
  int index = -1;
  // Finding the index of empty slots in cache, if any.
  for (int i=0; i<cache_size; i++) {
    if (cache[i].valid == false)
      index = i;
  }
  // Finding the index of the cache item with the least access time (LRU).
  if (index == -1) {
    index = getLRU();
  }
  // Inserting buf into cache at index.
  cache[index].valid = true;
  memcpy(cache[index].block, buf, JBOD_BLOCK_SIZE);
  cache[index].block_num = block_num;
  cache[index].disk_num = disk_num;
  clock += 1;
  cache[index].access_time = clock;
  return 1; 
}

bool cache_enabled(void) {
  if (cache_size!=0) {
    return true;
  }
  else {
    return false;
  }
}

void cache_print_hit_rate(void) {
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 *(float) num_hits / num_queries);
}
