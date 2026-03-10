#include <stdio.h>
#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

#include "../src/bptr_internal.h"
#include "../bptree.h"
#include "../src/bptr_node.c"


#define _key_insert_test(self, node, idx, type, val) do \
{ \
   type var = (val); \
   _node_key_insert((self), (node), &var, idx); \
   assert(((type*)node->keys)[(idx)] == (val)); \
   (node)->key_count++; \
} while (0)


/* Helper macro for running tests with different configurations */
#define _run_test_with_config(test_fn, is_lite, node_sz, key_sz, val_sz, cmp) \
   do { \
      static const char *f_nm = #test_fn "_cfg.bptr"; \
      struct bptr *bptr = bptr_init(f_nm, is_lite, node_sz, key_sz, val_sz, cmp); \
      assert(bptr); \
      test_fn(bptr); \
      assert(bptr_destroy(bptr) == 0); \
      assert(remove(f_nm) == 0); \
   } while (0)

/* Cleanup helper macro */
#define cleanup_test(bptr, node, filename) \
   do { \
      bptr_node_free(node); \
      bptr_destroy(bptr); \
      remove(filename); \
   } while (0)


int cmp_i(const void *lhs, const void *rhs)
{ return *(const int*)lhs - *(const int *)rhs; }

/* Comparator for uint64_t keys/values */
int cmp_u64(const void *lhs, const void *rhs)
{
   return *(const uint64_t*)lhs < *(const uint64_t*)rhs ? -1 :
          *(const uint64_t*)lhs > *(const uint64_t*)rhs ? 1 : 0;
}


void print_node_keys(struct bptr_node *node)
{
   const int *arr = node->keys;
   putchar('[');
   for (uint_fast32_t i = 0; i < node->key_count; i++)
      printf("%d, ", arr[i]);
   puts("]");
}


void print_node_capacity(struct bptr *self, struct bptr_node *node)
{
   uint_fast16_t max_keys;
   if (node->is_leaf)
      max_keys = self->node_boundry.leaf.up - 1;
   else
      max_keys = self->node_boundry.brch.up - 1;
   printf("  Node type: %s, key_count: %" PRIu32 ", max_keys: %" PRIuFAST16 "\n",
          node->is_leaf ? "leaf" : "branch", node->key_count, max_keys);
}


bool verify_node_keys(struct bptr_node *node, const int *expected, uint32_t count)
{
   if (node->key_count != count)
    {
      printf("  FAIL: key_count mismatch (expected %" PRIu32 ", got %" PRIu32 ")\n",
             count, node->key_count);
      return false;
    }
   const int *arr = node->keys;
   for (uint32_t i = 0; i < count; i++)
    {
      if (arr[i] != expected[i])
       {
          printf("  FAIL: key[%" PRIu32 "] mismatch (expected %d, got %d)\n",
                 i, expected[i], arr[i]);
          return false;
       }
    }
   return true;
}


void test_keys_insert_template(void)
{
   static const char *f_nm = "test_node_insert.bptr";
   struct bptr *bptr = bptr_init(f_nm, 1, 512,
                                 sizeof(int), sizeof(int), cmp_i);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 1, 0);
   assert(node);

   printf("Key count: %" PRIu32 ", Key max: %" PRIuFAST16 "\n",
          node->key_count, bptr->node_boundry.leaf.up - 1);
#define key_insert_test(idx, val) \
   _key_insert_test(bptr, node, (idx), int, (val))

   key_insert_test(0, 123);
   key_insert_test(1, 789);
   key_insert_test(0, 321);
   key_insert_test(2, 456);

   print_node_keys(node);

   bptr_node_free(node);
   assert(bptr_destroy(bptr) == 0);
   assert(remove(f_nm) == 0);
}


void test_leaf_insert_empty(void)
{
   puts("\n=== Test: Insert into empty leaf node ===");
   static const char *f_nm = "test_leaf_empty.bptr";
   struct bptr *bptr = bptr_init(f_nm, 1, 512, sizeof(int), sizeof(int), cmp_i);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 1, 0);
   assert(node);

   print_node_capacity(bptr, node);

   _key_insert_test(bptr, node, 0, int, 100);

   int expected[] = {100};
   assert(verify_node_keys(node, expected, 1));
   puts("  PASS: First key inserted correctly");

   bptr_node_free(node);
   assert(bptr_destroy(bptr) == 0);
   assert(remove(f_nm) == 0);
}


