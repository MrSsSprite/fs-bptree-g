# Node Cache Design

## Motivation

Currently every node access hits disk: `bptr_node_load` does malloc → fread → unmarshal, and `bptr_node_unload` does marshal → fwrite → free. Tree traversals (e.g. `bptr_find_node`) repeatedly load and unload the same internal nodes. A cache keeps recently touched nodes in RAM so subsequent accesses avoid disk I/O.

## Policy

Fixed-capacity pool. A node enters the cache whenever it is touched — both `bptr_node_load` (read from disk) and `bptr_node_new` (create) place the node in the cache.

When `bptr_node_unload` is called, the node is **not freed**. Instead it becomes *evictable* and joins a FIFO queue. The node unloaded longest ago is evicted first when space is needed.

If the pool is full and **every** node is still active (pinned by a caller, none yet unloaded), the operation fails — there is nothing safe to evict.

### Flush vs Evict

These are orthogonal operations:

| Operation | Persists to disk? | Removes from cache? |
|-----------|-------------------|---------------------|
| `flush`   | Yes               | No                  |
| `evict`   | No (drops changes)| Yes                 |

Callers compose them: flush to force-persist, evict to force-free, or flush-then-evict to persist-and-remove. For internal space reclamation the cache layer always flushes (if dirty) before evicting, so no data is lost on automatic eviction.

## Data Structure

### Overview

Two separate arrays:

- **Hash table** — open-addressing with Robin Hood probing. Maps `node_idx → pool_idx`. Entries are small (two words) so backward shift deletion is cheap and no tombstones are needed.
- **Cache pool** — array of `cache_capacity` slots. Each slot holds one `struct bptr_node` plus bookkeeping.

```
hash table                          cache pool
┌────────┬──────────┐              ┌─────────────────────────────┐
│node_idx│ pool_idx │──────────────▶ struct bptr_node (embedded) │
├────────┼──────────┤              │ refcount                    │
│node_idx│ pool_idx │───┐          │ node_idx (reverse key)      │
├────────┼──────────┤   │          │ evict_prev / evict_next     │
│  ...   │   ...    │   │          ├─────────────────────────────┤
└────────┴──────────┘   └──────────▶  ...                        │
                                   │                             │
                                   └─────────────────────────────┘
```

**Why separate?** Robin Hood probing requires backward shift deletion — shifting entries to fill gaps. Shifting a full `struct bptr_node` (with keys/vals pointers, metadata, etc.) would be expensive and error-prone. With a separate hash table the entries are just 16 bytes, making shift deletion fast and safe.

**Why Robin Hood?** Robin Hood hashing swaps entries during insertion so that no entry is ever far from its preferred position — probe distances stay short. Backward shift deletion naturally follows from this invariant and eliminates tombstone accumulation entirely.

### Pool entry state (reference counting)

A slot can be held by multiple callers simultaneously (e.g. two independent traversal paths loading the same node). Instead of a binary ACTIVE/INACTIVE flag, a reference count tracks how many callers currently hold the node:

```
refcount = 0  →  EMPTY     (slot unused)
refcount = 1  →  INACTIVE  (cached, no callers, evictable)
refcount ≥ 2  →  ACTIVE    (held by refcount − 1 callers)
```

| Operation | refcount transition |
|-----------|---------------------|
| `bptr_node_load` hits cache  | refcount++ |
| `bptr_node_load` misses, loads from disk into pool | refcount: 0→2 (one caller) |
| `bptr_node_new` creates node in pool | refcount: 0→2 (one caller) |
| `bptr_node_unload` | refcount−−; if it becomes 1, node joins eviction queue |
| Eviction | refcount: 1→0, slot freed |

When refcount drops from 2 to 1 the node is appended to the FIFO eviction queue tail. When refcount rises from 1 to 2 (a previously unloaded node is re-loaded from cache), it is removed from the queue.

### Eviction queue

A doubly-linked list threaded through the pool entries using `evict_prev` / `evict_next` indices. Head = oldest unload, tail = newest unload. Only entries with `refcount == 1` (INACTIVE) participate.

### Hash function (Fibonacci / multiplicative)

```c
static inline uint32_t cache_hash(bptr_node_t node_idx, uint32_t ht_capacity) {
    /* 2654435761 = floor(φ × 2^32) where φ = (√5−1)/2 */
    return (uint32_t)(((uint64_t)node_idx * 2654435761ULL) % (uint64_t)ht_capacity);
}
```

