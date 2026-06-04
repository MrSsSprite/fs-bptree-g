/*----------------------------- Public Includes ------------------------------*/
#include "bptr_internal.h"
#include "bptr_node.h"
#include "bptr_utils.h"
#include "../external/data_structures/compat/bit_ops.h"
#include <stdlib.h>
#include <stddef.h>
/*--------------------------- Public Includes END ----------------------------*/


/*------------------------------ Private Macros ------------------------------*/
#define CACHE_ENTRY_OF(node_p) \
   ((struct cache_pool_entry*) \
    ((char*)(node_p) - offsetof(struct cache_pool_entry, node)))
/*---------------------------- Private Macros END ----------------------------*/


/*---------------------- Private Function Declarations -----------------------*/
static inline
uint64_t fibonacci_hash_u64(uint64_t node_idx, uint_fast8_t shift);
static
uint64_t ht_lookup(struct bptr_cache *cache, bptr_node_t node_idx);
static void ht_insert
 (struct bptr_cache *cache, bptr_node_t node_idx, uint64_t pool_idx);
static void ht_delete(struct bptr_cache *cache, bptr_node_t node_idx);
static void evict_push(struct bptr_cache *cache, uint64_t pool_idx);
static uint64_t evict_pop(struct bptr_cache *cache);
static void evict_remove(struct bptr_cache *cache, uint64_t pool_idx);
static uint64_t pool_free_pop(struct bptr_cache *cache);
/*-------------------- Private Function Declarations END ---------------------*/


/*----------------------------- Private Structs ------------------------------*/
struct cache_ht_entry
{
   bptr_node_t node_idx;   // 0 = Empty
   uint64_t    pool_idx;   // index into pool
   uint64_t PSL;           // Probe Sequence Length for Robin Hood Probing
};

struct cache_pool_entry
{
   uint16_t refcnt;  // 0:EMPTY, 1:INACTIVE, >=2:ACTIVE
   // pool[] index; sentinel: index of self if head/tail
   // evict_next points to next free block if EMPTY;
   // or, 0 if every subsequent entry is free
   uint64_t evict_prev, evict_next;
   struct bptr_node node;
};

struct bptr_cache
{
   struct cache_ht_entry *ht;
   uint64_t ht_cap;         // Hash table capacity, power of 2
   uint_fast8_t hash_shift; // bit shift of hash table capacity
   struct cache_pool_entry *pool;
   uint64_t pool_sz;        // # of node cached in pool
   uint64_t pool_cap;       // Pool Capacity
   uint64_t pool_free;      // pool[] index; points to first free block
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
   cache->hash_shift = ds_clz(cache->ht_cap);

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
   // Entire block of memory free
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


struct bptr_node *bptr_node_fetch(struct bptr *self, bptr_node_t node_idx)
{
   struct bptr_cache *cache = self->cache;
   struct cache_pool_entry *pool_en;
   uint64_t pool_idx;
   int fn_err;

   if (node_idx == 0) { bptr_errno = BPTR_E_FN_INPUT; return NULL; }

   pool_idx = ht_lookup(cache, node_idx);
   if (pool_idx < cache->pool_cap)  // cache HIT
    {
      pool_en = cache->pool + pool_idx;
      if (pool_en->refcnt == 1)
         evict_remove(cache, pool_idx);
      pool_en->refcnt++;
      return &pool_en->node;
    }

   // Cache MISS
   pool_idx = pool_free_pop(cache);
   if (pool_idx == cache->pool_cap)  // No Free Slot
    {
      struct cache_pool_entry *victim_en;

      // Get thru Eviction
      pool_idx = evict_pop(cache);
      if (bptr_errno) goto EVICT_ERR;

      victim_en = cache->pool + pool_idx;
      if (victim_en->node.is_dirty)
       {
         if (bptr_node_flush(self, &victim_en->node) == 0)
          { goto NODE_FLUSH_ERR; }
       }

      ht_delete(cache, victim_en->node.node_idx);
      victim_en->refcnt = 0;
    }

   pool_en = cache->pool + pool_idx;
   fn_err = bptr_node_load(self, node_idx, &pool_en->node);
   if (fn_err) goto NODE_LOAD_ERR;

   pool_en->refcnt = 2;

   ht_insert(cache, node_idx, pool_idx);

   return &pool_en->node;

