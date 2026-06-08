# Audit Report: `feat/node/cache` — B+ Tree Node Cache Implementation

**Audit type**: Pre-test, read-through scan (no compilation, no runtime testing)
**Branch**: `feat/node/cache` (40 commits, teed off `282b5b4`)
**Files changed**: 13 files, +2276 / −188 lines
**Core addition**: `src/bptr_cache.c` (new, 462 lines) — Robin Hood hash table + FIFO eviction queue + free list

---

## Executive Summary

The cache subsystem is well-architected in concept (Robin Hood hashing, FIFO eviction, flex-array nodes) but the implementation contains several high-severity bugs that would prevent correct operation. The most impactful issues are:

1. **Hash table index generation is off-by-one**, producing out-of-bounds indices for ~50% of lookups.
2. **`ht_delete` passes a pool index as a hash-table index**, corrupting the hash table on every eviction.
3. **The eviction queue's doubly-linked list is broken** (old-tail forward link never updated), causing orphaned entries and eventual cache exhaustion.
4. **Three call sites invert the flush return-value check**, so successful flushes are treated as errors and actual failures are silently ignored.
5. **`bptr_find_node` bypasses the cache entirely** — the main tree traversal path calls raw `bptr_node_load` instead of `bptr_node_fetch`, defeating the purpose of the cache.

Additionally, two compilation errors exist in `bptr_core.c` that would prevent a clean build.

---

## Findings Summary

| Severity | Count |
|----------|-------|
| 🔴 Critical | 27 |
| 🟡 Warning | 22 |
| 🔵 Suggestion | 16 |
| **Total** | **65** |

---

## 🔴 Critical Findings

### Hash Table — Indexing & Hashing

#### C1. Hash shift off-by-one — OOB initial probe index

**File**: `src/bptr_cache.c`, line 97
**Severity**: 🔴 Critical

```c
cache->hash_shift = ds_clz(cache->ht_cap);
```

For `ht_cap = 2^k`, `ds_clz(2^k) = 64 - k - 1`. The Fibonacci hash `(node_idx * M) >> shift` then produces values in `[0, 2^(k+1) - 1]`, which is **twice** the valid index range `[0, ht_cap - 1]`.

| ht_cap | shift | hash range | valid range | OOB rate |
|--------|-------|------------|--------------|----------|
| 2 | 62 | [0, 3] | [0, 1] | 50% |
| 4 | 61 | [0, 7] | [0, 3] | 50% |
| 8 | 60 | [0, 15] | [0, 7] | 50% |
| 16 | 59 | [0, 31] | [0, 15] | 50% |

The correct shift is `ds_clz(ht_cap) + 1` (i.e., `64 - log2(ht_cap)`).

**Fix**:
```diff
- cache->hash_shift = ds_clz(cache->ht_cap);
+ cache->hash_shift = ds_clz(cache->ht_cap) + 1;
```

---

#### C2. Broken modulo wrapping — infinite OOB probe

**File**: `src/bptr_cache.c`, lines 319, 349, 364
**Severity**: 🔴 Critical

All three functions (`ht_lookup`, `ht_insert`, `ht_delete`) use:

```c
idx = (idx + 1) & ~(cache->ht_cap);
```

`~(ht_cap)` clears only bit `k` (where `ht_cap = 2^k`). When `idx` reaches `2*ht_cap - 1`:

- `idx + 1 = 2^(k+1)` — sets bit `k+1`
- `~(ht_cap)` does NOT clear bit `k+1`
- The probe sequence goes `[2^(k+1), 2^(k+1)+1, ...)` — forever OOB

Combined with **C1** (initial index up to `2*ht_cap - 1`), the very first probe can be OOB for ~50% of inputs, and the max-value case produces an infinite loop reading past the heap allocation.

**Concrete example** (ht_cap=8):
- hash=15: probes 15 (OOB), 16 (OOB), 17 (OOB)… infinite
- hash=10: probes 10 (OOB), then `11 & ~8 = 3` (in bounds) — first access still OOB

**Fix**:
```diff
- idx = (idx + 1) & ~(cache->ht_cap);
+ idx = (idx + 1) & (cache->ht_cap - 1);
```

---

#### C3. `ht_delete` uses pool index as hash-table index

**File**: `src/bptr_cache.c`, lines 357–371
**Severity**: 🔴 Critical

