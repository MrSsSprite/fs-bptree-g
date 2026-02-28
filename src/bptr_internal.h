#ifndef BPTR_INTERNAL_H
#define BPTR_INTERNAL_H

/*----------------------------- Public Includes ------------------------------*/
#include <stdio.h>
#include <stdint.h>
#include "../bptree.h"
/*--------------------------- Public Includes END ----------------------------*/

/*------------------------------ Public Defines ------------------------------*/
#define BPTR_NODE_METADATA_BYTE  64
#define BPTR_LITE_PTR_BYTE       4
#define BPTR_LITE_PTR_TYPE       uint32_t
#define BPTR_NORM_PTR_BYTE       8
#define BPTR_NORM_PTR_TYPE       uint64_t
#define BPTR_MAGIC_STR           "BPTR"
// Temporarily hardcoded in dev stage
// Default block size
#define BPTR_BLOCK_BYTE          512
/*---------------------------- Public Defines END ----------------------------*/


/*----------------------------- Public Typedefs ------------------------------*/
typedef uint_fast64_t bptr_node_t;
/*--------------------------- Public Typedefs END ----------------------------*/

/*------------------------------ Public Macros -------------------------------*/
#define BPTR_PTR_SIZE (self->is_lite ? BPTR_LITE_PTR_BYTE : BPTR_NORM_PTR_BYTE)
/*---------------------------- Public Macros END -----------------------------*/

/*----------------------------- Public Structs ------------------------------*/
struct bptr
{
   /* File IO */
   FILE *file;
   void *fbuf;

   /* ファイル識別情報 */
   uint_least32_t version;
   _Bool is_lite;
   uint_fast32_t block_size;
   
   /* 木構造 */
   bptr_node_t root_idx;
   uint_fast32_t node_size;
   /* boundry value that just EXCEED the max/min */
   struct
    {
      struct _bptr_node_boundry
       { uint_fast32_t low, up, t_value; }
      leaf, brch;
    }
   node_boundry;

   /* データ型 */
   uint_fast8_t key_type;
   uint_fast16_t value_size;

   /* メモリ管理 */
   struct
    {
      uint_fast64_t head, size;
    }
   free_list;

   /* 統計情報 */
   uint_fast64_t record_cnt;
   uint_fast64_t node_cnt;
   uint_fast32_t height;

   int (*compare)(const void *lhs, const void *rhs);
};
/*---------------------------- Public Structs END ----------------------------*/

/*------------------------ Public Function Prototypes ------------------------*/
/*---------------------- Public Function Prototypes END ----------------------*/

/*------------------------ Public Function Definition ------------------------*/
// Calculate key size in bytes
static inline uint_fast16_t _bptr_key_size(uint8_t key_type)
{
   switch (key_type)
    {
   case BPTR_KYTP_I8:
   case BPTR_KYTP_U8:
      return 1;
   case BPTR_KYTP_I16:
   case BPTR_KYTP_U16:
      return 2;
   case BPTR_KYTP_I32:
   case BPTR_KYTP_U32:
      return 4;
   case BPTR_KYTP_I64:
   case BPTR_KYTP_U64:
      return 8;
    }

   return 0;
}
/*---------------------- Public Function Definition END ----------------------*/

#endif
