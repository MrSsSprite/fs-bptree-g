#include <stdio.h>
#include <assert.h>
#include <inttypes.h>

#include "../src/bptr_internal.h"
#include "../bptree.h"
#include "../src/bptr_node.c"


int cmp_i(const void *lhs, const void *rhs)
{ return *(const int*)lhs - *(const int *)rhs; }


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
   printf("Key count: %" PRIu32 "\n", node->key_count);
   _node_key_insert(bptr, node, &val, 0);
   assert(((int*)node->keys)[0] == 123);

   bptr_node_free(node);
   assert(bptr_destroy(bptr) == 0);
   assert(remove(f_nm) == 0);
}


int main(void)
{
   test_insert_middle();

   return 0;
}