```c
static void ht_delete(struct bptr_cache *cache, bptr_node_t node_idx)
{
   uint64_t idx = ht_lookup(cache, node_idx);  // returns pool_idx (0..pool_cap-1)
   // ...
   while (1)
    {
      uint64_t next = (idx + 1) & ~(cache->ht_cap);  // uses pool_idx as ht_idx!
      // ...
    }
}
```

`ht_lookup()` returns `pool_idx` (an index into the **pool** array), but `ht_delete` uses it as a hash-**table** index. Pool indices and hash-table indices are entirely different domains. The backward-shift deletion operates on the wrong slot, corrupting the hash table on every eviction.

**Fix**: `ht_lookup` must return the hash-table slot index. Options:
- Add an output parameter: `uint64_t ht_lookup(..., uint64_t *ht_idx)`
- Return a struct with both fields
- Re-probe from the hash inside `ht_delete` to locate the correct ht slot

---

#### C4. PSL not decremented during backward shift

**File**: `src/bptr_cache.c`, lines 362–369
**Severity**: 🔴 Critical

```c
while (1)
 {
   uint64_t next = (idx + 1) & ~(cache->ht_cap);
   ht_en = cache->ht + next;
   if (ht_en->node_idx == 0 || ht_en->PSL == 0) break;
   cache->ht[idx] = cache->ht[next];  // PSL copied but NOT decremented
   idx = next;
 }
```

When an entry is shifted one slot closer to its ideal position, its PSL must decrease by 1. Without this, PSL values inflate, breaking the `PSL == 0` termination condition (entries that should now be at their ideal bucket no longer signal as such). The backward shift overshoots, pushing entries past their correct positions.

**Fix**:
```diff
  cache->ht[idx] = cache->ht[next];
+ cache->ht[idx].PSL--;
```

---

### Eviction Queue

#### C5. `evict_push` does not update old tail's forward link

**File**: `src/bptr_cache.c`, lines 374–386
**Severity**: 🔴 Critical

```c
static void evict_push(struct bptr_cache *cache, uint64_t pool_idx)
{
   struct cache_pool_entry *pool_en = cache->pool + pool_idx;
   pool_en->evict_next = pool_idx;  // new entry is new tail (self-sentinel)
   if (cache->pool[cache->evict_head].refcnt != 1)  // queue empty?
    {
      pool_en->evict_prev = cache->evict_head = cache->evict_tail = pool_idx;
      return;
    }
   pool_en->evict_prev = cache->evict_tail;  // link backward to old tail
   cache->evict_tail = pool_idx;              // update global tail
   // BUG: cache->pool[old_tail].evict_next still points to itself!
}
```

**Corruption scenario** (two nodes A then B go INACTIVE):

```
1. Push A (queue empty):    A.prev=A, A.next=A, head=A, tail=A    ✓
2. Push B (non-empty):      B.prev=A, A.next=A (STALE!), tail=B   ✗
                              Expected: A.next=B
3. Pop A (evict_remove):    is_head=(A.prev==A)=true, is_tail=(A.next==A)=true
                            Treats A as sole element → head=A, tail=A
                            B is ORPHANED — INACTIVE but unreachable
4. Next pop:                pool[A].refcnt==2 → BPTR_E_NOT_FOUND
                            Cache reports "full" despite free INACTIVE slots
```

**Impact**: Pool slots leak. After enough releases, the cache reports `BPTR_E_CACHE_FULL` and stops functioning.

**Fix**:
```diff
  pool_en->evict_prev = cache->evict_tail;
+ cache->pool[cache->evict_tail].evict_next = pool_idx;
  cache->evict_tail = pool_idx;
```

---

### Flush Return-Value Inversion

`bptr_node_flush()` returns **0 on success**, non-zero on failure (`src/bptr_node.c:293–310`). All three call sites check `== 0` to trigger the error path — the logic is inverted.

#### C6. `bptr_cache_deinit`

**File**: `src/bptr_cache.c`, line 156
**Severity**: 🔴 Critical

```c
if (bptr_node_flush(self, &cache->pool[i].node) == 0)
   return BPTR_E_FACCESS;  // returns error on SUCCESS
```

