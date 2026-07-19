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
 * @brief   Insert a key–value pair into a leaf node at a given index
 *
 * Inserts @p key and @p value into @p node at position @p idx , shifting
 * existing entries rightward to make room.  If @p node is already at maximum
 * capacity (@c node->key_count equals @c leaf.up - 1 ), the insertion is
 * delegated to @c bptr_node_split , which splits the node and may cascade
 * upward.
 *
 * @param[in,out] self  bptr object.  @c record_cnt is incremented on success.
 * @param[in,out] node  target leaf node.  Its @c keys , @c vals , and
 *                      @c key_count are modified.  If the node is full, a
 *                      split may also update @c node->parent , @c node->next ,
 *                      and global tree state (@c root_idx , @c height ,
 *                      @c node_cnt ).
 * @param[in]     idx   insertion index (0-based), typically obtained from
 *                      @c _node_key_search .  Must be in the range
 *                      @c [0, node->key_count] .
 * @param[in]     key   pointer to the key to copy into @c node->keys .
 * @param[in]     value pointer to the value to copy into @c node->vals .
 *
 * @return  error code
 * @retval  0           success; the key–value pair was inserted (possibly
 *                      via a split).
 * @retval  non-zero    failure; @c bptr_errno is set.  This only occurs when
 *                      the node was full and @c bptr_node_split failed.  In
 *                      that case @p node is left unmodified ( @c key_count
 *                      is restored by the split's rollback).
 *
 * @warning  This function is designed for leaf nodes.  Calling it on an
 *           internal (branch) node with a full key count causes the split
 *           path to misbehave because the capacity check compares against
 *           @c leaf.up rather than @c brch.up .
 * @warning  It is the caller's responsibility to ensure @p idx is valid
 *           (0 ≤ @p idx ≤ @c node->key_count ) and that @p key does not
 *           already exist in @p node .  Neither condition is checked
 *           here — violating them results in undefined behaviour.
 */
int bptr_node_insert
 (struct bptr *self, struct bptr_node *node, bptr_node_ki_t idx,
  const void *key, const void *value);
/*--------------------------- Public Functions END ---------------------------*/


#endif