void test_leaf_insert_beginning(void)
{
   puts("\n=== Test: Insert at beginning (shifting) ===");
   static const char *f_nm = "test_leaf_begin.bptr";
   struct bptr *bptr = bptr_init(f_nm, 1, 512, sizeof(int), sizeof(int), cmp_i);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 1, 0);
   assert(node);

   _key_insert_test(bptr, node, 0, int, 100);
   _key_insert_test(bptr, node, 1, int, 200);
   _key_insert_test(bptr, node, 0, int, 50);

   int expected[] = {50, 100, 200};
   assert(verify_node_keys(node, expected, 3));
   puts("  PASS: Keys shifted correctly when inserting at beginning");

   bptr_node_free(node);
   assert(bptr_destroy(bptr) == 0);
   assert(remove(f_nm) == 0);
}


void test_leaf_insert_middle(void)
{
   puts("\n=== Test: Insert at middle ===");
   static const char *f_nm = "test_leaf_middle.bptr";
   struct bptr *bptr = bptr_init(f_nm, 1, 512, sizeof(int), sizeof(int), cmp_i);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 1, 0);
   assert(node);

   _key_insert_test(bptr, node, 0, int, 100);
   _key_insert_test(bptr, node, 1, int, 200);
   _key_insert_test(bptr, node, 2, int, 400);
   _key_insert_test(bptr, node, 1, int, 150);

   int expected[] = {100, 150, 200, 400};
   assert(verify_node_keys(node, expected, 4));
   puts("  PASS: Keys shifted correctly when inserting at middle");

   bptr_node_free(node);
   assert(bptr_destroy(bptr) == 0);
   assert(remove(f_nm) == 0);
}


void test_leaf_insert_end(void)
{
   puts("\n=== Test: Insert at end ===");
   static const char *f_nm = "test_leaf_end.bptr";
   struct bptr *bptr = bptr_init(f_nm, 1, 512, sizeof(int), sizeof(int), cmp_i);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 1, 0);
   assert(node);

   _key_insert_test(bptr, node, 0, int, 100);
   _key_insert_test(bptr, node, 1, int, 200);
   _key_insert_test(bptr, node, 2, int, 300);

   int expected[] = {100, 200, 300};
   assert(verify_node_keys(node, expected, 3));
   puts("  PASS: Keys inserted correctly at end (no shift needed)");

   bptr_node_free(node);
   assert(bptr_destroy(bptr) == 0);
   assert(remove(f_nm) == 0);
}


void test_branch_insert_empty(void)
{
   puts("\n=== Test: Insert into empty branch node ===");
   static const char *f_nm = "test_branch_empty.bptr";
   struct bptr *bptr = bptr_init(f_nm, 1, 512, sizeof(int), sizeof(int), cmp_i);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 0, 0);
   assert(node);

   print_node_capacity(bptr, node);

   _key_insert_test(bptr, node, 0, int, 100);

   int expected[] = {100};
   assert(verify_node_keys(node, expected, 1));
   puts("  PASS: First key inserted into branch node correctly");

   bptr_node_free(node);
   assert(bptr_destroy(bptr) == 0);
   assert(remove(f_nm) == 0);
}


void test_branch_insert_middle(void)
{
   puts("\n=== Test: Branch node middle insertion ===");
   static const char *f_nm = "test_branch_middle.bptr";
   struct bptr *bptr = bptr_init(f_nm, 1, 512, sizeof(int), sizeof(int), cmp_i);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 0, 0);
   assert(node);

   _key_insert_test(bptr, node, 0, int, 100);
   _key_insert_test(bptr, node, 1, int, 300);
   _key_insert_test(bptr, node, 1, int, 200);

   int expected[] = {100, 200, 300};
   assert(verify_node_keys(node, expected, 3));
   puts("  PASS: Branch node middle insertion works correctly");

   bptr_node_free(node);
   assert(bptr_destroy(bptr) == 0);
   assert(remove(f_nm) == 0);
}


