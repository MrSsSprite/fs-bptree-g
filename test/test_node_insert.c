#include <stdio.h>
#include <assert.h>
#include <inttypes.h>

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


int cmp_i(const void *lhs, const void *rhs)
{ return *(const int*)lhs - *(const int *)rhs; }


void print_node_keys(struct bptr_node *node)
{
   const int *arr = node->keys;
   putchar('[');
   for (uint_fast32_t i = 0; i < node->key_count; i++)
      printf("%d, ", arr[i]);
   puts("]");
}


void test_keys_insert(void)
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


int main(void)
{
   test_keys_insert();

   return 0;
}
