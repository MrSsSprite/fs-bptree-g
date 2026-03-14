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
norm_brch_cfg = { "norm_brch_cfg.bptr", 0, 0, 512 },
size_1k_cfg = { "size_1k_cfg.bptr", 1, 1, 1024 },
size_1k_brch_cfg = { "size_1k_brch_cfg.bptr", 1, 0, 1024 },
size_4k_cfg = { "size_4k_cfg.bptr", 1, 1, 4096 },
size_4k_brch_cfg = { "size_4k_brch_cfg.bptr", 1, 0, 4096 },
size_4k_norm_cfg = { "size_4k_norm.bptr", 0, 1, 4096 },
size_8k_cfg = { "size_8k_cfg.bptr", 1, 1, 8192 },
size_16k_cfg = { "size_16k_cfg.bptr", 1, 1, 16384 };

bptr_config *configs[] =
 { &default_cfg, &default_brch_cfg, &norm_leaf_cfg, &norm_brch_cfg,
   &size_1k_cfg, &size_1k_brch_cfg, &size_4k_cfg, &size_4k_brch_cfg,
   &size_4k_norm_cfg, &size_8k_cfg, &size_16k_cfg };

bptr_config *all_leaf_configs[] =
 { &default_cfg, &norm_leaf_cfg, &size_1k_cfg, &size_4k_cfg,
   &size_4k_norm_cfg, &size_8k_cfg, &size_16k_cfg };

bptr_config *all_brch_configs[] =
 { &default_brch_cfg, &norm_brch_cfg, &size_1k_brch_cfg, &size_4k_brch_cfg };
/*--------------------------- Config Profiles END ----------------------------*/


/*---------------------------- Verification Helpers ---------------------------*/
bool verify_node_vals(struct bptr *bptr, struct bptr_node *node,
                      const void *expected, uint32_t count, size_t val_size)
{
   uint32_t val_count = node->key_count + (node->is_leaf ? 0 : 1);
   if (val_count != count)
    {
      printf("  FAIL: val_count mismatch (expected %" PRIu32 ", got %" PRIu32 ")\n",
             count, val_count);
      return false;
    }
   const char *arr = node->vals;
   const char *exp = expected;
   if (!node->is_leaf)
      val_size = bptr->is_lite ? BPTR_LITE_PTR_BYTE : BPTR_NORM_PTR_BYTE;
   for (uint32_t i = 0; i < count; i++)
    {
      if (memcmp(arr + i * val_size, exp + i * val_size, val_size) != 0)
       {
          printf("  FAIL: val[%" PRIu32 "] mismatch\n", i);
          return false;
       }
    }
   return true;
}
/*-------------------------- Verification Helpers END ------------------------*/


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
   if (!node->is_leaf) { test_insert_noincr(bptr, node, 0, int32_t, 999); i++; }
   for (sz_mx = (node->is_leaf ? bptr->node_boundry.leaf.up :
                                 bptr->node_boundry.brch.up + 1) - 1;
        i < sz_mx; i++)
      test_insert(bptr, node, i, int32_t, 2 * i);

   test_print_vals(bptr, node, int32_t, PRIi32);
   test_cleanup(cfg, bptr, node);
}


/*--------------------------- Position-based Tests ---------------------------*/
void test_val_insert_beginning(bptr_config *cfg)
{
   puts("\n=== Test: Insert value at beginning (shifting) ===");
   struct bptr *bptr = bptr_init_cfg(cfg, int32_t, int32_t, cmp_i32);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, cfg->is_leaf, 0);
   assert(node);
   test_print_properties(bptr, node);

   uint_fast32_t idx = 0;
   if (!node->is_leaf)
    {
      test_insert_noincr(bptr, node, idx, int32_t, 999);
      idx++;
    }
   test_insert(bptr, node, idx, int32_t, 100);
   idx++;
   test_insert(bptr, node, idx, int32_t, 200);
   idx++;
   test_insert(bptr, node, 0, int32_t, 50);

   /* For leaf: vals = [50, 100, 200], for branch: vals = [50, 999, 100, 200] */
   int32_t expected_leaf[] = {50, 100, 200};
   BPTR_LITE_PTR_TYPE expected_brch_lite[] = {50, 999, 100, 200};
   BPTR_NORM_PTR_TYPE expected_brch_norm[] = {50, 999, 100, 200};
   uint32_t expected_count = node->key_count + (node->is_leaf ? 0 : 1);
   const void *expected = node->is_leaf ? expected_leaf :
                                          (bptr->is_lite ? expected_brch_lite :
                                             (void*)       expected_brch_norm);
   assert(verify_node_vals(bptr, node, expected, expected_count, sizeof(int32_t)));
   puts("  PASS: Values shifted correctly when inserting at beginning");

   test_cleanup(cfg, bptr, node);
}


