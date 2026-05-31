# Cache Implementation TODO

## Checklist

### Step 1: Flex-array keys (struct change + transitional allocation)
- [x] 1a. Change `struct bptr_node` in `src/bptr_node.h`: replace `void *keys, *vals` with `void *vals; unsigned char keys[]`
- [ ] 1b. Adapt `_node_kv_malloc` in `src/bptr_node.c`: repoint `vals` into `keys[]` tail instead of malloc (transitional — do NOT remove yet)
- [ ] 1c. Update `bptr_node_new` allocation: `calloc(1, sizeof(struct bptr_node) + self->node_bound.buf_sz)`
- [ ] 1d. Update `bptr_node_load` allocation: `malloc(sizeof(struct bptr_node) + self->node_bound.buf_sz)`
- [ ] 1e. Simplify `bptr_node_free` to single `free(node)` — remove `free(node->keys); free(node->vals)`
- [ ] 1f. Fix `bptr_node_unmarshal` error path: remove `free(node->keys); free(node->vals)` (inline, no separate alloc)
- [ ] 1g. Remove any NULL checks on `node->keys` (flex array is never NULL)

### Step 2: Add cache-related fields to `struct bptr` + compute `node_buf_sz`
- [x] 2a. Forward-declare `struct bptr_cache` in `src/bptr_internal.h`
- [x] 2b. Add `node_buf_sz`, `cache` fields to `struct bptr` in `src/bptr_internal.h`
- [x] 2c. Compute `node_buf_sz` in `bptr_init` (using plan's overflow-inclusive formulas: `leaf.up`, `brch.up+1`)
- [x] 2d. Compute `node_buf_sz` in `bptr_load` (same formulas as 2c)
- [x] 2e. Add `#define BPTR_E_CACHE_FULL (5)` to `bptree.h`

### Step 3: Error code + min capacity constant
- [x] 3a. Add `#define BPTR_CACHE_CAPACITY_MIN (2)` to `bptree.h`

### Step 4: Cache module (`src/bptr_cache.h` + `src/bptr_cache.c`)
- [x] 4a. Create `src/bptr_cache.h`: define `struct cache_ht_entry`, `struct cache_pool_entry`, `struct bptr_cache` (with `free_head`), `CACHE_ENTRY_OF` macro, function declarations
- [x] 4b. Create `src/bptr_cache.c` with includes (`bptr_cache.h`, `bptr_node.h`, `bptr_io.h`)
- [ ] 4c. Implement `next_power_of_two` helper
- [x] 4d. Implement `cache_hash` (Fibonacci multiplicative) and `probe_distance` helpers
- [x] 4e. Implement Robin Hood hash table: `ht_lookup`, `ht_insert` (with swap-on-probe), `ht_delete` (backward shift)
- [x] 4f. Implement FIFO eviction queue: `evict_queue_append`, `evict_queue_remove`, `evict_queue_pop` (handle empty/single/head/tail/middle)
- [ ] 4g. Implement lazy free list: `POOL_ENTRY` macro, `free_list_pop` (with lazy init of trailing block), `free_list_push`
- [ ] 4h. Implement `bptr_cache_init`: allocate ht (2× capacity, next power of 2) and pool, init free_head=0, evict_head/tail=-1
- [ ] 4i. Implement `bptr_cache_destroy`: iterate pool, flush dirty, free ht+pool, assert no active refcount
- [ ] 4j. Implement `bptr_cache_fetch`: hash lookup → hit(refcount++, dequeue if was 1) / miss(find slot, evict if needed, flush dirty victim, fread, unmarshal, ht_insert, refcount=2). Handle flush failure (return victim to queue).
- [ ] 4k. Implement `bptr_cache_release`: assert refcount>=2, decrement, if becomes 1 append to eviction queue; add `pool_idx_of` helper
- [ ] 4l. Implement `bptr_cache_alloc`: find slot/evict, zero node, refcount=2, ht_insert; same eviction+flush logic as fetch
- [ ] 4m. Implement `bptr_cache_evict`: assert refcount==1, dequeue, ht_delete, free_list_push. Does NOT flush.

### Step 5: Expose internals (marshal, unmarshal, vacate_idx)
- [ ] 5a. Make `bptr_node_marshal` / `bptr_node_unmarshal` non-static; declare in `src/bptr_node.h`
- [ ] 5b. Move `iter_write`, `iter_tc_write`, `iter_read`, `iter_tc_read`, `buf_tc_write`, `buf_tc_read`, `_node_val_arr_size`, `_node_val_size` macros from `bptr_node.c` to `bptr_node.h`
- [ ] 5c. Add `bptr_node_vacate_idx(self, node_idx)` in `bptr_node.c`; declare in `bptr_node.h`
- [ ] 5d. Refactor `bptr_node_vacate(self, node)` as one-line wrapper calling `bptr_node_vacate_idx`

### Step 6: Rewrite node layer (load, unload, new, evict)
- [ ] 6a. Rewrite `bptr_node_load` → one-liner calling `bptr_cache_fetch(self, node_idx)`
- [ ] 6b. Rewrite `bptr_node_unload` → flush-if-dirty, then `bptr_cache_release(self, node)`
- [ ] 6c. Rewrite `bptr_node_new` → restructured flow: prealloc → load parent (determine level) → unload parent → `bptr_cache_alloc` → set fields → repoint `vals`. Error paths: `CACHE_ALLOC_ERR` (vacate disk, no unload/evict needed), `LOAD_PARENT_ERR` (vacate disk)
- [ ] 6d. Update `bptr_node_unmarshal`: remove `_node_kv_malloc` call; repoint `vals = keys + key_size * (is_leaf ? leaf.up : brch.up)` after reading level; read data directly into inline buffer; remove old malloc error path
- [ ] 6e. Add `bptr_node_evict(self, node)` → calls `bptr_cache_evict`; declare in `bptr_node.h`
- [ ] 6f. Fix `bptr_node_flush`: set `node->is_dirty = 0` on successful flush
- [ ] 6g. Remove `bptr_node_free` (nodes live in pool, not individually alloc'd; only caller was old `bptr_node_unload`)
- [ ] 6h. Remove `_node_kv_malloc` macro (no longer used)

### Step 7: Wire up core (init, load, unload)
- [ ] 7a. Add `uint32_t cache_capacity` param to `bptr_init` (declaration + definition); validate >= BPTR_CACHE_CAPACITY_MIN; store; call `bptr_cache_init` after `bptr_io_fcreat`; cleanup cache on error
- [ ] 7b. Add `uint32_t cache_capacity` param to `bptr_load` (declaration + definition); validate; store; call `bptr_cache_init` after `bptr_io_fload` + `_bptr_bound_set`; cleanup cache on error
- [ ] 7c. Update `bptr_unload`: call `bptr_cache_destroy(self)` BEFORE `bptr_io_fclose(self)`
- [ ] 7d. Remove stale TODOs: `bptr_node_unload` "involve cache", `bptr_io_fclose` "flush cached nodes", `bptr_core.c` "cache pool should be involved later"

### Step 8: Update public API declarations
- [ ] 8a. Update `bptr_init` declaration in `bptree.h` with `uint32_t cache_capacity` param
- [ ] 8a. Update `bptr_load` declaration in `bptree.h` with `uint32_t cache_capacity` param
- [ ] 8b. Add `bptr_node_evict` / `bptr_node_flush` declarations to `bptree.h` if callers need explicit cache control

### Step 9: Cleanup and final verification
- [ ] 9a. Grep for `TODO.*cache` and remove/update all stale comments
- [ ] 9b. Remove `node_bound.buf_sz` from struct + computation if fully replaced by `node_buf_sz`
- [ ] 9c. Manually verify all 8 edge cases from the plan's table
- [ ] 9d. Remove dead code: `bptr_node_free` (done in 6g), `_node_kv_malloc` (done in 6h), any orphaned helpers

---

## Pre-implementation Notes

### Key Design Decisions

- **No cacheless fallback.** Every node lives in the cache pool. `cache_capacity >= 2` is required (plan says >= 1, but any real traversal needs at least parent+child simultaneously active).
- **Allocation is "one and done."** The pool pre-allocates all slots up front (`calloc`). Individual nodes are never malloc'd or free'd separately — they live embedded in pool entries.
- **Refcount scheme:** 0 = EMPTY, 1 = INACTIVE (cached, evictable), >= 2 = ACTIVE (held by refcount−1 callers).
- **FIFO eviction:** oldest unload evicted first. Automatic eviction (during fetch/alloc) flushes dirty before evicting. Explicit eviction (`bptr_cache_evict`) does NOT flush.
- **fbuf is a scratch buffer.** `self->fbuf` is a single shared heap allocation; every operation (fread_node, marshal, unmarshal, flush_node, prealloc, vacate) reads from or writes to it. This is safe as long as each operation fully consumes what it put in fbuf before the next operation starts. The one subtle risk: cache eviction (triggered by `bptr_cache_fetch` on miss or `bptr_cache_alloc` when pool is full) internally calls marshal→flush on the victim, which overwrites fbuf. So: **don't hold un-consumed data in fbuf across a call that may evict.** In practice all code paths in this plan already satisfy this — each step fully drains fbuf before the next step runs.

### Critical Warnings

1. **Do NOT remove `_node_kv_malloc` in step 1.** The plan says to remove it in the flex-array step, but `bptr_node_new` and `bptr_node_unmarshal` still call it. If removed, the build breaks until step 6 wires up the cache. Adapt it to allocate a single flex-array-compatible block instead, and defer actual removal to step 6.
2. **`struct cache_ht_entry` is undefined in the plan.** It must be defined before use.
3. **`free_head` field is missing from `struct bptr_cache`.** The lazy free-list needs a head pointer.
4. **`bptr_node_flush` must set `is_dirty = 0` on success.** Currently it doesn't. In a cache world where nodes persist after flush, this is essential.
5. **Unmarshal must repoint `node->vals`** into the inline flex-array buffer after reading `level` and determining `is_leaf`.

---

## Step 1: Flex-array keys (struct change + transitional allocation)

### Files: `src/bptr_node.h`, `src/bptr_node.c`, `src/bptr_internal.h`

### What to do

#### 1a. Change `struct bptr_node` to use flex-array `keys`

In `src/bptr_node.h`, change:

```c
// BEFORE
struct bptr_node
{
   _Bool is_dirty, is_leaf;
   bptr_node_t node_idx;
   uint16_t flags, level;
   uint32_t key_count, checksum;
   bptr_node_t parent, prev, next;
   void *keys, *vals;
};

// AFTER
struct bptr_node
{
   _Bool is_dirty, is_leaf;
   bptr_node_t node_idx;
   uint16_t flags, level;
   uint32_t key_count, checksum;
   bptr_node_t parent, prev, next;
   void        *vals;           // pointer — must precede flex array
   unsigned char keys[];        // flex array — must be last member
};
```

**Caution:** `vals` must remain a pointer (it gets repointed into `keys[]` tail). `keys` becomes a flex array. The order is critical — flex array must be the last member.

#### 1b. Transitional: adapt `_node_kv_malloc` for single-block allocation

In `src/bptr_node.c`, change `_node_kv_malloc` to allocate ONE block instead of two separate mallocs. Use `self->node_bound.buf_sz` for sizing.

```c
// REPLACE the current _node_kv_malloc (two separate mallocs) with:
#define _node_kv_malloc(self, node) do \
{ \
   /* Single allocation: keys is the flex array, vals points into the tail */ \
   /* node_bound.buf_sz already covers max(leaf, brch) storage */ \
   /* Note: this transitional macro exists until step 6 wires up the cache */ \
   if ((node)->is_leaf) \
      (node)->vals = (node)->keys + (self)->key_size * (self)->node_bound.leaf.up; \
   else \
      (node)->vals = (node)->keys + (self)->key_size * (self)->node_bound.brch.up; \
} while (0)
```

**Caution:** `_node_kv_malloc` no longer does any `malloc`. It only repoints `vals`. The memory comes from the allocation that created the `struct bptr_node`. In the transitional period, `bptr_node_new` still `calloc`s the struct. `bptr_node_load` still `malloc`s the struct. Both need to allocate enough extra space for the flex array.

#### 1c. Update `bptr_node_new` allocation size

In `bptr_node_new` (line 233), change from `sizeof(struct bptr_node)` to include the buffer:

```c
// BEFORE
node = calloc(1, sizeof(struct bptr_node));

// AFTER
node = calloc(1, sizeof(struct bptr_node) + self->node_bound.buf_sz);
```

**Caution:** `self->node_bound.buf_sz` is computed in `_bptr_bound_set` and `bptr_init`/`bptr_load`. It currently uses `leaf.up - 1` for sizing. The flex-array buffer should be sized for the overflow case. Check if `node_bound.buf_sz` is large enough (see Step 2 about `node_buf_sz`).

#### 1d. Update `bptr_node_load` allocation size

In `bptr_node_load` (line 304), similarly:

```c
// BEFORE
node = malloc(sizeof(struct bptr_node));

// AFTER
node = malloc(sizeof(struct bptr_node) + self->node_bound.buf_sz);
```

#### 1e. Simplify `bptr_node_free`

In `bptr_node_free` (line 281-285), change from three frees to one:

```c
// BEFORE
void bptr_node_free(struct bptr_node *node)
{
   free(node->keys); free(node->vals);
   free(node);
}

// AFTER
void bptr_node_free(struct bptr_node *node)
{
   free(node);
}
```

**Caution:** This becomes the ONLY free needed — `keys` is inline (not a separate allocation) and `vals` points into the same block. This is correct during the transitional period because the node was allocated as `sizeof(struct bptr_node) + buf_sz`.

#### 1f. Fix `bptr_node_unmarshal` error path

In the error path of `bptr_node_unmarshal` (lines 393-396), remove the `free(node->keys); free(node->vals);` calls since they're no longer separate allocations:

```c
// BEFORE
if (node->keys == NULL || node->vals == NULL)
 {
   free(node->keys); free(node->vals);
   return 1;
 }

// AFTER (keys is inline, cannot be NULL; vals is a repointed pointer, cannot fail)
// Remove this NULL check entirely for keys/vals.
// The only failure would be if the caller didn't allocate enough space,
// which is a logic error, not a runtime alloc failure.
```

#### 1g. Remove NULL checks on `node->keys`

With flex-array `keys`, `node->keys` is never NULL (it's part of the struct). Remove any NULL checks on `node->keys` in the codebase.

### Cautions

- `node->keys` as a flex array means `sizeof(struct bptr_node)` no longer includes ANY key storage. All allocations must add the buffer size.
- `_node_val_size` and `_node_val_arr_size` macros (private in bptr_node.c) are unchanged; they don't depend on allocation.
- This step does NOT change behavior — it only changes how memory is laid out. The code should still compile and run correctly.

### Verification

- Build succeeds with `make` (or equivalent).
- Existing functionality works: insert, find, load, unload.
- Valgrind shows no memory errors on a small test.

---

## Step 2: Add cache-related fields to `struct bptr` + compute `node_buf_sz`

### Files: `src/bptr_internal.h`, `src/bptr_core.c`, `bptree.h`

### What to do

#### 2a. Forward-declare `struct bptr_cache` in `src/bptr_internal.h`

Near the top of the file (after includes, before `struct bptr`):

```c
struct bptr_cache;  // forward declaration; full definition in bptr_cache.h
```

#### 2b. Add fields to `struct bptr`

Inside `struct bptr` (in `src/bptr_internal.h`), add:

```c
/* Node cache */
uint32_t          cache_capacity;  // number of pool slots
uint32_t          node_buf_sz;     // extra bytes beyond sizeof(bptr_node) for keys+vals
struct bptr_cache *cache;
```

**Caution:** `node_buf_sz` is separate from the existing `node_bound.buf_sz`. The plan's `node_buf_sz` uses larger sizing (includes overflow slot). Keep both for now — `node_buf_sz` for cache allocation, `node_bound.buf_sz` for legacy transitional code in step 1.

#### 2c. Compute `node_buf_sz` in `bptr_init`

After `_bptr_bound_set(self)` and the existing `node_bound.buf_sz` computation, add:

```c
{
   size_t leaf_storage =
      self->node_bound.leaf.up * self->key_size +
      self->node_bound.leaf.up * self->value_size;
   size_t brch_storage =
      self->node_bound.brch.up * self->key_size +
      (self->node_bound.brch.up + 1) * BPTR_PTR_SIZE;
   self->node_buf_sz = (leaf_storage > brch_storage ? leaf_storage : brch_storage);
}
```

#### 2d. Compute `node_buf_sz` in `bptr_load`

Add the same computation after the existing `node_bound.buf_sz` block in `bptr_load`.

#### 2e. Add `BPTR_E_CACHE_FULL` error code to `bptree.h`

In the system errors section (positive values):

```c
#define BPTR_E_CACHE_FULL (4)      // Cache pool exhausted, all slots active
```

### Cautions

- Use `struct bptr_cache *` (pointer), not the full struct. This avoids circular includes.
- `node_buf_sz` formula uses `leaf.up` (not `leaf.up - 1`) and `brch.up + 1` vals — this is intentionally larger than `node_bound.buf_sz` to accommodate overflow slots during splits.
- `BPTR_E_CACHE_FULL` is a system-side error (positive), consistent with the error code convention.

### Verification

- Build succeeds.
- `node_buf_sz` is computed correctly (can printf-debug in init).

---

## Step 3: Error code + min capacity constant

### Files: `bptree.h`

### What to do

#### 3a. Add cache capacity constants

```c
#define BPTR_CACHE_CAPACITY_MIN (2)   // minimum: need at least parent+child
```

**Rationale:** The plan says >= 1, but any tree traversal loads a parent then a child before unloading the parent, requiring at least 2 simultaneously active slots. A single-slot cache would immediately fail.

### Verification

- Build succeeds.

---

## Step 4: Cache module (`src/bptr_cache.h` + `src/bptr_cache.c`)

### Files: `src/bptr_cache.h` (new), `src/bptr_cache.c` (new), `Makefile`

### What to do

#### 4a. Create `src/bptr_cache.h`

```c
#ifndef BPTR_CACHE_H
#define BPTR_CACHE_H

#include "bptr_internal.h"

/* ---- cache_ht_entry (MUST be defined — missing from plan) ---- */
struct cache_ht_entry {
    bptr_node_t node_idx;  // 0 = empty slot
    int32_t     pool_idx;  // index into pool[], -1 = unused
};

/* ---- container_of helper ---- */
#define CACHE_ENTRY_OF(node_ptr) \
    ((struct cache_pool_entry *)((char *)(node_ptr) - offsetof(struct cache_pool_entry, node)))

struct cache_pool_entry {
    int32_t          refcount;    // 0=EMPTY, 1=INACTIVE, >=2=ACTIVE
    bptr_node_t      node_idx;    // reverse key for hash table removal
    int32_t          evict_prev;  // -1 if head/not-in-queue
    int32_t          evict_next;  // -1 if tail/not-in-queue
    struct bptr_node node;        // MUST be last — keys[] FAM extends past
};

struct bptr_cache {
    struct cache_ht_entry   *ht;
    uint32_t                 ht_capacity;
    struct cache_pool_entry *pool;
    uint32_t                 capacity;
    uint32_t                 node_buf_sz;
    int32_t                  evict_head;   // -1 if queue empty
    int32_t                  evict_tail;   // -1 if queue empty
    int32_t                  free_head;    // -1 if no free slots (MISSING from plan!)
    size_t                   entry_size;   // sizeof(pool_entry) + node_buf_sz
};

int  bptr_cache_init(struct bptr *self);
void bptr_cache_destroy(struct bptr *self);
struct bptr_node *bptr_cache_fetch(struct bptr *self, bptr_node_t node_idx);
void bptr_cache_release(struct bptr *self, struct bptr_node *node);
struct bptr_node *bptr_cache_alloc(struct bptr *self, bptr_node_t node_idx);
void bptr_cache_evict(struct bptr *self, struct bptr_node *node);

#endif
```

**Caution:** `free_head` is added — it was missing from the plan. It tracks the first EMPTY pool slot via the lazy free list.

#### 4b. Create `src/bptr_cache.c`

Include headers:

```c
#include "bptr_cache.h"
#include "bptr_node.h"
#include "bptr_io.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
```

#### 4c. Implement `next_power_of_two` helper

```c
static inline uint32_t next_power_of_two(uint32_t n) {
    if (n == 0) return 1;
    n--;
    n |= n >> 1; n |= n >> 2; n |= n >> 4;
    n |= n >> 8; n |= n >> 16;
    return n + 1;
}
```

#### 4d. Implement hash function

```c
static inline uint32_t cache_hash(bptr_node_t node_idx, uint32_t ht_capacity) {
    return (uint32_t)(((uint64_t)node_idx * 2654435761ULL) % (uint64_t)ht_capacity);
}

static inline uint32_t probe_distance(uint32_t idx, bptr_node_t node_idx,
                                       uint32_t ht_capacity) {
    return (idx - cache_hash(node_idx, ht_capacity) + ht_capacity) % ht_capacity;
}
```

#### 4e. Implement Robin Hood hash table operations

**Lookup:**

```c
// Returns pool_idx, or -1 if not found
static int32_t ht_lookup(struct bptr_cache *cache, bptr_node_t node_idx) {
    uint32_t idx = cache_hash(node_idx, cache->ht_capacity);
    uint32_t dist = 0;
    while (cache->ht[idx].node_idx != 0) {
        if (dist > probe_distance(idx, cache->ht[idx].node_idx, cache->ht_capacity))
            return -1;  // not found (probe would have swapped past it)
        if (cache->ht[idx].node_idx == node_idx)
            return cache->ht[idx].pool_idx;
        idx = (idx + 1) % cache->ht_capacity;
        dist++;
    }
    return -1;
}
```

**Insert:**

```c
// Returns 0 on success, -1 if table is full (shouldn't happen with load factor < 0.5)
static int ht_insert(struct bptr_cache *cache, bptr_node_t node_idx, int32_t pool_idx) {
    bptr_node_t cur_key = node_idx;
    int32_t cur_val = pool_idx;
    uint32_t idx = cache_hash(node_idx, cache->ht_capacity);
    uint32_t cur_dist = 0;
    uint32_t capacity = cache->ht_capacity;

    while (1) {
        if (cache->ht[idx].node_idx == 0) {
            cache->ht[idx].node_idx = cur_key;
            cache->ht[idx].pool_idx = cur_val;
            return 0;
        }
        uint32_t existing_dist = probe_distance(idx, cache->ht[idx].node_idx, capacity);
        if (cur_dist > existing_dist) {
            // Swap: rich (short probe) yields to poor (long probe)
            bptr_node_t tmp_key = cache->ht[idx].node_idx;
            int32_t tmp_val = cache->ht[idx].pool_idx;
            cache->ht[idx].node_idx = cur_key;
            cache->ht[idx].pool_idx = cur_val;
            cur_key = tmp_key;
            cur_val = tmp_val;
            cur_dist = existing_dist;
        }
        idx = (idx + 1) % capacity;
        cur_dist++;
        if (cur_dist >= capacity) return -1;  // should never happen
    }
}
```

**Delete (backward shift):**

```c
static void ht_delete(struct bptr_cache *cache, bptr_node_t node_idx) {
    uint32_t idx = cache_hash(node_idx, cache->ht_capacity);
    uint32_t capacity = cache->ht_capacity;

    // Find the entry
    while (cache->ht[idx].node_idx != node_idx) {
        if (cache->ht[idx].node_idx == 0) return;  // not found
        idx = (idx + 1) % capacity;
    }

    // Backward shift deletion
    uint32_t j = idx;
    while (1) {
        uint32_t next = (j + 1) % capacity;
        if (cache->ht[next].node_idx == 0) break;
        uint32_t dist = probe_distance(next, cache->ht[next].node_idx, capacity);
        if (dist == 0) break;
        cache->ht[j] = cache->ht[next];
        j = next;
    }
    cache->ht[j].node_idx = 0;
    cache->ht[j].pool_idx = -1;
}
```

**Important:** Robin Hood insertion uses `uint32_t` for distances. Since `ht_capacity` is a power of 2 and at most 2× cache_capacity (which is typically small), distances won't overflow.

#### 4f. Implement FIFO eviction queue operations

```c
// Append to tail (when refcount drops 2→1)
static void evict_queue_append(struct bptr_cache *cache, int32_t pool_idx) {
    struct cache_pool_entry *entry = POOL_ENTRY(cache, pool_idx);
    entry->evict_prev = cache->evict_tail;
    entry->evict_next = -1;
    if (cache->evict_tail >= 0)
        POOL_ENTRY(cache, cache->evict_tail)->evict_next = pool_idx;
    else
        cache->evict_head = pool_idx;  // queue was empty
    cache->evict_tail = pool_idx;
}

// Remove from queue (when refcount rises 1→2)
static void evict_queue_remove(struct bptr_cache *cache, int32_t pool_idx) {
    struct cache_pool_entry *entry = POOL_ENTRY(cache, pool_idx);
    if (entry->evict_prev >= 0)
        POOL_ENTRY(cache, entry->evict_prev)->evict_next = entry->evict_next;
    else
        cache->evict_head = entry->evict_next;  // was head
    if (entry->evict_next >= 0)
        POOL_ENTRY(cache, entry->evict_next)->evict_prev = entry->evict_prev;
    else
        cache->evict_tail = entry->evict_prev;  // was tail
    entry->evict_prev = entry->evict_next = -1;
}

// Pop from head (oldest unload — for eviction)
static int32_t evict_queue_pop(struct bptr_cache *cache) {
    int32_t head = cache->evict_head;
    if (head < 0) return -1;
    evict_queue_remove(cache, head);
    return head;
}
```

**Caution:** Handle all 4 queue states: empty, single entry, head removal, tail removal, middle removal. The `evict_prev`/`evict_next` = -1 sentinel must be consistent.

#### 4g. Implement free list operations

```c
// Index into pool by entry index
#define POOL_ENTRY(cache, i) \
    ((struct cache_pool_entry *)((char *)(cache)->pool + (i) * (cache)->entry_size))

// Pop a free slot. Returns -1 if none available (and eviction queue is also empty).
static int32_t free_list_pop(struct bptr_cache *cache) {
    if (cache->free_head < 0) return -1;
    int32_t slot = cache->free_head;
    struct cache_pool_entry *entry = POOL_ENTRY(cache, slot);

    // Lazy init: if evict_next is 0 (from calloc), initialize next slot
    if (entry->evict_next == 0 && (uint32_t)(slot + 1) < cache->capacity) {
        POOL_ENTRY(cache, slot + 1)->evict_next = 0;
        cache->free_head = slot + 1;
    } else {
        cache->free_head = entry->evict_next;
    }
    // evict_prev is stale for EMPTY slots — cleared when slot is claimed
    entry->evict_next = -1;
    return slot;
}

// Push a freed slot back to free list (called after eviction)
static void free_list_push(struct bptr_cache *cache, int32_t slot) {
    struct cache_pool_entry *entry = POOL_ENTRY(cache, slot);
    entry->evict_next = cache->free_head;
    cache->free_head = slot;
}
```

**Caution:** The lazy approach: `calloc` zeros everything, so initially all `evict_next` are 0. `pool[0].evict_next == 0` means "everything from 0 to capacity-1 is free." When claiming slot 0, we initialize slot 1's `evict_next = 0`. This way we only initialize entries as they're about to be needed.

#### 4h. Implement `bptr_cache_init`

```c
int bptr_cache_init(struct bptr *self) {
    struct bptr_cache *cache;

    cache = calloc(1, sizeof(struct bptr_cache));
    if (cache == NULL) { bptr_errno = BPTR_E_OOM; return -1; }

    cache->capacity = self->cache_capacity;
    cache->node_buf_sz = self->node_buf_sz;
    cache->entry_size = sizeof(struct cache_pool_entry) + cache->node_buf_sz;

    // Hash table: next power of 2 >= capacity * 2
    cache->ht_capacity = next_power_of_two(cache->capacity * 2);
    cache->ht = calloc(cache->ht_capacity, sizeof(struct cache_ht_entry));
    if (cache->ht == NULL) { free(cache); bptr_errno = BPTR_E_OOM; return -1; }
    // ht entries already zeroed: node_idx=0 (empty), pool_idx=0 (but treated as -1 logically)
    // Actually need pool_idx=-1 for empty. Let's initialize:
    for (uint32_t i = 0; i < cache->ht_capacity; i++)
        cache->ht[i].pool_idx = -1;

    // Pool: calloc zeroes everything → all refcount=0 (EMPTY), evict_next=0
    cache->pool = calloc(cache->capacity, cache->entry_size);
    if (cache->pool == NULL) { free(cache->ht); free(cache); bptr_errno = BPTR_E_OOM; return -1; }

    cache->evict_head = cache->evict_tail = -1;
    cache->free_head = 0;   // first slot; evict_next=0 means all subsequent are free

    self->cache = cache;
    return 0;
}
```

#### 4i. Implement `bptr_cache_destroy`

```c
void bptr_cache_destroy(struct bptr *self) {
    struct bptr_cache *cache = self->cache;
    if (cache == NULL) return;

    for (uint32_t i = 0; i < cache->capacity; i++) {
        struct cache_pool_entry *entry = POOL_ENTRY(cache, i);
        if (entry->refcount >= 1 && entry->node.is_dirty) {
            // Flush dirty before destroy
            bptr_node_marshal(self, &entry->node);
            // Ignore flush errors on destroy — best effort
            bptr_io_flush_node(self, entry->node.node_idx);
        }
        // Assert no active callers
        assert(entry->refcount < 2 && "Node still active during cache destroy");
    }
    free(cache->ht);
    free(cache->pool);  // all inline buffers die with pool
    free(cache);
    self->cache = NULL;
}
```

**Caution:** `bptr_node_marshal` must be accessible (declared in bptr_node.h, see step 5). The assert catches callers who forgot to unload nodes before `bptr_unload`.

#### 4j. Implement `bptr_cache_fetch`

```c
struct bptr_node *bptr_cache_fetch(struct bptr *self, bptr_node_t node_idx) {
    struct bptr_cache *cache = self->cache;
    assert(node_idx != 0);  // 0 is invalid (empty tree / sentinel)

    int32_t pool_idx = ht_lookup(cache, node_idx);

    if (pool_idx >= 0) {
        // Cache HIT
        struct cache_pool_entry *entry = POOL_ENTRY(cache, pool_idx);
        if (entry->refcount == 1) {
            // INACTIVE → ACTIVE: remove from eviction queue
            evict_queue_remove(cache, pool_idx);
        }
        entry->refcount++;
        return &entry->node;
    }

    // Cache MISS — need to load from disk
    // Find a free slot (may need to evict)
    int32_t slot = free_list_pop(cache);
    if (slot < 0) {
        // No free slots — try to evict the oldest INACTIVE
        slot = evict_queue_pop(cache);
        if (slot < 0) {
            // Nothing to evict — cache is full with all active
            bptr_errno = BPTR_E_CACHE_FULL;
            return NULL;
        }
        // Evict: flush if dirty, remove from hash table, refcount 1→0
        struct cache_pool_entry *victim = POOL_ENTRY(cache, slot);
        if (victim->node.is_dirty) {
            bptr_node_marshal(self, &victim->node);
            if (bptr_io_flush_node(self, victim->node.node_idx) == 0) {
                // Flush failed — put victim BACK in eviction queue, return error
                evict_queue_append(cache, slot);
                return NULL;  // bptr_errno already set by io_flush_node
            }
        }
        ht_delete(cache, victim->node_idx);
        victim->refcount = 0;  // now EMPTY, will be reused below
    }

    // Now slot is ours. Read from disk.
    struct cache_pool_entry *entry = POOL_ENTRY(cache, slot);
    if (bptr_io_fread_node(self, node_idx)) {
        // Read failed — return slot to free list
        free_list_push(cache, slot);
        return NULL;  // bptr_errno already set
    }

    if (bptr_node_unmarshal(self, &entry->node)) {
        free_list_push(cache, slot);
        return NULL;
    }

    entry->node.node_idx = node_idx;
    entry->node.is_dirty = 0;
    entry->node_idx = node_idx;
    entry->refcount = 2;  // one caller (ACTIVE)

    ht_insert(cache, node_idx, slot);

    return &entry->node;
}
```

**Caution on eviction flush failure:** If flush fails, the victim is returned to the eviction queue (still INACTIVE). The fetch returns NULL. The dirty data is preserved in cache.

#### 4k. Implement `bptr_cache_release`

```c
void bptr_cache_release(struct bptr *self, struct bptr_node *node) {
    struct bptr_cache *cache = self->cache;
    struct cache_pool_entry *entry = CACHE_ENTRY_OF(node);

    assert(entry->refcount >= 2);  // must be ACTIVE; double-unload is a bug

    entry->refcount--;
    if (entry->refcount == 1) {
        // INACTIVE: add to eviction queue
        evict_queue_append(cache, (int32_t)(entry - cache->pool) / cache->entry_size);
        // Actually, we need a helper: pool_idx_of(cache, entry)
        // Let's use a proper calculation:
    }
}
```

**Caution:** Need a helper to convert entry pointer → pool index. Since pool entries are variably sized, use:

```c
static inline int32_t pool_idx_of(struct bptr_cache *cache, struct cache_pool_entry *entry) {
    ptrdiff_t offset = (char *)entry - (char *)cache->pool;
    return (int32_t)(offset / (ptrdiff_t)cache->entry_size);
}
```

#### 4l. Implement `bptr_cache_alloc`

```c
struct bptr_node *bptr_cache_alloc(struct bptr *self, bptr_node_t node_idx) {
    struct bptr_cache *cache = self->cache;
    assert(node_idx != 0);

    // Find a free slot (may evict)
    int32_t slot = free_list_pop(cache);
    if (slot < 0) {
        slot = evict_queue_pop(cache);
        if (slot < 0) {
            bptr_errno = BPTR_E_CACHE_FULL;
            return NULL;
        }
        struct cache_pool_entry *victim = POOL_ENTRY(cache, slot);
        if (victim->node.is_dirty) {
            bptr_node_marshal(self, &victim->node);
            if (bptr_io_flush_node(self, victim->node.node_idx) == 0) {
                evict_queue_append(cache, slot);
                return NULL;
            }
        }
        ht_delete(cache, victim->node_idx);
        victim->refcount = 0;
    }

    struct cache_pool_entry *entry = POOL_ENTRY(cache, slot);
    // Zero the node portion (not the bookkeeping)
    memset(&entry->node, 0, sizeof(struct bptr_node));
    entry->refcount = 2;  // one caller (ACTIVE)
    entry->node_idx = node_idx;
    entry->evict_prev = entry->evict_next = -1;

    ht_insert(cache, node_idx, slot);

    return &entry->node;
}
```

#### 4m. Implement `bptr_cache_evict`

```c
void bptr_cache_evict(struct bptr *self, struct bptr_node *node) {
    struct bptr_cache *cache = self->cache;
    struct cache_pool_entry *entry = CACHE_ENTRY_OF(node);

    assert(entry->refcount == 1);  // precondition: must be INACTIVE

    // Remove from eviction queue
    evict_queue_remove(cache, pool_idx_of(cache, entry));

    // Remove from hash table
    ht_delete(cache, entry->node_idx);

    // Return to free list
    entry->refcount = 0;  // EMPTY
    entry->evict_prev = -1;  // stale — clear for safety
    free_list_push(cache, pool_idx_of(cache, entry));
}
```

**Caution:** `bptr_cache_evict` does NOT flush. If the node is dirty, changes are lost. Caller should flush first if persistence matters.

### Verification

- Build succeeds (cache module compiles, links).
- All edge cases from the plan's table can be traced through the code.
- The free list, eviction queue, and hash table are independently testable.

---

## Step 5: Expose internals (marshal, unmarshal, vacate_idx)

### Files: `src/bptr_node.h`, `src/bptr_node.c`

### What to do

#### 5a. Make `bptr_node_marshal` and `bptr_node_unmarshal` non-static

In `src/bptr_node.c`, remove the `static inline` qualifiers. In `src/bptr_node.h`, add declarations:

```c
void bptr_node_marshal(struct bptr *self, struct bptr_node *node);
int  bptr_node_unmarshal(struct bptr *self, struct bptr_node *node);
```

#### 5b. Move serialization macros to `src/bptr_node.h`

The macros `iter_write`, `iter_tc_write`, `iter_read`, `iter_tc_read`, `buf_tc_write`, `buf_tc_read` are needed by `bptr_node_marshal`/`bptr_node_unmarshal` which are now called from `bptr_cache.c`. Move them from `bptr_node.c` to `bptr_node.h` (before the function declarations).

Also move `_node_val_arr_size` and `_node_val_size` macros to `bptr_node.h`.

**Caution:** These macros use `self` and `node` as parameters — they expect specific variable names in scope. Document this!

#### 5c. Add `bptr_node_vacate_idx`

In `src/bptr_node.c`, add:

```c
int bptr_node_vacate_idx(struct bptr *self, bptr_node_t node_idx) {
    uint16_t flags = 0;
    void *buf_it = self->fbuf;

    iter_write(buf_it, &flags, 2);
    if (self->is_lite) {
        BPTR_LITE_PTR_TYPE head = self->free_list.head;
        memcpy(buf_it, &head, sizeof(head));
    } else {
        BPTR_NORM_PTR_TYPE head = self->free_list.head;
        memcpy(buf_it, &head, sizeof(head));
    }

    if (bptr_io_flush_node(self, node_idx) == 0)
        return 2;  // flush failed, bptr_errno set by io_flush_node

    self->free_list.head = node_idx;
    self->free_list.cnt++;
    return 0;
}
```

In `src/bptr_node.h`, add:

```c
int bptr_node_vacate_idx(struct bptr *self, bptr_node_t node_idx);
```

#### 5d. Refactor `bptr_node_vacate` as a wrapper

The existing `bptr_node_vacate(struct bptr *self, struct bptr_node *node)` becomes:

```c
static inline
int bptr_node_vacate(struct bptr *self, struct bptr_node *node) {
    return bptr_node_vacate_idx(self, node->node_idx);
}
```

### Cautions

- The `iter_*` macros use `buf_it` as a local variable. Callers of marshal/unmarshal should not rely on `buf_it` being preserved.
- `_node_val_arr_size` depends on `self->is_lite` and `node->is_leaf` — ensure `self` and `node` are in scope at the call site.
- The `#define _WRITE_FL_HEAD` / `#undef` pattern in the original `bptr_node_vacate` should be cleaned up (use proper if/else as shown above).

### Verification

- Build succeeds.
- `bptr_node_vacate` still works correctly (it's now a thin wrapper).

---

## Step 6: Rewrite node layer (load, unload, new, evict)

### Files: `src/bptr_node.h`, `src/bptr_node.c`

### What to do

#### 6a. Rewrite `bptr_node_load`

```c
struct bptr_node *bptr_node_load(struct bptr *self, bptr_node_t node_idx) {
    return bptr_cache_fetch(self, node_idx);
}
```

Everything (hash lookup, disk read, unmarshal, refcount management) is handled by `bptr_cache_fetch`.

#### 6b. Rewrite `bptr_node_unload`

```c
int bptr_node_unload(struct bptr *self, struct bptr_node *node) {
    // Flush if dirty before releasing (preserves existing behavior: unload = persist + release)
    if (node->is_dirty) {
        if (bptr_node_flush(self, node) == 0)
            return 2;  // flush failed; bptr_errno already set
    }
    bptr_cache_release(self, node);
    return 0;
}
```

**Caution:** The plan says unload just calls `bptr_cache_release` (which does NOT flush). But the existing `bptr_node_unload` contract says it flushes dirty nodes. We preserve that: flush-then-release. The cache's automatic eviction also flushes, but explicit unload should still flush eagerly.

#### 6c. Rewrite `bptr_node_new`

Follow the plan's restructured flow:

```c
struct bptr_node *bptr_node_new(struct bptr *self, bptr_node_t parent) {
    uint16_t flags;
    struct bptr_node *node;
    bptr_node_t node_idx;

    // Step 1: Preallocate disk space
    node_idx = bptr_node_prealloc(self);
    if (node_idx == 0) goto PREALLOC_ERR;

    // Step 2: Determine is_leaf from parent's level
    int is_leaf;
    if (parent) {
        struct bptr_node *parent_node = bptr_node_load(self, parent);
        if (parent_node == NULL) { bptr_errno = 2; goto LOAD_PARENT_ERR; }
        is_leaf = (parent_node->level == 1);  // leaf is level 0, so child of level-1 is leaf
        // Actually: node->level = parent_node->level - 1; is_leaf = (node->level == 0)
        uint16_t child_level = parent_node->level - 1;
        is_leaf = (child_level == 0);
        if (bptr_node_unload(self, parent_node)) { bptr_errno = 200; goto LOAD_PARENT_ERR; }
    } else {
        // Root split: new root is one level above old root
        is_leaf = 0;  // new root is always branch
    }

    // Step 3: Allocate cache slot
    node = bptr_cache_alloc(self, node_idx);
    if (node == NULL) goto CACHE_ALLOC_ERR;  // bptr_errno already set

    // Step 4: Set fields
    if (parent)
        node->level = parent_node->level - 1;
    else
        node->level = self->height++;

    flags = BPTR_NODE_FLAG_VALID;
    if (node->level == 0) {
        flags |= BPTR_NODE_FLAG_LEAF;
        node->is_leaf = 1;
    } else {
        node->is_leaf = 0;
    }
    node->flags = flags;
    node->is_dirty = 1;
    node->key_count = 0;
    node->parent = parent;
    node->node_idx = node_idx;

    // Step 5: Repoint vals into inline keys[] tail
    if (node->is_leaf)
        node->vals = node->keys + self->key_size * self->node_bound.leaf.up;
    else
        node->vals = node->keys + self->key_size * self->node_bound.brch.up;

    return node;

CACHE_ALLOC_ERR:
    // cache_alloc failed — node is NOT in cache, just vacate disk space
    bptr_node_vacate_idx(self, node_idx);
    return NULL;
LOAD_PARENT_ERR:
    // parent load/unload failed — vacate disk space
    bptr_node_vacate_idx(self, node_idx);
PREALLOC_ERR:
    return NULL;
}
```

**Caution — critical error path distinctions:**
- If `bptr_cache_alloc` itself fails: node is NOT in cache. Just vacate disk space. No unload/evict needed.
- If cache_alloc succeeds but a later step fails (unlikely since everything after cache_alloc is just field assignment): unload (refcount 2→1) + evict (1→0) + vacate disk space. (Not expected to happen in practice since field assignments don't fail.)

**Caution — parent unloading in middle of new:**
- `parent_node` is loaded in step 2 and unloaded in step 2. If it was the only ACTIVE reference, the parent becomes INACTIVE (joins eviction queue). This is fine — it may be evicted later, but its data was already flushed when it was first created. If it's dirty, `bptr_node_unload` flushes it.

#### 6d. Update `bptr_node_unmarshal` for inline buffer

The unmarshal is called by `bptr_cache_fetch` on a cache miss. The node already lives in the pool with pre-allocated inline buffer. Unmarshal must repoint `vals` after reading `level`:

```c
int bptr_node_unmarshal(struct bptr *self, struct bptr_node *node) {
    void *buf_it = self->fbuf;

    iter_read(buf_it, &node->flags, 2);
    iter_read(buf_it, &node->level, 2);
    iter_read(buf_it, &node->key_count, 4);
    iter_read(buf_it, &node->checksum, 4);
    if (self->is_lite) {
        iter_tc_read(buf_it, node->parent, uint32_t);
        iter_tc_read(buf_it, node->next, uint32_t);
        iter_tc_read(buf_it, node->prev, uint32_t);
    } else {
        iter_tc_read(buf_it, node->parent, uint64_t);
        iter_tc_read(buf_it, node->next, uint64_t);
        iter_tc_read(buf_it, node->prev, uint64_t);
    }
    buf_it = (char*)self->fbuf + BPTR_NODE_METADATA_BYTE;

    node->is_leaf = (node->level == 0);
    node->is_dirty = 0;

    // Repoint vals into the inline flex-array buffer
    // (replaces the old _node_kv_malloc call)
    if (node->is_leaf)
        node->vals = node->keys + self->key_size * self->node_bound.leaf.up;
    else
        node->vals = node->keys + self->key_size * self->node_bound.brch.up;

    // Read keys and vals data directly into the inline buffer
    iter_read(buf_it, node->keys, self->key_size * node->key_count);
    iter_read(buf_it, node->vals, _node_val_arr_size(self, node));

    return 0;
}
```

**Caution:** `node->vals` is repointed BEFORE the data is read into it. The repoint uses `node->is_leaf` which was just set from `node->level`. This is correct and safe.

**Caution:** The error path that previously called `free(node->keys); free(node->vals)` is removed. There's no separate allocation to free — the inline buffer lives in the pool entry. If reading fails, the pool slot is returned to the free list by the caller (`bptr_cache_fetch`).

#### 6e. Add `bptr_node_evict` (new public function)

In `src/bptr_node.c`:

```c
void bptr_node_evict(struct bptr *self, struct bptr_node *node) {
    bptr_cache_evict(self, node);
}
```

In `src/bptr_node.h`:

```c
void bptr_node_evict(struct bptr *self, struct bptr_node *node);
```

#### 6f. Update `bptr_node_flush` to clear `is_dirty`

```c
bptr_node_t bptr_node_flush(struct bptr *self, struct bptr_node *node) {
    bptr_node_marshal(self, node);
    bptr_node_t result = bptr_io_flush_node(self, node->node_idx);
    if (result != 0)
        node->is_dirty = 0;  // successfully persisted
    return result;
}
```

**Caution:** The plan says flush is "unchanged" but in a cache world where nodes persist after flush, `is_dirty` MUST be cleared on success. Otherwise the node looks dirty forever, causing unnecessary re-flushes during eviction.

#### 6g. Remove `bptr_node_free`

`bptr_node_free` is no longer safe to call — nodes live in the cache pool, not as individual mallocs. Either:
- Remove it entirely (if no external callers), or
- Make it a no-op with an assertion that it should never be called.

Check for any callers of `bptr_node_free` outside `bptr_node_unload`. In the current code, `bptr_node_free` is called only from `bptr_node_unload` (line 295). After the rewrite, `bptr_node_unload` calls `bptr_cache_release` instead. So `bptr_node_free` can be removed.

#### 6h. Remove `_node_kv_malloc` macro

It's no longer used. Remove it from `src/bptr_node.c`.

### Verification

- `bptr_node_new` succeeds and returns a node with correctly repointed `vals`.
- `bptr_node_load` caches nodes correctly; second load of same node_idx hits cache.
- `bptr_node_unload` flushes dirty and releases to cache.
- `bptr_node_evict` removes from cache.
- Error paths: prealloc failure, cache-full failure, parent-load failure all clean up correctly.

---

## Step 7: Wire up core (init, load, unload)

### Files: `src/bptr_core.c`, `src/bptr_node.c`

### What to do

#### 7a. Update `bptr_init` signature and implementation

In `bptree.h` and `src/bptr_core.c`, add `uint32_t cache_capacity` parameter:

```c
struct bptr *bptr_init(
    const char *filename,
    _Bool is_lite,
    uint32_t node_size,
    uint16_t key_size,
    uint16_t value_size,
    int (*compare)(const void *lhs, const void *rhs),
    uint32_t cache_capacity   // NEW
);
```

In `bptr_init` body:
- Validate `cache_capacity >= BPTR_CACHE_CAPACITY_MIN` (2).
- Store `self->cache_capacity = cache_capacity`.
- After `bptr_io_fcreat(self, filename)` succeeds, compute `node_buf_sz` (if not already done in step 2c) and call `bptr_cache_init(self)`.
- On error, if cache was initialized, destroy it before freeing self.

#### 7b. Update `bptr_load` signature and implementation

In `bptree.h` and `src/bptr_core.c`, add `uint32_t cache_capacity` parameter:

```c
struct bptr *bptr_load(
    const char *filename,
    int (*compare)(const void *lhs, const void *rhs),
    uint32_t cache_capacity   // NEW
);
```

In `bptr_load` body:
- Validate `cache_capacity >= BPTR_CACHE_CAPACITY_MIN`.
- Store `self->cache_capacity = cache_capacity`.
- After `bptr_io_fload(self, filename)` succeeds and `_bptr_bound_set` completes, compute `node_buf_sz` and call `bptr_cache_init(self)`.
- On error, cleanup cache before freeing self.

#### 7c. Update `bptr_unload`

Add `bptr_cache_destroy(self)` before `bptr_io_fclose(self)`:

```c
int bptr_unload(struct bptr *self) {
    int err_code = 0;

    bptr_cache_destroy(self);  // flushes all dirty cached nodes, frees pool

    if (bptr_io_fclose(self))
        err_code = BPTR_E_FCLOSE;
    free(self);

    return err_code;
}
```

**Caution:** `bptr_cache_destroy` must come BEFORE `bptr_io_fclose` because destroy may need to flush nodes using `self->fbuf` and `self->file`. If fclose runs first, those are invalid.

#### 7d. Remove stale TODO comments

In `bptr_node_unload` (the old one at line 292): removed as part of rewrite in step 6.
In `bptr_io_fclose` (line 208, "TODO: flush cached nodes"): this is now handled by `bptr_cache_destroy` called before fclose. Remove the TODO.
In `bptr_core.c` (line 169, "TODO: temporary implementation; cache pool should be involved later"): `bptr_node_new` now uses the cache. Remove the TODO.

### Verification

- `bptr_init` and `bptr_load` succeed with valid cache_capacity.
- `bptr_init` and `bptr_load` fail with invalid cache_capacity (< 2).
- `bptr_unload` flushes all dirty cached nodes and frees all memory.
- Valgrind shows no leaks on full init → insert → unload cycle.

---

## Step 8: Update public API declarations

### Files: `bptree.h`

### What to do

#### 8a. Update function declarations

```c
struct bptr *bptr_init(
    const char *filename,
    _Bool is_lite,
    uint32_t node_size,
    uint16_t key_size,
    uint16_t value_size,
    int (*compare)(const void *lhs, const void *rhs),
    uint32_t cache_capacity
);

struct bptr *bptr_load(
    const char *filename,
    int (*compare)(const void *lhs, const void *rhs),
    uint32_t cache_capacity
);
```

#### 8b. Add `bptr_node_evict` and `bptr_node_flush` to public API if needed

If callers need explicit cache control, declare:

```c
void   bptr_node_evict(struct bptr *self, struct bptr_node *node);
bptr_node_t bptr_node_flush(struct bptr *self, struct bptr_node *node);
```

(These may already be declared in `bptr_node.h`; ensure public visibility.)

### Cautions

- This is a breaking API change. All existing callers of `bptr_init` and `bptr_load` must add the `cache_capacity` argument.
- Update any test code and CLI entry points.

### Verification

- Build succeeds with updated signatures.
- Any test programs compile with the new argument.

---

## Step 9: Cleanup and final verification

### Files: `src/bptr_node.c`, `src/bptr_core.c`, `src/bptr_io.c`

### What to do

#### 9a. Remove all remaining "TODO: involve cache mechanism" comments

Grep for `TODO.*cache` and remove or update each.

#### 9b. Remove `node_bound.buf_sz` usage if fully replaced by `node_buf_sz`

After step 6, the transitional `_node_kv_malloc` is removed. If nothing uses `node_bound.buf_sz`, remove it from `struct bptr` and the computation in `bptr_init`/`bptr_load`.

#### 9c. Final edge case testing

Manually verify each scenario from the plan's edge cases table:

| Scenario | How to verify |
|----------|---------------|
| Cache miss, pool full, eviction queue empty | Fill cache with active nodes, try loading another → BPTR_E_CACHE_FULL |
| Fetch INACTIVE node | Load, unload, load again → should hit cache, no disk read |
| Fetch ACTIVE node | Load same node twice without unloading → refcount 3, same pointer |
| Unload ACTIVE → INACTIVE | Load, unload → refcount goes 2→1, node in eviction queue |
| New node error path | Prealloc succeeds, cache_alloc fails → disk space properly vacated |
| Unload with dirty nodes | Modified node in cache, bptr_unload → all flushed before free |
| Explicit evict on dirty | Changes dropped (acceptable per design) |
| Evict on ACTIVE | Should assert/error |

#### 9d. Clean up dead code

- `bptr_node_free` — removed (step 6g).
- `_node_kv_malloc` — removed (step 6h).
- Any orphaned helper functions.

### Verification

- Full test suite passes.
- Valgrind shows no memory leaks or errors.
- All edge cases behave as specified.

---

## Summary of Issues Found in the Plan

| # | Severity | Issue | Resolution in this TODO |
|---|----------|-------|-------------------------|
| 1 | blocker | `struct cache_ht_entry` undefined | Defined in step 4a |
| 2 | blocker | `free_head` missing from `struct bptr_cache` | Added in step 4a |
| 3 | blocker | Removing `_node_kv_malloc` in step 1 breaks build | Step 1 adapts it; step 6 removes it |
| 4 | blocker | `vals` repointing in unmarshal not specified | Step 6d: unmarshal repoints after reading level |
| 5 | blocker | `bptr_node_free` would free cache-embedded memory | Removed in step 6g |
| 6 | blocker | `bptr_node_flush` doesn't clear `is_dirty` | Fixed in step 6f |
| 7 | blocker | `node_idx == 0` not handled by cache | assert added in step 4j/4l |
| 8 | blocker | Refcount underflow on double-unload | assert added in step 4k |
| 9 | major | `node_buf_sz` vs `node_bound.buf_sz` discrepancy | Both kept; `node_buf_sz` is cache-specific (step 2) |
| 10 | major | `BPTR_E_CACHE_FULL` no numeric value | Assigned value 4 (step 2e) |
| 11 | major | Missing container_of macro | Added in step 4a |
| 12 | major | Cache API signatures incomplete | Full signatures in step 4 |
| 13 | major | Circular include risk | Forward declaration in step 2a |
| 14 | major | Eviction flush failure not handled | Victim returned to queue on flush fail (step 4j) |
| 15 | major | Step 4 (cache module) needs step 5 (exposed internals) | Steps kept in plan order but both must compile together |
| 16 | major | `cache_capacity` minimum too low (1 → 2) | Raised to 2 (step 3a) |
| 17 | major | `bptr_node_evict` signature incomplete | Full signature in step 4m/6e |
| 18 | major | `iter_*` macros private but needed by cache | Moved to bptr_node.h (step 5b) |
| 19 | major | `bptr_node_vacate_idx` relationship to `bptr_node_vacate` | vacate becomes wrapper (step 5d) |
| 20 | minor | Queue removal edge cases underspecified | Handled in step 4f with all 4 cases |
| 21 | minor | int32_t index limits | Not a practical concern; cache_capacity is small |
| 22 | minor | Stale `evict_prev` in EMPTY slots | Documented; cleared in evict (step 4m) |
| 23 | note | Checksum TODOs out of scope | Preserved as-is |
| 24 | note | Buffer overrun fix in overflow scenarios | Plan's sizing is correct and intentional |
