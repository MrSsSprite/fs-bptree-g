#include "test_node_insert.h"
#include "../src/bptr_node.c"
#include <stddef.h>


/*---------------------------------- Macros ----------------------------------*/
#define bptr_init_cfg(cfg, key_type, val_type, cmp) \
   (bptr_init((cfg)->f_nm, (cfg)->is_lite, (cfg)->node_sz, \
              sizeof(key_type), sizeof(val_type), (cmp)))

#define test_cleanup(cfg, bptr, node) do \
{ \
   bptr_node_free(node); \
   bptr_destroy(bptr); \
   remove((cfg)->f_nm); \
} while (0)

#define test_print_properties(cfg, bptr, node) do \
{ \
   printf("file: \"%s\"\n", (cfg)->f_nm); \
   printf("is_lite=%d, is_leaf=%d, node_sz=%" PRIuFAST32 "\n", \
          bptr->is_lite, node->is_leaf, bptr->node_size); \
   fputs("boundry:\t", stdout); \
   if (node->is_leaf) \
      printf("up=%" PRIuFAST16 ", low=%" PRIuFAST16 "\n", \
             bptr->node_boundry.leaf.up, bptr->node_boundry.leaf.low); \
   else \
      printf("up=%" PRIuFAST16 ", low=%" PRIuFAST16 "\n", \
             bptr->node_boundry.brch.up, bptr->node_boundry.brch.low); \
} while (0)

#define _val_insert(bptr, node, idx, vtype, vval) do \
{ \
   vtype vvar; \
   BPTR_LITE_PTR_TYPE vvar_bl; \
   BPTR_NORM_PTR_TYPE vvar_bn; \
   vvar = vvar_bn = vvar_bl = (vval); \
   if ((node)->is_leaf) \
      _node_val_insert((bptr), (node), &vvar, (idx)); \
   else if ((bptr)->is_lite) \
      _node_val_insert((bptr), (node), &vvar_bl, (idx)); \
   else \
      _node_val_insert((bptr), (node), &vvar_bn, (idx)); \
} while (0)

#define _kv_insert(bptr, node, idx, ktype, vtype, kval, vval) do \
{ \
   ktype kvar; \
   kvar = (kval); \
   _node_key_insert((bptr), ((node)), &kvar, (idx)); \
   _val_insert(bptr, node, idx, vtype, vval); \
} while (0)

#define _print_keys(node, kpri, ktype) do \
{ \
   fputs("keys: [", stdout); \
   for (uint_fast32_t i = 0; i < node->key_count; i++) \
      printf("%" kpri ", ", ((ktype*)node->keys)[i]); \
   puts("]"); \
} while (0)

#define _print_vals(bptr, node, vpri, vtype) do \
{ \
   fputs("vals: [", stdout); \
   for (uint_fast32_t i = 0; i < _node_val_cnt(node); i++) \
    { \
      if ((node)->is_leaf) \
         printf("%" vpri ", ", ((vtype*)node->vals)[i]); \
      else if (bptr->is_lite) \
         printf("%" BPTR_LITE_PRI_TYPE ", ", \
                ((BPTR_LITE_PTR_TYPE*)node->vals)[i]); \
      else \
         printf("%" BPTR_NORM_PRI_TYPE ", ", \
                ((BPTR_NORM_PTR_TYPE*)node->vals)[i]); \
    } \
   puts("]"); \
} while (0)

#define _print_kvs(bptr, node, kpri, ktype, vpri, vtype) do \
{ \
   _print_keys(node, kpri, ktype); _print_vals(bptr, node, vpri, vtype); \
} while (0)
/*-------------------------------- Macros END --------------------------------*/


/*----------------------------- Config Profiles ------------------------------*/
bptr_config
default_cfg = { "kv_default.bptr", 1, 1, 512 },
default_brch_cfg = { "kv_default_brch.bptr", 1, 0, 512 },
norm_leaf_cfg = { "kv_norm_leaf.bptr", 0, 1, 512 },
norm_brch_cfg = { "kv_norm_brch.bptr", 0, 0, 512 },
size_1k_cfg = { "kv_1k.bptr", 1, 1, 1024 },
size_1k_brch_cfg = { "kv_1k_brch.bptr", 1, 0, 1024 },
size_4k_cfg = { "kv_4k.bptr", 1, 1, 4096 },
size_4k_brch_cfg = { "kv_4k_brch.bptr", 1, 0, 4096 },
size_8k_cfg = { "kv_8k.bptr", 1, 1, 8192 },
size_16k_cfg = { "kv_16k.bptr", 1, 1, 16384 };

