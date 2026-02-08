#ifndef BPTR_NODE_H
#define BPTR_NODE_H


/*----------------------------- Public Includes ------------------------------*/
#include "../bptree.h"
#include "bptr_internal.h"
#include "../external/data_structures/vector/vector.h"
/*--------------------------- Public Includes END ----------------------------*/


/*------------------------------ Public Defines ------------------------------*/
#define BPTR_NODE_FLAG_VALID     (0x0001u)      // 0：削除済み／空き、1：有効
#define BPTR_NODE_FLAG_LEAF      (0x0002u)      // 0：内部ノード、1：リーフノード
#define BPTR_NODE_FLAG_DIRTY     (0x0004u)      // 変更あり（書き戻し必要）
/*---------------------------- Public Defines END ----------------------------*/


/*------------------------------ Public Structs ------------------------------*/
struct bptr_node
{
   enum { BPTR_NODE_TYPE_BRCH, BPTR_NODE_TYPE_LEAF } type;
   _Bool is_dirty;
   bptr_node_t node_idx;   // node index in block size; 0 if not yet in file
   uint16_t flags, level;
   uint32_t key_count, checksum;
   bptr_node_t parent, prev, next;
   struct vector *keys, *vals;
};
/*---------------------------- Public Structs END ----------------------------*/


/*----------------------------- Public Functions -----------------------------*/
struct bptr_node *bptr_node_new
 (struct bptree *self, _Bool is_leaf, bptr_node_t parent);
int bptr_node_erase(bptr_node_t node);
void bptr_node_unload(struct bptr_node *node);
/*--------------------------- Public Functions END ---------------------------*/


#endif
