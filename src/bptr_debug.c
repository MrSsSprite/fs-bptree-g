/*----------------------------- Private Includes -----------------------------*/
#include "bptr_debug.h"
#include "bptr_internal.h"
#include <stdio.h>
#include <inttypes.h>
/*--------------------------- Private Includes END ---------------------------*/


/*----------------------- Private Function Prototytpes -----------------------*/
static const char *_bptr_key_type_to_string(uint_fast8_t key_type);
/*--------------------- Private Function Prototytpes END ---------------------*/


/*----------------------------- Public Functions -----------------------------*/
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
           _bptr_key_type_to_string(this->key_type),
           this->key_type);
   fprintf(stream, "  value_size:          %" PRIuFAST16 " bytes\n", this->value_size);
   fprintf(stream, "\n");

   /* Memory Management */
   fprintf(stream, "Memory Management:\n");
   fprintf(stream, "  free_list.head:      %" PRIuFAST64 "\n", this->free_list.head);
   fprintf(stream, "  free_list.size:      %" PRIuFAST64 "\n", this->free_list.size);
   fprintf(stream, "\n");

   /* Statistics */
   fprintf(stream, "Statistics:\n");
   fprintf(stream, "  record_count:        %" PRIuFAST64 "\n", this->record_count);
   fprintf(stream, "  node_count:          %" PRIuFAST64 "\n", this->node_count);
   fprintf(stream, "  tree_height:         %" PRIuFAST32 "\n", this->tree_height);
   fprintf(stream, "\n");

   fprintf(stream, "==========================================\n");

   return 0;
}
/*--------------------------- Public Functions END ---------------------------*/


/*---------------------------- Private Functions -----------------------------*/
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
/*-------------------------- Private Functions END ---------------------------*/