bptr_config *configs[] =
 { &default_cfg, &default_brch_cfg, &norm_leaf_cfg, &norm_brch_cfg,
   &size_1k_cfg, &size_1k_brch_cfg, &size_4k_cfg, &size_4k_brch_cfg,
   &size_8k_cfg, &size_16k_cfg };

bptr_config *all_leaf_configs[] =
 { &default_cfg, &norm_leaf_cfg, &size_1k_cfg, &size_4k_cfg,
   &size_8k_cfg, &size_16k_cfg };

bptr_config *all_brch_configs[] =
 { &default_brch_cfg, &norm_brch_cfg, &size_1k_brch_cfg, &size_4k_brch_cfg };
/*--------------------------- Config Profiles END ----------------------------*/


/*--------------------------- Forward Declaration ----------------------------*/
static inline
_Bool check_exp(struct bptr *bptr, struct bptr_node *node,
                const void *exp_keys, const void *exp_vals, uint32_t exp_kcnt);
/*------------------------- Forward Declaration END --------------------------*/


/*-------------------------------- Test Units --------------------------------*/
void test_default(bptr_config *cfg)
{
#define ktype_lcl int32_t
#define kpri_lcl PRIi32
#define vtype_lcl int32_t
#define vpri_lcl PRIi32
#define _kv_insert_lcl(bptr, node, idx, kval, vval) \
   _kv_insert(bptr, node, idx, ktype_lcl, vtype_lcl, kval, vval)
#define _print_kvs_lcl(bptr, node) \
   _print_kvs(bptr, node, kpri_lcl, ktype_lcl, vpri_lcl, vtype_lcl)
#define _val_insert_lcl(bptr, node, idx, vval) \
   _val_insert(bptr, node, idx, vtype_lcl, vval)


   puts("\n=== test_default ===");
   struct bptr *bptr = bptr_init_cfg(cfg, ktype_lcl, vtype_lcl, cmp_i32);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, cfg->is_leaf, 0);
   assert(node);
   test_print_properties(cfg, bptr, node);

   _kv_insert_lcl(bptr, node, 0, 1, 1); node->key_count++;
   _kv_insert_lcl(bptr, node, 1, 3, 3); node->key_count++;
   _kv_insert_lcl(bptr, node, 2, 5, 5); node->key_count++;
   _kv_insert_lcl(bptr, node, 2, 4, 4); node->key_count++;
   _kv_insert_lcl(bptr, node, 1, 2, 2); node->key_count++;
   if (!node->is_leaf)
      _val_insert_lcl(bptr, node, 0, 0);

   _print_kvs_lcl(bptr, node);

   int32_t key_exp[] = { 1, 2, 3, 4, 5 }, *val_exp = key_exp;
   BPTR_LITE_PTR_TYPE val_exp_bl[] = { 0, 1, 2, 3, 4, 5 };
   BPTR_NORM_PTR_TYPE val_exp_bn[] = { 0, 1, 2, 3, 4, 5 };
   if (node->is_leaf)
      check_exp(bptr, node, key_exp, val_exp, 5);
   else if (bptr->is_lite)
      check_exp(bptr, node, key_exp, val_exp_bl, 5);
   else
      check_exp(bptr, node, key_exp, val_exp_bn, 5);
   puts("check_exp succeeded!");

   test_cleanup(cfg, bptr, node);

#undef ktype_lcl
#undef kpri_lcl
#undef vtype_lcl
#undef vpri_lcl
#undef _kv_insert_lcl
#undef _print_kvs_lcl
#undef _val_insert_lcl
}
/*------------------------------ Test Units END ------------------------------*/


/*----------------------------------- Main -----------------------------------*/
int main(void)
{
   test_default(&default_cfg);
   test_default(&default_brch_cfg);

   return 0;
}
/*--------------------------------- Main END ---------------------------------*/


/*---------------------------- Utility Functions -----------------------------*/
static inline
_Bool check_exp(struct bptr *bptr, struct bptr_node *node,
                const void *exp_keys, const void *exp_vals, uint32_t exp_kcnt)
{
   if (node->key_count != exp_kcnt) return 0;
   for (uint_fast32_t idx = 0; idx < exp_kcnt; idx++)
    {
      ptrdiff_t offset = idx * bptr->key_size;
      if (memcmp(exp_keys + offset, node->keys + offset, bptr->key_size))
         return 0;
    }
   for (uint_fast32_t idx = 0; idx < _node_val_cnt(node); idx++)
    {
      uint_fast32_t val_sz = _node_val_size(bptr, node);
      ptrdiff_t offset = idx * val_sz;
      if (memcmp(exp_vals + offset, node->vals + offset, val_sz))
         return 0;
    }
   return 1;
}
/*-------------------------- Utility Functions END ---------------------------*/
