#include "test_node_insert.h"
#include "../src/bptr_node.c"


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

/* Combined key-value insertion macro - inserts value BEFORE key for proper pointer handling */
#define _kv_insert(self, node, idx, ktype, vtype, kval, vval) do \
{ \
   if ((node)->is_leaf) \
    { vtype vvar = (vval); _node_val_insert((self), (node), &vvar, (idx)); } \
   else if ((self)->is_lite) \
    { BPTR_LITE_PTR_TYPE vvar = (vval); _node_val_insert((self), (node), &vvar, (idx) + 1); } \
   else \
    { BPTR_NORM_PTR_TYPE vvar = (vval); _node_val_insert((self), (node), &vvar, (idx) + 1); } \
   ktype kvar = (kval); \
   _node_key_insert((self), (node), &kvar, (idx)); \
   (node)->key_count++; \
} while (0)

/* Print both keys and values for debugging */
#define test_print_kv(bptr, node, ktype, vtype, kpri, vpri) do \
{ \
   printf("Keys:  ["); \
   for (uint_fast32_t i = 0; i < (node)->key_count; i++) \
      printf("%" kpri ", ", ((ktype*)(node)->keys)[i]); \
   puts("]"); \
   printf("Vals:  ["); \
   for (uint_fast32_t i = 0, ed = (node)->key_count + ((node)->is_leaf ? 0 : 1); \
        i < ed; i++) \
   { \
      if ((node)->is_leaf) printf("%" vpri ", ", ((vtype*)(node)->vals)[i]); \
      else if ((bptr)->is_lite) printf("%" BPTR_LITE_PRI_TYPE ", ", \
                                       ((BPTR_LITE_PTR_TYPE*)(node)->vals)[i]); \
      else printf("%" BPTR_NORM_PRI_TYPE ", ", \
                 ((BPTR_NORM_PTR_TYPE*)(node)->vals)[i]); \
   } \
   puts("]"); \
} while (0)
/*-------------------------------- Macros End --------------------------------*/


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


/*---------------------------- Verification Helpers ---------------------------*/
/* Verify key-value pairs are correctly paired for leaf nodes */
bool verify_kv_pairs_leaf(struct bptr *bptr, struct bptr_node *node,
                          const void *expected_keys,
                          const void *expected_vals,
                          uint32_t count, size_t key_size, size_t val_size)
{
   if (node->key_count != count)
    {
      printf("  FAIL: key_count mismatch (expected %" PRIu32 ", got %" PRIu32 ")\n",
             count, node->key_count);
      return false;
    }
   const char *keys = node->keys;
   const char *vals = node->vals;
   const char *exp_keys = expected_keys;
   const char *exp_vals = expected_vals;
   for (uint32_t i = 0; i < count; i++)
    {
      if (memcmp(keys + i * key_size, exp_keys + i * key_size, key_size) != 0)
       {
          printf("  FAIL: key[%" PRIu32 "] mismatch\n", i);
          return false;
       }
      if (memcmp(vals + i * val_size, exp_vals + i * val_size, val_size) != 0)
       {
          printf("  FAIL: val[%" PRIu32 "] mismatch\n", i);
          return false;
       }
    }
   return true;
}

/* Verify key-value pairs for branch nodes (vals have key_count + 1 elements) */
bool verify_kv_pairs_brch(struct bptr *bptr, struct bptr_node *node,
                          const void *expected_keys,
                          const void *expected_vals,
                          uint32_t key_count, size_t key_size, size_t val_size)
{
   if (node->key_count != key_count)
    {
      printf("  FAIL: key_count mismatch (expected %" PRIu32 ", got %" PRIu32 ")\n",
             key_count, node->key_count);
      return false;
    }
   uint32_t val_count = key_count + 1;
   size_t ptr_size = bptr->is_lite ? BPTR_LITE_PTR_BYTE : BPTR_NORM_PTR_BYTE;

   const char *keys = node->keys;
   const char *vals = node->vals;
   const char *exp_keys = expected_keys;
   const char *exp_vals = expected_vals;

   /* Verify keys */
   for (uint32_t i = 0; i < key_count; i++)
    {
      if (memcmp(keys + i * key_size, exp_keys + i * key_size, key_size) != 0)
       {
          printf("  FAIL: key[%" PRIu32 "] mismatch\n", i);
          return false;
       }
    }
   /* Verify pointers (vals) */
   for (uint32_t i = 0; i < val_count; i++)
    {
      if (memcmp(vals + i * ptr_size, exp_vals + i * ptr_size, ptr_size) != 0)
       {
          printf("  FAIL: ptr[%" PRIu32 "] mismatch\n", i);
          return false;
       }
    }
   return true;
}
/*-------------------------- Verification Helpers END ------------------------*/


/*================================ Test Units =================================*/
/*-------------------------------- Phase 1: Basic Leaf Node Tests -------------*/

void test_kv_leaf_empty(bptr_config *cfg)
{
   puts("\n=== Test: Insert KV into empty leaf node ===");
   struct bptr *bptr = bptr_init_cfg(cfg, int32_t, int32_t, cmp_i32);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 1, 0);
   assert(node);
   test_print_properties(cfg, bptr, node);

   _kv_insert(bptr, node, 0, int32_t, int32_t, 100, 1000);

   int32_t exp_keys[] = {100};
   int32_t exp_vals[] = {1000};
   assert(verify_kv_pairs_leaf(bptr, node, exp_keys, exp_vals, 1,
                               sizeof(int32_t), sizeof(int32_t)));
   puts("  PASS: First KV pair inserted correctly");

   test_cleanup(cfg, bptr, node);
}


