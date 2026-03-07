/*----------------------------- Private Includes -----------------------------*/
#include "bptr_node.h"
#include "bptr_internal.h"
#include "bptr_io.h"
#include <stdlib.h>
#include <string.h>
/*--------------------------- Private Includes END ---------------------------*/


/*------------------------------ Private Macro -------------------------------*/
#define iter_write(iter, src, size) do \
{ \
   memcpy((iter), (src), (size)); \
   (iter) = (char*)(iter) + (size); \
} while (0)

#define buf_tc_write(buf, val, type) do \
{ \
   type _val_tc = (val); \
   memcpy((buf), &_val_tc, sizeof(_val_tc)); \
} while (0)

#define iter_tc_write(iter, val, type) do \
{ \
   buf_tc_write((iter), (val), type); \
   (iter) = (char*)(iter) + sizeof(type); \
} while (0)


#define iter_read(iter, dst, size) do \
{ \
   memcpy((dst), (iter), (size)); \
   (iter) = (char*)(iter) + (size); \
} while (0)

#define buf_tc_read(buf, dst, type) do \
{ \
   type val; \
   memcpy(&val, (buf), sizeof(val)); \
   (dst) = val; \
} while (0)

#define iter_tc_read(iter, dst, type) do \
{ \
   buf_tc_read((iter), (dst), type); \
   (iter) += sizeof(type); \
} while (0)

#define _node_kv_malloc(self, node) do \
{ \
   if ((node)->is_leaf) \
    { \
      (node)->keys = \
         malloc(((self)->node_boundry.leaf.up - 1) * (self)->key_size); \
      (node)->vals = \
         malloc(((self)->node_boundry.leaf.up - 1) * (self)->value_size); \
    } \
   else \
    { \
      (node)->keys = \
         malloc(((self)->node_boundry.brch.up - 1) * (self)->key_size); \
      (node)->vals = \
         malloc((self)->node_boundry.brch.up * BPTR_PTR_SIZE); \
    } \
} while (0)

#define _node_val_size(self, node) \
   ((node)->is_leaf ? (self)->value_size : \
                      ((self)->is_lite ? BPTR_LITE_PTR_BYTE : \
                                         BPTR_NORM_PTR_BYTE))
#define _node_val_arr_size(self, node) \
   ((node)->is_leaf ? \
      (self)->value_size * (node)->key_count : \
      ((self)->is_lite ? BPTR_LITE_PTR_BYTE : \
                         BPTR_NORM_PTR_BYTE * ((node)->key_count + 1)))
/*---------------------------- Private Macro END -----------------------------*/


/*---------------------- Private Function Declarations -----------------------*/
// Write node to fbuf
/**
 * @brief   Marshal node data from struct to self->fbuf
 *
 * @param[in,out] self  bptr obj.
 * @param[in]     node  node obj. to be serialized.
 *
 * @node    Serialization should be a more accurate name, but marshal is
 *          adopted as it's shorter.
 */
static inline
void bptr_node_marshal(struct bptr *self, struct bptr_node *node);
/**
 * @brief   Serialize node data from self->fbuf to node struct.
 *
 * @param[in,out] self  bptr obj.
 * @param[out]    node  node obj. to hold the deserialized data
 * @return        error code
 * @retval  0     success
 * @retval  1     malloc error
 *
 * @warning    This function allocates memory for node->keys and node->vals;
 *             thence, these two members should not have exclusive ownership
 *             on anything before a call to this function as their value will
 *             be overwritten.
 * @node       Serialization should be a more accurate name, but marshal is
 *             adopted as it's shorter.
 */
static inline
int bptr_node_unmarshal(struct bptr *self, struct bptr_node *node);
/**
 * @brief   preallocate node-sized space in file
 *
 * This function reserves a free node-sized block in self->file and returns a
 * node index (not direct file offset) referring to that block.
 *
 * @param   self  bptr obj.
 * @return  the node index that's allocated.
 * @retval  0     failure; bptr_errno is set.
 *
 * @remark  the return value is node indx. This means the actual file offset is
 *          calculated thru. `ret * self->node_size`.
 */
static inline
bptr_node_t bptr_node_prealloc (struct bptr *self);
/*-------------------- Private Function Declarations END ---------------------*/


