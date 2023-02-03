#include "common.h"
#include <inttypes.h>
#include <string.h> 
#include "trace.h"
#include "log.h"

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

#define test_bit(stat, flag) (((stat) & (flag)) != 0)
#define set_stat(stat, flag) ((stat) | (flag)) 
#define unset_stat(stat, flag) ((stat) & ~(flag))
#define clear_stat(stat) 0


#define get_tag(addr)   (((addr) >> (cache_obj.index_width + cache_obj.block_width)) & (exp2(cache_obj.tag_width) - 1)) 
#define get_index(addr) (((addr) >> cache_obj.block_width) & (cache_obj.nset - 1))
#define get_offset(addr) ((addr) & (cache_obj.block_size - 1))

#define make_blocknum(tag, index) (((tag) << cache_obj.index_width) | index)

// Replacement Policies
#define REPLACE_RAND 0

// Write Policies
#define WRITE_THROUGH 0
#define WRITE_BACK    1

// Write miss Policies
#define NWRITE_ALLOCATE 0
#define WRITE_ALLOCATE  1

struct policy{
  char *name;
  union {
    uint32_t (*replace)(uint32_t index);
    uint32_t (*write)();
    uint32_t (*wmiss)();
  };
}; 

static uint32_t replace_rand(uint32_t index) {
  return (rand() % cache_obj.nway);
}

static uint32_t write_back() {
  return 0;
}

static uint32_t write_alloc() {
  return 0;
}

static struct policy replace_policy[] = {
[REPLACE_RAND] {.name = "Random", .replace = replace_rand},
};

static struct policy write_policy[] = {
//[WRITE_THROUGH] {.name = "Write Through", .write = write_through}, 
[WRITE_BACK]    {.name = "Write Back",    .write = write_back   },
};

static struct policy wmiss_policy[] = {
//[NWRITE_ALLOCATE] {.name = "Non Write Allocate", wmiss = nwrite_alloc},
[WRITE_ALLOCATE]  {.name = "Write Allocate",     wmiss = write_alloc},
};

static struct {
  int nway; 
  int nset; 
  int block_size;
  int cache_size;

  int block_width;
  int index_width; 
  int tag_width;

  int replace_policy; 
  int write_policy; 
  int wmiss_policy;

  uint64_t hit;
  uint64_t miss;
} cache_obj;

static int access(uint32_t tag, uint32_t index) {
  for (int i = 0; i < cache_obj.nway; i++)
    if (cache[i][index].tag == tag && 
        test_bit(cache[i][index].status, CACHELINE_V)) {
        cache_obj.hit++;
        return i; // hit
    }
  cache_obj.miss++;
  return -1; // miss
} 

static void write_dirty(uint32_t way, uint32_t index) {
  uintptr_t blocknum = make_blocknum(cache[way][index].tag, index);
  if (test_bit(cache[way][index].status, CACHELINE_V) && 
    test_bit(cache[way][index].status, CACHELINE_D)) {
    mem_write(blocknum, cache[way][index].data);
#ifdef MTRACE
    MTRACE_WDIRTY(blocknum);
#endif
  }
}