void test_kv_leaf_beginning(bptr_config *cfg)
{
   puts("\n=== Test: Leaf - Insert KV at beginning (shifting) ===");
   struct bptr *bptr = bptr_init_cfg(cfg, int32_t, int32_t, cmp_i32);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 1, 0);
   assert(node);
   test_print_properties(cfg, bptr, node);

   _kv_insert(bptr, node, 0, int32_t, int32_t, 100, 1000);
   _kv_insert(bptr, node, 1, int32_t, int32_t, 200, 2000);
   _kv_insert(bptr, node, 0, int32_t, int32_t, 50, 500);

   int32_t exp_keys[] = {50, 100, 200};
   int32_t exp_vals[] = {500, 1000, 2000};
   assert(verify_kv_pairs_leaf(bptr, node, exp_keys, exp_vals, 3,
                               sizeof(int32_t), sizeof(int32_t)));
   puts("  PASS: Both keys and values shifted correctly at beginning");

   test_cleanup(cfg, bptr, node);
}


void test_kv_leaf_middle(bptr_config *cfg)
{
   puts("\n=== Test: Leaf - Insert KV at middle ===");
   struct bptr *bptr = bptr_init_cfg(cfg, int32_t, int32_t, cmp_i32);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 1, 0);
   assert(node);
   test_print_properties(cfg, bptr, node);

   _kv_insert(bptr, node, 0, int32_t, int32_t, 100, 1000);
   _kv_insert(bptr, node, 1, int32_t, int32_t, 200, 2000);
   _kv_insert(bptr, node, 2, int32_t, int32_t, 400, 4000);
   _kv_insert(bptr, node, 1, int32_t, int32_t, 150, 1500);

   int32_t exp_keys[] = {100, 150, 200, 400};
   int32_t exp_vals[] = {1000, 1500, 2000, 4000};
   assert(verify_kv_pairs_leaf(bptr, node, exp_keys, exp_vals, 4,
                               sizeof(int32_t), sizeof(int32_t)));
   puts("  PASS: Both keys and values shifted correctly at middle");

   test_cleanup(cfg, bptr, node);
}


void test_kv_leaf_end(bptr_config *cfg)
{
   puts("\n=== Test: Leaf - Insert KV at end ===");
   struct bptr *bptr = bptr_init_cfg(cfg, int32_t, int32_t, cmp_i32);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 1, 0);
   assert(node);
   test_print_properties(cfg, bptr, node);

   _kv_insert(bptr, node, 0, int32_t, int32_t, 100, 1000);
   _kv_insert(bptr, node, 1, int32_t, int32_t, 200, 2000);
   _kv_insert(bptr, node, 2, int32_t, int32_t, 300, 3000);

   int32_t exp_keys[] = {100, 200, 300};
   int32_t exp_vals[] = {1000, 2000, 3000};
   assert(verify_kv_pairs_leaf(bptr, node, exp_keys, exp_vals, 3,
                               sizeof(int32_t), sizeof(int32_t)));
   puts("  PASS: KV pair inserted correctly at end");

   test_cleanup(cfg, bptr, node);
}


void test_kv_leaf_correspondence(bptr_config *cfg)
{
   puts("\n=== Test: Leaf - Verify key-value correspondence maintained ===");
   struct bptr *bptr = bptr_init_cfg(cfg, int32_t, int32_t, cmp_i32);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 1, 0);
   assert(node);

   /* Insert multiple KV pairs and verify they stay paired */
   for (int i = 0; i < 5; i++)
      _kv_insert(bptr, node, i, int32_t, int32_t, i * 10, i * 100);

   /* Insert in middle and verify pairing still correct */
   _kv_insert(bptr, node, 2, int32_t, int32_t, 15, 150);

   int32_t exp_keys[] = {0, 10, 15, 20, 30, 40};
   int32_t exp_vals[] = {0, 100, 150, 200, 300, 400};
   assert(verify_kv_pairs_leaf(bptr, node, exp_keys, exp_vals, 6,
                               sizeof(int32_t), sizeof(int32_t)));
   puts("  PASS: Key-value correspondence maintained after all inserts");

   test_cleanup(cfg, bptr, node);
}
/*------------------------------- Phase 1 END -------------------------------*/


/*------------------------------ Phase 2: Basic Branch Node Tests ----------*/

