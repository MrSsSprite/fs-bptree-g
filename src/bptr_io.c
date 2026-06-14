/*----------------------------- Private Includes -----------------------------*/
#include "bptr_io.h"
#include "../bptree.h"
#include "bptr_internal.h"
#include "bptr_node.h"
#include "bptr_utils.h"
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
   *(uptr_type*)memit = self->free_list.cnt; \
   memit += (uptr_size); \
   *(uptr_type*)memit = self->node_cnt; \
   memit += (uptr_size); \
} while (0)

#define _bptr_fload_read_uptr_metadata(uptr_type, uptr_size) do \
{ \
   self->root_idx = *(uptr_type*)memit; memit += (uptr_size); \
   self->free_list.head = *(uptr_type*)memit; memit += (uptr_size); \
   self->free_list.cnt = *(uptr_type*)memit; memit += (uptr_size); \
   self->node_cnt = *(uptr_type*)memit; memit += (uptr_size); \
} while (0)
/*---------------------------- Private Macros END ----------------------------*/


/*{---------------------- Private Function Declaration -----------------------*/

/**
 * @brief   Serialize bptr header info into self->fbuf
 *
 * @param[in,out] self  bptr obj. Only @c fbuf member is modified.
 */
static inline
void bptr_header_marshal(struct bptr *self);
/**
 * @brief   Flush metadata from fbuf to the file
 *
 * @param[in,out] self  bptr obj. @c fbuf is read and @c file is read&written.
 *
 * @return        execution status
 * @retval        0 (BPTR_E_SUCCESS)   Success
 * @retval        BPTR_E_FACCESS       Failed on @c fwrite or @c fflush .
 *
 * @warning This function only writes @c fbuf member directly to header of the
 *          file. Caller shall fill @c fbuf correctly before calling this
 *          function (e.g., use @c bptr_header_marshal ).
 *
 * @see  bptr_header_marshal
 */
static inline
int _flush_to_header(struct bptr *self);
/**
 * @brief   Write bptr obj. content to file header.
 *
 * @param[in,out] self  bptr obj.
 *
 * @return        execution status
 * @retval        0 (BPTR_E_SUCCESS)   Success
 * @retval        BPTR_E_FACCESS       Failed on @c fwrite or @c fflush .
 *
 * @see  _flush_to_header
 * @see  bptr_header_marshal
 */
static inline
int _header_fwrite(struct bptr *self)
{ bptr_header_marshal(self); return _flush_to_header(self); }
/*}---------------------- Private Function Declaration -----------------------*/


/*----------------------------- Public Functions -----------------------------*/
int bptr_io_fcreat(struct bptr *self, const char *filename)
{
   _Bool has_set_err = 0;
   int err_code;
   char *memit;

   /* Create file */
   self->file = fopen(filename, "wbx+");
   if (self->file == NULL)
      goto FOPEN_ERR;
   /* malloc for file buffer */
   self->fbuf = calloc(1, self->node_size);
   if (self->fbuf == NULL)
      goto FBUF_MALLOC_ERR;

   switch (_header_fwrite(self))
    {
   case 0:  break;
   default:
      _set_err_code(BPTR_E_UNREACHABLE);
   case BPTR_E_FACCESS:
      goto FWRITE_ERR;
    }

   return 0;

FWRITE_ERR:      _set_err_code(BPTR_E_FACCESS);
   free(self->fbuf);
FBUF_MALLOC_ERR: _set_err_code(BPTR_E_OOM);
   fclose(self->file);
FOPEN_ERR:       _set_err_code(BPTR_E_FACCESS);
   return err_code;
}


