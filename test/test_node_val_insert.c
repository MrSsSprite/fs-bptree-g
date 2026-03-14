#include "test_node_insert.h"
#include "../src/bptr_node.c"


#define bptr_init_cfg(cfg, key_type, val_type, cmp) \
   (bptr_init((cfg)->f_nm, (cfg)->is_lite, (cfg)->node_sz, \
              sizeof(key_type), sizeof(val_type), (cmp)))

#define test_cleanup(cfg, bptr, node) do \
{ \
   bptr_node_free(node); \
   remove((cfg)->f_nm); \
} while (0)

#define test_print_vals(bptr, node, type, pri_t) do \
{ \
   type *arr = (node)->vals; \
   putchar('['); \
   for (uint_fast32_t i = 0, \
                      ed = (node)->key_count + ((node)->is_leaf ? 0 : 1); \
        i < ed; i++) \
      printf("%" pri_t ", ", arr[i]); \
   puts("]"); \
} while (0)

#define test_insert_noincr(bptr, node, idx, type, val) do \
{ \
   type var = (val); \
   _node_val_insert((bptr), (node), &var, (idx)); \
   assert(memcmp(((type*)(node)->vals) + (idx), &var, sizeof(type)) == 0); \
} while (0)

#define test_insert(bptr, node, idx, type, val) do \
{ \
   test_insert_noincr(bptr, node, idx, type, val); \
   (node)->key_count++; \
} while (0)


bptr_config
default_cfg = { "default_config.bptr", 1, 1, 512 },
default_brch_cfg = { "default_config.bptr", 1, 0, 512 };


void test_insert_empty(bptr_config *cfg)
{
   puts("\n=== Test: Insert into empty node ===");
   struct bptr *bptr = bptr_init_cfg(cfg, int32_t, int32_t, cmp_i32);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, cfg->is_leaf, 0);
   assert(node);

   uint_fast32_t idx = 0;
   if (!node->is_leaf)
    {
      test_insert_noincr(bptr, node, idx, int32_t, 321);
      idx++;
    }
   test_insert(bptr, node, idx, int32_t, 123);
   test_print_vals(bptr, node, int32_t, PRIi32);
   puts("\tPASS: Works correctly on default config");

   test_cleanup(cfg, bptr, node);
}


void test_to_full(bptr_config *cfg)
{
   struct bptr *bptr = bptr_init_cfg(cfg, int32_t, int32_t, cmp_i32);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, cfg->is_leaf, 0);
   assert(node);

   uint_fast32_t i = 0, sz_mx;
   if (!node->is_leaf) test_insert_noincr(bptr, node, i, int32_t, i);
   i++;
   for (sz_mx = (node->is_leaf ? bptr->node_boundry.leaf.up :
                                 bptr->node_boundry.brch.up + 1);
        i < sz_mx; i++)
      test_insert(bptr, node, i, int32_t, i);

   test_print_vals(bptr, node, int32_t, PRIi32);
}


int main(void)
{
   test_insert_empty(&default_cfg);
   test_insert_empty(&default_brch_cfg);
   test_to_full(&default_cfg);

   return 0;
}