void test_kv_brch_initial_ptr(bptr_config *cfg)
{
   if (cfg->is_leaf) return;
   puts("\n=== Test: Branch - Insert initial pointer before any keys ===");
   struct bptr *bptr = bptr_init_cfg(cfg, int32_t, int32_t, cmp_i32);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 0, 0);
   assert(node);
   test_print_properties(cfg, bptr, node);

   /* Branch node starts empty - first insert initial pointer without key */
   BPTR_LITE_PTR_TYPE init_ptr_lite = 999;
   BPTR_NORM_PTR_TYPE init_ptr_norm = 999;

   if (bptr->is_lite)
      _node_val_insert(bptr, node, &init_ptr_lite, 0);
   else
      _node_val_insert(bptr, node, &init_ptr_norm, 0);
   /* Now insert KV pairs - use correct type based on mode */
   if (bptr->is_lite)
    {
      _kv_insert(bptr, node, 0, int32_t, BPTR_LITE_PTR_TYPE, 100, 1100);
      _kv_insert(bptr, node, 1, int32_t, BPTR_LITE_PTR_TYPE, 200, 2200);
    }
   else
    {
      _kv_insert(bptr, node, 0, int32_t, BPTR_NORM_PTR_TYPE, 100, 1100);
      _kv_insert(bptr, node, 1, int32_t, BPTR_NORM_PTR_TYPE, 200, 2200);
    }

   /* Verify: ptrs = [999, 1100, 2200], keys = [100, 200] */
   int32_t exp_keys[] = {100, 200};
   if (bptr->is_lite)
    {
      BPTR_LITE_PTR_TYPE exp_ptrs[] = {999, 1100, 2200};
      assert(verify_kv_pairs_brch(bptr, node, exp_keys, exp_ptrs, 2,
                                  sizeof(int32_t), sizeof(BPTR_LITE_PTR_TYPE)));
    }
   else
    {
      BPTR_NORM_PTR_TYPE exp_ptrs[] = {999, 1100, 2200};
      assert(verify_kv_pairs_brch(bptr, node, exp_keys, exp_ptrs, 2,
                                  sizeof(int32_t), sizeof(BPTR_NORM_PTR_TYPE)));
    }
   puts("  PASS: Initial pointer + KV pairs work correctly");

   test_cleanup(cfg, bptr, node);
}


void test_kv_brch_beginning(bptr_config *cfg)
{
   if (cfg->is_leaf) return;
   puts("\n=== Test: Branch - Insert KV at beginning ===");
   struct bptr *bptr = bptr_init_cfg(cfg, int32_t, int32_t, cmp_i32);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 0, 0);
   assert(node);
   test_print_properties(cfg, bptr, node);

   /* Setup initial state */
   if (bptr->is_lite)
    { BPTR_LITE_PTR_TYPE init = 500; _node_val_insert(bptr, node, &init, 0); }
   else
    { BPTR_NORM_PTR_TYPE init = 500; _node_val_insert(bptr, node, &init, 0); }

   if (bptr->is_lite)
    {
      _kv_insert(bptr, node, 0, int32_t, BPTR_LITE_PTR_TYPE, 100, 600);
      _kv_insert(bptr, node, 1, int32_t, BPTR_LITE_PTR_TYPE, 200, 700);
      /* Insert at beginning - between ptr0 and ptr1 */
      _kv_insert(bptr, node, 0, int32_t, BPTR_LITE_PTR_TYPE, 50, 550);
    }
   else
    {
      _kv_insert(bptr, node, 0, int32_t, BPTR_NORM_PTR_TYPE, 100, 600);
      _kv_insert(bptr, node, 1, int32_t, BPTR_NORM_PTR_TYPE, 200, 700);
      /* Insert at beginning - between ptr0 and ptr1 */
      _kv_insert(bptr, node, 0, int32_t, BPTR_NORM_PTR_TYPE, 50, 550);
    }

   /* Verify: ptrs = [500, 550, 600, 700], keys = [50, 100, 200] */
   int32_t exp_keys[] = {50, 100, 200};
   if (bptr->is_lite)
    {
      BPTR_LITE_PTR_TYPE exp_ptrs[] = {500, 550, 600, 700};
      assert(verify_kv_pairs_brch(bptr, node, exp_keys, exp_ptrs, 3,
                                  sizeof(int32_t), sizeof(BPTR_LITE_PTR_TYPE)));
    }
   else
    {
      BPTR_NORM_PTR_TYPE exp_ptrs[] = {500, 550, 600, 700};
      assert(verify_kv_pairs_brch(bptr, node, exp_keys, exp_ptrs, 3,
                                  sizeof(int32_t), sizeof(BPTR_NORM_PTR_TYPE)));
    }
   puts("  PASS: Branch KV insertion at beginning works correctly");

   test_cleanup(cfg, bptr, node);
}