void test_val_insert_middle(bptr_config *cfg)
{
   puts("\n=== Test: Insert value at middle ===");
   struct bptr *bptr = bptr_init_cfg(cfg, int32_t, int32_t, cmp_i32);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, cfg->is_leaf, 0);
   assert(node);
   test_print_properties(bptr, node);

   uint_fast32_t idx = 0;
   if (!node->is_leaf)
    {
      test_insert_noincr(bptr, node, idx, int32_t, 999);
      idx++;
    }
   test_insert(bptr, node, idx, int32_t, 100);
   idx++;
   test_insert(bptr, node, idx, int32_t, 200);
   idx++;
   test_insert(bptr, node, idx, int32_t, 400);
   idx++;
   /* For leaf: insert at index 2, for branch: insert at index 2 (after 999) */
   test_insert(bptr, node, 2, int32_t, 150);

   /* For leaf: vals = [100, 200, 150, 400], for branch: vals = [999, 100, 150, 200, 400] */
   int32_t expected_leaf[] = {100, 200, 150, 400};
   BPTR_LITE_PTR_TYPE expected_brch_lite[] = {999, 100, 150, 200, 400};
   BPTR_NORM_PTR_TYPE expected_brch_norm[] = {999, 100, 150, 200, 400};
   uint32_t expected_count = node->key_count + (node->is_leaf ? 0 : 1);
   const void *expected = node->is_leaf ? expected_leaf :
                                          (bptr->is_lite ? expected_brch_lite :
                                             (void*)       expected_brch_norm);
   assert(verify_node_vals(bptr, node, expected, expected_count, sizeof(int32_t)));
   puts("  PASS: Values shifted correctly when inserting at middle");

   test_cleanup(cfg, bptr, node);
}


void test_val_insert_end(bptr_config *cfg)
{
   puts("\n=== Test: Insert value at end ===");
   struct bptr *bptr = bptr_init_cfg(cfg, int32_t, int32_t, cmp_i32);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, cfg->is_leaf, 0);
   assert(node);
   test_print_properties(bptr, node);

   uint_fast32_t idx = 0;
   if (!node->is_leaf)
    {
      test_insert_noincr(bptr, node, idx, int32_t, 999);
      idx++;
    }
   test_insert(bptr, node, idx, int32_t, 100);
   idx++;
   test_insert(bptr, node, idx, int32_t, 200);
   idx++;
   test_insert(bptr, node, idx, int32_t, 300);
   idx++;

   /* For leaf: vals = [100, 200, 300], for branch: vals = [999, 100, 200, 300] */
   int32_t expected_leaf[] = {100, 200, 300};
   BPTR_LITE_PTR_TYPE expected_brch_lite[] = {999, 100, 200, 300};
   BPTR_NORM_PTR_TYPE expected_brch_norm[] = {999, 100, 200, 300};
   uint32_t expected_count = node->key_count + (node->is_leaf ? 0 : 1);
   const void *expected = node->is_leaf ? expected_leaf :
                                          (bptr->is_lite ? expected_brch_lite :
                                             (void*)       expected_brch_norm);
   assert(verify_node_vals(bptr, node, expected, expected_count, sizeof(int32_t)));
   puts("  PASS: Values inserted correctly at end (no shift needed)");

   test_cleanup(cfg, bptr, node);
}
/*------------------------- Position-based Tests END -------------------------*/


