/*----------------------------- Private Includes -----------------------------*/
#include "bptr_io.h"
#include "../bptree.h"
#include "bptr_internal.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
/*--------------------------- Private Includes END ---------------------------*/


/*------------------------------ Private Macros ------------------------------*/
#define _bptr_fcreat_write_uptr_metadata(uptr_type, uptr_size) do \
{ \
   *(uptr_type*)memit = this->root_pointer; \
   memit += (uptr_size); \
   *(uptr_type*)memit = this->free_list.head; \
   memit += (uptr_size); \
   *(uptr_type*)memit = this->free_list.size; \
   memit += (uptr_size); \
   *(uptr_type*)memit = this->stats.node_count; \
   memit += (uptr_size); \
} while (0)

#define _bptr_fload_read_uptr_metadata(uptr_type, uptr_size) do \
{ \
   this->root_pointer = *(uptr_type*)memit; memit += (uptr_size); \
   this->free_list.head = *(uptr_type*)memit; memit += (uptr_size); \
   this->free_list.size = *(uptr_type*)memit; memit += (uptr_size); \
   this->stats.node_count = *(uptr_type*)memit; memit += (uptr_size); \
} while (0)
/*---------------------------- Private Macros END ----------------------------*/


/*----------------------------- Public Functions -----------------------------*/
int bptr_fcreat(struct bptree *this, const char *filename)
{
   int err_code;
   void *memit;

   /* Create file */
   this->file = fopen(filename, "wbx+");
   if (this->file == NULL)
    {
      err_code = 1;
      goto FOPEN_ERR;
    }
   /* malloc for file buffer */
   this->fbuf = malloc(this->block_size);
   if (this->fbuf == NULL)
    {
      err_code = 2;
      goto FBUF_MALLOC_ERR;
    }

   /* Write file header */
   memit = this->fbuf;
   strncpy(memit, BPTR_MAGIC_STR, 4);
   memit += 4;
   *(uint32_t*)memit = (this->is_lite) ?
                       0x80 | BPTR_CURRENT_VERSION : BPTR_CURRENT_VERSION;
   memit += 4;
   *(uint32_t*)memit = this->block_size;
   memit += 4;

   *(uint32_t*)memit = this->node_size;
   memit += 4;

   *(uint8_t*)memit = this->data_info.key_type;
   memit += 1;
   *(uint16_t*)memit = this->data_info.value_size;
   memit += 3; // +1 for padding

   *(uint64_t*)memit = this->stats.record_count;
   memit += 8;
   *(uint32_t*)memit = this->stats.tree_height;
   memit += 4;
   if (this->is_lite)
      _bptr_fcreat_write_uptr_metadata(BPTR_LITE_PTR_TYPE,
                                       BPTR_LITE_PTR_BYTE);
   else
      _bptr_fcreat_write_uptr_metadata(BPTR_NORM_PTR_TYPE,
                                       BPTR_NORM_PTR_BYTE);

   /* Flush the Buffer to the file */
   if (fwrite(this->fbuf, this->block_size, 1, this->file) != 1)
    {
      err_code = 3;
      goto FWRITE_ERR;
    }
   if (fflush(this->file))
    {
      err_code = 4;
      goto FWRITE_ERR;
    }

   return 0;

FWRITE_ERR:
   free(this->fbuf);
FBUF_MALLOC_ERR:
   fclose(this->file);
FOPEN_ERR:
   return err_code;
}


int bptr_fload(struct bptree *this, const char *filename)
{
   uint32_t mvb_buf[3];
   int err_code;
   void *memit;

   /* Open file */
   this->file = fopen(filename, "rb+");
   if (this->file == NULL)
    {
      err_code = 1;
      goto FOPEN_ERR;
    }

   /* Check magic, version and block size */
   if (fread(mvb_buf, 4, 3, this->file) != 3)
    {
      err_code = 2;
      goto MVB_READ_ERR;
    }
   if (strncmp((const char*)mvb_buf, BPTR_MAGIC_STR, 4))
    {
      err_code = -1;
      goto MVB_INVALID_ERR;
    }
   this->version = mvb_buf[1] & 0x7F;
   if (this->version != BPTR_CURRENT_VERSION)
    {
      err_code = -2;
      goto MVB_INVALID_ERR;
    }
   this->is_lite = (mvb_buf[1] & 0x80) ? 1 : 0;
   this->block_size = mvb_buf[2];
   if (this->block_size < BPTR_NODE_METADATA_BYTE + 24)
    {
      err_code = -3;
      goto MVB_INVALID_ERR;
    }

   /* malloc file buffer */
   this->fbuf = malloc(this->block_size);
   if (this->fbuf == NULL)
    {
      err_code = 3;
      goto FBUF_MALLOC_ERR;
    }

   /* Read the header block */
   rewind(this->file);
   if (fread(this->fbuf, this->block_size, 1, this->file) != 1)
    {
      err_code = 4;
      goto READ_HEADER_BLOCK_ERR;
    }

   /* load metadata in header block into handler */
   memit = this->fbuf + 12;
   this->node_size = *(uint32_t*)memit;
   memit += 4;
   this->data_info.key_type = *(uint8_t*)memit;
   memit += 1;
   this->data_info.value_size = *(uint16_t*)memit;
   memit += 3; // +1 for padding
   this->stats.record_count = *(uint64_t*)memit;
   memit += 8;
   this->stats.tree_height = *(uint32_t*)memit;
   memit += 4;
   if (this->is_lite)
      _bptr_fload_read_uptr_metadata(BPTR_LITE_PTR_TYPE, BPTR_LITE_PTR_BYTE);
   else
      _bptr_fload_read_uptr_metadata(BPTR_NORM_PTR_TYPE, BPTR_NORM_PTR_BYTE);

   return 0;

READ_HEADER_BLOCK_ERR:
   free(this->fbuf);
FBUF_MALLOC_ERR:
MVB_INVALID_ERR:
MVB_READ_ERR:
   fclose(this->file);
FOPEN_ERR:
   return err_code;
}

int bptr_fclose(struct bptree *this)
{
   int err_code = 0;

   if (fclose(this->file))
      err_code = 1;
   free(this->fbuf);

   return err_code;
}
/*--------------------------- Public Functions END ---------------------------*/


/*---------------------------- Private Functions -----------------------------*/
/*-------------------------- Private Functions END ---------------------------*/
