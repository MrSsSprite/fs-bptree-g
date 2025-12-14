#ifndef BPTR_IO_H
#define BPTR_IO_H

/*----------------------------- Public Includes ------------------------------*/
#include "bptr_internal.h"
/*--------------------------- Public Includes END ----------------------------*/

/*----------------------------- Public Functions -----------------------------*/
/**
 * @brief   create and initialize a new file for a new B+tree
 * @return  0 on success. Otherwise, the error code is returned.
 */
int bptr_fcreat(struct bptree *this, const char *filename);
/*--------------------------- Public Functions END ---------------------------*/

#endif
