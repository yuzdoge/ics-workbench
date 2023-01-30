#include "common.h"
#include <inttypes.h>
#include <string.h> 

void mem_read(uintptr_t block_num, uint8_t *buf);
void mem_write(uintptr_t block_num, const uint8_t *buf);

static uint64_t cycle_cnt = 0;

void cycle_increase(int n) { cycle_cnt += n; }

// TODO: implement the following functions

#define CACHELINE_V 0x1

struct cache_line {
  uint32_t status;
  uint32_t tag;
  uint8_t  data[BLOCK_SIZE];
};

static struct cache_line **cache;
static int nway, nset; 
static uint32_t index_width, tag_width;

#define test_bit(stat, flag) (((stat) & (flag)) != 0)
#define set_stat(stat, flag) ((stat) | (flag))
#define unset_stat(stat, flag) ((stat) & ~(flag))
#define clear_stat(stat) ((stat) & 0)


#define get_tag(addr)   (((addr) >> (index_width + BLOCK_WIDTH)) & (exp2(tag_width) - 1)) 
#define get_index(addr) (((addr) >> BLOCK_WIDTH) & (nset - 1))
#define get_offset(addr) ((addr) & (BLOCK_SIZE - 1))


static uint32_t replace(int policy) {
  return 0;
}

static int access(uint32_t tag, uint32_t index) {
  for (int i = 0; i < nway; i++)
    if (cache[i][index].tag == tag && 
        test_bit(cache[i][index].status, CACHELINE_V))
      return i;

  return -1;
} 

uint32_t cache_read(uintptr_t addr) {
  int i;
  uint32_t *word;
  uint32_t tag = get_tag(addr); 
  uint32_t index =  get_index(addr);
  uint32_t offset = get_offset(addr);
  //printf("%x:%x:%x\n", tag, index, offset);

  if ((i = access(tag, index)) > 0) {
    word = (void *)cache[i][index].data + (offset & ~(sizeof(*word) - 1));
  } else {
    replace(1);
    word = (void *)cache[i][index].data + (offset & ~(sizeof(*word) - 1));

  }
  return *word;
}

void cache_write(uintptr_t addr, uint32_t data, uint32_t wmask) {
}

/* addr = | tag | index | block offset| */

void init_cache(int total_size_width, int associativity_width) {
  //int size = exp2(total_size_width);
  index_width = total_size_width - BLOCK_WIDTH - associativity_width;
  tag_width = sizeof(uintptr_t) * 8 - index_width - BLOCK_WIDTH;
  nway = exp2(associativity_width);
  nset = exp2(index_width);

  cache = malloc(nway * sizeof(struct cache_line*));
  assert(cache);
  for (int i = 0; i < nway; i++) {
    cache[i] = malloc(nset * sizeof(struct cache_line)); 
    assert(cache[i]);
    memset(cache[i], 0, nset * sizeof(struct cache_line));
  }
}

void display_statistic(void) {
}

void free_cache() {
  assert(nway > 0);
  for (int i = 0; i < nway; i++)
    free(cache[i]);
  free(cache);
}
