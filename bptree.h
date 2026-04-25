#ifndef BPTREE_H
#define BPTREE_H

/*------------------------------ Public Include ------------------------------*/
#include <stdint.h>
/*---------------------------- Public Include END ----------------------------*/

/*------------------------------ Public Define -------------------------------*/
#define BPTR_CURRENT_VERSION (0u)

// Error Codes
#define BPTR_E_SUCCESS     (0)
// Sytem & Program-side Errors (Positive)
#define BPTR_E_OOM         (1u)      // OOM: Out Of Memory
#define BPTR_E_FACCESS     (2u)      // File Access Error
#define BPTR_E_FCLOSE      (3u)      // Failed at fclose
#define BPTR_E_UNREACHABLE (0xDEADu) // Unreachable (57005 dec.)
#define BPTR_E_TODO        (0xBEEFu) // Not yet implemented (48879 dec.)
// User & Argument Errors (Negative)
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
   int (*compare)(const void *lhs, const void *rhs)
);
struct bptr *bptr_load(const char *filename,
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
 * @return  execution status
 * @retval  0(BPTR_E_SUCCESS) Success
 * @retval  BPTR_E_FCLOSE     Failed at fclose
 */
int bptr_unload(struct bptr *self);
/*--------------------------- Public Functions END ---------------------------*/

#endif