`ht_capacity` is a power of 2 so the modulo compiles to a bitmask.

### Robin Hood insertion

Each hash entry carries an implicit *probe distance* — how far it is from its preferred bucket:

```
probe_distance = (current_index − hash(node_idx) + ht_capacity) % ht_capacity
```

On insertion, if the new entry has a larger probe distance than the existing entry at that slot, swap them (the "rich" entry with short distance gives way to the "poor" entry with long distance). Continue with the displaced entry.

### Backward shift deletion

After removing entry at index `i`:

```
j = i
loop:
    next = (j + 1) % ht_capacity
    if ht[next].node_idx == 0: break       // reached empty, done
    dist = probe_distance(next)
    if dist == 0: break                     // at its preferred position, cannot shift
    ht[j] = ht[next]                        // shift back
    j = next
ht[j].node_idx = 0                          // clear vacated slot
```

No tombstone. Probe sequences stay intact.

### Hash table sizing

The hash table is larger than the pool to keep the load factor low under Robin Hood probing. A reasonable default: `ht_capacity = next_power_of_two(cache_capacity * 2)`. At most `cache_capacity` entries exist at once, so the load factor never exceeds 0.5.

### `struct bptr_node` — flex-array keys

The node struct is changed to inline the keys array. `vals` remains a pointer, repointed into the tail of the same allocation:

```c
// src/bptr_node.h
struct bptr_node {
    _Bool        is_dirty, is_leaf;
    bptr_node_t  node_idx;
    uint16_t     flags, level;
    uint32_t     key_count, checksum;
    bptr_node_t  parent, prev, next;
    void        *vals;           // pointer — must precede flex array
    unsigned char keys[];        // flex array — must be last member
};
```

`keys` is now part of the struct allocation. `vals` is set to `keys + max_keys_of_type × key_size` by the allocator. The entire node — metadata, keys, and vals — lives in one contiguous block.

### Allocation sizing