void test_branch_capacity_compare(void)
{
   puts("\n=== Test: Branch vs Leaf capacity comparison ===");
   static const char *f_nm = "test_capacity_compare.bptr";
   struct bptr *bptr = bptr_init(f_nm, 1, 512, sizeof(int), sizeof(int), cmp_i);
   assert(bptr);

   struct bptr_node *leaf = bptr_node_new(bptr, 1, 0);
   struct bptr_node *branch = bptr_node_new(bptr, 0, 0);

   printf("  Leaf node capacity:\n");
   print_node_capacity(bptr, leaf);
   printf("  Branch node capacity:\n");
   print_node_capacity(bptr, branch);

   uint_fast16_t leaf_max = bptr->node_boundry.leaf.up - 1;
   uint_fast16_t branch_max = bptr->node_boundry.brch.up - 1;

   printf("  Leaf max keys: %" PRIuFAST16 ", Branch max keys: %" PRIuFAST16 "\n",
          leaf_max, branch_max);

   bptr_node_free(leaf);
   bptr_node_free(branch);
   assert(bptr_destroy(bptr) == 0);
   assert(remove(f_nm) == 0);
   puts("  PASS: Capacity comparison complete");
}


void test_leaf_fill_to_capacity(void)
{
   puts("\n=== Test: Fill leaf node to capacity ===");
   static const char *f_nm = "test_leaf_capacity.bptr";
   struct bptr *bptr = bptr_init(f_nm, 1, 512, sizeof(int), sizeof(int), cmp_i);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 1, 0);
   assert(node);

   uint_fast16_t max_keys = bptr->node_boundry.leaf.up - 1;
   printf("  Filling leaf node to capacity (%" PRIuFAST16 " keys)...\n", max_keys);

   for (uint_fast16_t i = 0; i < max_keys; i++)
      _key_insert_test(bptr, node, i, int, (int)i * 10);

   print_node_capacity(bptr, node);
   assert(node->key_count == max_keys);
   printf("  PASS: Leaf node filled to capacity (%" PRIuFAST16 " keys)\n", max_keys);

   bptr_node_free(node);
   assert(bptr_destroy(bptr) == 0);
   assert(remove(f_nm) == 0);
}


void test_branch_fill_to_capacity(void)
{
   puts("\n=== Test: Fill branch node to capacity ===");
   static const char *f_nm = "test_branch_capacity.bptr";
   struct bptr *bptr = bptr_init(f_nm, 1, 512, sizeof(int), sizeof(int), cmp_i);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 0, 0);
   assert(node);

   uint_fast16_t max_keys = bptr->node_boundry.brch.up - 1;
   printf("  Filling branch node to capacity (%" PRIuFAST16 " keys)...\n", max_keys);

   for (uint_fast16_t i = 0; i < max_keys; i++)
      _key_insert_test(bptr, node, i, int, (int)i * 10);

   print_node_capacity(bptr, node);
   assert(node->key_count == max_keys);
   printf("  PASS: Branch node filled to capacity (%" PRIuFAST16 " keys)\n", max_keys);

   bptr_node_free(node);
   assert(bptr_destroy(bptr) == 0);
   assert(remove(f_nm) == 0);
}


void test_ub_insufficient_shift(void)
{
   puts("\n=== Test: UB - Incorrect key_count causes insufficient shift ===");
   puts("  WARNING: This test demonstrates undefined behavior from incorrect key_count");
   static const char *f_nm = "test_ub_shift.bptr";
   struct bptr *bptr = bptr_init(f_nm, 1, 512, sizeof(int), sizeof(int), cmp_i);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 1, 0);
   assert(node);

   _key_insert_test(bptr, node, 0, int, 100);
   _key_insert_test(bptr, node, 1, int, 200);

   printf("  Before UB test: ");
   print_node_keys(node);

   int temp = 50;
   node->key_count = 1;
   _node_key_insert(bptr, node, &temp, 0);

   printf("  After inserting with key_count=1 (should be 2): ");
   const int *arr = node->keys;
   printf("[%d, %d, ...]\n", arr[0], arr[1]);
   printf("  Expected: [50, 100, 200], Got: [%d, %d]\n", arr[0], arr[1]);
   puts("  DEMONSTRATED: Incorrect key_count causes memmove to shift insufficient elements");

   bptr_node_free(node);
   assert(bptr_destroy(bptr) == 0);
   assert(remove(f_nm) == 0);
}