/*----------------------------- Public Functions -----------------------------*/
// node->{prev, next} are left uninitialized; the
// caller is responsible to write that
struct bptr_node *bptr_node_new
 (struct bptr *self, _Bool is_leaf, bptr_node_t parent)
{
   uint16_t flags;
   struct bptr_node *node;

   node = malloc(sizeof(struct bptr_node));
   if (node == NULL)
    { bptr_errno = 1; goto NODE_MALLOC_ERR; }

   flags = BPTR_NODE_FLAG_VALID;
   if (is_leaf)
    {
      flags |= BPTR_NODE_FLAG_LEAF;
      node->is_leaf = 1;
    }
   else
      node->is_leaf = 0;
   node->flags = flags;
   node->is_dirty = 1;
   node->key_count = 0;
   node->parent = parent;
   node->node_idx = bptr_node_prealloc(self);
   if (node->node_idx == 0) goto PREALLOC_ERR;
   _node_kv_malloc(self, node);
   if (node->keys == NULL || node->vals == NULL)
    { bptr_errno = 1; goto KV_MALLOC_ERR; }
   if (parent)
    {
      struct bptr_node *parent_node = bptr_node_load(self, parent);
      if (parent_node == NULL)
       { bptr_errno = 2; goto LOAD_PARENT_ERR; }
      node->level = parent_node->level + 1;
      bptr_node_unload(self, parent_node);
    }
   else
      node->level = 0;
   /* TODO: checksum */

   return node;

LOAD_PARENT_ERR:
KV_MALLOC_ERR:
   free(node->vals);
   free(node->keys);
PREALLOC_ERR:
   free(node);
NODE_MALLOC_ERR:
   return NULL;
}


void bptr_node_free(struct bptr_node *node)
{
   free(node->keys); free(node->vals);
   free(node);
}


int bptr_node_unload(struct bptr *self, struct bptr_node *node)
{
   //TODO: involve cache mechanism
   if (node->is_dirty && bptr_node_flush(self, node) == 0)
      return 2;
   bptr_node_free(node);
   return 0;
}


struct bptr_node *bptr_node_load(struct bptr *self, bptr_node_t node_idx)
{
   struct bptr_node *node;

   node = malloc(sizeof(struct bptr_node));
   if (node == NULL)
    {
      bptr_errno = 1;
      goto NODE_MALLOC_ERR;
    }

   // Read into fbuf
   if (bptr_io_fread_node(self, node_idx))
    {
      bptr_errno = -1;
      goto FREAD_NODE_ERR;
    }

   if (bptr_node_unmarshal(self, node))
    {
      bptr_errno = 1;
      goto UNMARSHAL_ERR;
    }
   node->is_dirty = 0;
   node->node_idx = node_idx;

   return node;

UNMARSHAL_ERR:
FREAD_NODE_ERR:
   free(node);
NODE_MALLOC_ERR:
   return NULL;
}


bptr_node_t bptr_node_flush(struct bptr *self, struct bptr_node *node)
{
   bptr_node_marshal(self, node);
   return bptr_io_flush_node(self, node->node_idx);
}
/*--------------------------- Public Functions END ---------------------------*/



/*---------------------------- Private Functions -----------------------------*/
static inline
void bptr_node_marshal(struct bptr *self, struct bptr_node *node)
{
   void *buf_it = self->fbuf;

   iter_write(buf_it, &node->flags, 2);
   iter_write(buf_it, &node->level, 2);
   iter_write(buf_it, &node->key_count, 4);
   iter_write(buf_it, &node->checksum, 4);
#define _WRITE_FIELDS(type) do { \
      iter_tc_write(buf_it, node->parent, type); \
      iter_tc_write(buf_it, node->next, type); \
      iter_tc_write(buf_it, node->prev, type); \
} while (0)
   if (self->is_lite)   _WRITE_FIELDS(BPTR_LITE_PTR_TYPE);
   else                 _WRITE_FIELDS(BPTR_NORM_PTR_TYPE);
#undef _WRITE_FIELDS
   buf_it = (char*)self->fbuf + BPTR_NODE_METADATA_BYTE;

   iter_write(buf_it, node->keys,
             self->key_size * node->key_count);
   iter_write(buf_it, node->vals, _node_val_arr_size(self, node));
}


