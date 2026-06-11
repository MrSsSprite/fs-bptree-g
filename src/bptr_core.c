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
struct node_idx_pair
{
   _Bool is_found;
   struct bptr_node *node;
   bptr_node_ki_t idx;
};
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


/*----------------------- Private Function Declaration -----------------------*/
static
struct node_idx_pair bptr_find_node(struct bptr *self, const void *key);
/*--------------------- Private Function Declaration END ---------------------*/


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
   struct bptr *self;

   /* Node must be large enough to at least contain
    * the metadata, 1 key and 2 childs */
   if (node_size < BPTR_NODE_METADATA_BYTE + key_size +
                   (is_lite ? BPTR_LITE_PTR_BYTE : BPTR_NORM_PTR_BYTE) * 2 ||
       cache_capacity < BPTR_CACHE_CAPACITY_MIN)
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
   _Bool has_set_err = 0;

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
   struct bptr *self;
   int fn_ret;

   /* malloc for the handler */
   self = malloc(sizeof (struct bptr));
   if (self == NULL) return NULL;

   fn_ret = bptr_io_fload(self, filename);
   if (fn_ret)
    {
      bptr_errno = 1;
      goto FLOAD_ERR;
    }
   _bptr_bound_set(self);
   self->compare = compare;

   if (bptr_cache_init(self, cache_capacity)) goto CACHE_INIT_ERR;

   return self;

   /*-------------------------- Error Handling Zone --------------------------*/
   _Bool has_set_err = 0;

   //TODO: deinit Error handle
   bptr_cache_deinit(self);
CACHE_INIT_ERR:      _set_errno(BPTR_E_OOM);
INVALID_FANOUT_ERR:  _set_errno(BPTR_E_FN_INPUT);
   bptr_io_fclose(self);
FLOAD_ERR:           _set_errno(BPTR_E_FACCESS);
   free(self);
BPTR_MALLOC_ERR:     _set_errno(BPTR_E_OOM);
   return NULL;
}

int bptr_unload(struct bptr *self)
{
   int fn_err;

   fn_err = bptr_cache_deinit(self);
   if (fn_err) goto CACHE_DEINIT_ERR;

   fn_err = bptr_io_fclose(self);
   if (fn_err) goto IO_FCLOSE_ERR;
   free(self);

   return 0;

   /*-------------------------- Error Handling Zone --------------------------*/
   _Bool has_set_err = 0;
   int err_code;

CACHE_DEINIT_ERR: _set_err_code(fn_err);
IO_FCLOSE_ERR:    _set_err_code(fn_err);
   return err_code;
}


// Replaces the value if the key already exists in the tree
int bptr_insert(struct bptr *self, const void *key, const void *value)
{
   struct node_idx_pair find_res;

   //TODO: temporary implementation; cache pool should be involved later

   find_res = bptr_find_node(self, key);
   if (bptr_errno)
      return bptr_errno < 0 ? -1 : 1;
   // Empty Tree
   if (find_res.node == NULL)
    {
      find_res.node = bptr_node_new(self, 0);
      if (find_res.node == NULL) return 1;
      find_res.node->prev = find_res.node->next = 0;
      find_res.idx = 0;
    }

   //TODO
   exit(BPTR_E_TODO);
}
/*---------------------- Public Function Definition END ----------------------*/


/*---------------------------- Private Functions -----------------------------*/
// bptr_errno is set to non-0 on error;
// the return value is valid iff bptr_errno is 0
// if the tree is not empty && the key is not found,
//    idx refers to the smallest key that is greater than the key for which the 
//    table is searched. idx ref to one-pass the last valid key
static
struct node_idx_pair bptr_find_node(struct bptr *self, const void *key)
{
   struct bptr_node *node;
   bptr_node_ki_t lo, md, up;
   int cmp_res, err_code;

   bptr_errno = 0;
   // Empty tree
   if (self->root_idx == 0)
    { bptr_errno = 0; return (struct node_idx_pair){ 0, NULL }; }

   for (node = bptr_node_fetch(self, self->root_idx); ;)
    {
      bptr_node_t node_idx;
      // Error: bptr_node_fetch() sets bptr_errno on error
      if (node == NULL) return (struct node_idx_pair){ 0, NULL };

      for (lo = 0, up = node->key_count, md = up / 2;
           lo < up; md = lo + (up - lo) / 2)
       {
         cmp_res = self->compare(key, (char*)node->keys + md * self->key_size);
         if (cmp_res < 0)        up = md;
         else if (cmp_res > 0)   lo = md + 1;
         else                    break;
       }

      // return when reaches a leaf node
      if (node->is_leaf)
       {
         if (cmp_res == 0) return (struct node_idx_pair){ 1, node, md };
         else { bptr_errno = 0; return (struct node_idx_pair){ 0, node, up }; }
       }

      // setup for next iteration
      node_idx = _node_brch_vals_get(self, node, cmp_res == 0 ? md + 1 : up);
      bptr_node_unload(self, node);
      node = bptr_node_fetch(self, node_idx);
    }

   // either returns due to empty tree or on leaf (in for loop)
   exit(BPTR_E_UNREACHABLE);
}
/*-------------------------- Private Functions END ---------------------------*/