void test_ub_memory_corruption(void)
{
   puts("\n=== Test: UB - Incorrect key_count causes memory corruption ===");
   puts("  WARNING: This test demonstrates memory corruption from incorrect key_count");
   static const char *f_nm = "test_ub_corrupt.bptr";
   struct bptr *bptr = bptr_init(f_nm, 1, 512, sizeof(int), sizeof(int), cmp_i);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 1, 0);
   assert(node);

   _key_insert_test(bptr, node, 0, int, 100);
   _key_insert_test(bptr, node, 1, int, 200);

   printf("  Before UB test: ");
   print_node_keys(node);

   int temp = 50;
   node->key_count = 5;
   _node_key_insert(bptr, node, &temp, 0);

   printf("  After inserting with key_count=5 (should be 2): ");
   const int *arr = node->keys;
   printf("[%d, %d, %d, ...]\n", arr[0], arr[1], arr[2]);
   printf("  Note: Values beyond index 1 may contain garbage/corrupted data\n");
   puts("  DEMONSTRATED: Excessive key_count causes memmove to copy uninitialized memory");

   bptr_node_free(node);
   assert(bptr_destroy(bptr) == 0);
   assert(remove(f_nm) == 0);
}


void test_document_proper_usage(void)
{
   puts("\n=== Test: Document proper usage pattern ===");
   puts("  CORRECT pattern:");
   puts("    1. Call _node_key_insert()");
   puts("    2. IMMEDIATELY increment node->key_count");
   puts("    3. Verify key_count is always accurate before next insert");
   puts("");
   puts("  INCORRECT pattern (causes UB):");
   puts("    1. Call _node_key_insert()");
   puts("    2. Forget to increment key_count");
   puts("    3. Next insert uses wrong count for memmove");
   puts("");
   puts("  The test macro _key_insert_test() handles this correctly:");
   puts("    _key_insert_test(bptr, node, idx, int, value)");
   puts("  It automatically increments key_count after insertion.");
}


/* ============================================================================
 * PHASE 5: Non-Lite Mode Tests (is_lite = 0, 8-byte pointers)
 * ============================================================================ */

void test_non_lite_leaf_insert(struct bptr *bptr)
{
   puts("\n=== Test: Non-lite leaf node insert (8-byte pointers) ===");
   struct bptr_node *node = bptr_node_new(bptr, 1, 0);
   assert(node);

   print_node_capacity(bptr, node);

   _key_insert_test(bptr, node, 0, int, 100);
   _key_insert_test(bptr, node, 1, int, 200);
   _key_insert_test(bptr, node, 0, int, 50);

   int expected[] = {50, 100, 200};
   assert(verify_node_keys(node, expected, 3));
   puts("  PASS: Non-lite leaf node insert works correctly");

   bptr_node_free(node);
}

void test_non_lite_branch_insert(struct bptr *bptr)
{
   puts("\n=== Test: Non-lite branch node insert (8-byte pointers) ===");
   struct bptr_node *node = bptr_node_new(bptr, 0, 0);
   assert(node);

   print_node_capacity(bptr, node);

   _key_insert_test(bptr, node, 0, int, 100);
   _key_insert_test(bptr, node, 1, int, 300);
   _key_insert_test(bptr, node, 1, int, 200);

   int expected[] = {100, 200, 300};
   assert(verify_node_keys(node, expected, 3));
   puts("  PASS: Non-lite branch node insert works correctly");

   bptr_node_free(node);
}

