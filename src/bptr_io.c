/*----------------------------- Private Includes -----------------------------*/
#include "bptr_io.h"
#include "../bptree.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
/*--------------------------- Private Includes END ---------------------------*/


/*------------------------------ Private Macros ------------------------------*/
#define _bptr_write_uptr_metadata_tobuffer(uptr_type, uptr_size) do \
{ \
   *(uptr_type*)memit = this->root_pointer; \
   memit += uptr_size; \
   *(uptr_type*)memit = this->free_list.head; \
   memit += uptr_size; \
   *(uptr_type*)memit = this->free_list.size; \
   memit += uptr_size; \
   *(uptr_type*)memit = this->stats.node_count; \
   memit += uptr_size; \
} while (0)
/*---------------------------- Private Macros END ----------------------------*/


/*----------------------------- Public Functions -----------------------------*/
int bptr_fcreat(struct bptree *this, const char *filename)
{
   int err_code;
   FILE *ftemp;
   void *memblk, *memit;
   uint32_t tv_u32;

   /* Create file */
   this->file = fopen(filename, "wbx+");
   if (this->file == NULL)
    {
      err_code = 1;
      goto FOPEN_ERROR;
    }

   /* Write file header */
   memblk = malloc(this->block_size);
   if (memblk == NULL)
    {
      err_code = 2;
      goto MEM_BLK_ALLOC_ERROR;
    }

   memit = memblk;
   strncpy(memit, BPTR_MAGIC_STR, 4);
   memit += 4;
   *(uint32_t*)memit = BPTR_CURRENT_VERSION;
   memit += 4;
   *(uint32_t*)memit = this->block_size;
   memit += 4;

   *(uint32_t*)memit = this->node_size;
   memit += 4;

   *(uint8_t*)memit = this->data_info.key_type;
   memit += 1;
   *(uint16_t*)memit = this->data_info.value_size;
   memit += 2;

   *(uint64_t*)memit = this->stats.record_count;
   memit += 8;
   *(uint32_t*)memit = this->stats.tree_height;
   memit += 4;
   if (this->is_lite)
      _bptr_write_uptr_metadata_tobuffer(BPTR_LITE_PTR_TYPE, BPTR_LITE_PTR_BYTE);
   else
      _bptr_write_uptr_metadata_tobuffer(BPTR_NORM_PTR_TYPE, BPTR_NORM_PTR_BYTE);

   /* Flush the Buffer to the file */
   if (fwrite(memblk, this->block_size, 1, this->file) != 1)
    {
      err_code = 3;
      goto FWRITE_ERROR;
    }
   if (fflush(this->file))
    {
      err_code = 4;
      goto FWRITE_ERROR;
    }

   free(memblk);
   return 0;

FWRITE_ERROR:
   free(memblk);
MEM_BLK_ALLOC_ERROR:
   fclose(this->file);
FOPEN_ERROR:
   return err_code;
}
/*--------------------------- Public Functions END ---------------------------*/


/*---------------------------- Private Functions -----------------------------*/
/*-------------------------- Private Functions END ---------------------------*/