void test_kv_brch_middle(bptr_config *cfg)
{
   if (cfg->is_leaf) return;
   puts("\n=== Test: Branch - Insert KV at middle ===");
   struct bptr *bptr = bptr_init_cfg(cfg, int32_t, int32_t, cmp_i32);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 0, 0);
   assert(node);
   test_print_properties(cfg, bptr, node);

   BPTR_LITE_PTR_TYPE init = 100;
   if (bptr->is_lite) _node_val_insert(bptr, node, &init, 0);
   else { BPTR_NORM_PTR_TYPE tmp = 100; _node_val_insert(bptr, node, &tmp, 0); }

   if (bptr->is_lite)
    {
      _kv_insert(bptr, node, 0, int32_t, BPTR_LITE_PTR_TYPE, 200, 300);
      _kv_insert(bptr, node, 1, int32_t, BPTR_LITE_PTR_TYPE, 400, 500);
      /* Insert in middle */
      _kv_insert(bptr, node, 1, int32_t, BPTR_LITE_PTR_TYPE, 250, 350);
    }
   else
    {
      _kv_insert(bptr, node, 0, int32_t, BPTR_NORM_PTR_TYPE, 200, 300);
      _kv_insert(bptr, node, 1, int32_t, BPTR_NORM_PTR_TYPE, 400, 500);
      /* Insert in middle */
      _kv_insert(bptr, node, 1, int32_t, BPTR_NORM_PTR_TYPE, 250, 350);
    }

   /* Verify: ptrs = [100, 300, 350, 500], keys = [200, 250, 400] */
   int32_t exp_keys[] = {200, 250, 400};
   if (bptr->is_lite)
    {
      BPTR_LITE_PTR_TYPE exp_ptrs[] = {100, 300, 350, 500};
      assert(verify_kv_pairs_brch(bptr, node, exp_keys, exp_ptrs, 3,
                                  sizeof(int32_t), sizeof(BPTR_LITE_PTR_TYPE)));
    }
   else
    {
      BPTR_NORM_PTR_TYPE exp_ptrs[] = {100, 300, 350, 500};
      assert(verify_kv_pairs_brch(bptr, node, exp_keys, exp_ptrs, 3,
                                  sizeof(int32_t), sizeof(BPTR_NORM_PTR_TYPE)));
    }
   puts("  PASS: Branch KV insertion at middle works correctly");

   test_cleanup(cfg, bptr, node);
}


void test_kv_brch_end(bptr_config *cfg)
{
   if (cfg->is_leaf) return;
   puts("\n=== Test: Branch - Insert KV at end ===");
   struct bptr *bptr = bptr_init_cfg(cfg, int32_t, int32_t, cmp_i32);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 0, 0);
   assert(node);
   test_print_properties(cfg, bptr, node);

   BPTR_LITE_PTR_TYPE init = 100;
   if (bptr->is_lite) _node_val_insert(bptr, node, &init, 0);
   else { BPTR_NORM_PTR_TYPE tmp = 100; _node_val_insert(bptr, node, &tmp, 0); }

   if (bptr->is_lite)
    {
      _kv_insert(bptr, node, 0, int32_t, BPTR_LITE_PTR_TYPE, 200, 300);
      _kv_insert(bptr, node, 1, int32_t, BPTR_LITE_PTR_TYPE, 400, 500);
      /* Insert at end */
      _kv_insert(bptr, node, 2, int32_t, BPTR_LITE_PTR_TYPE, 600, 700);
    }
   else
    {
      _kv_insert(bptr, node, 0, int32_t, BPTR_NORM_PTR_TYPE, 200, 300);
      _kv_insert(bptr, node, 1, int32_t, BPTR_NORM_PTR_TYPE, 400, 500);
      /* Insert at end */
      _kv_insert(bptr, node, 2, int32_t, BPTR_NORM_PTR_TYPE, 600, 700);
    }

   /* Verify: ptrs = [100, 300, 500, 700], keys = [200, 400, 600] */
   int32_t exp_keys[] = {200, 400, 600};
   if (bptr->is_lite)
    {
      BPTR_LITE_PTR_TYPE exp_ptrs[] = {100, 300, 500, 700};
      assert(verify_kv_pairs_brch(bptr, node, exp_keys, exp_ptrs, 3,
                                  sizeof(int32_t), sizeof(BPTR_LITE_PTR_TYPE)));
    }
   else
    {
      BPTR_NORM_PTR_TYPE exp_ptrs[] = {100, 300, 500, 700};
      assert(verify_kv_pairs_brch(bptr, node, exp_keys, exp_ptrs, 3,
                                  sizeof(int32_t), sizeof(BPTR_NORM_PTR_TYPE)));
    }
   puts("  PASS: Branch KV insertion at end works correctly");

   test_cleanup(cfg, bptr, node);
}


void test_kv_brch_ptr_count(bptr_config *cfg)
{
   if (cfg->is_leaf) return;
   puts("\n=== Test: Branch - Verify ptr_count = key_count + 1 ===");
   struct bptr *bptr = bptr_init_cfg(cfg, int32_t, int32_t, cmp_i32);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 0, 0);
   assert(node);

   BPTR_LITE_PTR_TYPE init = 0;
   if (bptr->is_lite) _node_val_insert(bptr, node, &init, 0);
   else { BPTR_NORM_PTR_TYPE tmp = 0; _node_val_insert(bptr, node, &tmp, 0); }

   for (uint32_t i = 0; i < 5; i++)
    {
      if (bptr->is_lite)
         _kv_insert(bptr, node, i, int32_t, BPTR_LITE_PTR_TYPE,
                    (int32_t)(i * 100), (BPTR_LITE_PTR_TYPE)(i * 1000));
      else
         _kv_insert(bptr, node, i, int32_t, BPTR_NORM_PTR_TYPE,
                    (int32_t)(i * 100), (BPTR_NORM_PTR_TYPE)(i * 1000));
      /* Verify ptr_count = key_count + 1 after each insert */
      uint32_t expected_ptr_count = node->key_count + 1;
      uint32_t actual_val_count = node->key_count + 1; /* Branch has key_count + 1 vals */
      assert(expected_ptr_count == actual_val_count);
    }
   puts("  PASS: Pointer count always equals key_count + 1");

   test_cleanup(cfg, bptr, node);
}
/*------------------------------- Phase 2 END -------------------------------*/


