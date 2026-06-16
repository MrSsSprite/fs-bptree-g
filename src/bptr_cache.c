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

#define GET_POOL_EN(cache, idx) ((struct cache_pool_entry*) \
   ((char*)(cache)->pool + (idx) * (cache)->pool_en_sz))

#define POOL_EN_ED(cache) ((struct cache_pool_entry*) \
   ((char*)(cache)->pool + (cache)->pool_cap * (cache)->pool_en_sz))

#define POOL_EN_INCR(cache, iter) \
   ((struct cache_pool_entry*)((char*)(iter) + (cache)->pool_en_sz))

#define POOL_EN_DECR(cache, iter) \
   ((struct cache_pool_entry*)((char*)(iter) - (cache)->pool_en_sz))

#define POOL_EN_LEAP(cache, iter, step) \
   ((struct cache_pool_entry*)((char*)(iter) + (step) * (cache)->pool_en_sz))

#define POOL_EN_DIFF(cache, lhs, rhs) \
   (((char*)(lhs) - (char*)(rhs)) / (cache)->pool_en_sz)

#define POOL_EN_IDX(cache, iter) POOL_EN_DIFF(cache, iter, (cache)->pool)
/*---------------------------- Private Macros END ----------------------------*/


/*---------------------- Private Function Declarations -----------------------*/
static inline
uint64_t fibonacci_hash_u64(uint64_t node_idx, uint_fast8_t shift);
static struct cache_ht_entry *ht_lookup
 (struct bptr_cache *cache, bptr_node_t node_idx);
static void ht_insert
 (struct bptr_cache *cache, bptr_node_t node_idx, uint64_t pool_idx);
static void ht_delete(struct bptr_cache *cache, bptr_node_t node_idx);
static void evict_push(struct bptr_cache *cache, uint64_t pool_idx);
static uint64_t evict_pop(struct bptr_cache *cache);
static void evict_remove(struct bptr_cache *cache, uint64_t pool_idx);
static uint64_t pool_free_pop(struct bptr_cache *cache);
static void pool_free_push(struct bptr_cache *cache, uint64_t pool_idx);
/*-------------------- Private Function Declarations END ---------------------*/


/*----------------------------- Private Structs ------------------------------*/
struct cache_ht_entry
{
   bptr_node_t node_idx;   // 0 = Empty
   uint64_t    pool_idx;   // index into pool
   uint64_t psl;           // Probe Sequence Length for Robin Hood Probing
};

struct cache_pool_entry
{
   uint16_t refcnt;  // 0:EMPTY, 1:INACTIVE, >=2:ACTIVE
   // pool[] index; sentinel: index of self if head/tail
   // evict_next points to next free block if EMPTY;
   // or, self if every subsequent entry is free, pool_cap if being the last
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
   // pool entry size with buffer and alignment applied
   uint64_t pool_en_sz;
   // head points to non-INACTIVE (but valid) node if empty
   uint64_t evict_head, evict_tail;
};
/*--------------------------- Private Structs END ----------------------------*/


/*----------------------------- Public Functions -----------------------------*/
int bptr_cache_init(struct bptr *self, uint64_t pool_cap)
{
   struct bptr_cache *cache;
   uint64_t node_buf_sz;
   size_t pool_sz;

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
   cache->hash_shift = ds_clz(cache->ht_cap) + 1;

   {
      uint64_t leaf_storage =
         (uint64_t)(self->node_bound.leaf.up - 1) * self->key_size +
         (uint64_t)(self->node_bound.leaf.up - 1) * self->value_size;
      uint64_t brch_storage =
         (uint64_t)(self->node_bound.brch.up - 1) * self->key_size +
         (uint64_t)self->node_bound.brch.up * BPTR_PTR_SIZE;
      node_buf_sz = (leaf_storage > brch_storage ? leaf_storage : brch_storage);
      node_buf_sz = (node_buf_sz + 7) & ~7;
   }
   cache->pool_en_sz = sizeof(struct cache_pool_entry) + node_buf_sz;
   pool_sz = cache->pool_en_sz * cache->pool_cap;
   cache->pool = calloc(cache->pool_cap, cache->pool_en_sz);
   if (cache->pool == NULL) goto POOL_MALLOC_ERR;
   cache->ht = calloc(cache->ht_cap, sizeof(struct cache_ht_entry));
   if (cache->ht == NULL) goto HT_MALLOC_ERR;
   // Both pool and ht are 0 initialized.
   // cache->ht[i].node_idx == 0 : Empty
   // GET_POOL_EN(cache, i)->refcnt == 0 : EMPTY

   cache->pool_sz = 0;
   cache->pool_free = 0;
   cache->evict_head = cache->evict_tail = 0;

   self->cache = cache;
   return 0;

   /*========================== Error Handling Zone ==========================*/
   _Bool has_set_err = 0;
   int err_code;

__LAST_ERR__:
   free(cache->ht);
HT_MALLOC_ERR:        _set_err_code(BPTR_E_OOM);
   free(cache->pool);
POOL_MALLOC_ERR:      _set_err_code(BPTR_E_OOM);
SIZE_TOO_LARGE_ERR:   _set_err_code(BPTR_E_GT_MAXSIZE);
   free(cache);
MALLOC_ERR:           _set_err_code(BPTR_E_OOM);
INVALID_INPUT:        _set_err_code(BPTR_E_FN_INPUT);
   return err_code;
}


