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
int bptr_fcreat(struct bptree *self, const char *filename);
/**
 * @brief   load an existing B+tree from a file
 * @return  0 on success. Otherwise, the error code is returned.
 */
int bptr_fload(struct bptree *self, const char *filename);
/**
 * @brief   close the file and clean up file-related resources
 * @return  0 on success. Otherwise, the error code is returned.
 */
int bptr_fclose(struct bptree *self);
int bptr_fread_node(struct bptree *self, bptr_node_t node_idx);
bptr_node_t bptr_flush_node(struct bptree *self, bptr_node_t node_idx);
/*--------------------------- Public Functions END ---------------------------*/

#endif