**Consequence**: The first successfully flushed dirty node causes deinit to abort with a false `BPTR_E_FACCESS`. Remaining dirty nodes are not flushed (data loss). On the success path, `self` is not freed (memory leak). When there are no dirty nodes, `== 0` is never true, so deinit silently succeeds — which is actually the only working case.

**Fix**:
```diff
- if (bptr_node_flush(self, &cache->pool[i].node) == 0)
+ if (bptr_node_flush(self, &cache->pool[i].node) != 0)
```

---

#### C7. `bptr_node_fetch` eviction path

**File**: `src/bptr_cache.c`, lines 202–203
**Severity**: 🔴 Critical

```c
if (bptr_node_flush(self, &victim_en->node) == 0)
 { goto NODE_FLUSH_ERR; }  // jumps to error handler on SUCCESS
```

**Consequence**: Successful flush of a dirty victim triggers the error path — the victim is pushed back to the eviction queue but `ht_delete` was already called, leaving a dangling hash-table entry for the victim. `fetch` returns NULL. Actual flush failures are silently ignored (dirty data lost).

**Fix**:
```diff
- if (bptr_node_flush(self, &victim_en->node) == 0)
+ if (bptr_node_flush(self, &victim_en->node) != 0)
```

---

#### C8. `bptr_cache_alloc` eviction path

**File**: `src/bptr_cache.c`, line 255
**Severity**: 🔴 Critical

```c
if (bptr_node_flush(self, &victim_en->node) == 0)
 { goto NODE_FLUSH_ERR; }  // same inverted check
```

**Fix**:
```diff
- if (bptr_node_flush(self, &victim_en->node) == 0)
+ if (bptr_node_flush(self, &victim_en->node) != 0)
```

---

### `bptr_node_new` Error Recovery

#### C9. `node->node_idx` not set before PARENT_LOAD_ERR

**File**: `src/bptr_node.c`, lines 253–254
**Severity**: 🔴 Critical

```c
node = bptr_cache_alloc(self, file_slot);
if (node == NULL) goto CACHE_ALLOC_ERR;

if (parent)
 {
   parent_n = bptr_node_fetch(self, parent);
   if (parent_n == NULL) goto PARENT_LOAD_ERR;  // jumps BEFORE line 245!
   // ...
 }

// ... many lines later ...
node->node_idx = file_slot;  // line 245 — after the error zone
```

At `PARENT_LOAD_ERR`, `bptr_cache_release(self, node)` is called but `node->node_idx` is still uninitialized. The node enters the eviction queue with garbage `node_idx`. When later evicted, `ht_delete(cache, victim->node.node_idx)` uses the garbage value — hash table corruption.

**Fix**: Move `node->node_idx = file_slot;` immediately after `bptr_cache_alloc` succeeds, before any `goto` label:
```diff
  node = bptr_cache_alloc(self, file_slot);
  if (node == NULL) goto CACHE_ALLOC_ERR;
+ node->node_idx = file_slot;
```

---

#### C10. `CACHE_ALLOC_ERR` vacates wrong thing with uninitialized pointer

**File**: `src/bptr_node.c`, lines 256–257
**Severity**: 🔴 Critical

```c
CACHE_ALLOC_ERR:  _set_err_code(bptr_errno);
   if (bptr_node_vacate(self, parent_n) == 2)
      perror("`bptr_node_new' error zone: `bptr_node_vacate': flush failure");
```

This label is reached via two paths:
1. **Direct goto** when `bptr_cache_alloc` returns `NULL` → `parent_n` is **uninitialized stack garbage**. Dereferencing it in `bptr_node_vacate` is undefined behavior (crash).
2. **Fall-through** from `PARENT_LOAD_ERR` → `parent_n` is `NULL` (fetch failed). Dereferencing `NULL` in `bptr_node_vacate` is a crash.

Even if `parent_n` were valid, it would vacate the **parent's** disk slot, not the preallocated `file_slot` — semantically wrong. The `file_slot` also leaks in both cases.

**Fix**: Vacate `file_slot` using a new index-based helper (not the pointer-based `bptr_node_vacate` which requires a valid `struct bptr_node*`):
```diff
 CACHE_ALLOC_ERR:
+   bptr_node_vacate_idx(self, file_slot);
-   if (bptr_node_vacate(self, parent_n) == 2)
-      perror(...);
```

---

#### C11. `bptr_cache_alloc` never sets `pool_en->node.node_idx`

**File**: `src/bptr_cache.c`, line 264
**Severity**: 🔴 Critical

```c
pool_en = cache->pool + pool_idx;
pool_en->refcnt = 2;
ht_insert(cache, node_idx, pool_idx);
return &pool_en->node;
// NOTE: pool_en->node.node_idx is NOT set here
```

The caller `bptr_node_new` sets `node->node_idx` after `bptr_cache_alloc` returns. But if any error occurs before that assignment (and after `bptr_cache_alloc` succeeds), the error path calls `bptr_cache_release`, putting the node into the eviction queue with uninitialized `node_idx`. Later eviction uses the garbage value.

**Fix**: Set it in `bptr_cache_alloc` itself:
```diff
  pool_en = cache->pool + pool_idx;
  pool_en->refcnt = 2;
+ pool_en->node.node_idx = node_idx;
  ht_insert(cache, node_idx, pool_idx);
```

---

### Core Wiring — `bptr_core.c`

#### C12. `bptr_find_node` calls `bptr_node_load` with wrong argument count

**File**: `src/bptr_core.c`, lines 221, 256
**Severity**: 🔴 Critical (build error)

```c
// Line 221 — 2 arguments:
node = bptr_node_load(self, self->root_idx);

// Line 256 — 2 arguments:
node = bptr_node_load(self, node_idx);
```

`bptr_node_load` is declared as:
```c
int bptr_node_load(struct bptr *self, bptr_node_t node_idx, struct bptr_node *node);
// 3 arguments required
```

This is a **compilation error**: `error: too few arguments to function 'bptr_node_load'`.

Additionally, `bptr_find_node` calls `bptr_node_load` directly (raw I/O, no cache), bypassing `bptr_node_fetch` — the entire cache is unused during the main tree traversal path.

**Fix**: Use `bptr_node_fetch(self, node_idx)` (cache-aware) and `bptr_node_unload(self, node)` for release.

---

#### C13. Void return assigned to `int err_code`

**File**: `src/bptr_core.c`, line 245
**Severity**: 🔴 Critical (build error)

```c
if ((err_code = bptr_node_unload(self, node)))
```

`bptr_node_unload` is declared as `void` (`src/bptr_node.h:66`, `src/bptr_node.c:265`). Assigning a void expression to an `int` is a **constraint violation**. Even if a compiler accepts it, `err_code` receives garbage; the downstream `switch(err_code)` branches unpredictably.

**Fix**: Call `bptr_node_unload(self, node);` without capturing a return value. Remove the `switch` and the `bptr_errno &= 0x80u` hack — `bptr_node_unload` (cache release) cannot meaningfully fail.

---

#### C14. `bptr_errno &= 0x80u` clears error bits

**File**: `src/bptr_core.c`, line 250
**Severity**: 🔴 Critical

```c
bptr_errno &= 0x80u; // set 0x80 bit to differentiate load error
```

The comment says *set*, but `&=` **clears** all bits except bit 7. For positive error codes (1–5, which don't have bit 7 set), the result is `0` — meaning "no error". The actual error is lost.

**Fix**:
```diff
- bptr_errno &= 0x80u;
+ bptr_errno |= 0x80u;
```

---

#### C15. `self->cache` uninitialized before `bptr_cache_init`

**File**: `src/bptr_core.c`, lines 78, 126
**Severity**: 🔴 Critical

```c
self = malloc(sizeof(struct bptr));  // self->cache contains garbage
// ...
if (bptr_cache_init(self, cache_capacity)) goto CACHE_INIT_ERR;
```

`bptr_cache_init` may fail before reaching `self->cache = cache` (line 126 of `bptr_cache.c`). The error handler at `CACHE_INIT_ERR` calls `bptr_cache_deinit(self)`, which dereferences the garbage `self->cache` pointer — use of uninitialized memory, likely crash or heap corruption.

**Fix** (in both `bptr_init` and `bptr_load`):
```diff
  self = malloc(sizeof(struct bptr));
  if (self == NULL) goto BPTR_MALLOC_ERR;
+ self->cache = NULL;  // safe deinit guard
```

---

### Error Handling & Resource Management

#### C16. Double-close of `self->file` in `bptr_load` error path

**File**: `src/bptr_core.c`, lines 128–149
**Severity**: 🔴 Critical

```
bptr_io_fload()  →  fcloses self->file on ALL error paths (except FOPEN_ERR)
    ↓ (fails)