static uint32_t* cache_ctrl(int write, uintptr_t addr) {

  assert(addr < MEM_SIZE);

  int way; 
  uint32_t *word;
  uint32_t tag = get_tag(addr); 
  uint32_t index =  get_index(addr);
  uint32_t offset = get_offset(addr);

  uint32_t odata, ndata; 
  uint32_t ostat, nstat; 
  uintptr_t obase, nbase;

  way = access(tag, index);

  if (!write) {
    if (way >= 0) {
      word = (void *)cache[way][index].data + (offset & ~(sizeof(*word) - 1));
      nstat=ostat = cache[way][index].status;
      nbase=obase = make_blocknum(cache[way][index].tag, index); 
      ndata=odata = *word; 

    } else {
      way = replace_policy[cache_obj.replace_policy].replace(index);

      word = (void *)cache[way][index].data + (offset & ~(sizeof(*word) - 1));
      ostat = cache[way][index].status;
      obase = make_blocknum(cache[way][index].tag, index); 
      odata = *word;

      if (cache_obj.write_policy == WRITE_BACK) {
        write_dirty(way, index);
        //printf("<<<<<<<<<<<<<<<<<<<<read miss>>>>>>>>>\n");
      }

      mem_read((addr >> cache_obj.block_width), cache[way][index].data);

      cache[way][index].status = clear_stat(cache[way][index].status);
      cache[way][index].status = set_stat(cache[way][index].status, CACHELINE_V);
      cache[way][index].tag = tag;

      nstat = cache[way][index].status;
      nbase = make_blocknum(cache[way][index].tag, index); 
      ndata = *word;

    }
#ifdef
    MTRACE_R(addr, way, ostat, obase, offset, odata, nstat, nbase, ndata);
#endif

  } else {

    if (way >= 0) {
      if (cache_obj.write_policy == WRITE_BACK) {
        cache[way][index].status = set_stat(cache[way][index].status, CACHELINE_D);
      }
    } else {

      if (cache_obj.wmiss_policy == WRITE_ALLOCATE) {
        way = replace_policy[cache_obj.replace_policy].replace(index);

        if (cache_obj.write_policy == WRITE_BACK) {
          write_dirty(way, index);
          //printf("<<<<<<<<<<<<<<<<<<<<write miss>>>>>>>>>\n");
        }

        mem_read((addr >> cache_obj.block_width), cache[way][index].data);
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

#define ADDR_LEN sizeof(uintptr_t) * 8
void init_cache(int total_size_width, int associativity_width) {

  cache_obj.replace_policy = REPLACE_RAND;
  cache_obj.write_policy = WRITE_BACK;
  cache_obj.wmiss_policy = WRITE_ALLOCATE;

  cache_obj.block_width = BLOCK_WIDTH;
  cache_obj.block_size  = BLOCK_SIZE;
  cache_obj.cache_size = exp2(total_size_width);
  cache_obj.index_width = total_size_width - cache_obj.block_width - associativity_width;
  assert(cache_obj.index_width >= 0);

  cache_obj.tag_width = ADDR_LEN - cache_obj.index_width - cache_obj.block_width;
  assert(cache_obj.tag_width >= 0);

  cache_obj.nway = exp2(associativity_width);
  cache_obj.nset = exp2(cache_obj.index_width);

  cache = malloc(cache_obj.nway * sizeof(struct cache_line*));
  assert(cache);
  for (int i = 0; i < cache_obj.nway; i++) {
    cache[i] = malloc(cache_obj.nset * sizeof(struct cache_line)); 
    assert(cache[i]);
    memset(cache[i], 0, cache_obj.nset * sizeof(struct cache_line));
  }
}


void display_statistic(void) {
  uint64_t tot_access = cache_obj.hit + cache_obj.miss;
  double hit_rate = (double)cache_obj.hit / tot_access;
  LOG("Cache Configuration:\n");
  LOG("Policies:\n");
  LOG("\tReplacement Policy: %s\n", replace_policy[cache_obj.replace_policy].name);
  LOG("\tWrite Policy: %s\n", write_policy[cache_obj.write_policy].name);
  LOG("\tWrite Miss Policy: %s\n", wmiss_policy[cache_obj.wmiss_policy].name);
  LOG("Cache Size: %d\n", cache_obj.cache_size);
  LOG("Block Size: %d\n", cache_obj.block_size);
  LOG("Set Number: %d\n", cache_obj.nset);
  LOG("Associativity: %d\n", cache_obj.nway);
  LOG("\n");
  LOG("Statistic:\n");
  LOG("Total Memory Access: %ld\n", tot_access);
  LOG("Hit / Miss: %ld / %ld\n", cache_obj.hit, cache_obj.miss);
  LOG("Hit Rate: %0.4lf%%\n", 100 * hit_rate);
  LOG("\n");
/* hit div tot_access = 0 . h1 h2 h3 h4
  uint64_t tmp = 10000 * hit / tot_access;
  uint64_t h1 = tmp / 1000; tmp %= 1000;
  uint64_t h2 = tmp / 100;  tmp %= 100; 
  uint64_t h3 = tmp / 10;   tmp %= 10;
  uint64_t h4 = tmp;
  LOG("Hit Rate: %ld%ld.%ld%ld%%    %lf\n", h1, h2, h3, h4, 100 * aa);
*/
}

void free_cache(void) {
  assert(cache_obj.nway > 0); // prevent free_cache() from being invoked before init_cache()
  for (int i = 0; i < cache_obj.nway; i++)
    free(cache[i]);
  free(cache);
}
