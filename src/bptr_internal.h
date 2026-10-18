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

#ifdef BPTR_TESTING
#define BPTR_STATIC
#else
#define BPTR_STATIC static
#endif
/*---------------------------- Public Defines END ----------------------------*/


/*----------------------------- Public Typedefs ------------------------------*/
typedef uint_fast64_t bptr_node_t;
typedef uint32_t bptr_node_ki_t;
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

   struct bptr_cache *cache;

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
       { uint_fast32_t low, up; }
      leaf, brch;
    }
   node_bound;

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

struct node_idx_pair
{
   _Bool is_found;
   struct bptr_node *node;
   bptr_node_ki_t idx;
};
/*---------------------------- Public Structs END ----------------------------*/

/*------------------------ Public Function Prototypes ------------------------*/
/**
 * @brief   Find insertion point for the given @p key .
 *
 * Search @p self for the insertion point for @p key . This function returns a
 * @c struct to represent the result. The result is @b NOT valid if @c node is
 * @c NULL ; is such scenario, there was an error occurred during program run,
 * and @c bptr_errno is set to indicate what erroneous condition was encounted.
 *
 * @param[in]  self     B+Tree instance.
 * @param[in]  key      target key
 *
 * @return  Search result. It's valid iff either @c node is not @c NULL .
 *
 * @see  struct node_idx_pair
 */
struct node_idx_pair bptr_find_node(struct bptr *self, const void *key);
/*---------------------- Public Function Prototypes END ----------------------*/

#endif