/*---------------------------- Phase 3: Position-Based Verification ---------*/

void test_kv_multiple_ordered_verification(bptr_config *cfg)
{
   puts("\n=== Test: Multiple KV pairs - verify order maintained ===");
   struct bptr *bptr = bptr_init_cfg(cfg, int32_t, int32_t, cmp_i32);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, cfg->is_leaf, 0);
   assert(node);
   test_print_properties(cfg, bptr, node);

   uint_fast32_t idx = 0;
   if (!node->is_leaf)
    {
      if (bptr->is_lite)
       { BPTR_LITE_PTR_TYPE init = 999; _node_val_insert(bptr, node, &init, 0); }
      else
       { BPTR_NORM_PTR_TYPE init = 999; _node_val_insert(bptr, node, &init, 0); }
    }
   /* Insert multiple KV pairs in order */
   _kv_insert(bptr, node, idx, int32_t, int32_t, 10, 100); idx++;
   _kv_insert(bptr, node, idx, int32_t, int32_t, 20, 200); idx++;
   _kv_insert(bptr, node, idx, int32_t, int32_t, 30, 300); idx++;
   _kv_insert(bptr, node, idx, int32_t, int32_t, 40, 400); idx++;

   /* Verify arrays are correctly ordered */
   int32_t exp_keys[] = {10, 20, 30, 40};
   int32_t exp_vals[] = {100, 200, 300, 400};

   if (node->is_leaf)
      assert(verify_kv_pairs_leaf(bptr, node, exp_keys, exp_vals, 4,
                                  sizeof(int32_t), sizeof(int32_t)));
   else
   {
      if (bptr->is_lite)
       {
          BPTR_LITE_PTR_TYPE exp_ptrs[] = {999, 100, 200, 300, 400};
          assert(verify_kv_pairs_brch(bptr, node, exp_keys, exp_ptrs, 4,
                                      sizeof(int32_t), sizeof(BPTR_LITE_PTR_TYPE)));
       }
      else
       {
          BPTR_NORM_PTR_TYPE exp_ptrs[] = {999, 100, 200, 300, 400};
          assert(verify_kv_pairs_brch(bptr, node, exp_keys, exp_ptrs, 4,
                                      sizeof(int32_t), sizeof(BPTR_NORM_PTR_TYPE)));
       }
   }
   puts("  PASS: Keys and values arrays correctly ordered");

   test_cleanup(cfg, bptr, node);
}


void test_kv_interleaved_insert_verification(bptr_config *cfg)
{
   puts("\n=== Test: Interleaved inserts verify KV correspondence ===");
   struct bptr *bptr = bptr_init_cfg(cfg, int32_t, int32_t, cmp_i32);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, cfg->is_leaf, 0);
   assert(node);

   /* Insert at various positions and verify correspondence maintained */
   if (!node->is_leaf)
    {
      BPTR_LITE_PTR_TYPE vval_bl = 999; BPTR_NORM_PTR_TYPE vval_bn = 999;
      if (bptr->is_lite)
         _node_val_insert(bptr, node, &vval_bl, 0);
      else
         _node_val_insert(bptr, node, &vval_bn, 0);
    }
   _kv_insert(bptr, node, 0, int32_t, int32_t, 100, 1000);
   _kv_insert(bptr, node, 1, int32_t, int32_t, 300, 3000);
   _kv_insert(bptr, node, 1, int32_t, int32_t, 200, 2000);  /* Middle */
   {  /* Beginning */
      int32_t kval = 50, vval = 500;
      BPTR_LITE_PTR_TYPE vval_bl = vval; BPTR_NORM_PTR_TYPE vval_bn = vval;
      _node_key_insert(bptr, node, &kval, 0);
      if (node->is_leaf)
         _node_val_insert(bptr, node, &vval, 0);
      else if (bptr->is_lite)
         _node_val_insert(bptr, node, &vval_bl, 0);
      else
         _node_val_insert(bptr, node, &vval_bn, 0);
      node->key_count++;
   }
   _kv_insert(bptr, node, 4, int32_t, int32_t, 400, 4000);  /* End */

   int32_t exp_keys[] = {50, 100, 200, 300, 400},
           exp_vals_leaf[] = {500, 1000, 2000, 3000, 4000};
   BPTR_LITE_PTR_TYPE exp_vals_brch_lite[] = { 500, 999, 1000, 2000, 3000, 4000 };
   BPTR_NORM_PTR_TYPE exp_vals_brch_norm[] = { 500, 999, 1000, 2000, 3000, 4000 };
   if (node->is_leaf)
      assert(verify_kv_pairs_leaf(bptr, node, exp_keys, exp_vals_leaf, 5,
                                  sizeof(int32_t), sizeof(int32_t)));
   else if (bptr->is_lite)
      assert(verify_kv_pairs_brch(bptr, node, exp_keys, exp_vals_brch_lite,
                                  5, sizeof(int32_t), BPTR_LITE_PTR_BYTE));
   else
      assert(verify_kv_pairs_brch(bptr, node, exp_keys, exp_vals_brch_norm,
                                  5, sizeof(int32_t), BPTR_NORM_PTR_BYTE));
   puts("  PASS: KV correspondence maintained through interleaved inserts");

   test_cleanup(cfg, bptr, node);
}
/*------------------------------- Phase 3 END -------------------------------*/