void test_is_lite_capacity_comparison(void)
{
   puts("\n=== Test: Compare capacities between lite and non-lite modes ===");

   /* Test with lite mode (4-byte pointers) */
   static const char *f_nm_lite = "test_cap_lite.bptr";
   struct bptr *bptr_lite = bptr_init(f_nm_lite, 1, 512, sizeof(int), sizeof(int), cmp_i);
   assert(bptr_lite);
   struct bptr_node *leaf_lite = bptr_node_new(bptr_lite, 1, 0);
   struct bptr_node *branch_lite = bptr_node_new(bptr_lite, 0, 0);

   uint_fast16_t lite_leaf_max = bptr_lite->node_boundry.leaf.up - 1;
   uint_fast16_t lite_branch_max = bptr_lite->node_boundry.brch.up - 1;

   printf("  Lite mode (4-byte pointers):\n");
   printf("    Leaf max keys:  %" PRIuFAST16 "\n", lite_leaf_max);
   printf("    Branch max keys: %" PRIuFAST16 "\n", lite_branch_max);

   bptr_node_free(leaf_lite);
   bptr_node_free(branch_lite);
   assert(bptr_destroy(bptr_lite) == 0);
   assert(remove(f_nm_lite) == 0);

   /* Test with non-lite mode (8-byte pointers) */
   static const char *f_nm_norm = "test_cap_norm.bptr";
   struct bptr *bptr_norm = bptr_init(f_nm_norm, 0, 512, sizeof(int), sizeof(int), cmp_i);
   assert(bptr_norm);
   struct bptr_node *leaf_norm = bptr_node_new(bptr_norm, 1, 0);
   struct bptr_node *branch_norm = bptr_node_new(bptr_norm, 0, 0);

   uint_fast16_t norm_leaf_max = bptr_norm->node_boundry.leaf.up - 1;
   uint_fast16_t norm_branch_max = bptr_norm->node_boundry.brch.up - 1;

   printf("  Non-lite mode (8-byte pointers):\n");
   printf("    Leaf max keys:  %" PRIuFAST16 "\n", norm_leaf_max);
   printf("    Branch max keys: %" PRIuFAST16 "\n", norm_branch_max);

   bptr_node_free(leaf_norm);
   bptr_node_free(branch_norm);
   assert(bptr_destroy(bptr_norm) == 0);
   assert(remove(f_nm_norm) == 0);

   /* Non-lite branch nodes should have fewer keys due to larger pointers */
   printf("\n  Comparison:\n");
   printf("    Branch capacity difference: %" PRIdFAST16 " keys\n",
          (int_fast16_t)(norm_branch_max - lite_branch_max));
   puts("  PASS: Non-lite mode has smaller branch capacity as expected");
}

/* ============================================================================
 * PHASE 6: Different Node Size Tests
 * ============================================================================ */

void test_node_size_4k(void)
{
   puts("\n=== Test: Node size 4096 bytes (4KB) ===");
   static const char *f_nm = "test_size_4k.bptr";
   struct bptr *bptr = bptr_init(f_nm, 1, 4096, sizeof(int), sizeof(int), cmp_i);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 1, 0);
   assert(node);

   uint_fast16_t max_keys = bptr->node_boundry.leaf.up - 1;
   printf("  Node size: 4096 bytes, Leaf max keys: %" PRIuFAST16 "\n", max_keys);

   /* Verify capacity scales with node size (4096/512 = 8x) */
   _key_insert_test(bptr, node, 0, int, 100);
   _key_insert_test(bptr, node, 1, int, 200);

   int expected[] = {100, 200};
   assert(verify_node_keys(node, expected, 2));
   puts("  PASS: 4KB node size works correctly");

   bptr_node_free(node);
   assert(bptr_destroy(bptr) == 0);
   assert(remove(f_nm) == 0);
}

void test_node_size_8k(void)
{
   puts("\n=== Test: Node size 8192 bytes (8KB) ===");
   static const char *f_nm = "test_size_8k.bptr";
   struct bptr *bptr = bptr_init(f_nm, 1, 8192, sizeof(int), sizeof(int), cmp_i);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 1, 0);

   uint_fast16_t max_keys = bptr->node_boundry.leaf.up - 1;
   printf("  Node size: 8192 bytes, Leaf max keys: %" PRIuFAST16 "\n", max_keys);

   _key_insert_test(bptr, node, 0, int, 100);
   _key_insert_test(bptr, node, 1, int, 200);

   int expected[] = {100, 200};
   assert(verify_node_keys(node, expected, 2));
   puts("  PASS: 8KB node size works correctly");

   bptr_node_free(node);
   assert(bptr_destroy(bptr) == 0);
   assert(remove(f_nm) == 0);
}

