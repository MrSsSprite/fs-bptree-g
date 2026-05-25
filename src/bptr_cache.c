/*----------------------------- Public Includes ------------------------------*/
#include "bptr_internal.h"
#include "bptr_node.h"
#include "bptr_utils.h"
#include "../external/data_structures/compat/bit_ops.h"
#include <stdlib.h>
/*--------------------------- Public Includes END ----------------------------*/


/*----------------------------- Private Structs ------------------------------*/
struct cache_ht_entry
{
   bptr_node_t node_idx;   // 0 = Empty
   uint64_t    pool_idx;   // index into pool
};

struct cache_pool_entry
{
   uint16_t refcnt;  // 0:EMPTY, 1:INACTIVE, >=2:ACTIVE
   uint64_t PSL;
   // pool[] index; index of self if head/tail
   // evict_next points to next free block if EMPTY;
   // or, 0 if every subsequent entry are free
   uint64_t evict_prev, evict_next;
   struct bptr_node node;
};

struct bptr_cache
{
   struct cache_ht_entry *ht;
   uint64_t ht_cap;     // Hash table capacity, power of 2
   struct cache_pool_entry *pool;
   uint64_t pool_sz;    // # of node cached in pool
   uint64_t pool_cap;   // Pool Capacity
   uint64_t pool_free;  // pool[] index; points to first free block
   // head points to non-INACTIVE (but valid) node if empty
   uint64_t evict_head, evict_tail;
};
/*--------------------------- Private Structs END ----------------------------*/