/*----------------------------- Phase 4: Full Capacity Tests ----------------*/

void test_kv_fill_leaf_to_capacity(bptr_config *cfg)
{
   if (!cfg->is_leaf) return;
   puts("\n=== Test: Fill leaf node to capacity with KV pairs ===");
   struct bptr *bptr = bptr_init_cfg(cfg, int32_t, int32_t, cmp_i32);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 1, 0);
   assert(node);

   uint_fast16_t max_keys = bptr->node_boundry.leaf.up - 1;
   printf("  Filling leaf to %" PRIuFAST16 " keys...\n", max_keys);

   for (uint_fast16_t i = 0; i < max_keys; i++)
      _kv_insert(bptr, node, i, int32_t, int32_t, (int32_t)i, (int32_t)(i * 10));

   assert(node->key_count == max_keys);
   printf("  PASS: Leaf filled to capacity (%" PRIuFAST16 " KV pairs)\n", max_keys);

   test_cleanup(cfg, bptr, node);
}


void test_kv_fill_brch_to_capacity(bptr_config *cfg)
{
   if (cfg->is_leaf) return;
   puts("\n=== Test: Fill branch node to capacity with KV pairs ===");
   struct bptr *bptr = bptr_init_cfg(cfg, int32_t, int32_t, cmp_i32);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 0, 0);
   assert(node);

   uint_fast16_t max_keys = bptr->node_boundry.brch.up - 1;
   printf("  Filling branch to %" PRIuFAST16 " keys...\n", max_keys);

   /* Insert initial pointer */
   BPTR_LITE_PTR_TYPE init = 0;
   if (bptr->is_lite) _node_val_insert(bptr, node, &init, 0);
   else { BPTR_NORM_PTR_TYPE tmp = 0; _node_val_insert(bptr, node, &tmp, 0); }

   for (uint_fast16_t i = 0; i < max_keys; i++)
   {
      if (bptr->is_lite)
         _kv_insert(bptr, node, i, int32_t, BPTR_LITE_PTR_TYPE,
                    (int32_t)(i * 10), (BPTR_LITE_PTR_TYPE)(i * 100));
      else
         _kv_insert(bptr, node, i, int32_t, BPTR_NORM_PTR_TYPE,
                    (int32_t)(i * 10), (BPTR_NORM_PTR_TYPE)(i * 100));
   }

   assert(node->key_count == max_keys);
   assert(node->key_count + 1 == max_keys + 1); /* Verify ptr count */
   printf("  PASS: Branch filled to capacity (%" PRIuFAST16 " keys, %" PRIuFAST16 " ptrs)\n",
          max_keys, max_keys + 1);

   test_cleanup(cfg, bptr, node);
}
/*------------------------------- Phase 4 END -------------------------------*/


/*--------------------------- Phase 5: Configuration Variations ------------*/

void test_kv_different_node_sizes(void)
{
   puts("\n=== Test: Different node sizes with KV insertion ===");

   uint32_t sizes[] = {512, 1024, 4096, 8192};
   const char *filenames[] = {
      "kv_size_512.bptr", "kv_size_1k.bptr",
      "kv_size_4k.bptr", "kv_size_8k.bptr"
   };

   for (int i = 0; i < 4; i++)
    {
      bptr_config cfg = { filenames[i], 1, 1, sizes[i] };
      struct bptr *bptr = bptr_init_cfg(&cfg, int32_t, int32_t, cmp_i32);
      assert(bptr);
      struct bptr_node *node = bptr_node_new(bptr, 1, 0);
      assert(node);

      _kv_insert(bptr, node, 0, int32_t, int32_t, 100, 1000);
      _kv_insert(bptr, node, 1, int32_t, int32_t, 200, 2000);

      int32_t exp_keys[] = {100, 200};
      int32_t exp_vals[] = {1000, 2000};
      assert(verify_kv_pairs_leaf(bptr, node, exp_keys, exp_vals, 2,
                                  sizeof(int32_t), sizeof(int32_t)));

      printf("  Node size %" PRIu32 ": PASS\n", sizes[i]);
      test_cleanup(&cfg, bptr, node);
    }

   puts("  PASS: All node sizes work correctly with KV insertion");
}