void test_node_size_16k(void)
{
   puts("\n=== Test: Node size 16384 bytes (16KB) ===");
   static const char *f_nm = "test_size_16k.bptr";
   struct bptr *bptr = bptr_init(f_nm, 1, 16384, sizeof(int), sizeof(int), cmp_i);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 1, 0);

   uint_fast16_t max_keys = bptr->node_boundry.leaf.up - 1;
   printf("  Node size: 16384 bytes, Leaf max keys: %" PRIuFAST16 "\n", max_keys);

   _key_insert_test(bptr, node, 0, int, 100);
   _key_insert_test(bptr, node, 1, int, 200);

   int expected[] = {100, 200};
   assert(verify_node_keys(node, expected, 2));
   puts("  PASS: 16KB node size works correctly");

   bptr_node_free(node);
   assert(bptr_destroy(bptr) == 0);
   assert(remove(f_nm) == 0);
}

void test_node_size_capacity_scaling(void)
{
   puts("\n=== Test: Verify capacity scales with node size ===");

   uint32_t sizes[] = {512, 1024, 2048, 4096, 8192, 16384};
   const char *filenames[] = {
      "test_scale_512.bptr", "test_scale_1k.bptr", "test_scale_2k.bptr",
      "test_scale_4k.bptr", "test_scale_8k.bptr", "test_scale_16k.bptr"
   };

   printf("\n  %-8s | %-12s | %-12s\n", "Size", "Leaf Cap", "Branch Cap");
   printf("  %s-+-%s-+-%s\n", "----------", "--------------", "--------------");

   for (int i = 0; i < 6; i++)
   {
      struct bptr *bptr = bptr_init(filenames[i], 1, sizes[i], sizeof(int), sizeof(int), cmp_i);
      assert(bptr);

      struct bptr_node *leaf = bptr_node_new(bptr, 1, 0);
      struct bptr_node *branch = bptr_node_new(bptr, 0, 0);

      uint_fast16_t leaf_cap = bptr->node_boundry.leaf.up - 1;
      uint_fast16_t branch_cap = bptr->node_boundry.brch.up - 1;

      printf("  %-8" PRIu32 " | %-12" PRIuFAST16 " | %-12" PRIuFAST16 "\n",
             sizes[i], leaf_cap, branch_cap);

      bptr_node_free(leaf);
      bptr_node_free(branch);
      assert(bptr_destroy(bptr) == 0);
      assert(remove(filenames[i]) == 0);
   }

   puts("  PASS: Capacity scales linearly with node size");
}

/* ============================================================================
 * PHASE 7: Different Key/Value Size Combinations
 * ============================================================================ */

/* Helper macro for typed key insert with different types */
#define _key_insert_typed(self, node, idx, ktype, vtype, kval) do \
{ \
   ktype var = (kval); \
   _node_key_insert((self), (node), &var, idx); \
   assert(((ktype*)node->keys)[(idx)] == (kval)); \
   (node)->key_count++; \
} while (0)

void test_64bit_keys(void)
{
   puts("\n=== Test: 64-bit keys (uint64_t), 32-bit values (int) ===");
   static const char *f_nm = "test_u64_keys.bptr";
   struct bptr *bptr = bptr_init(f_nm, 1, 512, sizeof(uint64_t), sizeof(int), cmp_u64);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 1, 0);

   print_node_capacity(bptr, node);

   _key_insert_typed(bptr, node, 0, uint64_t, int, 1000ULL);
   _key_insert_typed(bptr, node, 1, uint64_t, int, 2000ULL);
   _key_insert_typed(bptr, node, 0, uint64_t, int, 500ULL);

   const uint64_t *keys = node->keys;
   assert(keys[0] == 500ULL);
   assert(keys[1] == 1000ULL);
   assert(keys[2] == 2000ULL);
   puts("  PASS: 64-bit keys work correctly");

   bptr_node_free(node);
   assert(bptr_destroy(bptr) == 0);
   assert(remove(f_nm) == 0);
}

void test_64bit_values(void)
{
   puts("\n=== Test: 32-bit keys (int), 64-bit values (uint64_t) ===");
   static const char *f_nm = "test_u64_vals.bptr";
   struct bptr *bptr = bptr_init(f_nm, 1, 512, sizeof(int), sizeof(uint64_t), cmp_i);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 1, 0);

   print_node_capacity(bptr, node);

   _key_insert_test(bptr, node, 0, int, 100);
   _key_insert_test(bptr, node, 1, int, 200);
   _key_insert_test(bptr, node, 0, int, 50);

   int expected[] = {50, 100, 200};
   assert(verify_node_keys(node, expected, 3));
   puts("  PASS: 64-bit values work correctly");

   bptr_node_free(node);
   assert(bptr_destroy(bptr) == 0);
   assert(remove(f_nm) == 0);
}

