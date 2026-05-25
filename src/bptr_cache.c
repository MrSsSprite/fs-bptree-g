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


/*----------------------------- Public Functions -----------------------------*/
int bptr_cache_init(struct bptr *self, uint64_t pool_cap)
{
   struct bptr_cache *cache;

   // Necessary. the result of clz is undefined if input is 0.
   // Or, if msb is set, no valid value represented in uint64_t can contain it
   // given capacity is restricted to be power of 2.
   if (pool_cap == 0 || pool_cap & (uint64_t)1 << 63) goto INVALID_INPUT;

   cache = malloc(sizeof (struct bptr_cache));
   if (cache == NULL) goto MALLOC_ERR;

   cache->pool_cap = pool_cap;
   cache->ht_cap = (uint64_t)1 << (64 - ds_clz(pool_cap));
   if (  // Load factor check: 80% full
#if defined(__SIZEOF_INT128__)
    ((unsigned __int128)5 * cache->pool_cap) > ((unsigned __int128)4 * cache->ht_cap)
#else
    // Fallback if 128-bit integers aren't supported
    (cache->pool_cap <= SIZE_MAX / 5 && cache->ht_cap <= SIZE_MAX / 4) ?
    (5 * cache->pool_cap > 4 * cache->ht_cap) :
    ((double)cache->pool_cap / cache->ht_cap > 0.8)
#endif
   )
    {
      if (cache->ht_cap & (uint64_t)1 << 63) goto SIZE_TOO_LARGE_ERR;
      cache->ht_cap <<= 1;
    }

   cache->pool =
      malloc((sizeof(struct cache_pool_entry) + self->node_bound.buf_sz) *
             cache->pool_cap);
   if (cache->pool == NULL) goto POOL_MALLOC_ERR;
   cache->ht = malloc(sizeof(struct cache_ht_entry) * cache->ht_cap);
   if (cache->ht == NULL) goto HT_MALLOC_ERR;

   cache->pool_sz = 0;
   cache->pool_free = 0;
   cache->evict_head = cache->evict_tail = 0;

   // 0 Initialize buffers
   for (uint64_t i = 0; i < cache->ht_cap; i++)
      cache->ht[i].node_idx = 0;
   for (uint64_t i = 0; i < cache->pool_cap; i++)
      cache->pool[i].refcnt = 0;
   cache->pool[0].evict_next = 0;

   self->cache = cache;
   return 0;

   /*========================== Error Handling Zone ==========================*/
   _Bool has_set_err = 0;
   int err_code;

__LAST_ERR__:
   free(cache->ht);
HT_MALLOC_ERR:      _set_err_code(BPTR_E_OOM);
   free(cache->pool);
POOL_MALLOC_ERR:    _set_err_code(BPTR_E_OOM);
SIZE_TOO_LARGE_ERR: _set_err_code(BPTR_E_GT_MAXSIZE);
   free(cache);
MALLOC_ERR:         _set_err_code(BPTR_E_OOM);
INVALID_INPUT:      _set_err_code(BPTR_E_FN_INPUT);
   return err_code;
}


int bptr_cache_deinit(struct bptr *self)
{
   struct bptr_cache *cache = self->cache;

   if (cache == NULL) return BPTR_E_ITRNL_STATE;

   for (uint64_t i = 0; i < cache->pool_cap; i++)
    {
      if (cache->pool[i].refcnt == 0 || !cache->pool[i].node.is_dirty)
         continue;
      if (bptr_node_flush(self, &cache->pool[i].node) == 0)
         return BPTR_E_FACCESS;
    }

   free(cache->ht);
   free(cache->pool);
   free(cache);

   self->cache = NULL;

   return 0;
}
/*--------------------------- Public Functions END ---------------------------*/