int bptr_cache_deinit(struct bptr *self)
{
   struct bptr_cache *cache = self->cache;

   if (cache == NULL) return BPTR_E_ITRNL_STATE;

   for (struct cache_pool_entry *pool_en = cache->pool,
                                *pool_ed = POOL_EN_ED(cache);
        pool_en < pool_ed; pool_en = POOL_EN_INCR(cache, pool_en))
    {
      if (pool_en->refcnt == 0 || !pool_en->node.is_dirty)
         continue;
      if (bptr_node_flush(self, &pool_en->node))
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

   {  // Cache HIT
      struct cache_ht_entry *ht_en = ht_lookup(cache, node_idx);
      if (ht_en)  // cache HIT
       {
         pool_idx = ht_en->pool_idx;
         pool_en = GET_POOL_EN(cache, pool_idx);
         if (pool_en->refcnt == 1)
            evict_remove(cache, pool_idx);
         pool_en->refcnt++;
         return &pool_en->node;
       }
   }

   // Cache MISS
   pool_idx = pool_free_pop(cache);
   if (pool_idx == cache->pool_cap)  // No Free Slot
    {
      struct cache_pool_entry *victim_en;

      // Get thru Eviction
      pool_idx = evict_pop(cache);
      if (bptr_errno) goto EVICT_ERR;

      victim_en = GET_POOL_EN(cache, pool_idx);
      if (victim_en->node.is_dirty)
       {
         if (bptr_node_flush(self, &victim_en->node))
          { goto NODE_FLUSH_ERR; }
       }

      ht_delete(cache, victim_en->node.node_idx);
      victim_en->refcnt = 0;
    }

   pool_en = GET_POOL_EN(cache, pool_idx);
   fn_err = bptr_node_load(self, node_idx, &pool_en->node);
   if (fn_err) goto NODE_LOAD_ERR;

   pool_en->refcnt = 2;

   ht_insert(cache, node_idx, pool_idx);

   return &pool_en->node;

   /*-------------------------- Error Handling Area --------------------------*/
   _Bool has_set_err = 0;

NODE_LOAD_ERR:  _set_errno(fn_err);
   pool_free_push(cache, pool_idx); // return to free pool even from eviction
   if (0)   // enter only on jump
    {
NODE_FLUSH_ERR: _set_errno(BPTR_E_FACCESS);
      evict_push(cache, pool_idx);
EVICT_ERR:      _set_errno(BPTR_E_CACHE_FULL);
    }
   return NULL;
}


// This function does not update ht as node_idx is not known yet.
struct bptr_node *bptr_cache_alloc(struct bptr *self, bptr_node_t node_idx)
{
   struct bptr_cache *cache = self->cache;
   struct cache_pool_entry *pool_en;
   uint64_t pool_idx;
   int fn_err;

   pool_idx = pool_free_pop(cache);
   if (pool_idx == cache->pool_cap)  // No Free Slot
    {
      struct cache_pool_entry *victim_en;

      // Get thru Eviction
      pool_idx = evict_pop(cache);
      if (bptr_errno) goto EVICT_ERR;

      victim_en = GET_POOL_EN(cache, pool_idx);
      if (victim_en->node.is_dirty)
       {
         if (bptr_node_flush(self, &victim_en->node))
          { goto NODE_FLUSH_ERR; }
       }

      ht_delete(cache, victim_en->node.node_idx);
      victim_en->refcnt = 0;
    }

   pool_en = GET_POOL_EN(cache, pool_idx);
   pool_en->refcnt = 2;
   ht_insert(cache, node_idx, pool_idx);
   return &pool_en->node;

   /*-------------------------- Error Handling Area --------------------------*/
   _Bool has_set_err = 0;

   if (0)   // enters only on jump
    {
NODE_FLUSH_ERR: _set_errno(BPTR_E_FACCESS);
      evict_push(cache, pool_idx);
EVICT_ERR:      _set_errno(BPTR_E_CACHE_FULL);
    }
   return NULL;
}


// Caller is responsible of ensuring that it's valid to relese `node`
void bptr_cache_release(struct bptr *self, struct bptr_node *node)
{
   struct bptr_cache *cache = self->cache;
   struct cache_pool_entry *pool_en = CACHE_ENTRY_OF(node);

   if (--pool_en->refcnt == 1)
      evict_push(cache, POOL_EN_IDX(cache, pool_en));
}


void bptr_cache_reclaim(struct bptr *self, struct bptr_node *node)
{
   struct bptr_cache *cache = self->cache;
   struct cache_pool_entry *pool_en = CACHE_ENTRY_OF(node);

   pool_en->refcnt = 0;
   pool_free_push(cache, POOL_EN_IDX(cache, pool_en));
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


static struct cache_ht_entry *ht_lookup
 (struct bptr_cache *cache, bptr_node_t node_idx)
{
   struct cache_ht_entry *ht_en;
   uint64_t psl;
   uint64_t idx = fibonacci_hash_u64(node_idx, cache->hash_shift);

   psl = 0;
   ht_en = cache->ht + idx;
   while (ht_en->node_idx)
    {
      if (ht_en->psl < psl)
         return NULL;
      if (ht_en->node_idx == node_idx)
         return ht_en;

      idx = (idx + 1) & ~(cache->ht_cap);
      psl++;
    }

   return NULL;
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

      if (cur.psl > ht_en->psl)
       {
         struct cache_ht_entry tmp_en = *ht_en;
         *ht_en = cur;
         cur = tmp_en;
       }

      idx = (idx + 1) & ~(cache->ht_cap);
      cur.psl++;
    }
}


static void ht_delete(struct bptr_cache *cache, bptr_node_t node_idx)
{
   struct cache_ht_entry *ht_en = ht_lookup(cache, node_idx);
   uint64_t idx;

   if (ht_en == NULL) return; // not found

   idx = ht_en - cache->ht;
   while (1)
    {
      uint64_t next = (idx + 1) & ~(cache->ht_cap);
      ht_en = cache->ht + next;
      if (ht_en->node_idx == 0 || ht_en->psl == 0) break;
      cache->ht[idx] = *ht_en;
      cache->ht[idx].psl--;
      idx = next;
    }
   cache->ht[idx].node_idx = 0;
}


static void evict_push(struct bptr_cache *cache, uint64_t pool_idx)
{
   struct cache_pool_entry *pool_en = GET_POOL_EN(cache, pool_idx);

   pool_en->evict_next = pool_idx;  // new entry is new tail (self-sentinel)
   if (GET_POOL_EN(cache, cache->evict_head)->refcnt != 1) // queue empty?
    {
      pool_en->evict_prev = cache->evict_head = cache->evict_tail = pool_idx;
      return;
    }
   pool_en->evict_prev = cache->evict_tail;  // link backward to old tail
   // link old tail to new tail
   GET_POOL_EN(cache, cache->evict_tail)->evict_next = pool_idx;
   cache->evict_tail = pool_idx;             // update global tail
}


// Caller should set refcnt to any non-1 value; otherwise, it leads to
// undefined behavior after removing last element from eviction list
static void evict_remove(struct bptr_cache *cache, uint64_t pool_idx)
{
   struct cache_pool_entry *pool_en = GET_POOL_EN(cache, pool_idx);
   _Bool is_head = pool_en->evict_prev == pool_idx,
         is_tail = pool_en->evict_next == pool_idx;

   if (is_head)
      cache->evict_head = pool_en->evict_next;
   else
      GET_POOL_EN(cache, pool_en->evict_prev)->evict_next =
         is_tail ? pool_en->evict_prev : pool_en->evict_next;

   if (is_tail)
      cache->evict_tail = pool_en->evict_prev;
   else
      GET_POOL_EN(cache, pool_en->evict_next)->evict_prev =
         is_head ? pool_en->evict_next : pool_en->evict_prev;
}


// Return oldest unloaded entry for eviction
// Caller should set refcnt to any non-1 value; otherwise, it leads to
// undefined behavior after removing last element from eviction list
static uint64_t evict_pop(struct bptr_cache *cache)
{
   uint64_t ret = cache->evict_head;

   bptr_errno = 0;

   if (GET_POOL_EN(cache, ret)->refcnt != 1)
    { bptr_errno = BPTR_E_NOT_FOUND; return 0; }

   evict_remove(cache, ret);
   return ret;
}


// This function increments cache->pool_sz on success
static uint64_t pool_free_pop(struct bptr_cache *cache)
{
   uint64_t pool_idx = cache->pool_free;
   struct cache_pool_entry *pool_en = GET_POOL_EN(cache, pool_idx);

   if (cache->pool_sz++ >= cache->pool_cap)
    {
      cache->pool_sz--;
      bptr_errno = BPTR_E_CACHE_FULL;
      return cache->pool_cap;
    }

   if (pool_en->evict_next == pool_idx) // Trailing free block
    {
      if (pool_idx + 1 < cache->pool_cap)
         POOL_EN_INCR(cache, pool_en)->evict_next = pool_idx + 1;
      cache->pool_free++;
    }
   else
      cache->pool_free = pool_en->evict_next;

   return pool_idx;
}


// This function decrements cache->pool_sz on success
static void pool_free_push(struct bptr_cache *cache, uint64_t pool_idx)
{
   GET_POOL_EN(cache, pool_idx)->evict_next = cache->pool_free;
   cache->pool_free = pool_idx;
   cache->pool_sz--;
}
/*-------------------------- Private Functions END ---------------------------*/