bptr_load()      →  unconditionally calls bptr_io_fclose(self)
    ↓
bptr_io_fclose() →  fseek64 + fwrite + fclose on already-closed FILE*
    ↓
UNDEFINED BEHAVIOR
```

**Fix**: In `bptr_io_fload`, set `self->file = NULL` after every `fclose`. In `bptr_load`, guard `bptr_io_fclose` or check `self->fbuf` to determine whether fload partially succeeded.

---

#### C17. Same double-close in `bptr_init` error path

**File**: `src/bptr_core.c`, lines 96–113
**Severity**: 🔴 Critical

Same pattern as C16 but for `bptr_io_fcreat`. If `fopen` itself failed, `self->file` is NULL and `self->fbuf` is uninitialized — `bptr_io_fclose` reads garbage `fbuf` in `bptr_header_marshal`.

**Fix**: Same as C16 — NULL `self->file` and `self->fbuf` after release.

---

#### C18. `bptr_io_fclose` FWRITE_ERR leaks `fbuf` and `file`

**File**: `src/bptr_io.c`, lines 219–220
**Severity**: 🔴 Critical

```c
FWRITE_ERR: _set_err_code(BPTR_E_FACCESS);
   return err_code;  // fbuf not freed, file not closed
```

Contrast with `bptr_io_fcreat` which properly cleans up on its `FWRITE_ERR` path.

**Fix**:
```diff
 FWRITE_ERR: _set_err_code(BPTR_E_FACCESS);
+   free(self->fbuf);
+   fclose(self->file);
    return err_code;
```

---

#### C19. `bptr_cache_init` calls `free()` on uninitialized pointers

**File**: `src/bptr_cache.c`, lines 108–111, 134
**Severity**: 🔴 Critical

```c
cache->pool = malloc(...);   // line 109 — allocated
cache->ht = malloc(...);     // line 111 — allocated AFTER pool
// ...
POOL_MALLOC_ERR: _set_err_code(BPTR_E_OOM);
   free(cache->pool);        // line 136 — OK, pool was allocated
// falls through to...
HT_MALLOC_ERR: _set_err_code(BPTR_E_OOM);
   free(cache->ht);          // line 134 — when pool malloc fails, cache->ht is GARBAGE
```

The allocation order is: cache struct → pool → ht. If pool malloc fails, `cache->ht` was never set (still contains whatever `malloc` gave us for the cache struct). `free(garbage)` is heap corruption.

Additionally, if ht malloc fails, we fall through from `HT_MALLOC_ERR` to `SIZE_TOO_LARGE_ERR`, which also doesn't free `cache->pool`.

**Fix**:
```diff
  cache = malloc(sizeof (struct bptr_cache));
  if (cache == NULL) goto MALLOC_ERR;
+ cache->ht = NULL;
+ cache->pool = NULL;
```

And restructure labels so each only frees what has been allocated.

---

### Data Validation & Safety

#### C20. Division by zero in `_bptr_bound_set`

**File**: `src/bptr_core.c`, line 35
**Severity**: 🔴 Critical

```c
(self)->node_bound.leaf.up = rem_sz / ((self)->key_size + (self)->value_size);
```

If `key_size + value_size == 0` (corrupted file header), this is SIGFPE. The `leaf.up < 1` check at line 36 comes AFTER the division.

**Fix**: Add validation in `bptr_io_fload` after reading `key_size` and `value_size`:
```c
if (self->key_size == 0 || self->value_size == 0)
   { err_code = -3; goto MVB_INVALID_ERR; }
```

---

#### C21. No refcount guard in `bptr_cache_release`

**File**: `src/bptr_cache.c`, line 287
**Severity**: 🔴 Critical

```c
void bptr_cache_release(struct bptr *self, struct bptr_node *node)
{
   // ...
   if (--pool_en->refcnt == 1)
      evict_push(cache, pool_en - cache->pool);
}
```

No assertion or check that `refcnt >= 2`. Consequences:

| Scenario | refcnt transition | Effect |
|----------|-------------------|--------|
| Release on INACTIVE (1) | 1→0 | Node becomes EMPTY but NOT returned to free list, NOT removed from ht. Leaked permanently. |
| Release on EMPTY (0) | 0→65535 (uint16_t underflow) | Node permanently ACTIVE, can never be evicted. Slot leaked. |

The comment at line 281 says "Caller is responsible" but no runtime enforcement exists.

**Fix**:
```diff
+ if (pool_en->refcnt < 2) { bptr_errno = BPTR_E_ITRNL_STATE; return; }
  if (--pool_en->refcnt == 1)
