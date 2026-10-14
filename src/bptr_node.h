#ifndef BPTR_NODE_H
#define BPTR_NODE_H


/*----------------------------- Public Includes ------------------------------*/
#include "../bptree.h"
#include "bptr_internal.h"
/*--------------------------- Public Includes END ----------------------------*/


/*------------------------------ Public Defines ------------------------------*/
#define BPTR_NODE_FLAG_VALID     (0x0001u)      // 0：削除済み／空き、1：有効
#define BPTR_NODE_FLAG_LEAF      (0x0002u)      // 0：内部ノード、1：リーフノード
/*---------------------------- Public Defines END ----------------------------*/


/*------------------------------ Public Macros -------------------------------*/
#define _node_brch_vals_get(self, node, idx) \
   ((self)->is_lite ? *((BPTR_LITE_PTR_TYPE*)(node)->vals + (idx)) : \
                      *((BPTR_NORM_PTR_TYPE*)(node)->vals + (idx)))

#define _node_val_cnt(node) \
   ((node)->is_leaf ? (node)->key_count : (node)->key_count + 1)
/*---------------------------- Public Macros END -----------------------------*/


/*----------------------------- Public Typedefs ------------------------------*/
typedef uint32_t bptr_node_ki_t;
/*--------------------------- Public Typedefs END ----------------------------*/


/*------------------------------ Public Structs ------------------------------*/
struct bptr_node
{
   _Bool is_dirty, is_leaf;
   bptr_node_t node_idx;   // node index in block size; 0 if not yet in file
   uint16_t flags, level;
   uint32_t key_count, checksum;
   bptr_node_t parent, prev, next;
   void *vals;
   char keys[];
};
/*---------------------------- Public Structs END ----------------------------*/


/*----------------------------- Public Functions -----------------------------*/
struct bptr_node *bptr_node_new
 (struct bptr *self, bptr_node_t parent);
 
int bptr_node_erase(bptr_node_t node_idx);
int bptr_node_load
 (struct bptr *self, bptr_node_t node_idx, struct bptr_node *node);
/**
 * @brief   unload a bptr node
 *
 * Calling this function claims that a node is no longer needed, and the
 * program can hence opt to free it at any time.
 *
 * @param   self  bptr obj.
 * @param   node  the node to be unloaded
 *
 * @note    The node may still be cached depending on the implementation.
 * @note    The library is responsible for flushing, when necessary. Thus,
 *          user is not (though allowed) responsible to flush the node.
 */
void bptr_node_unload(struct bptr *self, struct bptr_node *node);
/**
 * @brief   flush a bptr node to file.
 * @param   self  bptr obj.
 * @param   node  the node to be flushed
 * @return  same as if bptr_io_flush_node is called (bptr_io_flush_node sets
 *          @c bptr_errno on failure).
 * @return  0           Success
 * @return  BPTR_E_...  Corresponding type of failure
 * @see     bptr_io_flush_node
 */
int bptr_node_flush(struct bptr *self, struct bptr_node *node);
struct bptr_node *bptr_node_fetch(struct bptr *self, bptr_node_t node_idx);
/**
 * @brief   Search for a key within a node's key array
 *
 * Performs a binary search to locate the first key in @p node that is not less
 * than @p key . This index serves as the exact match or the potential insertion
 * point.
 *
 * @param[in]  self     B+Tree instance.
 * @param[in]  node     node to be searched.
 * @param[in]  key      target key to locate.
 * @param[out] result   Index of insertion point found by the function. (i.e.,
 *                      the index of the lower bound of @p key)
 *                      If the key is found, this is the index of the match.
 *                      If not found, this is the insertion point (the index of
 *                      the smallest key greater than @p key , or @c
 *                      node->key_count if @p key is the largest).
 *
 * @retval  0        @p key is found in the B+Tree..
 * @retval  1        @p key is not found in the B+Tree.
 */
static inline
_Bool bptr_node_key_search(struct bptr *self, struct bptr_node *node,
                           const void *key, bptr_node_ki_t *result)
{
   uint_fast32_t lo, md, hi;
   // Edge Case: empty (i.e., key_count == 0)
   int cmp_res = -1;

   for (lo = 0, hi = node->key_count, md = hi / 2;
        lo != hi; md = lo + (hi - lo) / 2)
    {
      cmp_res = self->compare(key, node->keys + md * self->key_size);
      if (cmp_res < 0)
         hi = md;
      else if (cmp_res > 0)
         lo = md + 1;
      else
         break;
    }

   *result = md;
   if (cmp_res == 0) return 0;
   else              return 1;
}
/*--------------------------- Public Functions END ---------------------------*/


#endif
