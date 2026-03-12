#include "test_node_insert.h"
#include "../src/bptr_node.c"


bptr_config
default_cfg = { "default_config.bptr", 1, 1, 512, INT32_T, INT32_T, cmp_i };


void test_leaf_insert_empty(bptr_config *cfg)
{
   puts("\n=== Test: Insert into empty leaf node ===");
   struct bptr *bptr = bptr_init(cfg->f_nm, cfg->is_lite, cfg->node_sz,
                                 data_size(cfg->key_type),
                                 data_size(cfg->val_type), cfg->compare);
   assert(bptr);
   struct bptr_node *node = bptr_node_new(bptr, cfg->is_leaf, 0);
   assert(node);

   int32_t val = 123;
   _node_val_insert(bptr, node, &val, 0);
   assert(((int32_t*)node->vals)[0] == 123);
   puts("\tPASS: Works correctly on default config");
   remove(cfg->f_nm);
}


int main(void)
{
   test_leaf_insert_empty(&default_cfg);

   return 0;
}