int bptr_io_fload(struct bptr *self, const char *filename)
{
   uint32_t mvb_buf[3];
   char *memit;

   /* Open file */
   self->file = fopen(filename, "rb+");
   if (self->file == NULL) goto FOPEN_ERR;

   /* Check magic, version and block size */
   if (fread(mvb_buf, 4, 3, self->file) != 3) goto MVB_READ_ERR;
   if (strncmp((const char*)mvb_buf, BPTR_MAGIC_STR, 4))
      goto MVB_INVALID_ERR;
   self->version = mvb_buf[1] & 0x7F;
   if (self->version != BPTR_CURRENT_VERSION)
      goto MVB_INVALID_ERR;
   self->is_lite = (mvb_buf[1] & 0x80) ? 1 : 0;
   self->node_size = mvb_buf[2];
   if (self->node_size < BPTR_NODE_METADATA_BYTE + 24)
      goto MVB_INVALID_ERR;

   /* malloc file buffer */
   self->fbuf = calloc(1, self->node_size);
   if (self->fbuf == NULL)
      goto FBUF_MALLOC_ERR;

   /* Read the header block */
   rewind(self->file);
   if (fread(self->fbuf, self->node_size, 1, self->file) != 1)
      goto READ_HEADER_BLOCK_ERR;

   /* load metadata in header block into handler */
   memit = self->fbuf + 12;
   self->key_size = *(uint16_t*)memit;
   if (self->key_size == 0) goto INVALID_KV_SIZE;
   memit += 2;
   self->value_size = *(uint16_t*)memit;
   if (self->value_size == 0) goto INVALID_KV_SIZE;
   memit += 2;
   self->record_cnt = *(uint64_t*)memit;
   memit += 8;
   self->height = *(uint32_t*)memit;
   memit += 4;
   if (self->is_lite)
      _bptr_fload_read_uptr_metadata(BPTR_LITE_PTR_TYPE, BPTR_LITE_PTR_BYTE);
   else
      _bptr_fload_read_uptr_metadata(BPTR_NORM_PTR_TYPE, BPTR_NORM_PTR_BYTE);

   return 0;

   /*-------------------------- Error Handling Zone --------------------------*/
   _Bool has_set_err = 0;
   int err_code;

INVALID_KV_SIZE:       _set_err_code(BPTR_E_ITRNL_STATE);
READ_HEADER_BLOCK_ERR: _set_err_code(BPTR_E_FACCESS);
   free(self->fbuf);
FBUF_MALLOC_ERR:       _set_err_code(BPTR_E_OOM);
MVB_INVALID_ERR:       _set_err_code(BPTR_E_ITRNL_STATE);
MVB_READ_ERR:          _set_err_code(BPTR_E_FACCESS);
   fclose(self->file);
FOPEN_ERR:             _set_err_code(BPTR_E_FACCESS);
   return err_code;
}

int bptr_io_fclose(struct bptr *self)
{
   _Bool has_set_err = 0;
   int err_code = 0;

   switch (_header_fwrite(self))
    {
   case 0: break;
   default:
      _set_err_code(BPTR_E_UNREACHABLE);
   case BPTR_E_FACCESS:
      goto FWRITE_ERR;
    }

   if (fclose(self->file))
      // don't goto err handler as further access is undefined even on error
      err_code = BPTR_E_FCLOSE;
   free(self->fbuf);

   return err_code;

FWRITE_ERR: _set_err_code(BPTR_E_FACCESS);
   return err_code;
}


// Undefined if node_idx is 0
int bptr_io_fread_node(struct bptr *self, bptr_node_t node_idx)
{
   bptr_off_t offset = node_idx * self->node_size;

   
   if (fseek64(self->file, offset, SEEK_SET))
      return 2;

   if (fread(self->fbuf, self->node_size, 1, self->file) != 1)
      return 3;

   return 0;
}


int bptr_io_flush_node(struct bptr *self, bptr_node_t node_idx)
{
   bptr_off_t pos;

   
   if (node_idx == 0)
      goto INVALID_INPUT_ERR;
   else  // update node
    {
      if (fseek64(self->file, node_idx * self->node_size, SEEK_SET))
         goto FACCESS_ERR;
    }

   // calculate the location of node in block size
   pos = ftell64(self->file);
   if (pos == -1L) goto FACCESS_ERR;
   pos /= self->node_size;

   if (fwrite(self->fbuf, self->node_size, 1, self->file) != 1)
      goto FACCESS_ERR;
   if (fflush(self->file))
      goto FACCESS_ERR;

   return 0;

   /*-------------------------- Error Handling Zone --------------------------*/
   int err_code;
   _Bool has_set_err = 0;

FACCESS_ERR:         _set_err_code(BPTR_E_FACCESS);
INVALID_INPUT_ERR:   _set_err_code(BPTR_E_FN_INPUT);
   return err_code;
}
/*--------------------------- Public Functions END ---------------------------*/

/*{--------------------------- Private Functions -----------------------------*/
static inline
void bptr_header_marshal(struct bptr *self)
{
   char *memit = self->fbuf;

   strncpy(memit, BPTR_MAGIC_STR, 4);
   memit += 4;
   *(uint32_t*)memit = (self->is_lite) ?
                       0x80 | BPTR_CURRENT_VERSION : BPTR_CURRENT_VERSION;
   memit += 4;
   *(uint32_t*)memit = self->node_size;
   memit += 4;

   *(uint16_t*)memit = self->key_size;
   memit += 2;
   *(uint16_t*)memit = self->value_size;
   memit += 2; // +1 for padding

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
}


// Flush the Buffer to the file
static inline
int _flush_to_header(struct bptr *self)
{
   if (fseek64(self->file, 0, SEEK_SET))
      return BPTR_E_FACCESS;
   if (fwrite(self->fbuf, self->node_size, 1, self->file) != 1)
      return BPTR_E_FACCESS;
   if (fflush(self->file))
      return BPTR_E_FACCESS;

   return 0;
}
/*}--------------------------- Private Functions -----------------------------*/
