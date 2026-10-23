/*----------------------------- Private Includes -----------------------------*/
#include "../bptree.h"
#include "bptr_internal.h"
#include "bptr_io.h"
#include "bptr_node.h"
#include "bptr_cache.h"
#include "bptr_utils.h"
#include <stdlib.h>
#include <string.h>
/*--------------------------- Private Includes END ---------------------------*/


/*----------------------------- Private Structs ------------------------------*/
/*--------------------------- Private Structs END ----------------------------*/


/*------------------------------ Private Macros ------------------------------*/
#define _bptr_bound_set(self) do \
{  /* t >= (rem_sz / kv_sz + 1) / 2 */ \
   uint_fast32_t rem_sz = (self)->node_size - BPTR_NODE_METADATA_BYTE; \
   (self)->node_bound.brch.up = \
      (rem_sz - BPTR_PTR_SIZE) / ((self)->key_size + BPTR_PTR_SIZE) + 1; \
   if ((self)->node_bound.brch.up < 3) \
    { bptr_errno = -1; goto INVALID_FANOUT_ERR; } \
   (self)->node_bound.brch.low = CEIL_DIV((self)->node_bound.brch.up, 2) - 2; \
   /* should be B/(K + V) + 1; +1 is detained so that B/(K+V) can be used for \
    * temporary storage. \
    */ \
   (self)->node_bound.leaf.up = rem_sz / ((self)->key_size + (self)->value_size); \
   if ((self)->node_bound.leaf.up < 1) \
    { bptr_errno = -1; goto INVALID_FANOUT_ERR; } \
   (self)->node_bound.leaf.low = CEIL_DIV((self)->node_bound.leaf.up, 2) - 1; \
   (self)->node_bound.leaf.up += 1; \
} while (0)
/*---------------------------- Private Macros END ----------------------------*/


/*----------------------------- Public Variable ------------------------------*/
int bptr_errno;
/*--------------------------- Public Variable END ----------------------------*/


/*------------------------ Public Function Definition ------------------------*/
struct bptr *bptr_init
(
   const char *filename,
   _Bool is_lite,
   uint32_t node_size,
   uint16_t key_size,
   uint16_t value_size,
   uint64_t cache_capacity,
   int (*compare)(const void *lhs, const void *rhs)
)
{
   _Bool has_set_err = 0;
   struct bptr *self;

   /* Node must be large enough to at least contain
    * the metadata, 1 key and 2 childs */
   if (node_size < BPTR_NODE_METADATA_BYTE + key_size +
                   (is_lite ? BPTR_LITE_PTR_BYTE : BPTR_NORM_PTR_BYTE) * 2 ||
       cache_capacity < BPTR_CACHE_CAPACITY_MIN ||
       key_size == 0 || value_size == 0)
      goto INVALID_SIZE_ERR;

   /* malloc for the handler */
   self = malloc(sizeof (struct bptr));
   if (self == NULL) goto BPTR_MALLOC_ERR;

   /* Write Metadata */
   self->version = BPTR_CURRENT_VERSION;
   self->is_lite = is_lite;
   self->free_list.head = self->free_list.cnt = self->root_idx = 0;
   self->node_size = node_size;
   self->key_size = key_size;
   self->value_size = value_size;
   _bptr_bound_set(self);  // goto INVALID_FANOUT_ERR on error
   self->record_cnt = 0;
   self->node_cnt = 0;
   self->height = 0;
   self->compare = compare;

   if (bptr_cache_init(self, cache_capacity)) goto CACHE_INIT_ERR;

   /* Construct the file */
   if (bptr_io_fcreat(self, filename))
      goto FOPEN_ERR;

   return self;

/* Error Handle */
   // TODO: fclose Error handle
   bptr_io_fclose(self);
FOPEN_ERR:           _set_errno(BPTR_E_FACCESS);
   //TODO: deinit Error handle
   bptr_cache_deinit(self);
CACHE_INIT_ERR:      _set_errno(BPTR_E_OOM);
INVALID_FANOUT_ERR:  _set_errno(BPTR_E_FN_INPUT);
   free(self);
BPTR_MALLOC_ERR:     _set_errno(BPTR_E_OOM);
INVALID_SIZE_ERR:    _set_errno(BPTR_E_FN_INPUT);
   return NULL;
}