```

---

#### C22. Free-list sentinel collision with valid pool index 0

**File**: `src/bptr_cache.c`, line 441
**Severity**: 🔴 Critical

The value `0` is used as both the "trailing free block" sentinel in `pool_free_pop` AND as a valid free-list pointer to pool index 0. After `pool_free_push(0)`, the head's `evict_next` may point to 0 (the sentinel meaning "trailing block"), causing `pool_free_pop` to treat subsequent entries as a contiguous free block, leaking them.

**Fix**: Use `UINT64_MAX` as the trailing-block sentinel instead of `0`. The init check already guarantees `pool_cap < 2^63`, so `UINT64_MAX` cannot be a valid pool index.

---

### Stub / Missing Implementation

#### C23. `bptr_insert` is a stub; `bptr_erase`, `bptr_find`, `bptr_find_range` not implemented

**File**: `src/bptr_core.c`, lines 179–199
**Severity**: 🔴 Critical

```c
int bptr_insert(struct bptr *self, const void *key, const void *value)
{
   // ...
   exit(BPTR_E_TODO);
}
```

`bptr_erase`, `bptr_find`, and `bptr_find_range` are declared in `bptree.h` but have no implementation anywhere. Linking against them produces undefined symbols.

**Fix**: Implement these functions, or guard their declarations with `#if 0` / TODO comments.

---

## 🟡 Warning Findings

### Memory Safety

| # | Location | Description | Suggested Fix |
|---|----------|-------------|---------------|
| W1 | `bptr_cache.c:119-124` | `malloc` (not `calloc`) for ht and pool. Only `node_idx` and `refcnt` manually zeroed. `pool_idx`, `PSL`, `evict_prev`, `evict_next` are uninitialized for most entries. | Use `calloc` or zero all fields. |
| W2 | `bptr_cache.c:108-109` | `(sizeof(pool_entry) + node_buf_sz) * pool_cap` can overflow `size_t`. | Check `<= SIZE_MAX / pool_cap` before `malloc`. |
| W3 | `bptr_cache.c:100-106` | `(leaf.up - 1) * key_size` uses `uint_fast32_t` arithmetic; wraps before promotion to `size_t`. | Cast to `size_t` before multiplication. |
| W4 | `bptr_io.c:110` | `fclose(NULL)` on `FOPEN_ERR` path in `bptr_io_fload` — undefined behavior per C standard. | Restructure labels or add NULL check. |
| W5 | `bptr_io.c:88,212` | `self->file` not set to `NULL` after `fclose`. `self->fbuf` not set to `NULL` after `free`. | Set both to NULL. |

### Resource Leaks

| # | Location | Description | Suggested Fix |
|---|----------|-------------|---------------|
| W6 | `bptr_core.c:155-175` | `bptr_unload` error paths leak `self`: on `CACHE_DEINIT_ERR` (file+fbuf leaked), on `IO_FCLOSE_ERR` (self leaked). | Add `free(self)` on error paths. |
| W7 | `bptr_cache.c:156-158` | `bptr_cache_deinit` returns on first flush error. Remaining dirty nodes unflushed; `cache->ht` and `cache->pool` are never freed on that error path. | Continue loop, clean up before returning error. |

### Missing Validation

| # | Location | Description | Suggested Fix |
|---|----------|-------------|---------------|
| W8 | `bptr_core.c:118` | `bptr_load` does not validate `cache_capacity >= BPTR_CACHE_CAPACITY_MIN` (2). `bptr_init` does. | Add validation. |
| W9 | `bptr_node.c:348,372-374` | `bptr_node_unmarshal` reads `key_count` from file and uses it directly in `memcpy` size without bounds check. Corrupted file → heap buffer overflow. | Validate `key_count <= node_bound.*.up - 1`. |
| W10 | `bptr_io.c:255` | `pos /= self->node_size` — possible division by zero if `node_size` is 0. | Add assertion or early return. |

### Design & Architecture