static inline
int bptr_node_unmarshal(struct bptr *self, struct bptr_node *node)
{
   void *buf_it = self->fbuf;

   iter_read(buf_it, &node->flags, 2);
   iter_read(buf_it, &node->level, 2);
   iter_read(buf_it, &node->key_count, 4);
   iter_read(buf_it, &node->checksum, 4);
#define _READ_FIELDS(type) do { \
      iter_tc_read(buf_it, node->parent, type); \
      iter_tc_read(buf_it, node->next, type); \
      iter_tc_read(buf_it, node->prev, type); \
} while (0)
   if (self->is_lite)   _READ_FIELDS(uint32_t);
   else                 _READ_FIELDS(uint64_t);
#undef _READ_FIELDS
   buf_it = (char*)self->fbuf + BPTR_NODE_METADATA_BYTE;

   node->is_leaf = (self->height == node->level);
   node->is_dirty = 0;
   _node_kv_malloc(self, node);
   if (node->keys == NULL || node->vals == NULL)
    {
      free(node->keys); free(node->vals);
      return 1;
    }
   iter_read(buf_it, node->keys,
            self->key_size * node->key_count);
   iter_read(buf_it, node->vals, _node_val_arr_size(self, node));

   return 0;
}

static
bptr_node_t bptr_node_prealloc (struct bptr *self)
{
   bptr_node_t ret;

   if (self->free_list.cnt)
    {
      ret = self->free_list.head;
      if (bptr_io_fread_node(self, self->free_list.head))
       { bptr_errno = -1; return 0; }
      if (self->is_lite)
         buf_tc_read((char*)self->fbuf + 2, self->free_list.head,
                     BPTR_LITE_PTR_TYPE);
      else
         buf_tc_read((char*)self->fbuf + 2, self->free_list.head,
                     BPTR_NORM_PTR_TYPE);
      self->free_list.cnt--;
    }
   else
    {
      static const char dummy = 0;
      bptr_off_t offset;

      if (fseek64(self->file, 0, SEEK_END))
       { bptr_errno = -1; return 0; }
      offset = ftell64(self->file);
      if (offset == -1)
       { bptr_errno = -1; return 0; }
      if (fseek64(self->file, self->node_size - 1, SEEK_CUR))
       { bptr_errno = -1; return 0; }
      if (fwrite(&dummy, 1, 1, self->file) != 1)
       { bptr_errno = -1; return 0; }
      if (fflush(self->file))
       { bptr_errno = -1; return 0; }
      ret = offset / self->node_size;
    }
   return ret;
}


// It's caller's responsibility to check it's valid to insert the node
static inline
void _node_key_insert(struct bptr *self, struct bptr_node *node,
                      const void *key, uint_fast32_t idx)
{
   uint_fast32_t idx_plus1 = idx + 1,
                 /* up is max_SIZE + 1; max_INDEX is max_SIZE - 1 */
                 ed = (node->is_leaf ? self->node_boundry.leaf.up :
                                       self->node_boundry.brch.up) - 1;
   char *tar_p = (char*)node->keys + idx * self->key_size;

   // insert idx is not last element
   if (idx_plus1 != ed)
      // reserve slot
      memmove(tar_p + self->key_size, tar_p,
              (node->key_count - idx) * self->key_size);
   // copy into slot
   memcpy(tar_p, key, self->key_size);
}


// It's caller's responsibility to check it's valid to insert the node
static inline
void _node_val_insert(struct bptr *self, struct bptr_node *node,
                      const void *val, uint_fast32_t idx)
{
   uint_fast32_t idx_plus1 = idx + 1,
                 /* up is max_SIZE + 1; max_INDEX is max_SIZE - 1
                  * leaf: val_cnt == key_cnt; branch: val_cnt == key_cnt + 1 */
                 ed = (node->is_leaf ? self->node_boundry.leaf.up :
                                       self->node_boundry.brch.up + 1) - 1;
   uint_fast16_t val_sz = _node_val_size(self, node);
   char *tar_p = (char*)node->vals + idx * val_sz;

   // insert idx is not last element
   if (idx_plus1 != ed)
      // reserve slot
      memmove(tar_p + val_sz, tar_p,
              (node->key_count + (node->is_leaf ? 0 : 1) - idx) * val_sz);
   // copy into slot
   memcpy(tar_p, val, val_sz);
}
/*-------------------------- Private Functions END ---------------------------*/