void test_kv_lite_vs_norm(void)
{
   puts("\n=== Test: Compare KV insertion in lite vs norm mode ===");

   /* Lite mode */
   bptr_config cfg_lite = { "kv_lite.bptr", 1, 0, 512 };
   struct bptr *bptr_lite = bptr_init_cfg(&cfg_lite, int32_t, int32_t, cmp_i32);
   assert(bptr_lite);
   struct bptr_node *node_lite = bptr_node_new(bptr_lite, 0, 0);
   assert(node_lite);

   BPTR_LITE_PTR_TYPE init = 999;
   _node_val_insert(bptr_lite, node_lite, &init, 0);
   _kv_insert(bptr_lite, node_lite, 1, int32_t, BPTR_LITE_PTR_TYPE, 100, 1100);
   _kv_insert(bptr_lite, node_lite, 2, int32_t, BPTR_LITE_PTR_TYPE, 200, 2200);

   int32_t exp_keys[] = {100, 200};
   BPTR_LITE_PTR_TYPE exp_ptrs_lite[] = {999, 1100, 2200};
   assert(verify_kv_pairs_brch(bptr_lite, node_lite, exp_keys, exp_ptrs_lite, 2,
                               sizeof(int32_t), sizeof(BPTR_LITE_PTR_TYPE)));
   puts("  Lite mode: PASS");

   bptr_node_free(node_lite);
   bptr_destroy(bptr_lite);
   remove(cfg_lite.f_nm);

   /* Norm mode */
   bptr_config cfg_norm = { "kv_norm.bptr", 0, 0, 512 };
   struct bptr *bptr_norm = bptr_init_cfg(&cfg_norm, int32_t, int32_t, cmp_i32);
   assert(bptr_norm);
   struct bptr_node *node_norm = bptr_node_new(bptr_norm, 0, 0);
   assert(node_norm);

   BPTR_NORM_PTR_TYPE init_n = 999;
   _node_val_insert(bptr_norm, node_norm, &init_n, 0);
   _kv_insert(bptr_norm, node_norm, 1, int32_t, BPTR_NORM_PTR_TYPE, 100, 1100);
   _kv_insert(bptr_norm, node_norm, 2, int32_t, BPTR_NORM_PTR_TYPE, 200, 2200);

   BPTR_NORM_PTR_TYPE exp_ptrs_norm[] = {999, 1100, 2200};
   assert(verify_kv_pairs_brch(bptr_norm, node_norm, exp_keys, exp_ptrs_norm, 2,
                               sizeof(int32_t), sizeof(BPTR_NORM_PTR_TYPE)));
   puts("  Norm mode: PASS");

   bptr_node_free(node_norm);
   bptr_destroy(bptr_norm);
   remove(cfg_norm.f_nm);

   puts("  PASS: Both modes work correctly");
}


void test_kv_different_types(void)
{
   puts("\n=== Test: KV insertion with different value types ===");

   /* uint64_t values */
   bptr_config cfg1 = { "kv_u64_vals.bptr", 1, 1, 512 };
   struct bptr *bptr1 = bptr_init_cfg(&cfg1, int32_t, uint64_t, cmp_i32);
   assert(bptr1);
   struct bptr_node *node1 = bptr_node_new(bptr1, 1, 0);
   assert(node1);

   _kv_insert(bptr1, node1, 0, int32_t, uint64_t, 100, 0x1000ULL);
   _kv_insert(bptr1, node1, 1, int32_t, uint64_t, 200, 0x2000ULL);

   int32_t exp_keys1[] = {100, 200};
   uint64_t exp_vals1[] = {0x1000ULL, 0x2000ULL};
   assert(verify_kv_pairs_leaf(bptr1, node1, exp_keys1, exp_vals1, 2,
                               sizeof(int32_t), sizeof(uint64_t)));
   puts("  uint64_t values: PASS");

   test_cleanup(&cfg1, bptr1, node1);

   /* key16_t values */
   bptr_config cfg2 = { "kv_k16_vals.bptr", 1, 1, 512 };
   struct bptr *bptr2 = bptr_init_cfg(&cfg2, int32_t, key16_t, cmp_i32);
   assert(bptr2);
   struct bptr_node *node2 = bptr_node_new(bptr2, 1, 0);
   assert(node2);

   key16_t v1 = {{1, 2, 3, 4}};
   key16_t v2 = {{5, 6, 7, 8}};
   int32_t key = 100;
   _node_key_insert(bptr2, node2, &key, 0);
   _node_val_insert(bptr2, node2, &v1, 0);
   node2->key_count++;
   key = 200;
   _node_key_insert(bptr2, node2, &key, 1);
   _node_val_insert(bptr2, node2, &v2, 1);
   node2->key_count++;

   int32_t exp_keys2[] = {100, 200};
   key16_t exp_vals2[] = {v1, v2};
   assert(verify_kv_pairs_leaf(bptr2, node2, exp_keys2, exp_vals2, 2,
                               sizeof(int32_t), sizeof(key16_t)));
   puts("  key16_t values: PASS");

   test_cleanup(&cfg2, bptr2, node2);

   puts("  PASS: Different value types work correctly");
}
/*------------------------------- Phase 5 END -------------------------------*/


/*----------------------------- Phase 6: Edge Cases -------------------------*/

void test_kv_boundary_positions(void)
{
   puts("\n=== Test: KV insert at boundary positions ===");
   bptr_config cfg = { "kv_boundaries.bptr", 1, 1, 512 };
   struct bptr *bptr = bptr_init_cfg(&cfg, int32_t, int32_t, cmp_i32);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 1, 0);
   assert(node);

   /* Fill node */
   for (int i = 0; i < 5; i++)
      _kv_insert(bptr, node, i, int32_t, int32_t, (i + 1) * 100, (i + 1) * 1000);

   /* Insert at beginning */
   _kv_insert(bptr, node, 0, int32_t, int32_t, 50, 500);
   /* Insert at end */
   _kv_insert(bptr, node, 6, int32_t, int32_t, 600, 6000);
   /* Insert at second position */
   _kv_insert(bptr, node, 1, int32_t, int32_t, 75, 750);

   int32_t exp_keys[] = {50, 75, 100, 200, 300, 400, 500, 600};
   int32_t exp_vals[] = {500, 750, 1000, 2000, 3000, 4000, 5000, 6000};
   assert(verify_kv_pairs_leaf(bptr, node, exp_keys, exp_vals, 8,
                               sizeof(int32_t), sizeof(int32_t)));
   puts("  PASS: All boundary positions work correctly");

   test_cleanup(&cfg, bptr, node);
}


