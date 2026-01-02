/*----------------------------- Private Includes -----------------------------*/
#include "../bptree.h"
#include "bptr_debug.h"
#include "bptr_internal.h"
#include "bptr_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
/*--------------------------- Private Includes END ---------------------------*/


/*------------------------------ Private Macros ------------------------------*/
#define _bptr_boundry_set(ref_exp, value_sz) do \
{  /* t >= (rem_sz / kv_sz + 1) / 2 */ \
   (ref_exp).t_value = \
      (  (this->node_size - BPTR_NODE_METADATA_BYTE - (value_sz)) / \
         (_bptr_key_size(this->data_info.key_type) + (value_sz)) + 1 \
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
   this->free_list.head = this->free_list.size = this->root_pointer = 0;
   this->node_size = node_size;
   /* t >= (rem_sz / kv_sz + 1) / 2 */
   this->data_info.key_type = key_type;
   _bptr_boundry_set(this->node_boundry.internal, BPTR_PTR_SIZE);
   _bptr_boundry_set(this->node_boundry.leaf, value_size);
   this->data_info.value_size = value_size;
   this->stats.record_count = 0;
   this->stats.node_count = 0;
   this->stats.tree_height = 0;

   /* Construct the file */
   if (bptr_fcreat(this, filename)) 
      goto FOPEN_ERR;

   return this;

/* Error Handle */
FOPEN_ERR:
INVALID_T_VAL_ERR:
   free(this);
   return NULL;
}


struct bptree *bptr_load(const char *filename)
{
   struct bptree *this;
   int fn_ret;

   /* malloc for the handler */
   this = malloc(sizeof (struct bptree));
   if (this == NULL) return NULL;

   fn_ret = bptr_fload(this, filename);
   if (fn_ret)
    {
      bptr_errno = 1;
      goto FLOAD_ERR;
    }
   _bptr_boundry_set(this->node_boundry.internal, BPTR_PTR_SIZE);
   _bptr_boundry_set(this->node_boundry.leaf, this->data_info.value_size);

   return this;

INVALID_T_VAL_ERR:
   bptr_fclose(this);
FLOAD_ERR:
   free(this);
   return NULL;
}

int bptr_destroy(struct bptree *this)
{
   int err_code = 0;

   if (bptr_fclose(this))
      err_code = 1;
   free(this);

   return err_code;
}


/*------------------------ Public Function Definition ------------------------*/
/* Debug Functions */

static const char *_bptr_key_type_to_string(uint_fast8_t key_type)
{
   switch (key_type)
   {
      case BPTR_KYTP_U8:  return "BPTR_KYTP_U8";
      case BPTR_KYTP_I8:  return "BPTR_KYTP_I8";
      case BPTR_KYTP_U16: return "BPTR_KYTP_U16";
      case BPTR_KYTP_I16: return "BPTR_KYTP_I16";
      case BPTR_KYTP_U32: return "BPTR_KYTP_U32";
      case BPTR_KYTP_I32: return "BPTR_KYTP_I32";
      case BPTR_KYTP_U64: return "BPTR_KYTP_U64";
      case BPTR_KYTP_I64: return "BPTR_KYTP_I64";
      default:            return "UNKNOWN";
   }
}

int bptr_dump_handler(const struct bptree *this, FILE *stream)
{
   if (this == NULL || stream == NULL)
      return -1;

   /* Print header */
   fprintf(stream, "==========================================\n");
   fprintf(stream, "B+Tree Handler Dump\n");
   fprintf(stream, "==========================================\n\n");

   /* File IO Members */
   fprintf(stream, "File IO:\n");
   fprintf(stream, "  file pointer:        %p\n", (void*)this->file);
   fprintf(stream, "  file buffer:         %p\n", this->fbuf);
   fprintf(stream, "\n");

   /* File Identification */
   fprintf(stream, "File Identification:\n");
   fprintf(stream, "  version:             %" PRIuLEAST32 "\n", this->version);
   fprintf(stream, "  is_lite:             %s\n", this->is_lite ? "true" : "false");
   fprintf(stream, "  block_size:          %" PRIuFAST32 " bytes\n", this->block_size);
   fprintf(stream, "\n");

   /* Tree Structure */
   fprintf(stream, "Tree Structure:\n");
   fprintf(stream, "  root_pointer:        %" PRIuFAST64 "\n", this->root_pointer);
   fprintf(stream, "  node_size:           %" PRIuFAST32 " bytes\n", this->node_size);
   fprintf(stream, "  node_boundry.leaf:\n");
   fprintf(stream, "    low:               %" PRIuFAST16 "\n", this->node_boundry.leaf.low);
   fprintf(stream, "    up:                %" PRIuFAST16 "\n", this->node_boundry.leaf.up);
   fprintf(stream, "    t_value:           %" PRIuFAST16 "\n", this->node_boundry.leaf.t_value);
   fprintf(stream, "  node_boundry.internal:\n");
   fprintf(stream, "    low:               %" PRIuFAST16 "\n", this->node_boundry.internal.low);
   fprintf(stream, "    up:                %" PRIuFAST16 "\n", this->node_boundry.internal.up);
   fprintf(stream, "    t_value:           %" PRIuFAST16 "\n", this->node_boundry.internal.t_value);
   fprintf(stream, "\n");

   /* Data Type Information */
   fprintf(stream, "Data Type Information:\n");
   fprintf(stream, "  key_type:            %s (value: %" PRIuFAST8 ")\n",
           _bptr_key_type_to_string(this->data_info.key_type),
           this->data_info.key_type);
   fprintf(stream, "  value_size:          %" PRIuFAST16 " bytes\n", this->data_info.value_size);
   fprintf(stream, "\n");

   /* Memory Management */
   fprintf(stream, "Memory Management:\n");
   fprintf(stream, "  free_list.head:      %" PRIuFAST64 "\n", this->free_list.head);
   fprintf(stream, "  free_list.size:      %" PRIuFAST64 "\n", this->free_list.size);
   fprintf(stream, "\n");

   /* Statistics */
   fprintf(stream, "Statistics:\n");
   fprintf(stream, "  record_count:        %" PRIuFAST64 "\n", this->stats.record_count);
   fprintf(stream, "  node_count:          %" PRIuFAST64 "\n", this->stats.node_count);
   fprintf(stream, "  tree_height:         %" PRIuFAST32 "\n", this->stats.tree_height);
   fprintf(stream, "\n");

   fprintf(stream, "==========================================\n");

   return 0;
}
/*---------------------- Public Function Definition END ----------------------*/
