/*----------------------------- Private Includes -----------------------------*/
#include "bptr_node.h"
#include "bptr_internal.h"
#include "bptr_io.h"
#include <stdlib.h>
#include <string.h>
/*--------------------------- Private Includes END ---------------------------*/


/*------------------------------ Private Macro -------------------------------*/
#define buf_write(iter, src, size) do \
{ \
   memcpy((iter), (src), (size)); \
   (iter) = (char*)(iter) + (size); \
} while (0)

#define buf_tc_write(iter, val, type) do \
{ \
   type *_tmp_ptr = (iter); \
   *_tmp_ptr++ = (val); \
   (iter) = _tmp_ptr; \
} while (0)


#define buf_read(iter, dst, size) do \
{ \
   memcpy((dst), (iter), (size)); \
   (iter) = (char*)(iter) + (size); \
} while (0)

#define buf_tc_read(iter, dst, type) do \
{ \
   type *_tmp_ptr = (iter); \
   (dst) = *_tmp_ptr++; \
   (iter) = _tmp_ptr; \
} while (0)
   
/*---------------------------- Private Macro END -----------------------------*/


/*---------------------- Private Function Declarations -----------------------*/
int bptr_node_marshal(struct bptree *self, struct bptr_node *node);
int bptr_node_unmarshal(struct bptree *self, struct bptr_node *node);
/*-------------------- Private Function Declarations END ---------------------*/


/*----------------------------- Public Functions -----------------------------*/
struct bptr_node *bptr_node_new
 (struct bptree *self, _Bool is_leaf, bptr_node_t parent)
{
   uint16_t flags;
   struct bptr_node *node;

   node = malloc(sizeof(struct bptr_node));
   if (node == NULL) goto NODE_MALLOC_ERR;
   flags = BPTR_NODE_FLAG_VALID;
   node->is_dirty = 1;
   node->node_idx = 0;
   node->key_count = 0;
   node->next = node->prev = 0;
   node->parent = parent;
   node->keys = vec_new(_bptr_key_size(self->key_type));
   node->vals = vec_new(self->value_size);
   if (node->keys == NULL || node->vals == NULL) goto KV_MALLOC_ERR;
   if (parent)
    {
      //TODO: level
    }
   else
      node->level = 0;
   if (is_leaf)
    {
      flags |= BPTR_NODE_FLAG_LEAF;
      node->type = BPTR_NODE_TYPE_LEAF;
    }
   else
      node->type = BPTR_NODE_TYPE_BRCH;
   node->flags = flags;
   /* TODO: checksum */

   return node;

KV_MALLOC_ERR:
   vec_free(node->vals);
   vec_free(node->keys);
NODE_MALLOC_ERR:
   bptr_errno = 1;
   return NULL;
}


void bptr_node_unload(struct bptr_node *node)
{
   free(node->keys);
   free(node->vals);
   free(node);
}


struct bptr_node *bptr_node_load(struct bptree *self, bptr_node_t node_idx)
{
   // Read into fbuf
   if (bptr_fread_node(self, node_idx))
    {
      bptr_errno = 1;
      return NULL;
    }

   // TODO
}
/*--------------------------- Public Functions END ---------------------------*/


/*---------------------------- Private Functions -----------------------------*/
int bptr_node_marshal(struct bptree *self, struct bptr_node *node)
{
   void *buf_it = self->fbuf;

   buf_write(buf_it, &node->flags, 2);
   buf_write(buf_it, &node->level, 2);
   buf_write(buf_it, &node->key_count, 4);
   buf_write(buf_it, &node->checksum, 4);
#define _WRITE_FIELDS(type) do { \
      buf_tc_write(buf_it, node->parent, type); \
      buf_tc_write(buf_it, node->next, type); \
      buf_tc_write(buf_it, node->prev, type); \
} while (0)
   if (self->is_lite)   _WRITE_FIELDS(uint32_t);
   else                 _WRITE_FIELDS(uint64_t);
#undef _WRITE_FIELDS

   buf_it = (char*)self->fbuf + BPTR_NODE_METADATA_BYTE;
   buf_write(buf_it, node->keys,
             _bptr_key_size(self->key_type) * node->key_count);
   buf_write(buf_it, node->vals, self->value_size * (node->key_count + 1));

   return 0;
}


int bptr_node_unmarshal(struct bptree *self, struct bptr_node *node)
{
   void *buf_it = self->fbuf;

   buf_read(buf_it, &node->flags, 2);
   buf_read(buf_it, &node->level, 2);
   buf_read(buf_it, &node->key_count, 4);
   buf_read(buf_it, &node->checksum, 4);
#define _READ_FIELDS(type) do { \
      buf_tc_read(buf_it, node->parent, type); \
      buf_tc_read(buf_it, node->next, type); \
      buf_tc_read(buf_it, node->prev, type); \
} while (0)
   if (self->is_lite)   _READ_FIELDS(uint32_t);
   else                 _READ_FIELDS(uint64_t);
#undef _READ_FIELDS

   /* TODO: read key value */

   return 0;
}
/*-------------------------- Private Functions END ---------------------------*/
