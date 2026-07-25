# Audit Report: `bptr_insert` Implementation

**Date:** 2026-07-24
**Scope:** `bptr_insert()` and its entire call chain
**Methodology:** Two-pass: (1) manual trace of full call chain by author, (2) automated workflow — 5 dimensional review agents + 2 adversarial skeptics per finding (56 agents total). All findings confirmed by ≥2 independent skeptics.

---

## Executive Summary

**16 unique confirmed bugs:** 5 Critical, 7 High, 2 Medium, 2 Low.

The five critical bugs render the tree non-functional or silently data-corrupting. The seven high-severity bugs cause silent data loss (missing `is_dirty` flags), cache-reference leaks, stale-global-errno, and incomplete error rollback. Taken together, `bptr_insert` cannot be considered production-ready; even a single-record insert–find smoke test would fail.

---

## Critical Bugs

### ✅ 1. `self->root_idx` never set when inserting into empty tree
- **File & Line:** `src/bptr_core.c:192`
- **Description:** The empty-tree branch creates a root node via `bptr_node_new(self, 0)` but never assigns `self->root_idx = find_res.node->node_idx`. The sentinel stays 0 ("empty").
- **Impact:** Every subsequent `bptr_insert` or `bptr_find_node` sees `root_idx == 0` and treats the tree as empty. A second insert creates a second orphan root — the first is permanently leaked. The tree is broken after one insert.

### ✅ 2. Double increment of `node->key_count` in `bptr_node_insert` non-split path
- **File & Line:** `src/bptr_node.c:436` and `:446`
- **Description:** `++node->key_count` at line 436 pre-increments as a tentative fullness check. On the non-full path, this increment is never undone, and line 446 increments a second time. Net: +2 per insert instead of +1.
- **Impact:** `key_count` diverges from reality (2N instead of N). `_node_key_insert` and `_node_val_insert` use inflated `key_count` for memmove sizing, shifting garbage from beyond valid arrays. Marshal writes garbage to disk. Buffer overrun when `key_count` exceeds allocation.

### ~~3. `_node_key_search` returns `md` instead of `lo` for not-found keys~~
- **File & Line:** `src/bptr_node.c:702`
- **Description:** Binary search returns `md` (last midpoint) unconditionally. When the final comparison was `key > K[md]`, `md` is one less than the correct insertion index `lo`.
- **Impact:** Wrong insertion index in three call sites — `bptr_node_split:723` (new element placement during split), `bptr_node_split:843` (separator into parent), `_node_promote:1001` (promoted key into parent). Keys inserted at wrong positions → unsorted nodes → broken B+Tree invariant. The sibling function `bptr_find_node` correctly returns `up` (= `lo`); this was a copy-paste error.

### 4. `bptr_errno` stale after successful split
- **File & Line:** `src/bptr_node.c:723` / `src/bptr_core.c:206`
- **Description:** `bptr_node_split` calls `_node_key_search` which sets `bptr_errno = -1` (not-found, expected). The split succeeds but `bptr_errno` is never reset to 0. `bptr_node_insert` returns 0 (success). `bptr_insert` returns 0 (success). But `bptr_errno` reads `-1` (`BPTR_E_FN_INPUT`).
- **Impact:** Caller sees success (return 0) but global `bptr_errno` indicates an error. Any code path checking `bptr_errno` after a successful split-insert gets a false error. Violates the API contract that `bptr_errno` is meaningful.

### 5. Cache reference leak on every successful insert
- **File & Line:** `src/bptr_core.c:206`
- **Description:** On success, `bptr_insert` returns without calling `bptr_node_unload` on `find_res.node`. The node's cache refcnt stays incremented (from `bptr_node_fetch` or `bptr_node_new`). Every successful insert adds +1 to the refcnt of the target leaf.
- **Impact:** Cache slots are never fully released. The cache pool fills with "pinned" nodes. Eviction is disabled for these nodes. Eventually the cache pool is exhausted and `bptr_cache_alloc` fails with `BPTR_E_CACHE_FULL`. This is a deterministic leak — every insert permanently pins a cache slot.

---

## High-Severity Bugs

