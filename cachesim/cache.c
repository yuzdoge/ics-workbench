#include "common.h"
#include <inttypes.h>
#include <string.h> 
#include "trace.h"
#include "stat.h"

void mem_read(uintptr_t block_num, uint8_t *buf);
void mem_write(uintptr_t block_num, const uint8_t *buf);

static uint64_t cycle_cnt = 0;

void cycle_increase(int n) { cycle_cnt += n; }

// TODO: implement the following functions

#define pos(x) exp2(x)
#define CACHELINE_V 0x1 
#define CACHELINE_D 0x2 

struct cache_line {
  uint32_t status;
  uint32_t tag;
  uint8_t  data[BLOCK_SIZE];
};

static struct cache_line **cache;
static int nway, nset; 
static int index_width, tag_width;
static uint64_t hit, miss;

#define test_bit(stat, flag) (((stat) & (flag)) != 0)
#define set_stat(stat, flag) ((stat) | (flag)) 
#define unset_stat(stat, flag) ((stat) & ~(flag))
#define clear_stat(stat) 0


#define get_tag(addr)   (((addr) >> (index_width + BLOCK_WIDTH)) & (exp2(tag_width) - 1)) 
#define get_index(addr) (((addr) >> BLOCK_WIDTH) & (nset - 1))
#define get_offset(addr) ((addr) & (BLOCK_SIZE - 1))

#define make_blocknum(tag, index) (((tag) << index_width) | index)

// Replacement Policies
#define REPLACE_RAND 0

// Write Policies
#define WRITE_THROUGH 0
#define WRITE_BACK    1

// Write miss Policies
#define NWRITE_ALLOCATE 0
#define WRITE_ALLOCATE  1

static struct {
  int replace_policy; 
  int write_policy; 
  int wmiss_policy;
} cache_obj;


static uint32_t replace_policy(int policy, uint32_t index) {
  int i;
  switch (policy) {
    default: i = rand() % nway;
  }
  return i;
}

static int access(uint32_t tag, uint32_t index) {
  for (int i = 0; i < nway; i++)
    if (cache[i][index].tag == tag && 
        test_bit(cache[i][index].status, CACHELINE_V)) {
        hit++;
        return i; // hit
    }
  miss++;
  return -1; // miss
} 

static void write_dirty(uint32_t way, uint32_t index) {
  uintptr_t blocknum = make_blocknum(cache[way][index].tag, index);
  if (test_bit(cache[way][index].status, CACHELINE_V) && 
    test_bit(cache[way][index].status, CACHELINE_D)) {
    mem_write(blocknum, cache[way][index].data);
    //printf("write!!!\n");
  }
}

static uint32_t* cache_ctrl(int write, uintptr_t addr) {

  assert(addr < MEM_SIZE);

  int way; 
  uint32_t *word;
  uint32_t tag = get_tag(addr); 
  uint32_t index =  get_index(addr);
  uint32_t offset = get_offset(addr);

  way = access(tag, index);

  if (!write) {
    if (way >= 0) {

    } else {
      way = replace_policy(cache_obj.replace_policy, index);

      if (cache_obj.write_policy == WRITE_BACK) {
        write_dirty(way, index);
        //printf("<<<<<<<<<<<<<<<<<<<<read miss>>>>>>>>>\n");
      }

      mem_read((addr >> BLOCK_WIDTH), cache[way][index].data);

      cache[way][index].status = clear_stat(cache[way][index].status);
      cache[way][index].status = set_stat(cache[way][index].status, CACHELINE_V);
      cache[way][index].tag = tag;
    }
  } else {

    if (way >= 0) {
      if (cache_obj.write_policy == WRITE_BACK) {
        cache[way][index].status = set_stat(cache[way][index].status, CACHELINE_D);
      }
    } else {

      if (cache_obj.wmiss_policy == WRITE_ALLOCATE) {
        way = replace_policy(cache_obj.replace_policy, index);

        if (cache_obj.write_policy == WRITE_BACK) {
          write_dirty(way, index);
          //printf("<<<<<<<<<<<<<<<<<<<<write miss>>>>>>>>>\n");
        }

        mem_read((addr >> BLOCK_WIDTH), cache[way][index].data);
        cache[way][index].status = clear_stat(cache[way][index].status);
        cache[way][index].status = set_stat(cache[way][index].status, CACHELINE_V);
        cache[way][index].status = set_stat(cache[way][index].status, CACHELINE_D);
        cache[way][index].tag = tag;
      }

    }

  }

  word = (void *)cache[way][index].data + (offset & ~(sizeof(*word) - 1));

  return word;

}

uint32_t cache_read(uintptr_t addr) {
  uint32_t *word = cache_ctrl(0, addr);
  return *word;
}

void cache_write(uintptr_t addr, uint32_t data, uint32_t wmask) {
  uint32_t *word = cache_ctrl(1, addr);
  *word = (*word & ~wmask) | (data & wmask); 
}

/* addr = | tag | index | block offset| */

void init_cache(int total_size_width, int associativity_width) {

  cache_obj.replace_policy = REPLACE_RAND;
  cache_obj.write_policy = WRITE_BACK;
  cache_obj.wmiss_policy = WRITE_ALLOCATE;

  //int size = exp2(total_size_width);
  index_width = total_size_width - BLOCK_WIDTH - associativity_width;
  assert(index_width >= 0);

  tag_width = sizeof(uintptr_t) * 8 - index_width - BLOCK_WIDTH;
  assert(tag_width >= 0);

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
  uint64_t tot_access = hit + miss;
  // hit div tot_access = 0 . h1 h2 h3 h4
  uint64_t tmp = 10000 * hit / tot_access;
  uint64_t h1 = tmp / 1000; tmp %= 1000;
  uint64_t h2 = tmp / 100;  tmp %= 100; 
  uint64_t h3 = tmp / 10;   tmp %= 10;
  uint64_t h4 = tmp;
  double aa = (double)hit / tot_access;
  LOG("Cache Configure:\n");
  LOG("\n");
  LOG("Total Memory Access: %ld\n", tot_access);
  LOG("Hit / Miss    %ld / %ld\n", hit, miss);
  LOG("Hit Rate: %ld%ld.%ld%ld%%    %lf\n", h1, h2, h3, h4, aa);
}

void free_cache(void) {
  assert(nway > 0); // prevent free_cache() from being invoked before init_cache()
  for (int i = 0; i < nway; i++)
    free(cache[i]);
  free(cache);
}
