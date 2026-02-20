/*----------------------------- Private Includes -----------------------------*/
#include "../bptree.h"
#include "bptr_internal.h"
#include "bptr_io.h"
#include "bptr_utils.h"
#include <stdlib.h>
#include <string.h>
/*--------------------------- Private Includes END ---------------------------*/


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


/*----------------------------- Public Variable ------------------------------*/
int bptr_errno;
/*--------------------------- Public Variable END ----------------------------*/


/*------------------------ Public Function Definition ------------------------*/
struct bptree *bptr_init
(
   const char *filename,
   _Bool is_lite,
   uint32_t node_size,
   uint16_t key_size,
   uint16_t value_size
)
{
   struct bptree *self;

   /* Node must be large enough to at least contain
    * the metadata, 1 key and 2 childs */
   if (node_size < BPTR_NODE_METADATA_BYTE + key_size +
                   (is_lite ? BPTR_LITE_PTR_BYTE : BPTR_NORM_PTR_BYTE) * 2)
      return NULL;

   /* malloc for the handler */
   self = malloc(sizeof (struct bptree));
   if (self == NULL) return NULL;

   /* Write Metadata */
   self->version = BPTR_CURRENT_VERSION;
   self->is_lite = is_lite;
   self->block_size = BPTR_BLOCK_BYTE;
   self->free_list.head = self->free_list.size = self->root_pointer = 0;
   self->node_size = node_size;
   self->key_size = key_size;
   self->value_size = value_size;
   _bptr_boundry_set(self);
   self->record_count = 0;
   self->node_count = 0;
   self->tree_height = 0;

   /* Construct the file */
   if (bptr_fcreat(self, filename)) 
      goto FOPEN_ERR;

   return self;

/* Error Handle */
FOPEN_ERR:
INVALID_FANOUT_ERR:
   free(self);
   return NULL;
}


struct bptree *bptr_load(const char *filename)
{
   struct bptree *self;
   int fn_ret;

   /* malloc for the handler */
   self = malloc(sizeof (struct bptree));
   if (self == NULL) return NULL;

   fn_ret = bptr_fload(self, filename);
   if (fn_ret)
    {
      bptr_errno = 1;
      goto FLOAD_ERR;
    }
   _bptr_boundry_set(self);

   return self;

INVALID_FANOUT_ERR:
   bptr_fclose(self);
FLOAD_ERR:
   free(self);
   return NULL;
}

int bptr_destroy(struct bptree *self)
{
   int err_code = 0;

   if (bptr_fclose(self))
      err_code = 1;
   free(self);

   return err_code;
}


int bptr_insert(struct bptree *self, const void *kv)
{
   if (self->root_pointer == 0)
      ;

   //TODO
}
/*---------------------- Public Function Definition END ----------------------*/
