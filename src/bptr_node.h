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
   void *keys, *vals;
};
/*---------------------------- Public Structs END ----------------------------*/


/*----------------------------- Public Functions -----------------------------*/
struct bptr_node *bptr_node_new
 (struct bptr *self, _Bool is_leaf, bptr_node_t parent);
void bptr_node_free(struct bptr_node *node);
 
int bptr_node_erase(bptr_node_t node_idx);
struct bptr_node *bptr_node_load(struct bptr *self, bptr_node_t node_idx);
/**
 * @brief   unload a bptr node
 *
 * Calling this function claims that a node is no longer needed, and the
 * program can hence opt to free it at any time.
 *
 * @param   self  bptr obj.
 * @param   node  the node to be unloaded
 * @retval  0  success
 * @retval  2  bptr_node_flush failure. bptr_node_flush sets bptr_errno
 *
 * @note    The node may still be cached depending on the implementation.
 */
int bptr_node_unload(struct bptr *self, struct bptr_node *node);
/**
 * @brief   flush a bptr node to file.
 * @param   self  bptr obj.
 * @param   node  the node to be flushed
 * @return  same as if bptr_io_flush_node is called (bptr_io_flush_node sets
 *          @c bptr_errno on failure).
 * @see     bptr_io_flush_node
 */
bptr_node_t bptr_node_flush(struct bptr *self, struct bptr_node *node);
/*--------------------------- Public Functions END ---------------------------*/


#endif
