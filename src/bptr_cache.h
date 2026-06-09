#ifndef __BPTR_CACHE_H
#define __BPTR_CACHE_H

/*----------------------------- Public Includes ------------------------------*/
#include <stdint.h>
#include "bptr_internal.h"
/*--------------------------- Public Includes END ----------------------------*/

/*----------------------------- Public Functions -----------------------------*/
int bptr_cache_init(struct bptr *self, uint64_t pool_cap);
int bptr_cache_deinit(struct bptr *self);
struct bptr_node *bptr_node_fetch(struct bptr *self, bptr_node_t node_idx);
struct bptr_node *bptr_cache_alloc(struct bptr *self, bptr_node_t node_idx);
void bptr_cache_release(struct bptr *self, struct bptr_node *node);
/**
 * @brief   Reclaim cache slot space
 *
 * Send cache slot back to free list directly and immediately, circumventing
 * the eviction queue mechanism.
 *
 * @param   self  bptr struct
 * @param   node  address of node of the pool entry to be reclaimed
 *
 * @remark  node is never flushed in this function (even if dirty).
 * @note    This function is designed to be called in error path after cache
 *          space is requested.
 */
void bptr_cache_reclaim(struct bptr *self, struct bptr_node *node);
/*--------------------------- Public Functions END ---------------------------*/

#endif