void test_kv_all_pairs_preserved(void)
{
   puts("\n=== Test: Verify ALL KV pairs preserved after insertions ===");
   bptr_config cfg = { "kv_preserve.bptr", 1, 1, 512 };
   struct bptr *bptr = bptr_init_cfg(&cfg, int32_t, int32_t, cmp_i32);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 1, 0);
   assert(node);

   /* Build expected array progressively */
   int32_t exp_keys[10];
   int32_t exp_vals[10];

   for (int i = 0; i < 5; i++)
    {
      exp_keys[i] = (i + 1) * 100;
      exp_vals[i] = (i + 1) * 1000;
      _kv_insert(bptr, node, i, int32_t, int32_t, exp_keys[i], exp_vals[i]);
    }

   /* Insert in middle */
   exp_keys[5] = 250;
   exp_vals[5] = 2500;
   for (int i = 5; i > 2; i--)
    {
      exp_keys[i] = exp_keys[i - 1];
      exp_vals[i] = exp_vals[i - 1];
    }
   exp_keys[2] = 250;
   exp_vals[2] = 2500;
   _kv_insert(bptr, node, 2, int32_t, int32_t, 250, 2500);

   int32_t final_keys[] = {100, 200, 250, 300, 400, 500};
   int32_t final_vals[] = {1000, 2000, 2500, 3000, 4000, 5000};
   assert(verify_kv_pairs_leaf(bptr, node, final_keys, final_vals, 6,
                               sizeof(int32_t), sizeof(int32_t)));
   puts("  PASS: All KV pairs preserved correctly");

   test_cleanup(&cfg, bptr, node);
}


void test_kv_single_insert_stress(void)
{
   puts("\n=== Test: Single element insert stress ===");
   bptr_config cfg = { "kv_stress.bptr", 1, 1, 512 };
   struct bptr *bptr = bptr_init_cfg(&cfg, int32_t, int32_t, cmp_i32);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 1, 0);
   assert(node);

   /* Repeatedly insert at position 0 (worst case for shifting) */
   for (int i = 0; i < 10; i++)
      _kv_insert(bptr, node, 0, int32_t, int32_t, i, i * 10);

   /* Verify all inserted correctly (in reverse order) */
   for (int i = 0; i < 10; i++)
    {
      assert(((int32_t*)node->keys)[i] == 9 - i);
      assert(((int32_t*)node->vals)[i] == (9 - i) * 10);
    }
   puts("  PASS: Repeated beginning inserts work correctly");

   test_cleanup(&cfg, bptr, node);
}
/*------------------------------- Phase 6 END -------------------------------*/


/*---------------------------------- Main -----------------------------------*/

int main(void)
{
   puts("========================================");
   puts(" B+Tree Combined Key-Value Insert Test");
   puts("========================================");

   puts("\n--- Phase 1: Basic Leaf Node Tests ---");
   for (size_t i = 0; i < sizeof(all_leaf_configs)/sizeof(all_leaf_configs[0]); i++)
    {
      test_kv_leaf_empty(all_leaf_configs[i]);
      test_kv_leaf_beginning(all_leaf_configs[i]);
      test_kv_leaf_middle(all_leaf_configs[i]);
      test_kv_leaf_end(all_leaf_configs[i]);
      test_kv_leaf_correspondence(all_leaf_configs[i]);
    }

   puts("\n--- Phase 2: Basic Branch Node Tests ---");
   for (size_t i = 0; i < sizeof(all_brch_configs)/sizeof(all_brch_configs[0]); i++)
    {
      test_kv_brch_initial_ptr(all_brch_configs[i]);
      test_kv_brch_beginning(all_brch_configs[i]);
      test_kv_brch_middle(all_brch_configs[i]);
      test_kv_brch_end(all_brch_configs[i]);
      test_kv_brch_ptr_count(all_brch_configs[i]);
    }

   puts("\n--- Phase 3: Position-Based Verification ---");
   for (size_t i = 0; i < sizeof(configs)/sizeof(configs[0]); i++)
    {
      test_kv_multiple_ordered_verification(configs[i]);
      test_kv_interleaved_insert_verification(configs[i]);
    }

   puts("\n--- Phase 4: Full Capacity Tests ---");
   for (size_t i = 0; i < sizeof(all_leaf_configs)/sizeof(all_leaf_configs[0]); i++)
      test_kv_fill_leaf_to_capacity(all_leaf_configs[i]);
   for (size_t i = 0; i < sizeof(all_brch_configs)/sizeof(all_brch_configs[0]); i++)
      test_kv_fill_brch_to_capacity(all_brch_configs[i]);

   puts("\n--- Phase 5: Configuration Variations ---");
   test_kv_different_node_sizes();
   test_kv_lite_vs_norm();
   test_kv_different_types();

   puts("\n--- Phase 6: Edge Cases ---");
   test_kv_boundary_positions();
   test_kv_all_pairs_preserved();
   test_kv_single_insert_stress();

   puts("\n========================================");
   puts(" All KV insertion tests passed!");
   puts("========================================");

   return 0;
}