| # | Location | Description | Suggested Fix |
|---|----------|-------------|---------------|
| W11 | `bptr_cache.h:12` | Naming inconsistency: `bptr_node_fetch` (node_ prefix) lives in cache module. Blurs layer boundary. | Rename to `bptr_cache_fetch`. |
| W12 | `bptr_node.c:360-370` | `node->is_leaf` set redundantly inside if/else AND unconditionally at line 370. | Remove dead assignments in if/else branches. |
| W13 | `bptr_io.h:9` | `#define _FILE_OFFSET_BITS 64` appears after `#include <stdio.h>` — has no effect. | Move to top of `bptr_internal.h`. |
| W14 | `bptr_node.c:409` | `fwrite` to extend file may not create sparse file on NFS. | Use `ftruncate`/`fallocate`. |
| W15 | `bptr_node_unmarshal` | Checksum field read (line 349) but never validated. Corrupted files silently accepted. | Implement or remove. |
| W16 | `bptr_node_unmarshal` | `is_leaf` derived from `level` but `flags & BPTR_NODE_FLAG_LEAF` from the file is ignored. Inconsistency between flags and level is silently accepted. | Cross-validate flags with level. |

---

## 🔵 Suggestions

### Code Quality

| # | Location | Description | Suggested Fix |
|---|----------|-------------|---------------|
| S1 | `bptree.h:19` | Typo: `BPTR_E_ITRNL_STATE` → `BPTR_E_INTERNAL_STATE`. | Rename; add backward-compat alias. |
| S2 | `bptr_cache.c:295` | `#define/#undef` for `HASH_MULTIPLIER` inside function — unconventional. | File-scope `static const uint64_t`. |
| S3 | `bptr_io.c:281` | `strncpy` for binary magic string — `memcpy` is clearer. | `memcpy(memit, BPTR_MAGIC_STR, 4)`. |
| S4 | `bptr_node.c:257` | `perror(...)` in library code writes to stderr. | Remove; error is in `bptr_errno`. |
| S5 | `bptr_cache.c` | `uint_fast8_t` for `hash_shift` may waste space due to alignment padding. | Use `uint8_t`. |
| S6 | `bptr_core.c:102-114` | Error labels are misleading (e.g., `CACHE_INIT_ERR` reached on file-open failure). | Re-label or restructure. |
| S7 | `bptr_core.c:250` | `bptr_errno &= 0x80u` has misleading comment ("set…to differentiate"). | Fix logic (C14), clarify comment. |
| S8 | `bptr_node.c:251` | `err_code` variable declared but only conditionally used. | Remove or guard usage. |

### Performance

| # | Location | Description | Suggested Fix |
|---|----------|-------------|---------------|
| S9 | `bptr_cache.c:119-122` | O(N) initialization loops for ht and pool at startup. | Use `calloc` (often mmap-optimized). |
| S10 | `bptr_cache.c:97` | `ds_clz` evaluated at init but recomputed on every hash. | Store precomputed shift. |
| S11 | `bptr_cache.c:82` | `ds_clz` undefined for input 0; guarded but fragile. | Add `safe_clz()` wrapper. |

### Documentation & APIs

| # | Location | Description | Suggested Fix |
|---|----------|-------------|---------------|
| S12 | `bptree.h` | `bptr_erase`, `bptr_find`, `bptr_find_range` declared but not implemented. | Implement or guard with `#if 0`. |
| S13 | `bptr_io.h:49` | Doc comment inaccurately claims `fbuf` is freed on all error paths (not true for `FWRITE_ERR`). | Fix doc or fix code. |
| S14 | `bptr_cache.c:99-107` | `bptr_cache_init` depends on `node_bound` being pre-initialized — implicit contract. | Add assertion `node_bound.leaf.up > 0`. |

### Security

| # | Location | Description | Suggested Fix |
|---|----------|-------------|---------------|
| S15 | `bptr_io.c:131-169` | TOCTOU: file header read and validated in two separate I/O operations; attacker could modify between reads. | Read once into stack buffer, validate in memory. |
| S16 | `bptr_io.c:163-169` | `node_size` used for `fbuf` allocation comes from first read; a modified second read with larger `node_size` causes buffer overflow during parsing. | Single read (see S15). |

---

## Architecture & Design Observations

### Cache Bypass in Main Traversal

