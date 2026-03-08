#include <stdio.h>
#include <assert.h>
#include <inttypes.h>

#include "../src/bptr_internal.h"
#include "../bptree.h"
#include "../src/bptr_node.c"


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


void test_insert_middle(void)
{
   int val;
   static const char *f_nm = "test_node_insert.bptr";
   struct bptr *bptr = bptr_init(f_nm, 1, 512,
                                 sizeof(int), sizeof(int), cmp_i);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, 1, 0);
   assert(node);

   val = 123;
   printf("Key count: %" PRIu32 ", Key max: %" PRIuFAST16 "\n", node->key_count, bptr->node_boundry.leaf.up - 1);
   _node_key_insert(bptr, node, &val, 0);
   assert(((int*)node->keys)[0] == 123);
   node->key_count++;

   val = 789;
   _node_key_insert(bptr, node, &val, 1);
   assert(((int*)node->keys)[1] == 789);
   node->key_count++;

   val = 321;
   _node_key_insert(bptr, node, &val, 0);
   assert(((int*)node->keys)[0] == 321);
   node->key_count++;

   val = 456;
   _node_key_insert(bptr, node, &val, 2);
   assert(((int*)node->keys)[2] == 456);
   node->key_count++;

   print_node_keys(node);

   bptr_node_free(node);
   assert(bptr_destroy(bptr) == 0);
   assert(remove(f_nm) == 0);
}


int main(void)
{
   test_insert_middle();

   return 0;
}