/*---------------------------- Branch-specific Tests -------------------------*/
void test_branch_val_insert_beginning(bptr_config *cfg)
{
   if (cfg->is_leaf) return; /* Skip leaf configs */
   puts("\n=== Test: Branch node first value insertion ===");
   struct bptr *bptr = bptr_init_cfg(cfg, int32_t, int32_t, cmp_i32);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 0, 0);
   assert(node);
   test_print_properties(bptr, node);

   /* Branch: first value inserted without key, then insert with key */
   test_insert_noincr(bptr, node, 0, int32_t, 500);
   test_insert(bptr, node, 1, int32_t, 1000);
   test_insert(bptr, node, 0, int32_t, 250);

   /* vals = [250, 500, 1000], key_count = 2, expected_count = 3 */
   BPTR_LITE_PTR_TYPE expected_brch_lite[] = {250, 500, 1000};
   BPTR_NORM_PTR_TYPE expected_brch_norm[] = {250, 500, 1000};
   uint32_t expected_count = 3;
   const void *expected = bptr->is_lite ? expected_brch_lite : (void*)expected_brch_norm;
   assert(verify_node_vals(bptr, node, expected, expected_count, sizeof(int32_t)));
   puts("  PASS: Branch first value insertion works correctly");

   test_cleanup(cfg, bptr, node);
}


void test_branch_val_insert_middle(bptr_config *cfg)
{
   if (cfg->is_leaf) return; /* Skip leaf configs */
   puts("\n=== Test: Branch node middle insertion ===");
   struct bptr *bptr = bptr_init_cfg(cfg, int32_t, int32_t, cmp_i32);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 0, 0);
   assert(node);
   test_print_properties(bptr, node);

   test_insert_noincr(bptr, node, 0, int32_t, 100);
   test_insert(bptr, node, 1, int32_t, 300);
   test_insert(bptr, node, 1, int32_t, 200);

   /* vals = [100, 200, 300], key_count = 2, expected_count = 3 */
   BPTR_LITE_PTR_TYPE expected_brch_lite[] = {100, 200, 300};
   BPTR_NORM_PTR_TYPE expected_brch_norm[] = {100, 200, 300};
   uint32_t expected_count = 3;
   const void *expected = bptr->is_lite ? expected_brch_lite : (void*)expected_brch_norm;
   assert(verify_node_vals(bptr, node, expected, expected_count, sizeof(int32_t)));
   puts("  PASS: Branch node middle insertion works correctly");

   test_cleanup(cfg, bptr, node);
}


void test_branch_val_insert_end(bptr_config *cfg)
{
   if (cfg->is_leaf) return; /* Skip leaf configs */
   puts("\n=== Test: Branch node end insertion ===");
   struct bptr *bptr = bptr_init_cfg(cfg, int32_t, int32_t, cmp_i32);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 0, 0);
   assert(node);
   test_print_properties(bptr, node);

   test_insert_noincr(bptr, node, 0, int32_t, 100);
   test_insert(bptr, node, 1, int32_t, 200);
   test_insert(bptr, node, 2, int32_t, 300);

   /* vals = [100, 200, 300], key_count = 2, expected_count = 3 */
   BPTR_LITE_PTR_TYPE expected_brch_lite[] = {100, 200, 300};
   BPTR_NORM_PTR_TYPE expected_brch_norm[] = {100, 200, 300};
   uint32_t expected_count = 3;
   const void *expected = bptr->is_lite ? expected_brch_lite : (void*)expected_brch_norm;
   assert(verify_node_vals(bptr, node, expected, expected_count, sizeof(int32_t)));
   puts("  PASS: Branch node end insertion works correctly");

   test_cleanup(cfg, bptr, node);
}
/*-------------------------- Branch-specific Tests END -----------------------*/


