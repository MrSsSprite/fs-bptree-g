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
      (  (node_size - BPTR_NODE_METADATA_BYTE - (value_sz)) / \
         (_bptr_key_size(key_type) + (value_sz)) + 1 \
      ) / 2; \
   if ((ref_exp).t_value <= 2) goto INVALID_T_VAL_ERROR; \
   (ref_exp).low = (ref_exp).t_value - 2; \
   (ref_exp).up = 2 * (ref_exp).t_value; \
} while (0)
/*---------------------------- Private Macros END ----------------------------*/


/*----------------------- Private Function Declaration -----------------------*/
inline uint_fast16_t _bptr_key_size(uint8_t key_type);
   /*--------------------- Private Function Declaration END ---------------------*/


struct bptree *bptr_init
(
   const char *filename,
   int is_lite,
   uint32_t node_size,
   uint8_t key_type,
   uint16_t value_size
)
{
   struct bptree *this;

   /* Node must be large enough to at least contain
    * the metadata, 1 key and 2 childs */
   if (node_size < BPTR_NODE_METADATA_BYTE + _bptr_key_size(key_type) +
                   (is_lite ? BPTR_LITE_PTR_BYTE : BPTR_NORM_PTR_BYTE) * 2)
      return NULL;

   /* malloc for the handler */
   this = malloc(sizeof (struct bptree));
   if (this == NULL) return NULL;

   /* Write Metadata */
   this->version = BPTR_CURRENT_VERSION;
   this->is_lite = is_lite;
   this->block_size = BPTR_BLOCK_BYTE;
   this->ptr_sz = is_lite ? BPTR_LITE_PTR_BYTE : BPTR_NORM_PTR_BYTE;
   this->free_list.head = this->free_list.size = this->root_pointer = 0;
   this->node_size = node_size;
   /* t >= (rem_sz / kv_sz + 1) / 2 */
   _bptr_boundry_set(this->node_boundry.internal, this->ptr_sz);
   _bptr_boundry_set(this->node_boundry.leaf, value_size);
   this->data_info.key_type = key_type;
   this->data_info.value_size = value_size;
   this->stats.record_count = 0;
   this->stats.node_count = 0;
   this->stats.tree_height = 0;

   /* Construct the file */
   if (bptr_fcreat(this, filename)) 
      goto FOPEN_ERROR;

   return this;

/* Error Handle */
INVALID_T_VAL_ERROR:
FOPEN_ERROR:
   free(this);
   return NULL;
}


inline uint_fast16_t _bptr_key_size(uint8_t key_type)
{
   switch (key_type)
    {
   case BPTR_KYTP_I8:
   case BPTR_KYTP_U8:
      return 1;
   case BPTR_KYTP_I16:
   case BPTR_KYTP_U16:
      return 2;
   case BPTR_KYTP_I32:
   case BPTR_KYTP_U32:
      return 4;
   case BPTR_KYTP_I64:
   case BPTR_KYTP_U64:
      return 8;
    }

   return 0;
}