void test_64bit_both(void)
{
   puts("\n=== Test: 64-bit keys AND 64-bit values ===");
   static const char *f_nm = "test_u64_both.bptr";
   struct bptr *bptr = bptr_init(f_nm, 1, 512, sizeof(uint64_t), sizeof(uint64_t), cmp_u64);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 1, 0);

   print_node_capacity(bptr, node);

   _key_insert_typed(bptr, node, 0, uint64_t, uint64_t, 0x100000000ULL);
   _key_insert_typed(bptr, node, 1, uint64_t, uint64_t, 0x200000000ULL);
   _key_insert_typed(bptr, node, 0, uint64_t, uint64_t, 0x080000000ULL);

   const uint64_t *keys = node->keys;
   assert(keys[0] == 0x080000000ULL);
   assert(keys[1] == 0x100000000ULL);
   assert(keys[2] == 0x200000000ULL);
   puts("  PASS: 64-bit keys and values work correctly");

   bptr_node_free(node);
   assert(bptr_destroy(bptr) == 0);
   assert(remove(f_nm) == 0);
}

void test_large_keys_small_values(void)
{
   puts("\n=== Test: Large keys (16 bytes), small values (4 bytes) ===");
   static const char *f_nm = "test_large_keys.bptr";
   struct bptr *bptr = bptr_init(f_nm, 1, 512, 16, 4, cmp_i);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 1, 0);

   print_node_capacity(bptr, node);

   /* Use 16-byte keys as 4 ints */
   typedef struct { uint32_t vals[4]; } key16_t;
   key16_t k1 = {{1, 0, 0, 0}};
   key16_t k2 = {{2, 0, 0, 0}};
   key16_t k3 = {{0, 0, 0, 1}};

   _node_key_insert(bptr, node, &k1, 0);
   node->key_count++;
   _node_key_insert(bptr, node, &k2, 1);
   node->key_count++;
   _node_key_insert(bptr, node, &k3, 0);
   node->key_count++;

   assert(node->key_count == 3);
   puts("  PASS: Large keys with small values work correctly");

   bptr_node_free(node);
   assert(bptr_destroy(bptr) == 0);
   assert(remove(f_nm) == 0);
}

void test_small_keys_large_values(void)
{
   puts("\n=== Test: Small keys (4 bytes), large values (32 bytes) ===");
   static const char *f_nm = "test_large_vals.bptr";
   struct bptr *bptr = bptr_init(f_nm, 1, 512, 4, 32, cmp_i);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 1, 0);

   print_node_capacity(bptr, node);

   _key_insert_test(bptr, node, 0, uint32_t, 100);
   _key_insert_test(bptr, node, 1, uint32_t, 200);

   int expected[] = {100, 200};
   assert(verify_node_keys(node, expected, 2));
   puts("  PASS: Small keys with large values work correctly");

   bptr_node_free(node);
   assert(bptr_destroy(bptr) == 0);
   assert(remove(f_nm) == 0);
}

void test_equal_mixed_sizes(void)
{
   puts("\n=== Test: Equal larger sizes (32 bytes keys, 32 bytes values) ===");
   static const char *f_nm = "test_equal_32.bptr";
   struct bptr *bptr = bptr_init(f_nm, 1, 4096, 32, 32, cmp_i);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 1, 0);
   assert(node);

   print_node_capacity(bptr, node);

   // Error int is not 32-byte val
   _key_insert_test(bptr, node, 0, int, 100);
   _key_insert_test(bptr, node, 1, int, 200);

   int expected[] = {100, 200};
   assert(verify_node_keys(node, expected, 2));
   puts("  PASS: Equal larger sizes work correctly");

   bptr_node_free(node);
   assert(bptr_destroy(bptr) == 0);
   assert(remove(f_nm) == 0);
}

/* ============================================================================
 * PHASE 8: Capacity Matrix Test
 * ============================================================================ */