/*------------------------------ Edge Case Tests -----------------------------*/
void test_insert_second_val(void)
{
   puts("\n=== Test: Insert second value (going from 1 to 2) ===");
   bptr_config cfg = { "test_second_val.bptr", 1, 1, 512 };
   struct bptr *bptr = bptr_init_cfg(&cfg, int32_t, int32_t, cmp_i32);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 1, 0);
   assert(node);

   test_insert(bptr, node, 0, int32_t, 100);
   test_insert(bptr, node, 1, int32_t, 200);

   int32_t expected[] = {100, 200};
   assert(verify_node_vals(bptr, node, expected, 2, sizeof(int32_t)));
   puts("  PASS: Second value insertion works correctly");

   test_cleanup(&cfg, bptr, node);
}


void test_insert_penultimate(void)
{
   puts("\n=== Test: Insert at second-to-last position ===");
   bptr_config cfg = { "test_penultimate.bptr", 1, 1, 512 };
   struct bptr *bptr = bptr_init_cfg(&cfg, int32_t, int32_t, cmp_i32);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 1, 0);
   assert(node);

   test_insert(bptr, node, 0, int32_t, 100);
   test_insert(bptr, node, 1, int32_t, 200);
   test_insert(bptr, node, 2, int32_t, 400);
   test_insert(bptr, node, 2, int32_t, 300);

   int32_t expected[] = {100, 200, 300, 400};
   assert(verify_node_vals(bptr, node, expected, 4, sizeof(int32_t)));
   puts("  PASS: Penultimate value insertion works correctly");

   test_cleanup(&cfg, bptr, node);
}


void test_val_shift_preserves_all(void)
{
   puts("\n=== Test: Verify ALL values preserved after shift ===");
   bptr_config cfg = { "test_preserve_vals.bptr", 1, 1, 512 };
   struct bptr *bptr = bptr_init_cfg(&cfg, int32_t, int32_t, cmp_i32);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 1, 0);
   assert(node);

   /* Fill with multiple values */
   for (int i = 0; i < 5; i++)
      test_insert(bptr, node, i, int32_t, (i + 1) * 100);

   /* Insert in middle and verify all preserved */
   test_insert(bptr, node, 2, int32_t, 250);

   int32_t expected[] = {100, 200, 250, 300, 400, 500};
   assert(verify_node_vals(bptr, node, expected, 6, sizeof(int32_t)));
   puts("  PASS: All values preserved after shift");

   test_cleanup(&cfg, bptr, node);
}


void test_val_boundary_integrity(void)
{
   puts("\n=== Test: Values at boundaries not corrupted ===");
   bptr_config cfg = { "test_boundary.bptr", 1, 1, 512 };
   struct bptr *bptr = bptr_init_cfg(&cfg, int32_t, int32_t, cmp_i32);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 1, 0);
   assert(node);

   test_insert(bptr, node, 0, int32_t, 111);
   test_insert(bptr, node, 1, int32_t, 222);
   test_insert(bptr, node, 2, int32_t, 333);
   test_insert(bptr, node, 3, int32_t, 444);

   /* Insert at beginning and verify end not corrupted */
   test_insert(bptr, node, 0, int32_t, 0);
   test_insert(bptr, node, 5, int32_t, 555);

   int32_t expected[] = {0, 111, 222, 333, 444, 555};
   assert(verify_node_vals(bptr, node, expected, 6, sizeof(int32_t)));
   puts("  PASS: Boundary values remain intact");

   test_cleanup(&cfg, bptr, node);
}
/*---------------------------- Edge Case Tests END ---------------------------*/


/*-------------------------- Configuration Variation Tests -------------------*/

void test_different_node_sizes(void)
{
   puts("\n=== Test: Different node sizes (1K, 4K, 8K, 16K) ===");

   uint32_t sizes[] = {1024, 4096, 8192, 16384};
   const char *filenames[] = {
      "test_val_1k.bptr", "test_val_4k.bptr",
      "test_val_8k.bptr", "test_val_16k.bptr"
   };

   for (int i = 0; i < 4; i++)
    {
      bptr_config cfg = { filenames[i], 1, 1, sizes[i] };
      struct bptr *bptr = bptr_init_cfg(&cfg, int32_t, int32_t, cmp_i32);
      assert(bptr);
      struct bptr_node *node = bptr_node_new(bptr, 1, 0);
      assert(node);

      test_insert(bptr, node, 0, int32_t, 100);
      test_insert(bptr, node, 1, int32_t, 200);

      int32_t expected[] = {100, 200};
      assert(verify_node_vals(bptr, node, expected, 2, sizeof(int32_t)));

      printf("  Node size %" PRIu32 ": PASS\n", sizes[i]);
      test_cleanup(&cfg, bptr, node);
    }

   puts("  PASS: All node sizes work correctly");
}