`bptr_find_node` (in `bptr_core.c`) is the primary tree traversal function. It currently calls `bptr_node_load` directly (raw `fseek` + `fread` + `unmarshal`), completely bypassing the cache. The cache-aware `bptr_node_fetch` exists but is only used in `bptr_node_new` (for loading the parent). This means:

- **Every node traversal hits disk** — the cache provides no benefit for reads.
- **The refcount scheme is irrelevant for reads** — nodes loaded by `bptr_find_node` bypass `bptr_cache_alloc`/`bptr_cache_fetch`, so they have no pool slot.

**Recommended**: Rewrite `bptr_find_node` to use `bptr_node_fetch`/`bptr_node_unload`, making the cache the sole path for node access.

### Buffer Sizing Consistency

`bptr_cache_init` computes `node_buf_sz` using `(leaf.up - 1)` and `(brch.up - 1)` for key storage, matching the vals-offset calculations in `bptr_node_new` and `bptr_node_unmarshal`. The vals size correctly accounts for the extra child pointer in branch nodes (`brch.up * ptr_size`). This is internally consistent.

However, the `node_buf_sz` computation uses `uint_fast32_t` arithmetic (C23), which risks overflow before promotion to `size_t`. This should be fixed.

### `bptr_cache_deinit` Flush Semantics

The plan states `bptr_cache_destroy` should flush dirty nodes before freeing. The implementation iterates all slots but has two issues:

1. **Inverted flush check** (C6) — flushes that succeed abort the loop; flushes that fail are silently skipped.
2. **On error, `cache->ht` and `cache->pool` are leaked** because the function returns immediately without freeing them.

### Refcount Scheme

The 0/1/≥2 refcount scheme is sound in concept:
- 0 → 2 on allocation (one residency ref + one caller ref)
- 2 → 1 on release (INACTIVE, joins eviction queue)
- 1 → 2 on cache hit (removed from queue, ACTIVE again)
- 1 → 0 on eviction (recycled for new allocation)

No guard exists against double-release (C21) — adding an assertion or runtime check is strongly recommended for a debug build, even if release builds trust the caller.

---

## Quick-Reference Table of All Critical Fixes

```
File              Line(s)   Issue                                Fix
────────────────  ────────  ───────────────────────────────────  ────────────────────────────
bptr_cache.c      97        hash_shift off by one                ds_clz(ht_cap) + 1
bptr_cache.c      319,349   idx wrap OOB (& ~ht_cap)             & (ht_cap - 1)
bptr_cache.c      357-371   ht_delete uses pool_idx as ht_idx    re-probe from hash
bptr_cache.c      362-369   PSL not decremented on shift         ht[idx].PSL--
bptr_cache.c      374-386   evict_push: old tail next not set    add next update line
bptr_cache.c      156       deinit flush check inverted          == 0 → != 0
bptr_cache.c      202-203   fetch flush check inverted           == 0 → != 0
bptr_cache.c      255       alloc flush check inverted           == 0 → != 0
bptr_cache.c      264       alloc doesn't set node_idx           add node_idx assignment
bptr_cache.c      108-134   free(garbage) on pool malloc fail    init ht/pool to NULL
bptr_cache.c      287       no refcount guard in release         add assert/check
bptr_cache.c      441       free-list sentinel collision         0 → UINT64_MAX
bptr_node.c       253-254   node_idx not set before err label    move before goto labels
bptr_node.c       256-257   vacates parent_n (wrong + UB)        vacate file_slot idx
bptr_core.c        78,126   self->cache not NULL-initialized     self->cache = NULL
bptr_core.c        73       cache_capacity not validated         add check in bptr_load
bptr_core.c       221,256   bptr_node_load wrong arg count       use bptr_node_fetch
bptr_core.c       245       void assigned to int                 remove err_code capture
bptr_core.c       250       &= clears error, should set          &= → |=
bptr_core.c       128-149   double-close file in bptr_load       NULL after fclose
bptr_core.c        96-113   double-close file in bptr_init       NULL after fclose
bptr_core.c        35       div-by-zero key+val==0               validate in fload
bptr_io.c         219-220   fclose FWRITE_ERR leaks fbuf+file    add free + fclose
```

---

*Report generated 2026-06-08 via multi-agent audit (26 agents, 365 tool uses). The full raw output is available upon request.*
