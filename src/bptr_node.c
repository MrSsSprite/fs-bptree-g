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
   *_tmp_ptr++ = (type)(val); \
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

//TODO: fix leaf kv size calculation. # of keys and # of vals should be equal
#define _node_kv_malloc(self, node) do \
{ \
   uint_fast32_t up_bound = _node_is_leaf(self, node) ? \
                            self->node_boundry.leaf.up : \
                            self->node_boundry.brch.up; \
   (node)->keys = malloc(_bptr_key_size((self)->key_type) * ((up_bound) - 1)); \
   (node)->vals = malloc(_node_val_arr_size(self, node)); \
} while (0)

#define _node_val_size(self, node) \
   (_node_is_leaf(self, node) ? self->value_size : \
                                (self->is_lite ? BPTR_LITE_PTR_BYTE : \
                                                 BPTR_NORM_PTR_BYTE))
#define _node_val_arr_size(self, node) \
   (_node_is_leaf((self), (node)) ? \
      (self)->value_size * (node)->key_count : \
      ((self)->is_lite ? BPTR_LITE_PTR_BYTE : BPTR_NORM_PTR_BYTE * \
         ((node)->key_count + 1)))
/*---------------------------- Private Macro END -----------------------------*/


/*---------------------- Private Function Declarations -----------------------*/
static inline
void bptr_node_marshal(struct bptr *self, struct bptr_node *node);
static inline
int bptr_node_unmarshal(struct bptr *self, struct bptr_node *node);
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
    {
      bptr_errno = 1;
      goto NODE_MALLOC_ERR;
    }
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
   node->node_idx = 0;
   _node_kv_malloc(self, node);
   if (node->keys == NULL || node->vals == NULL)
    {
      bptr_errno = 1;
      goto KV_MALLOC_ERR;
    }
   if (parent)
    {
      struct bptr_node *parent_node = bptr_node_load(self, parent);
      if (parent_node == NULL)
       {
         bptr_errno = 2;
         goto LOAD_PARENT_ERR;
       }
      node->level = parent_node->level + 1;
      bptr_node_unload(parent_node);
    }
   else
      node->level = 0;
   /* TODO: checksum */

   return node;

LOAD_PARENT_ERR:
KV_MALLOC_ERR:
   free(node->vals);
   free(node->keys);
   free(node);
NODE_MALLOC_ERR:
   return NULL;
}


void bptr_node_free(struct bptr_node *node)
{
   free(node->keys); free(node->vals);
   free(node);
}


void bptr_node_unload(struct bptr_node *node)
{
   //TODO: involve cache mechanism
   bptr_node_free(node);
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


int bptr_node_flush(struct bptr *self, struct bptr_node *node)
{
   bptr_node_marshal(self, node);
   return bptr_io_flush_node(self, node->node_idx);
}
/*--------------------------- Public Functions END ---------------------------*/



/*---------------------------- Private Functions -----------------------------*/
// Write to fbuf
static inline
void bptr_node_marshal(struct bptr *self, struct bptr_node *node)
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
   if (self->is_lite)   _WRITE_FIELDS(BPTR_LITE_PTR_TYPE);
   else                 _WRITE_FIELDS(BPTR_NORM_PTR_TYPE);
#undef _WRITE_FIELDS
   buf_it = (char*)self->fbuf + BPTR_NODE_METADATA_BYTE;
   
   buf_write(buf_it, node->keys,
             _bptr_key_size(self->key_type) * node->key_count);
   buf_write(buf_it, node->vals, _node_val_arr_size(self, node));
}


// This function allocates memory for keys and vals.
// keys and vals shouldn't own memory before a call to this function
static inline
int bptr_node_unmarshal(struct bptr *self, struct bptr_node *node)
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
   buf_it = (char*)self->fbuf + BPTR_NODE_METADATA_BYTE;
   
   node->is_leaf = _node_is_leaf(self, node);
   node->is_dirty = 0;
   _node_kv_malloc(self, node);
   if (node->keys == NULL || node->vals == NULL)
    {
      free(node->keys); free(node->vals);
      return 1;
    }
   buf_read(buf_it, node->keys,
            _bptr_key_size(self->key_type) * node->key_count);
   buf_read(buf_it, node->vals, _node_val_arr_size(self, node));

   return 0;
}
/*-------------------------- Private Functions END ---------------------------*/