void test_lite_vs_norm_capacity(void)
{
   puts("\n=== Test: Compare val capacities between lite and norm modes ===");

   /* Test with lite mode (4-byte pointers for branch values) */
   bptr_config cfg_lite = { "test_val_cap_lite.bptr", 1, 0, 512 };
   struct bptr *bptr_lite = bptr_init_cfg(&cfg_lite, int32_t, int32_t, cmp_i32);
   assert(bptr_lite);
   struct bptr_node *branch_lite = bptr_node_new(bptr_lite, 0, 0);
   assert(branch_lite);

   uint_fast16_t lite_branch_max = bptr_lite->node_boundry.brch.up;
   printf("  Lite mode (4-byte branch ptrs): max vals = %" PRIuFAST16 "\n",
          lite_branch_max);

   /* Test with non-lite mode (8-byte pointers for branch values) */
   bptr_config cfg_norm = { "test_val_cap_norm.bptr", 0, 0, 512 };
   struct bptr *bptr_norm = bptr_init_cfg(&cfg_norm, int32_t, int32_t, cmp_i32);
   assert(bptr_norm);
   struct bptr_node *branch_norm = bptr_node_new(bptr_norm, 0, 0);
   assert(branch_norm);

   uint_fast16_t norm_branch_max = bptr_norm->node_boundry.brch.up;
   printf("  Non-lite mode (8-byte branch ptrs): max vals = %" PRIuFAST16 "\n",
          norm_branch_max);

   /* Non-lite branch nodes should have fewer values due to larger pointers */
   printf("  Branch capacity difference: %" PRIdFAST16 " vals\n",
          (int_fast16_t)(norm_branch_max - lite_branch_max));

   bptr_node_free(branch_lite);
   bptr_node_free(branch_norm);
   bptr_destroy(bptr_lite);
   bptr_destroy(bptr_norm);
   remove(cfg_lite.f_nm);
   remove(cfg_norm.f_nm);

   puts("  PASS: Capacity comparison complete");
}


void test_large_values(void)
{
   puts("\n=== Test: Large value types (uint64_t, key16_t, key32_t) ===");

   /* Test with uint64_t values */
   bptr_config cfg_u64 = { "test_val_u64.bptr", 1, 1, 512 };
   struct bptr *bptr_u64 = bptr_init_cfg(&cfg_u64, int32_t, uint64_t, cmp_i32);
   assert(bptr_u64);
   struct bptr_node *node_u64 = bptr_node_new(bptr_u64, 1, 0);
   assert(node_u64);

   _node_val_insert(bptr_u64, node_u64, &(uint64_t){0x100000000ULL}, 0);
   node_u64->key_count++;
   _node_val_insert(bptr_u64, node_u64, &(uint64_t){0x200000000ULL}, 1);
   node_u64->key_count++;

   uint64_t expected_u64[] = {0x100000000ULL, 0x200000000ULL};
   assert(verify_node_vals(bptr_u64, node_u64, expected_u64, 2, sizeof(uint64_t)));
   puts("  uint64_t values: PASS");

   bptr_node_free(node_u64);
   bptr_destroy(bptr_u64);
   remove(cfg_u64.f_nm);

   /* Test with key16_t values (16 bytes) */
   bptr_config cfg_k16 = { "test_val_k16.bptr", 1, 1, 512 };
   struct bptr *bptr_k16 = bptr_init_cfg(&cfg_k16, int32_t, key16_t, cmp_i32);
   assert(bptr_k16);
   struct bptr_node *node_k16 = bptr_node_new(bptr_k16, 1, 0);
   assert(node_k16);

   key16_t v1 = {{1, 2, 3, 4}};
   key16_t v2 = {{5, 6, 7, 8}};
   _node_val_insert(bptr_k16, node_k16, &v1, 0);
   node_k16->key_count++;
   _node_val_insert(bptr_k16, node_k16, &v2, 1);
   node_k16->key_count++;

   key16_t expected_k16[] = {v1, v2};
   assert(verify_node_vals(bptr_k16, node_k16, expected_k16, 2, sizeof(key16_t)));
   puts("  key16_t values: PASS");

   bptr_node_free(node_k16);
   bptr_destroy(bptr_k16);
   remove(cfg_k16.f_nm);

   /* Test with key32_t values (32 bytes) */
   bptr_config cfg_k32 = { "test_val_k32.bptr", 1, 1, 4096 };
   struct bptr *bptr_k32 = bptr_init_cfg(&cfg_k32, int32_t, key32_t, cmp_i32);
   assert(bptr_k32);
   struct bptr_node *node_k32 = bptr_node_new(bptr_k32, 1, 0);
   assert(node_k32);

   key32_t v3 = KEY32(100);
   key32_t v4 = KEY32(200);
   _node_val_insert(bptr_k32, node_k32, &v3, 0);
   node_k32->key_count++;
   _node_val_insert(bptr_k32, node_k32, &v4, 1);
   node_k32->key_count++;

   key32_t expected_k32[] = {v3, v4};
   assert(verify_node_vals(bptr_k32, node_k32, expected_k32, 2, sizeof(key32_t)));
   puts("  key32_t values: PASS");

   bptr_node_free(node_k32);
   bptr_destroy(bptr_k32);
   remove(cfg_k32.f_nm);

   puts("  PASS: All large value types work correctly");
}


