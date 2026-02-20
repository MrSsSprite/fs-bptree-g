/*----------------------------- Private Includes -----------------------------*/
#include "../bptree.h"
#include "bptr_internal.h"
#include "bptr_io.h"
#include <stdlib.h>
#include <string.h>
/*--------------------------- Private Includes END ---------------------------*/


/*------------------------------ Private Macros ------------------------------*/
#define _bptr_boundry_set(ref_exp, value_sz) do \
{  /* t >= (rem_sz / kv_sz + 1) / 2 */ \
   (ref_exp).t_value = \
      (  (this->node_size - BPTR_NODE_METADATA_BYTE - (value_sz)) / \
         (_bptr_key_size(this->key_type) + (value_sz)) + 1 \
      ) / 2; \
   if ((ref_exp).t_value <= 2) goto INVALID_T_VAL_ERR; \
   (ref_exp).low = (ref_exp).t_value - 2; \
   (ref_exp).up = 2 * (ref_exp).t_value; \
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
   /* t >= (rem_sz / kv_sz + 1) / 2 */
   self->key_size = key_size;
   _bptr_boundry_set(self->node_boundry.internal, BPTR_PTR_SIZE);
   _bptr_boundry_set(self->node_boundry.leaf, value_size);
   self->value_size = value_size;
   self->record_count = 0;
   self->node_count = 0;
   self->tree_height = 0;

   /* Construct the file */
   if (bptr_fcreat(self, filename)) 
      goto FOPEN_ERR;

   return self;

/* Error Handle */
FOPEN_ERR:
INVALID_T_VAL_ERR:
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
   _bptr_boundry_set(self->node_boundry.internal, BPTR_PTR_SIZE);
   _bptr_boundry_set(self->node_boundry.leaf, self->value_size);

   return self;

INVALID_T_VAL_ERR:
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
