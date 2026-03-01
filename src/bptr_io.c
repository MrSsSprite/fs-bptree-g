/*----------------------------- Private Includes -----------------------------*/
#include "bptr_io.h"
#include "../bptree.h"
#include "bptr_internal.h"
#include "bptr_node.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
/*--------------------------- Private Includes END ---------------------------*/


/*------------------------------ Private Macros ------------------------------*/
#define _bptr_fcreat_write_uptr_metadata(uptr_type, uptr_size) do \
{ \
   *(uptr_type*)memit = self->root_idx; \
   memit += (uptr_size); \
   *(uptr_type*)memit = self->free_list.head; \
   memit += (uptr_size); \
   *(uptr_type*)memit = self->free_list.size; \
   memit += (uptr_size); \
   *(uptr_type*)memit = self->node_cnt; \
   memit += (uptr_size); \
} while (0)

#define _bptr_fload_read_uptr_metadata(uptr_type, uptr_size) do \
{ \
   self->root_idx = *(uptr_type*)memit; memit += (uptr_size); \
   self->free_list.head = *(uptr_type*)memit; memit += (uptr_size); \
   self->free_list.size = *(uptr_type*)memit; memit += (uptr_size); \
   self->node_cnt = *(uptr_type*)memit; memit += (uptr_size); \
} while (0)
/*---------------------------- Private Macros END ----------------------------*/


/*----------------------------- Public Functions -----------------------------*/
int bptr_io_fcreat(struct bptr *self, const char *filename)
{
   int err_code;
   void *memit;

   /* Create file */
   self->file = fopen(filename, "wbx+");
   if (self->file == NULL)
    {
      err_code = 1;
      goto FOPEN_ERR;
    }
   /* malloc for file buffer */
   self->fbuf = malloc(self->block_size);
   if (self->fbuf == NULL)
    {
      err_code = 2;
      goto FBUF_MALLOC_ERR;
    }

   /* Write file header */
   memit = self->fbuf;
   strncpy(memit, BPTR_MAGIC_STR, 4);
   memit += 4;
   *(uint32_t*)memit = (self->is_lite) ?
                       0x80 | BPTR_CURRENT_VERSION : BPTR_CURRENT_VERSION;
   memit += 4;
   *(uint32_t*)memit = self->block_size;
   memit += 4;

   *(uint32_t*)memit = self->node_size;
   memit += 4;

   *(uint16_t*)memit = self->key_size;
   memit += 1;
   *(uint16_t*)memit = self->value_size;
   memit += 3; // +1 for padding

   *(uint64_t*)memit = self->record_cnt;
   memit += 8;
   *(uint32_t*)memit = self->height;
   memit += 4;
   if (self->is_lite)
      _bptr_fcreat_write_uptr_metadata(BPTR_LITE_PTR_TYPE,
                                       BPTR_LITE_PTR_BYTE);
   else
      _bptr_fcreat_write_uptr_metadata(BPTR_NORM_PTR_TYPE,
                                       BPTR_NORM_PTR_BYTE);

   /* Flush the Buffer to the file */
   if (fwrite(self->fbuf, self->block_size, 1, self->file) != 1)
    {
      err_code = 3;
      goto FWRITE_ERR;
    }
   if (fflush(self->file))
    {
      err_code = 4;
      goto FWRITE_ERR;
    }

   return 0;

FWRITE_ERR:
   free(self->fbuf);
FBUF_MALLOC_ERR:
   fclose(self->file);
FOPEN_ERR:
   return err_code;
}


