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
int bptr_io_fcreat(struct bptr *this, const char *filename);
/**
 * @brief   load an existing B+tree from a file
 * @return  0 on success. Otherwise, the error code is returned.
 */
int bptr_io_fload(struct bptr *this, const char *filename);
/**
 * @brief   close the file and clean up file-related resources
 * @return  0 on success. Otherwise, the error code is returned.
 */
int bptr_io_fclose(struct bptr *this);
int bptr_io_fread_node(struct bptr *this, bptr_node_t node_idx);
bptr_node_t bptr_io_flush_node(struct bptr *this, bptr_node_t node_idx);
/*--------------------------- Public Functions END ---------------------------*/

#endif
