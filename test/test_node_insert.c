#include "test_node_insert.h"


int cmp_i(const void *lhs, const void *rhs)
{ return *(const int*)lhs - *(const int *)rhs; }

/* Comparator for uint64_t keys/values */
int cmp_u64(const void *lhs, const void *rhs)
{
   return *(const uint64_t*)lhs < *(const uint64_t*)rhs ? -1 :
          *(const uint64_t*)lhs > *(const uint64_t*)rhs ? 1 : 0;
}

/* Comparator for key32_t (32-byte keys) */
int cmp_key32(const void *lhs, const void *rhs)
{
   return memcmp(lhs, rhs, sizeof(key32_t));
}
