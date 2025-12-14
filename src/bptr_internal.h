#ifndef BPTR_INTERNAL_H
#define BPTR_INTERNAL_H

/*----------------------------- Public Includes ------------------------------*/
#include <stdio.h>
#include <stdint.h>
/*--------------------------- Public Includes END ----------------------------*/

/*------------------------------ Public Defines ------------------------------*/
#define BPTR_NODE_METADATA_BYTE  64
#define BPTR_LITE_PTR_BYTE       4
#define BPTR_LITE_PTR_TYPE       uint32_t
#define BPTR_NORM_PTR_BYTE       8
#define BPTR_NORM_PTR_TYPE       uint64_t
#define BPTR_MAGIC_STR           "BPTR"
#define BPTR_BLOCK_BYTE          512
/*---------------------------- Public Defines END ----------------------------*/

/*----------------------------- Public Structs ------------------------------*/
struct bptree
{
   /* File IO */
   FILE *file;

   /* ファイル識別情報 */
   uint_least32_t version;
   _Bool is_lite;
   uint_fast32_t block_size;
   
   /* 木構造 */
   uint_fast64_t root_pointer;
   uint_fast32_t node_size;
   uint_fast8_t ptr_sz;
   /* boundry value that just EXCEED the max/min */
   struct
    {
      struct _bptr_node_boundry
       {
         uint_fast16_t low, up;
         uint_fast16_t t_value;
       }
      leaf, internal;
    }
   node_boundry;

   /* データ型 */
   struct
    {
       uint_fast8_t key_type;
       uint_fast16_t value_size;
    }
   data_info;

   /* メモリ管理 */
   struct
    {
      uint_fast64_t head, size;
    }
   free_list;

   /* 統計情報 */
   struct
    {
       uint_fast64_t record_count;
       uint_fast64_t node_count;
       uint_fast32_t tree_height;
    }
   stats;
};
/*---------------------------- Public Structs END ----------------------------*/

#endif