Leaf and branch nodes need different amounts of space for keys+vals. Since the node type is not known at allocation time (it's determined by level, which is read from disk or derived from the parent), every allocation is sized for the worst case.

Using the existing `node_bound` values (which already include the `+1` overflow slot for splits):

```
leaf_max_keys = node_bound.leaf.up      // includes overflow
brch_max_keys = node_bound.brch.up
brch_max_vals = node_bound.brch.up + 1  // one more child ptr than key

leaf_storage  = key_size × leaf_max_keys  +  value_size × leaf_max_keys
brch_storage  = key_size × brch_max_keys  +  ptr_size × brch_max_vals
node_buf_sz   = max(leaf_storage, brch_storage)
```

Total allocation for one node: `sizeof(struct bptr_node) + node_buf_sz`

After allocation, the caller sets `node->vals` based on actual type:

```
Leaf:   node->vals = node->keys + key_size × leaf_max_keys
Branch: node->vals = node->keys + key_size × brch_max_keys
```

The trade-off: entries of the "smaller" type waste some bytes (at most ~25% in extreme configs). Acceptable for a cache.

### Cache pool entry (opaque, in `src/bptr_cache.c`)

`struct bptr_node` ends with `keys[]`, so it must be the last field:

```c
struct cache_pool_entry {
    int32_t          refcount;    // 0=EMPTY, 1=INACTIVE, ≥2=ACTIVE
    bptr_node_t      node_idx;    // reverse key, for hash table removal on eviction
    int32_t          evict_prev;  // index in pool[]; −1 if head of FIFO queue
    int32_t          evict_next;  // index in pool[]; −1 if tail of FIFO queue
    struct bptr_node node;        // MUST be last — keys[] FAM extends past bookkeeping
};

struct bptr_cache {
    struct cache_ht_entry   *ht;
    uint32_t                 ht_capacity;
    struct cache_pool_entry *pool;          // allocated as one contiguous block
    uint32_t                 capacity;      // pool size (= self->cache_capacity)
    uint32_t                 node_buf_sz;   // extra bytes beyond sizeof(bptr_node)
    int32_t                  evict_head;    // −1 if queue empty
    int32_t                  evict_tail;
};
```

Pool allocation:

```c
size_t entry_size = sizeof(struct cache_pool_entry) + node_buf_sz;
pool = calloc(capacity, entry_size);
```

Indexing: `entry_at(i) = (struct cache_pool_entry *)((char*)pool + i * entry_size)`.

`evict_next` points to next free block if EMPTY, or 0 if every subsequent entry
are free.  This program adopt lazy approach, in the trailing huge free block,
only the first free entry's `evict_next` is initialized. Next free block needs
to be initialized when the first free entry in the free block is claimed.

On eviction nothing is individually freed — the keys[] and vals data dies with the pool slot.

### Caching is compulsory

There is no cacheless fallback. `_node_kv_malloc` is removed — every node allocation goes through the cache pool, which provides the inline `keys[]` + `vals` buffer. `bptr_node_free` becomes just `free(node)` — the single allocation covers everything.

### `struct bptr` additions (`src/bptr_internal.h`)

```c
uint32_t          cache_capacity;  // number of pool slots
uint32_t          node_buf_sz;     // extra bytes beyond sizeof(bptr_node) for keys+vals
struct bptr_cache *cache;
```

`cache_capacity` represents the node cache space only. `node_buf_sz` is computed at init/load time from `node_bound` and used for all node allocations. The hash table and bookkeeping add further overhead beyond what `cache_capacity` accounts for.

## New and Changed API

### Public (`bptree.h`)

- `bptr_init` and `bptr_load` gain a `uint32_t cache_capacity` parameter.
- `cache_capacity` must be ≥ 1. Caching is compulsory.
- New error code: `BPTR_E_CACHE_FULL` — returned when the cache pool is full and every slot has refcount ≥ 2.

### New cache module (`src/bptr_cache.h` / `src/bptr_cache.c`)

| Function | Purpose |
|----------|---------|
| `bptr_cache_init(self)` | Allocate hash table (`2 × capacity`, next power of 2) and pool. All pool slots refcount=0, all hash entries node_idx=0. |
| `bptr_cache_destroy(self)` | For each pool slot with refcount ≥ 1: flush if dirty. Free ht and pool arrays (inline buffers die with the pool). |
| `bptr_cache_fetch(self, node_idx)` | Hash lookup. Hit → refcount++ (if was 1, dequeue from FIFO), return &node. Miss → find empty pool slot (evicting oldest INACTIVE if needed), read from disk, insert into hash table, refcount=2, return &node. Returns NULL if no slot available. |
| `bptr_cache_release(self, node)` | refcount−−. If refcount becomes 1: append to eviction queue tail. Called by `bptr_node_unload`. |
| `bptr_cache_alloc(self, node_idx)` | Find empty pool slot (evicting oldest INACTIVE if needed), zero the embedded struct. Insert into hash table. refcount=2 (one caller — the creator). Called by `bptr_node_new`. |
| `bptr_cache_evict(self, node)` | Precondition: refcount == 1 (INACTIVE). Remove from eviction queue. Remove from hash table (backward shift). refcount = 0. Does NOT flush. Inline buffer is recycled with the slot (no separate free). |

### Node API changes (`src/bptr_node.h` / `src/bptr_node.c`)

- **`bptr_node_load`** — calls `bptr_cache_fetch`.
- **`bptr_node_unload`** — calls `bptr_cache_release`.
- **`bptr_node_new`** — restructured (see below).
- **`bptr_node_flush`** — unchanged: marshal to fbuf, write to disk, node stays cached.
- **`bptr_node_evict`** — **new**: calls `bptr_cache_evict`. Precondition: node must be INACTIVE (refcount == 1).
- **`bptr_node_free`** — simplified to `free(node)` only. With flex-array keys the entire node is one allocation.
- **`_node_kv_malloc`** — **removed**. Replaced by `vals = keys + max_key_bytes` pointer repoint after allocation.
- **`bptr_node_marshal` / `bptr_node_unmarshal`** — made non-static. Unmarshal reads into the inline `node.keys[]` and the repointed `node.vals`; both targets live in the single allocation.
- **`bptr_node_vacate`** — refactored: new `bptr_node_vacate_idx(self, node_idx)` helper taking a raw node index.

### `bptr_node_new` — restructured flow

Disk preallocation must happen before `bptr_cache_alloc` because the cache slot must be placed at the hash-correct position, which requires knowing `node_idx`:

```
1. bptr_node_prealloc(self)                 → node_idx
2. Load parent, read level, unload parent    → determines is_leaf
3. bptr_cache_alloc(self, node_idx)         → pool slot, refcount=2, inserted in hash table
4. Set fields: level, flags, is_leaf, is_dirty=1, key_count=0, parent, node_idx
5. Repoint node->vals into the inline keys[] tail
6. Return node

Error path: bptr_node_unload + bptr_node_evict + bptr_node_vacate_idx
           (unload drops refcount 2→1, evict removes from cache, vacate returns disk space)
```

Step order matters: prealloc uses `self->fbuf` for free-list I/O. Parent load/unload uses `self->fbuf`. `bptr_cache_alloc` may evict (uses `self->fbuf` for marshal+flush). These are sequential and single-threaded — fbuf contents from earlier steps are not needed by later steps, so no conflict.

### Core changes (`src/bptr_core.c`)

- `bptr_init` — store `cache_capacity`, call `bptr_cache_init` after `bptr_io_fcreat`.
- `bptr_load` — store `cache_capacity`, call `bptr_cache_init` after `bptr_io_fload`.
- `bptr_unload` — call `bptr_cache_destroy` before `bptr_io_fclose` (flushes all dirty, frees all memory).

## Edge Cases

| Scenario | Behaviour |
|----------|-----------|
| Cache miss, pool full, eviction queue empty | Return NULL, set bptr_errno to BPTR_E_CACHE_FULL |
| Fetch an INACTIVE node (refcount=1) | refcount++ (→2), remove from FIFO queue, return; no disk read |
| Fetch an already-ACTIVE node (refcount≥2) | refcount++, return same pointer |
| `bptr_node_unload` on an ACTIVE node | refcount−−; if refcount becomes 1, append to FIFO queue |
| `bptr_node_new` fails after prealloc + cache_alloc | Unload (2→1) + evict (1→0) + vacate disk space |
| `bptr_unload` with dirty cached nodes | `bptr_cache_destroy` iterates all pool slots, flushes if dirty, frees all |
| Explicit `bptr_node_evict` on a dirty node | Changes are dropped (caller should flush first if persistence matters) |
| `bptr_node_evict` on an ACTIVE node (refcount≥2) | Logic error — precondition violated. Assert/return error. |

## Compatibility with `feat/node/split`

The split branch calls `load`/`unload` multiple times (load parent, load sibling, create new node, etc.) and holds several node pointers simultaneously. All held nodes have refcount ≥ 2. The cache naturally supports this — as long as `cache_capacity` exceeds the maximum number of simultaneously active nodes (typically 3–4 for a split), no cache-full failure will occur.

## Implementation Order

1. **Flex-array keys** — change `struct bptr_node` in `bptr_node.h`: replace `void *keys, *vals` with `void *vals; unsigned char keys[]`. Remove `_node_kv_malloc` macro. Simplify `bptr_node_free` to single `free(node)`. Node allocation becomes `sizeof(struct bptr_node) + node_buf_sz` with `vals` repointed into the keys[] tail. Remove NULL checks on `keys`.
2. **Struct fields** — add `cache_capacity`, `node_buf_sz`, and `cache` to `struct bptr` in `bptr_internal.h`
3. **Error code** — add `BPTR_E_CACHE_FULL` to `bptree.h`
4. **Cache module** — create `src/bptr_cache.h` and `src/bptr_cache.c`:
   - Robin Hood hash table (insert, lookup, delete with backward shift)
   - FIFO eviction queue helpers
   - `bptr_cache_init` / `bptr_cache_destroy`
   - `bptr_cache_fetch` / `bptr_cache_release` / `bptr_cache_alloc` / `bptr_cache_evict`
5. **Expose internals** — declare `bptr_node_marshal`, `bptr_node_unmarshal`, `bptr_node_vacate_idx` in `bptr_node.h`
6. **Rewrite node layer** — rewrite load/unload/new to call cache functions; add `bptr_node_evict`; refactor vacate; make marshal/unmarshal non-static
7. **Wire up core** — init/load compute `node_buf_sz` and call `bptr_cache_init`; unload calls `bptr_cache_destroy`; update signatures
8. **Update public API** — add `cache_capacity` param to `bptr_init` and `bptr_load` declarations
9. **Cleanup** — remove stale `TODO: involve cache mechanism` comments