### 6. `node->is_dirty` not set after non-split insertion — data silently discarded
- **File & Line:** `src/bptr_node.c:444–448`
- **Description:** The non-split path of `bptr_node_insert` modifies `node->keys`, `node->vals`, and `node->key_count` but never sets `node->is_dirty = 1`. Cache eviction checks `is_dirty` and skips flushing when it's 0.
- **Impact:** **Silent data loss.** Records inserted into an existing node loaded from disk are never persisted. The first insert survives (new nodes have `is_dirty = 1` from `bptr_node_new`), but every subsequent insert into that node is lost on eviction.

### 7. `parent_n->is_dirty` not set in leaf-split parent update
- **File & Line:** `src/bptr_node.c:846`
- **Description:** When a leaf splits and the parent has room, the separator key and child pointer are inserted into `parent_n` (lines 843–846), but `parent_n->is_dirty` is never set.
- **Impact:** The parent's modification (new separator + child pointer) is silently lost on eviction. The split child becomes unreachable from the parent, corrupting the tree structure.

### 8. `par_n->is_dirty` not set in `_node_promote` non-split path
- **File & Line:** `src/bptr_node.c:1004`
- **Description:** Same class of bug as #7, but in `_node_promote` (called during internal-node splits). When the parent is not full, the promoted key and child pointer are inserted but `is_dirty` is not set.
- **Impact:** Promoted separator keys and child pointers silently lost on eviction. The internal-node split effectively never happened from disk's perspective.

### 9. Incomplete split error rollback: `node->next` / `next_n->prev` not restored
- **File & Line:** `src/bptr_node.c:770` and `:762`
- **Description:** `bptr_node_split` modifies leaf linked-list pointers (`node->next`, `next_n->prev`) in pre-work (lines 757–771) before main split logic. On error, `_node_drop(new_n)` frees the new node but does NOT restore the original linked-list pointers.
- **Impact:** After rollback, `node->next` and `next_n->prev` point to a vacated/freed file slot. Range scans following these pointers dereference garbage. Both nodes are marked dirty → corruption persisted to disk on eviction.

### 10. Split error: `parent_n` modifications not rolled back for pre-existing parent
- **File & Line:** `src/bptr_node.c:972`
- **Description:** During internal-node split, `_node_promote` modifies `parent_n` (inserts key + child). If `CHILD_N_UPDATE_ERR` occurs later and `has_new_parent == false`, the error handler calls `bptr_node_unload(self, parent_n)` — which just releases the cache reference without undoing the in-memory modifications.
- **Impact:** Parent node left with extra key + dangling child pointer (to dropped `new_n`). If `is_dirty` was set by a recursive split within `_node_promote`, the corrupted parent is flushed to disk.

### 11. Stale hash-table entry after `bptr_cache_reclaim`
- **File & Line:** `src/bptr_cache.c:317`
- **Description:** `bptr_cache_reclaim` sets `refcnt = 0` and pushes the slot to the free list, but does NOT call `ht_delete` to remove the `node_idx → pool_idx` mapping. Eviction paths (`bptr_node_fetch:232`, `bptr_cache_alloc:285`) correctly delete the HT entry — `reclaim` does not.
- **Impact:** If the same `node_idx` is reused (via free-list reallocation), `ht_lookup` finds the stale entry and returns the old cache data without reading from disk. The stale data may belong to a completely different node.

### 12. `bptr_errno` not set when returning `BPTR_E_KEY_EXIST`
- **File & Line:** `src/bptr_core.c:210`
- **Description:** The `KEY_EXIST_ERR` label uses `_set_err_code(BPTR_E_KEY_EXIST)` which sets the local `err_code` variable but does NOT set the global `bptr_errno`. The return value is correct, but `bptr_errno` remains whatever `bptr_find_node` left it (typically 0).
- **Impact:** Callers checking `bptr_errno` after a failed `bptr_insert` (duplicate key) see 0 (success) instead of `BPTR_E_KEY_EXIST`. Violates the dual-return-value contract.

---

## Medium-Severity Bugs

### 13. Node keys/values arrays not restored on split error rollback
- **File & Line:** `src/bptr_node.c:959`
- **Description:** `PAR_SPLIT_ERR` restores `node->key_count = max_sz` but does NOT undo the data movement already performed (memcpy of keys/vals to `new_n`, insertion of new key/val into `node`).
- **Impact:** After a failed split, the original node has correct `key_count` but corrupted array contents (mix of original data, spuriously-inserted new key, gaps). If the node is dirty, the corruption is persisted.

