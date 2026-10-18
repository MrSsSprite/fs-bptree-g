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

/**
 * @brief   Search result identifying a leaf and a key position within it.
 *
 * Returned by bptr_find_node() for either an existing key or its insertion
 * position.
 *
 * @warning The result is usable only when `node` is not NULL; otherwise,
 *          ignore `is_found` and `idx` and inspect @c bptr_errno for the cause.
 *
 * @see     bptr_find_node()
 */
struct bptr_find_res
{
   _Bool is_found;         ///< Whether target key is found.
   struct bptr_node *node; ///< Target leaf or NULL on error
   bptr_node_ki_t idx;     ///< Insertion position of the @p key
};
/*---------------------------- Public Structs END ----------------------------*/

/*------------------------ Public Function Prototypes ------------------------*/
/**
 * @brief   Find the leaf and key index for a match or insertion.
 *
 * Search @p self for @p key using the tree's comparison function. If the key
 * is absent, locate its insertion position within the target leaf. This
 * function does not insert a key or create a node for an empty tree.
 *
 * @param[in,out] self  Initialized B+Tree instance whose cache is used during
 *                      the search.
 * @param[in]     key   Target key in the representation expected by the tree's
 *                      comparison function.
 *
 * @return  A bptr_find_res with the following members on success:
 *          - `node`: The target leaf, never @c NULL on success. Caller is
 *                    responsible for releasing `node` (through
 *                    `bptr_node_unload()`) if `node` is not @c NULL .
 *          - `is_found`: 1 if the key exists, or 0 if it is absent.
 *          - `idx`: The zero-based index of the matching key, or, if absent,
 *                   the index of the first key greater than @p key in this
 *                   leaf. If all keys in the leaf are smaller, this is
 *                   `node->key_count`, indicating an insertion after the last
 *                   key.
 *
 * @remark  On success, the caller must release the returned leaf with
 *          bptr_node_unload() when it is no longer needed.
 *
 * @note    If no usable position is obtained, `node` is NULL and the other
 *          members must be ignored. An empty tree sets @c bptr_errno to
 *          @c BPTR_E_EMPTY_TREE; a node-fetch failure sets it to the
 *          corresponding error code. A missing key in a nonempty tree is a
 *          successful search for an insertion position.
 * @note    Determine success from `node`. The function does not guarantee
 *          that @c bptr_errno is cleared on success.
 *
 * @see     bptr_find_res, bptr_node_unload()
 */
struct bptr_find_res bptr_find_node(struct bptr *self, const void *key);
/*---------------------- Public Function Prototypes END ----------------------*/

#endif