struct bptr *bptr_load(const char *filename, uint64_t cache_capacity,
                       int (*compare)(const void *lhs, const void *rhs))
{
   _Bool has_set_err = 0;
   struct bptr *self;
   int fn_ret;

   if (cache_capacity < BPTR_CACHE_CAPACITY_MIN)
      goto INVALID_INP_ERR;

   /* malloc for the handler */
   self = malloc(sizeof (struct bptr));
   if (self == NULL) goto BPTR_MALLOC_ERR;

   if (bptr_io_fload(self, filename)) goto FLOAD_ERR;
   _bptr_bound_set(self);
   self->compare = compare;

   if (bptr_cache_init(self, cache_capacity)) goto CACHE_INIT_ERR;

   return self;

   /*-------------------------- Error Handling Zone --------------------------*/
   //TODO: deinit Error handle
   bptr_cache_deinit(self);
CACHE_INIT_ERR:      _set_errno(BPTR_E_OOM);
INVALID_FANOUT_ERR:  _set_errno(BPTR_E_FN_INPUT);
   bptr_io_fclose(self);
FLOAD_ERR:           _set_errno(BPTR_E_FACCESS);
   free(self);
BPTR_MALLOC_ERR:     _set_errno(BPTR_E_OOM);
INVALID_INP_ERR:     _set_errno(BPTR_E_FN_INPUT);
   return NULL;
}

int bptr_unload(struct bptr *self)
{
   _Bool has_set_err = 0;
   int fn_err;

   fn_err = bptr_cache_deinit(self);
   if (fn_err) goto CACHE_DEINIT_ERR;

   fn_err = bptr_io_fclose(self);
   if (fn_err) goto IO_FCLOSE_ERR;
   free(self);

   return 0;

   /*-------------------------- Error Handling Zone --------------------------*/
   int err_code;

CACHE_DEINIT_ERR: _set_err_code(fn_err);
IO_FCLOSE_ERR:    _set_err_code(fn_err);
   if (bptr_errno == BPTR_E_FCLOSE) free(self);
   return err_code;
}


// Replaces the value if the key already exists in the tree
int bptr_insert(struct bptr *self, const void *key, const void *value)
{
   struct bptr_find_res find_res;
   _Bool has_set_err = 0;
   int err_code;
   _Bool has_new_node;


   if (self->root_idx == 0)   // Empty Tree
    {
      find_res.node = bptr_node_new(self, 0);
      if (find_res.node == NULL) return bptr_errno;
      find_res.node->prev = find_res.node->next = 0;
      find_res.idx = 0;
      self->root_idx = find_res.node->node_idx;
      self->node_cnt++;
      has_new_node = 1;
    }
   else
    {
      has_new_node = 0;
      find_res = bptr_find_node(self, key);
      if (find_res.node == NULL) goto FIND_NODE_ERR;
      if (find_res.is_found) goto KEY_EXIST_ERR;
    }

   err_code = bptr_node_insert(self, find_res.node, find_res.idx, key, value);
   if (err_code) goto NODE_INSERT_ERR;

   bptr_node_unload(self, find_res.node);
   return 0;

   /*-------------------------- Error Handling Zone --------------------------*/
NODE_INSERT_ERR: has_set_err = 1;
KEY_EXIST_ERR:   _set_err_code(BPTR_E_KEY_EXIST);
   if (has_new_node)
    { bptr_node_root_drop(self, find_res.node); self->root_idx = 0; }
   else              bptr_node_unload(self, find_res.node);
FIND_NODE_ERR:   _set_err_code(bptr_errno);
NODE_NEW_ERR:    _set_err_code(bptr_errno);
   return err_code;
}


struct bptr_find_res bptr_find_node(struct bptr *self, const void *key)
{
   struct bptr_node *node;

   // Empty tree
   if (self->root_idx == 0)
    {
      bptr_errno = BPTR_E_EMPTY_TREE;
      return (struct bptr_find_res){ 0, NULL };
    }

   for (node = bptr_node_fetch(self, self->root_idx); ;)
    {
      bptr_node_t node_idx;
      bptr_node_ki_t srch_res;
      _Bool is_found;   // must be boolean type for correct arithmetic

      // Error: bptr_node_fetch() sets bptr_errno on error
      if (node == NULL) return (struct bptr_find_res){ 0, NULL };

      is_found = bptr_node_key_search(self, node, key, &srch_res);

      if (node->is_leaf)   // base case: leaf node
         return (struct bptr_find_res){ is_found, node, srch_res };

      // setup for next iteration
      // on exact match, search path is routed to (key_idx + 1)th child;
      // if not matched, search path is routed to key_idx th child.
      node_idx = _node_brch_vals_get(self, node, srch_res + is_found);
      bptr_node_unload(self, node);
      node = bptr_node_fetch(self, node_idx);
    }

   // either returns due to empty tree or on leaf (in for loop)
   exit(BPTR_E_UNREACHABLE);
}
/*---------------------- Public Function Definition END ----------------------*/


/*---------------------------- Private Functions -----------------------------*/
/*-------------------------- Private Functions END ---------------------------*/