### 14. Children's `parent` pointers not restored on `CHILD_N_UPDATE_ERR`
- **File & Line:** `src/bptr_node.c:939`
- **Description:** The parent-pointer fixup loop (lines 935–943) updates children's `parent` to point to `new_n`. If a fetch fails mid-loop, some children have updated parents. The error handler drops `new_n` but does not revert those children's `parent` pointers back to the original node.
- **Impact:** After error, some children have stale `parent` pointers to the now-dropped `new_n`. Acknowledged by the `TODO` comment on line 956.

---

## Low-Severity Bugs

### 15. Dead label `NODE_NEW_ERR` — never jumped to
- **File & Line:** `src/bptr_core.c:214`
- **Description:** The label is defined but no `goto` targets it. It can only be reached by fallthrough from `FIND_NODE_ERR`, at which point `has_set_err` is already 1, making its `_set_err_code` call a guaranteed no-op. The empty-tree path handles `bptr_node_new` failure with a direct `return bptr_errno`, bypassing the label entirely.

### 16. Uninitialized `cmp_res` in `bptr_find_node` when `key_count == 0`
- **File & Line:** `src/bptr_core.c:231`
- **Description:** `cmp_res` is uninitialized. If `key_count == 0` for any node during traversal, the binary search loop never executes and `cmp_res` is read uninitialized. The sibling function `_node_key_search` (line 686) explicitly initializes its equivalent to `-1` with comment "Edge Case: empty".

---

## Summary by Severity

| Severity | Count | Bugs |
|----------|-------|------|
| Critical | 5 | #1 root_idx, #2 double-increment, #3 _node_key_search, #4 stale bptr_errno, #5 cache leak |
| High     | 7 | #6–#12 (is_dirty ×3, rollback ×2, stale HT entry, bptr_errno not set) |
| Medium   | 2 | #13–#14 |
| Low      | 2 | #15–#16 |

---

## Root Cause Analysis

Several bugs share common root causes:

- **The `key_count` double-increment (#2)** is the single most damaging bug — it corrupts every non-split insertion. The pre-increment-in-condition pattern (`if (++x == y)`) is inherently error-prone; replacing with `if (x + 1 == y)` would eliminate the entire class of bugs.

- **The `is_dirty` omissions (#6, #7, #8)** reveal a systematic failure: there is no invariant enforcement that "any function modifying node data must set `is_dirty = 1`." Every call site that modifies keys/vals/key_count must be audited for this.

- **The `bptr_errno` issues (#4, #12)** show confusion between two error-reporting channels (return value and global `bptr_errno`). The codebase needs a clear, enforced convention for which channel is authoritative.

- **The error-rollback incompleteness (#9, #10, #13, #14)** is the hardest to fix — `bptr_node_split` makes many mutations before fallible work, and the rollback only restores a subset. A transactional approach (save state before mutations) or deferred-mutation approach (compute first, mutate later) would be more robust.

---

## Recommendations (Priority Order)

1. **Fix `self->root_idx`** — one-line addition in the empty-tree block.
2. **Fix `key_count` double-increment** — change `++node->key_count == ...` to `node->key_count + 1 == ...` in `bptr_node_insert:436`.
3. **Fix `_node_key_search` return** — change `return md;` to `return lo;` at line 702.
4. **Fix `is_dirty` omissions** — add `node->is_dirty = 1` in `bptr_node_insert` non-split path (after line 446), `parent_n->is_dirty = 1` after line 846, and `par_n->is_dirty = 1` after line 1004.
5. **Fix cache reference leak** — add `bptr_node_unload(self, find_res.node)` before `return 0` in `bptr_insert`.
6. **Clear `bptr_errno` on success** — add `bptr_errno = 0` before `return 0` in `bptr_insert` and at the end of `bptr_node_split` success path.
7. **Set `bptr_errno` on `KEY_EXIST_ERR`** — change to `_set_errno(BPTR_E_KEY_EXIST)` or add an explicit `bptr_errno = BPTR_E_KEY_EXIST`.
8. **Fix `bptr_cache_reclaim`** — add `ht_delete` call.
9. **Complete split error rollback** — restore `node->next`, `next_n->prev`, children's `parent` pointers, and node data arrays on error.
10. **Add smoke tests** — all critical bugs are caught by: insert one record, find it, verify `bptr_errno == 0`.
