#ifndef BPTREE_H
#define BPTREE_H

/*------------------------------ Public Include ------------------------------*/
#include <stdint.h>
/*---------------------------- Public Include END ----------------------------*/

/*------------------------------ Public Define -------------------------------*/
#define BPTR_CURRENT_VERSION (0u)

#define BPTR_CACHE_CAPACITY_MIN  (2u)

// Error Codes
#define BPTR_E_SUCCESS     (0)
// Sytem & Program-side Errors (Positive)
#define BPTR_E_OOM         (1)      // OOM: Out Of Memory
#define BPTR_E_FACCESS     (2)      // File Access Error
#define BPTR_E_FCLOSE      (3)      // Failed at fclose
#define BPTR_E_ITRNL_STATE (4)      // Invalid Internal State of struct bptr
#define BPTR_E_CACHE_FULL  (5)      // Cache pool full and no eviction possible
#define BPTR_E_FSEEK       (6)      // Failed at fseek (errno is set)
#define BPTR_E_UNREACHABLE (0xDEAD) // Unreachable (57005 dec.)
#define BPTR_E_TODO        (0xBEEF) // Not yet implemented (48879 dec.)
// User & Argument Errors (Negative)
#define BPTR_E_FN_INPUT    (-1)
#define BPTR_E_GT_MAXSIZE  (-2)
#define BPTR_E_NOT_FOUND   (-3)     // Result Not Found
#define BPTR_E_KEY_EXIST   (-4)     // Key already exist in the B+Tree
/*---------------------------- Public Define END -----------------------------*/

/*------------------------ Public Struct Declaration -------------------------*/
struct bptr;
/*---------------------- Public Struct Declaration END -----------------------*/

/*----------------------- Public Variable Declaration ------------------------*/
extern int bptr_errno;
/*--------------------- Public Variable Declaration END ----------------------*/
/*----------------------------- Public Functions -----------------------------*/
struct bptr *bptr_init
(
   const char *filename,
   _Bool is_lite,
   uint32_t node_size,
   uint16_t key_size,
   uint16_t value_size,
   uint64_t cache_capacity,
   int (*compare)(const void *lhs, const void *rhs)
);
struct bptr *bptr_load(const char *filename, uint64_t cache_capacity,
                       int (*compare)(const void *lhs, const void *rhs));
int bptr_insert(struct bptr *self, const void *key, const void *value);
int bptr_erase(struct bptr *self, const void *key);
const void *bptr_find(struct bptr *self, const void *key);
int bptr_find_range(struct bptr *self, const void *bg, const void *ed,
                    void **res_it);
/**
 * @brief   unload the bptr obj.
 *
 * Unload the bptr object and flush the content.
 *
 * @param[in,out] self  bptr obj. to be unloaded
 * @return  execution result
 * @retval  0(BPTR_E_SUCCESS) Success
 * @retval  BPTR_E_FCLOSE     Failed at fclose
 * @retval  other             Failed on cache deinit.
 *
 * @note    If any error except @c BPTR_E_FCLOSE is captured, @c self->file
 *          remains validly opened.
 * @remark  If @c BPTR_E_FCLOSE is captured, any further access to @c self->file
 *          leads to undefined behavior.
 */
int bptr_unload(struct bptr *self);
/*--------------------------- Public Functions END ---------------------------*/

#endif