   /*-------------------------- Error Handling Area --------------------------*/
   _Bool has_set_err = 0;

NODE_FLUSH_ERR: _set_errno(BPTR_E_FACCESS);
   evict_push(cache, pool_idx);
NODE_LOAD_ERR:  _set_errno(fn_err);
EVICT_ERR:      _set_errno(BPTR_E_CACHE_FULL);
   return NULL;
}


// Caller is responsible of ensuring that it's valid to relese `node`
void bptr_cache_release(struct bptr *self, struct bptr_node *node)
{
   struct bptr_cache *cache = self->cache;
   struct cache_pool_entry *pool_en = CACHE_ENTRY_OF(node);

   if (--pool_en->refcnt == 1)
      evict_push(cache, pool_en - cache->pool);
}
/*--------------------------- Public Functions END ---------------------------*/


/*---------------------------- Private Functions -----------------------------*/
static inline
uint64_t fibonacci_hash_u64(uint64_t node_idx, uint_fast8_t shift)
{
#define HASH_MULTIPLIER 11400714819323198485ULL
   return (node_idx * HASH_MULTIPLIER) >> shift;
#undef HASH_MULTIPLIER
}


static
uint64_t ht_lookup(struct bptr_cache *cache, bptr_node_t node_idx)
{
   struct cache_ht_entry *ht_en;
   uint64_t psl;
   uint64_t idx = fibonacci_hash_u64(node_idx, cache->hash_shift);

   psl = 0;
   ht_en = cache->ht + idx;
   while (ht_en->node_idx)
    {
      if (ht_en->PSL < psl)
         return cache->pool_cap;
      if (ht_en->node_idx == node_idx)
         return ht_en->pool_idx;

      idx = (idx + 1) & ~(cache->ht_cap);
      psl++;
    }

   return cache->pool_cap;
}


// Caller is responsible of checking that it's valid to insert into the ht
static
void ht_insert(struct bptr_cache *cache, bptr_node_t node_idx, uint64_t pool_idx)
{
   struct cache_ht_entry cur = { .node_idx = node_idx, .pool_idx = pool_idx, 0 };
   uint64_t idx = fibonacci_hash_u64(cur.node_idx, cache->hash_shift);
   struct cache_ht_entry *ht_en;

   while (1)
    {
      ht_en = cache->ht + idx;

      if (ht_en->node_idx == 0)
       { *ht_en = cur; return; }

      if (cur.PSL > ht_en->PSL)
       {
         struct cache_ht_entry tmp_en = *ht_en;
         *ht_en = cur;
         cur = tmp_en;
       }

      idx = (idx + 1) & ~(cache->ht_cap);
      cur.PSL++;
    }
}


static void ht_delete(struct bptr_cache *cache, bptr_node_t node_idx)
{
   uint64_t idx = ht_lookup(cache, node_idx);
   struct cache_ht_entry *ht_en;

   if (idx == cache->pool_cap) return; // not found

   while (1)
    {
      uint64_t next = (idx + 1) & ~(cache->ht_cap);
      ht_en = cache->ht + next;
      if (ht_en->node_idx == 0 || ht_en->PSL == 0) break;
      cache->ht[idx] = cache->ht[next];
      idx = next;
    }
   cache->ht[idx].node_idx = 0;
}


static void evict_push(struct bptr_cache *cache, uint64_t pool_idx)
{
   struct cache_pool_entry *pool_en = cache->pool + pool_idx;

   pool_en->evict_next = pool_idx;  // self == sentinel value
   if (cache->pool[cache->evict_head].refcnt != 1) // if empty
    {
      pool_en->evict_prev = cache->evict_head = cache->evict_tail = pool_idx;
      return;
    }
   pool_en->evict_prev = cache->evict_tail;
   cache->evict_tail = pool_idx;
}


// Caller should set refcnt to any non-1 value; otherwise, it leads to
// undefined behavior after removing last element from eviction list
static void evict_remove(struct bptr_cache *cache, uint64_t pool_idx)
{
   struct cache_pool_entry *pool_en = cache->pool + pool_idx;
   _Bool is_head = pool_en->evict_prev == pool_idx,
         is_tail = pool_en->evict_next == pool_idx;

   if (is_head)
      cache->evict_head = pool_en->evict_next;
   else
      cache->pool[pool_en->evict_prev].evict_next =
         is_tail ? pool_en->evict_prev : pool_en->evict_next;

   if (is_tail)
      cache->evict_tail = pool_en->evict_prev;
   else
      cache->pool[pool_en->evict_next].evict_prev =
         is_head ? pool_en->evict_next : pool_en->evict_prev;
}


// Return oldest unloaded entry for eviction
// Caller should set refcnt to any non-1 value; otherwise, it leads to
// undefined behavior after removing last element from eviction list
static uint64_t evict_pop(struct bptr_cache *cache)
{
   uint64_t ret = cache->evict_head;

   bptr_errno = 0;

   if (cache->pool[ret].refcnt != 1)
    { bptr_errno = BPTR_E_NOT_FOUND; return 0; }

   evict_remove(cache, ret);
   return ret;
}


// This function increments cache->pool_sz on success
static uint64_t pool_free_pop(struct bptr_cache *cache)
{
   uint64_t pool_idx = cache->pool_free;
   struct cache_pool_entry *pool_en = cache->pool + pool_idx;

   if (cache->pool_sz++ >= cache->pool_cap)
    {
      cache->pool_sz--;
      bptr_errno = BPTR_E_CACHE_FULL;
      return cache->pool_cap;
    }

   if (pool_en->evict_next == 0) // Trailing free block
    {
      // Logically, if following cond is true, pool_en is the last free block
      if (pool_idx + 1 < cache->pool_cap)
         pool_en[1].evict_next = 0;
      cache->pool_free++;
    }
   else
      cache->pool_free = pool_en->evict_next;

   return pool_idx;
}


// This function decrements cache->pool_sz on success
static void pool_free_push(struct bptr_cache *cache, uint64_t pool_idx)
{
   cache->pool[pool_idx].evict_next = cache->pool_free;
   cache->pool_free = pool_idx;
   cache->pool_sz--;
}
/*-------------------------- Private Functions END ---------------------------*/
