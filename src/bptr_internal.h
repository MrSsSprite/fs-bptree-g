#ifndef BPTR_INTERNAL_H
#define BPTR_INTERNAL_H

/*----------------------------- Public Includes ------------------------------*/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
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
#define BPTR_NODE_BYTE_DEFAULT   512
/*---------------------------- Public Defines END ----------------------------*/


/*----------------------------- Public Typedefs ------------------------------*/
typedef uint_fast64_t bptr_node_t;
/*--------------------------- Public Typedefs END ----------------------------*/

/*------------------------------ Public Macros -------------------------------*/
#define BPTR_PTR_SIZE (self->is_lite ? BPTR_LITE_PTR_BYTE : BPTR_NORM_PTR_BYTE)

#if defined(__has_builtin)
    #if __has_builtin(__builtin_unreachable)
        #define BPTR_UNREACHABLE() __builtin_unreachable()
    #else
        #define BPTR_UNREACHABLE() exit(BPTR_E_UNREACHABLE)
    #endif
#elif defined(_MSC_VER)
    /* MSVC equivalent */
    #define BPTR_UNREACHABLE() __assume(0)
#elif defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5))
    /* Older GCC (4.5+) doesn't always have __has_builtin but has the intrinsic */
    #define BPTR_UNREACHABLE() __builtin_unreachable()
#else
    /* Fallback for everyone else */
    #define BPTR_UNREACHABLE() exit(BPTR_E_UNREACHABLE)
#endif
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

   /* 木構造 */
   bptr_node_t root_idx;
   uint_fast32_t node_size;
   /* boundry value that just EXCEED the max/min */
   struct
    {
      struct _bptr_node_boundry
       { uint_fast16_t low, up; }
      leaf, brch;
    }
   node_boundry;

   /* データ型 */
   uint_fast16_t key_size;
   uint_fast16_t value_size;

   /* メモリ管理 */
   struct _bptr_free_list
    { bptr_node_t head, cnt; }
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

#endif