void test_small_values_large_keys(void)
{
   puts("\n=== Test: Asymmetric key/value sizes (large keys, small values) ===");

   bptr_config cfg = { "test_val_asym.bptr", 1, 1, 512 };
   struct bptr *bptr = bptr_init_cfg(&cfg, key32_t, int32_t, cmp_key32);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 1, 0);
   assert(node);

   test_insert(bptr, node, 0, int32_t, 100);
   test_insert(bptr, node, 1, int32_t, 200);
   test_insert(bptr, node, 0, int32_t, 50);

   int32_t expected[] = {50, 100, 200};
   assert(verify_node_vals(bptr, node, expected, 3, sizeof(int32_t)));
   puts("  PASS: Asymmetric sizes work correctly");

   test_cleanup(&cfg, bptr, node);
}
/*------------------------ Configuration Variation Tests END ----------------*/


/*---------------------------------- Main -----------------------------------*/
int main(void)
{
   puts("========================================");
   puts("  B+Tree Node Value Insertion Test Suite");
   puts("========================================");

   puts("\n--- Phase 1: Basic Tests ---");
   for (size_t i = 0; i < sizeof(configs)/sizeof(configs[0]); i++)
    {
      test_insert_empty(configs[i]);
      test_to_full(configs[i]);
    }

   puts("\n--- Phase 2: Position-based Insertion Tests ---");
   for (size_t i = 0; i < sizeof(configs)/sizeof(configs[0]); i++)
    {
      test_val_insert_beginning(configs[i]);
      test_val_insert_middle(configs[i]);
      test_val_insert_end(configs[i]);
    }

   puts("\n--- Phase 3: Branch-specific Tests ---");
   for (size_t i = 0; i < sizeof(all_brch_configs)/sizeof(all_brch_configs[0]); i++)
    {
      test_branch_val_insert_beginning(all_brch_configs[i]);
      test_branch_val_insert_middle(all_brch_configs[i]);
      test_branch_val_insert_end(all_brch_configs[i]);
    }

   puts("\n--- Phase 4: Edge Case Tests ---");
   test_insert_second_val();
   test_insert_penultimate();
   test_val_shift_preserves_all();
   test_val_boundary_integrity();

   puts("\n--- Phase 5: Configuration Variation Tests ---");
   test_different_node_sizes();
   test_lite_vs_norm_capacity();
   test_large_values();
   test_small_values_large_keys();

   puts("\n========================================");
   puts("  All tests completed successfully!");
   puts("========================================");

   return 0;
}