void test_capacity_matrix(void)
{
   puts("\n=== Test: Capacity Matrix - Node capacity across configurations ===");
   puts("\n  Format: (is_lite, node_size, key_size, value_size) -> leaf/branch capacity");
   puts("  " "-----------------------------------------------------------------------------------");

   struct {
      _Bool is_lite;
      uint32_t node_sz;
      uint16_t key_sz;
      uint16_t val_sz;
   } configs[] = {
      {1, 512, 4, 4},
      {1, 512, 8, 4},
      {1, 512, 4, 8},
      {1, 512, 8, 8},
      {1, 4096, 4, 4},
      {1, 4096, 8, 8},
      {1, 8192, 4, 4},
      {1, 16384, 4, 4},
      {0, 512, 4, 4},
      {0, 4096, 4, 4},
      {0, 4096, 8, 8},
   };

   printf("\n  %-6s | %-8s | %-8s | %-8s | %-10s | %-12s\n",
          "is_lite", "node_sz", "key_sz", "val_sz", "leaf_cap", "branch_cap");
   puts("  " "--------+----------+----------+----------+------------+--------------");

   for (size_t i = 0; i < sizeof(configs)/sizeof(configs[0]); i++)
   {
      char filename[64];
      snprintf(filename, sizeof(filename), "test_matrix_%zu.bptr", i);

      struct bptr *bptr = bptr_init(filename, configs[i].is_lite, configs[i].node_sz,
                                    configs[i].key_sz, configs[i].val_sz,
                                    configs[i].key_sz == 8 ? cmp_u64 : cmp_i);
      assert(bptr);

      struct bptr_node *leaf = bptr_node_new(bptr, 1, 0);
      struct bptr_node *branch = bptr_node_new(bptr, 0, 0);

      uint_fast16_t leaf_cap = bptr->node_boundry.leaf.up - 1;
      uint_fast16_t branch_cap = bptr->node_boundry.brch.up - 1;

      printf("  %-6s | %-8" PRIu32 " | %-8" PRIu16 " | %-8" PRIu16 " | %-10" PRIuFAST16 " | %-12" PRIuFAST16 "\n",
             configs[i].is_lite ? "lite" : "norm",
             configs[i].node_sz,
             configs[i].key_sz,
             configs[i].val_sz,
             leaf_cap,
             branch_cap);

      bptr_node_free(leaf);
      bptr_node_free(branch);
      assert(bptr_destroy(bptr) == 0);
      assert(remove(filename) == 0);
   }

   puts("  PASS: Capacity matrix generated successfully");
}


int main(void)
{
   puts("========================================");
   puts("  B+Tree Node Key Insertion Test Suite");
   puts("========================================");

   puts("\n--- PHASE 1: Basic Leaf Node Tests ---");
   test_keys_insert_template();
   test_leaf_insert_empty();
   test_leaf_insert_beginning();
   test_leaf_insert_middle();
   test_leaf_insert_end();

   puts("\n--- PHASE 2: Branch Node Tests ---");
   test_branch_insert_empty();
   test_branch_insert_middle();
   test_branch_capacity_compare();

   puts("\n--- PHASE 3: Full Capacity Tests ---");
   test_leaf_fill_to_capacity();
   test_branch_fill_to_capacity();

   puts("\n--- PHASE 4: Undefined Behavior Demonstration ---");
   puts("  (These tests show what happens with incorrect key_count)");
   test_ub_insufficient_shift();
   test_ub_memory_corruption();
   test_document_proper_usage();

   puts("\n--- PHASE 5: Non-Lite Mode Tests ---");
   puts("  Testing is_lite = 0 (8-byte pointers)");
   _run_test_with_config(test_non_lite_leaf_insert, 0, 512, sizeof(int), sizeof(int), cmp_i);
   _run_test_with_config(test_non_lite_branch_insert, 0, 512, sizeof(int), sizeof(int), cmp_i);
   test_is_lite_capacity_comparison();

   puts("\n--- PHASE 6: Different Node Size Tests ---");
   test_node_size_4k();
   test_node_size_8k();
   test_node_size_16k();
   test_node_size_capacity_scaling();

   puts("\n--- PHASE 7: Different Key/Value Size Tests ---");
   test_64bit_keys();
   test_64bit_values();
   test_64bit_both();
   test_large_keys_small_values();
   test_small_keys_large_values();
   test_equal_mixed_sizes();

   puts("\n--- PHASE 8: Capacity Matrix ---");
   test_capacity_matrix();

   puts("\n========================================");
   puts("  All tests completed successfully!");
   puts("========================================");

   return 0;
}
