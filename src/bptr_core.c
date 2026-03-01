/*----------------------------- Private Includes -----------------------------*/
#include "../bptree.h"
#include "bptr_internal.h"
#include "bptr_io.h"
#include "bptr_node.h"
#include "bptr_utils.h"
#include <stdlib.h>
#include <string.h>
/*--------------------------- Private Includes END ---------------------------*/


/*----------------------------- Private Structs ------------------------------*/
struct node_idx_pair
{
   _Bool is_found;
   struct bptr_node *node;
   bptr_node_ki_t idx;
};
/*--------------------------- Private Structs END ----------------------------*/


/*------------------------------ Private Macros ------------------------------*/
#define _bptr_boundry_set(self) do \
{  /* t >= (rem_sz / kv_sz + 1) / 2 */ \
   uint_fast32_t rem_sz = (self)->node_size - BPTR_NODE_METADATA_BYTE; \
   (self)->node_boundry.brch.up = \
      (rem_sz - BPTR_PTR_SIZE) / ((self)->key_size + BPTR_PTR_SIZE) + 1; \
   if ((self)->node_boundry.brch.up < 3) \
    { bptr_errno = -1; goto INVALID_FANOUT_ERR; } \
   (self)->node_boundry.brch.low = CEIL_DIV((self)->node_boundry.brch.up, 2) - 2; \
   /* should be B/(K + V) + 1; +1 is detained so that B/(K+V) can be used for \
    * temporary storage. \
    */ \
   (self)->node_boundry.leaf.up = rem_sz / ((self)->key_size + (self)->value_size); \
   if ((self)->node_boundry.leaf.up < 1) \
    { bptr_errno = -1; goto INVALID_FANOUT_ERR; } \
   (self)->node_boundry.leaf.low = CEIL_DIV((self)->node_boundry.leaf.up, 2) - 1; \
   (self)->node_boundry.leaf.up += 1; \
} while (0)
/*---------------------------- Private Macros END ----------------------------*/


/*----------------------- Private Function Declaration -----------------------*/
static
struct node_idx_pair bptr_find_node(struct bptr *self, const void *key);
/*--------------------- Private Function Declaration END ---------------------*/


/*----------------------------- Public Variable ------------------------------*/
int bptr_errno;
/*--------------------------- Public Variable END ----------------------------*/


/*------------------------ Public Function Definition ------------------------*/
struct bptr *bptr_init
(
   const char *filename,
   _Bool is_lite,
   uint32_t node_size,
   uint16_t key_size,
   uint16_t value_size,
   int (*compare)(const void *lhs, const void *rhs)
   )
{
   struct bptr *self;

   /* Node must be large enough to at least contain
    * the metadata, 1 key and 2 childs */
   if (node_size < BPTR_NODE_METADATA_BYTE + key_size +
                   (is_lite ? BPTR_LITE_PTR_BYTE : BPTR_NORM_PTR_BYTE) * 2)
      return NULL;

   /* malloc for the handler */
   self = malloc(sizeof (struct bptr));
   if (self == NULL) return NULL;

   /* Write Metadata */
   self->version = BPTR_CURRENT_VERSION;
   self->is_lite = is_lite;
   self->block_size = BPTR_BLOCK_BYTE;
   self->free_list.head = self->free_list.size = self->root_idx = 0;
   self->node_size = node_size;
   self->key_size = key_size;
   self->value_size = value_size;
   _bptr_boundry_set(self);
   self->record_cnt = 0;
   self->node_cnt = 0;
   self->height = 0;

   /* Construct the file */
   if (bptr_io_fcreat(self, filename)) 
      goto FOPEN_ERR;

   return self;

/* Error Handle */
FOPEN_ERR:
INVALID_FANOUT_ERR:
   free(self);
   return NULL;
}


struct bptr *bptr_load(const char *filename)
{
   struct bptr *self;
   int fn_ret;

   /* malloc for the handler */
   self = malloc(sizeof (struct bptr));
   if (self == NULL) return NULL;

   fn_ret = bptr_io_fload(self, filename);
   if (fn_ret)
    {
      bptr_errno = 1;
      goto FLOAD_ERR;
    }
   _bptr_boundry_set(self);

   return self;

INVALID_FANOUT_ERR:
   bptr_io_fclose(self);
FLOAD_ERR:
   free(self);
   return NULL;
}

int bptr_destroy(struct bptr *self)
{
   int err_code = 0;

   if (bptr_io_fclose(self))
      err_code = 1;
   free(self);

   return err_code;
}


// Replaces the value if the key already exists in the tree
int bptr_insert(struct bptr *self, const void *key, const void *value)
{
   struct node_idx_pair find_res;

   //TODO: temporary implementation; cache pool should be involved later

   find_res = bptr_find_node(self, key);
   if (bptr_errno)
      return bptr_errno < 0 ? -1 : 1;
   // Empty Tree
   if (find_res.node == NULL)
    {
      find_res.node = bptr_node_new(self, 1, 0);
      if (find_res.node == NULL) return 1;
      find_res.node->prev = find_res.node->next = 0;
      find_res.idx = 0;
    }
   //TODO
}
/*---------------------- Public Function Definition END ----------------------*/


/*---------------------------- Private Functions -----------------------------*/
// bptr_errno is set to non-0 on error;
// the return value is valid iff bptr_errno is 0
// if the tree is not empty && the key is not found,
//    idx refers to the smallest key that is greater than the key for which the 
//    table is searched. idx ref to one-pass the last valid key
static
struct node_idx_pair bptr_find_node(struct bptr *self, const void *key)
{
   struct bptr_node *node;
   bptr_node_ki_t lo, md, up;
   int cmp_res;

   bptr_errno = 0;
   // Empty tree
   if (self->root_idx == 0)
    { bptr_errno = 0; return (struct node_idx_pair){ 0, NULL }; }

   for (node = bptr_node_load(self, self->root_idx); ;)
    {
      // Error: bptr_node_load() sets bptr_errno on error
      if (node == NULL) return (struct node_idx_pair){ 0, NULL };

      for (lo = 0, up = node->key_count, md = up / 2;
           lo < up; md = lo + (up - lo) / 2)
       {
         cmp_res = self->compare(key, (char*)node->keys + md * self->key_size);
         if (cmp_res < 0)        up = md;
         else if (cmp_res > 0)   lo = md + 1;
         else                    break;
       }

      // early return
      if (_node_is_leaf(self, node))
       {
         if (cmp_res == 0) return (struct node_idx_pair){ 1, node, md };
         else { bptr_errno = 0; return (struct node_idx_pair){ 0, node, up }; }
       }
      bptr_node_t node_idx = _node_brch_vals_get(self, node,
                                                 cmp_res == 0 ? md + 1 : up);
    }
}
/*-------------------------- Private Functions END ---------------------------*/
