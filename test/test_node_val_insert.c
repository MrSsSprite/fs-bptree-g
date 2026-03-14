#include "test_node_insert.h"
#include "../src/bptr_node.c"


/*---------------------------------- Macros ----------------------------------*/
#define bptr_init_cfg(cfg, key_type, val_type, cmp) \
   (bptr_init((cfg)->f_nm, (cfg)->is_lite, (cfg)->node_sz, \
              sizeof(key_type), sizeof(val_type), (cmp)))

#define test_cleanup(cfg, bptr, node) do \
{ \
   bptr_node_free(node); \
   remove((cfg)->f_nm); \
} while (0)

#define test_print_properties(bptr, node) do \
{ \
   printf("file: \"%s\"\n", cfg->f_nm); \
   printf("is_lite=%d, is_leaf=%d, node_sz=%" PRIuFAST32 "\n", bptr->is_lite, node->is_leaf, bptr->node_size); \
   fputs("boundry:\t", stdout); \
   if (node->is_leaf) \
      printf("up=%" PRIuFAST16 ", low=%" PRIuFAST16 "\n", \
             bptr->node_boundry.leaf.up, bptr->node_boundry.leaf.low); \
   else \
      printf("up=%" PRIuFAST16 ", low=%" PRIuFAST16 "\n", \
             bptr->node_boundry.brch.up, bptr->node_boundry.brch.low); \
} while (0)

#define test_print_vals(bptr, node, type, pri_t) do \
{ \
   void *arr = (node)->vals; \
   putchar('['); \
   for (uint_fast32_t i = 0, \
                      ed = (node)->key_count + ((node)->is_leaf ? 0 : 1); \
        i < ed; i++) \
    { \
      if ((node)->is_leaf) printf("%" pri_t ", ", ((type*)arr)[i]); \
      else if ((bptr)->is_lite) printf("%" BPTR_LITE_PRI_TYPE ", ", ((BPTR_LITE_PTR_TYPE*)arr)[i]); \
      else                      printf("%" BPTR_NORM_PRI_TYPE ", ", ((BPTR_NORM_PTR_TYPE*)arr)[i]); \
    } \
   puts("]"); \
} while (0)

#define test_insert_noincr(bptr, node, idx, type, val) do \
{ \
   type var = (val); \
   if ((node)->is_leaf) \
    { \
      _node_val_insert((bptr), (node), &var, (idx)); \
       assert(memcmp(((type*)(node)->vals) + (idx), &var, sizeof(var)) == 0); \
    } \
   else if ((bptr)->is_lite) \
    { \
      BPTR_LITE_PTR_TYPE varp = var; \
      _node_val_insert((bptr), (node), &varp, (idx)); \
      assert(memcmp(((BPTR_LITE_PTR_TYPE*)(node)->vals) + (idx), \
                    &varp, sizeof(BPTR_LITE_PTR_TYPE)) == 0); \
    } \
   else \
    { \
      BPTR_NORM_PTR_TYPE varp = var; \
      _node_val_insert((bptr), (node), &varp, (idx)); \
      assert(memcmp(((BPTR_NORM_PTR_TYPE*)(node)->vals) + (idx), \
                    &varp, sizeof(BPTR_NORM_PTR_TYPE)) == 0); \
    }  \
} while (0)

#define test_insert(bptr, node, idx, type, val) do \
{ \
   test_insert_noincr(bptr, node, idx, type, val); \
   (node)->key_count++; \
} while (0)
/*-------------------------------- Macros End --------------------------------*/


/*----------------------------- Config Profiles ------------------------------*/
bptr_config
default_cfg = { "default_config.bptr", 1, 1, 512 },
default_brch_cfg = { "default_brch_cfg.bptr", 1, 0, 512 },
norm_leaf_cfg = { "norm_leaf_cfg.bptr", 0, 1, 512 },
norm_brch_cfg = { "norm_brch_cfg.bptr", 0, 0, 512 };

bptr_config *configs[] =
 { &default_cfg, &default_brch_cfg, &norm_leaf_cfg, &norm_brch_cfg };
/*--------------------------- Config Profiles END ----------------------------*/


/*-------------------------------- Test Units --------------------------------*/
void test_insert_empty(bptr_config *cfg)
{
   puts("\n=== Test: Insert into empty node ===");
   struct bptr *bptr = bptr_init_cfg(cfg, int32_t, int32_t, cmp_i32);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, cfg->is_leaf, 0);
   assert(node);
   test_print_properties(bptr, node);

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
   puts("\n=== Test: Insert until full ===");
   struct bptr *bptr = bptr_init_cfg(cfg, int32_t, int32_t, cmp_i32);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, cfg->is_leaf, 0);
   assert(node);
   test_print_properties(bptr, node);

   uint_fast32_t i = 0, sz_mx;
   // because val count == key count + 1 on branch node, insert one value into 
   // it without incremnt to assure it works correctly
   if (!node->is_leaf) { test_insert_noincr(bptr, node, 0, int32_t, 999); i++; }
   for (sz_mx = (node->is_leaf ? bptr->node_boundry.leaf.up :
                                 bptr->node_boundry.brch.up + 1) - 1;
        i < sz_mx; i++)
      test_insert(bptr, node, i, int32_t, 2 * i);

   test_print_vals(bptr, node, int32_t, PRIi32);
   test_cleanup(cfg, bptr, node);
}
/*------------------------------ Test Units END ------------------------------*/


int main(void)
{
   for (bptr_config **cfg_it = configs,
                    **ed = cfg_it + sizeof(configs)/sizeof(*configs);
        cfg_it < ed; cfg_it++)
    {
      test_insert_empty(*cfg_it);
      test_to_full(*cfg_it);
    }

   return 0;
}