int bptr_io_fload(struct bptr *self, const char *filename)
{
   uint32_t mvb_buf[3];
   int err_code;
   void *memit;

   /* Open file */
   self->file = fopen(filename, "rb+");
   if (self->file == NULL)
    {
      err_code = 1;
      goto FOPEN_ERR;
    }

   /* Check magic, version and block size */
   if (fread(mvb_buf, 4, 3, self->file) != 3)
    {
      err_code = 2;
      goto MVB_READ_ERR;
    }
   if (strncmp((const char*)mvb_buf, BPTR_MAGIC_STR, 4))
    {
      err_code = -1;
      goto MVB_INVALID_ERR;
    }
   self->version = mvb_buf[1] & 0x7F;
   if (self->version != BPTR_CURRENT_VERSION)
    {
      err_code = -2;
      goto MVB_INVALID_ERR;
    }
   self->is_lite = (mvb_buf[1] & 0x80) ? 1 : 0;
   self->block_size = mvb_buf[2];
   if (self->block_size < BPTR_NODE_METADATA_BYTE + 24)
    {
      err_code = -3;
      goto MVB_INVALID_ERR;
    }

   /* malloc file buffer */
   self->fbuf = malloc(self->block_size);
   if (self->fbuf == NULL)
    {
      err_code = 3;
      goto FBUF_MALLOC_ERR;
    }

   /* Read the header block */
   rewind(self->file);
   if (fread(self->fbuf, self->block_size, 1, self->file) != 1)
    {
      err_code = 4;
      goto READ_HEADER_BLOCK_ERR;
    }

   /* load metadata in header block into handler */
   memit = self->fbuf + 12;
   self->node_size = *(uint32_t*)memit;
   memit += 4;
   self->key_size = *(uint16_t*)memit;
   memit += 1;
   self->value_size = *(uint16_t*)memit;
   memit += 3; // +1 for padding
   self->record_cnt = *(uint64_t*)memit;
   memit += 8;
   self->height = *(uint32_t*)memit;
   memit += 4;
   if (self->is_lite)
      _bptr_fload_read_uptr_metadata(BPTR_LITE_PTR_TYPE, BPTR_LITE_PTR_BYTE);
   else
      _bptr_fload_read_uptr_metadata(BPTR_NORM_PTR_TYPE, BPTR_NORM_PTR_BYTE);

   return 0;

READ_HEADER_BLOCK_ERR:
   free(self->fbuf);
FBUF_MALLOC_ERR:
MVB_INVALID_ERR:
MVB_READ_ERR:
   fclose(self->file);
FOPEN_ERR:
   return err_code;
}

int bptr_io_fclose(struct bptr *self)
{
   int err_code = 0;

   if (fclose(self->file))
      err_code = 1;
   free(self->fbuf);

   return err_code;
}


// Undefined if node_idx is 0
int bptr_io_fread_node(struct bptr *self, bptr_node_t node_idx)
{
   long offset = node_idx * self->node_size;

   
   if (fseek(self->file, offset, SEEK_SET))
      return 2;

   if (fread(self->fbuf, self->node_size, 1, self->file) != 1)
      return 3;

   return 0;
}


// node_idx == 0 means new node
bptr_node_t bptr_io_flush_node(struct bptr *self, bptr_node_t node_idx)
{
   long pos;

   
   if (node_idx == 0)   // new node
    {
      if (self->free_list.size)
       {
         if (bptr_io_fread_node(self, self->free_list.head))
          { bptr_errno = 1; return 0; }
         if (*(uint16_t*)self->fbuf & BPTR_NODE_FLAG_VALID)
          { bptr_errno = -1; return 0; }
         pos = self->free_list.head * self->node_size;
#define _FETCH_NEXT_FREE_NODE(T) do \
 { self->free_list.head = *(T*)((uint16_t*)self->fbuf + 1); } \
while (0)
         if (self->is_lite)   _FETCH_NEXT_FREE_NODE(BPTR_LITE_PTR_TYPE);
         else                 _FETCH_NEXT_FREE_NODE(BPTR_NORM_PTR_TYPE);
#undef _FETCH_NEXT_FREE_NODE
         self->free_list.size--;
         if (fseek(self->file, pos, SEEK_SET))
          { bptr_errno = 1; return 0; }
       }
      else
       {
         if (fseek(self->file, 0, SEEK_END))
          { bptr_errno = 1; return 0; }
       }
    }
   else                 // update node
    {
      if (fseek(self->file, node_idx * self->node_size, SEEK_SET))
       { bptr_errno = 1; return 0; }
    }

   // calculate the location of node in block size
   pos = ftell(self->file);
   if (pos == -1L)
    {
      bptr_errno = 2;
      return 0;
    }
   pos /= self->block_size;

   if (fwrite(self->fbuf, self->node_size, 1, self->file) != 1)
    {
      bptr_errno = 3;
      return 0;
    }
   if (fflush(self->file))
    {
      bptr_errno = 4;
      return 0;
    }

   return pos;
}
/*--------------------------- Public Functions END ---------------------------*/
