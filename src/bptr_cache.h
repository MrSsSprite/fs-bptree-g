#ifndef __BPTR_CACHE_H
#define __BPTR_CACHE_H

/*----------------------------- Public Includes ------------------------------*/
#include <stdint.h>
#include "bptr_internal.h"
/*--------------------------- Public Includes END ----------------------------*/

/*----------------------------- Public Functions -----------------------------*/
int bptr_cache_init(struct bptr *self, uint64_t pool_cap);
int bptr_cache_deinit(struct bptr *self);
/*--------------------------- Public Functions END ---------------------------*/

#endif
