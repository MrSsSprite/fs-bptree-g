#include <stdio.h>
#include <assert.h>

#include "../src/bptr_internal.h"
#include "../bptree.h"
#include "../src/bptr_node.c"


int cmp_i(const void *lhs, const void *rhs)
{ return *(const int*)lhs - *(const int *)rhs; }


void test_insert_middle(void)
{
   int val;
   struct bptr *bptr = bptr_init("test_node_insert.bptr", 1, 512,
                                 sizeof(int), sizeof(int), cmp_i);
   struct bptr_node *node = bptr_node_new(bptr, 1, 0);

   assert(bptr);
   assert(node);
   val = 0;
   _node_key_insert(bptr, node, &val, 0);
}


int main(void)
{
   test_insert_middle();

   return 0;
}
