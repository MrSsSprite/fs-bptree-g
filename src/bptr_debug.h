#ifndef BPTR_DEBUG_H
#define BPTR_DEBUG_H

/*----------------------------- Public Includes ------------------------------*/
#include <stdint.h>
#include <stdio.h>
#include "../bptree.h"
/*--------------------------- Public Includes END ----------------------------*/

/*---------------------------- Public Functions -----------------------------*/
/**
 * @brief   Print all members of a bptree handler for debugging purposes
 * @param   self Pointer to the bptree handler to dump
 * @param   stream Output stream (e.g., stdout, stderr, or a file)
 * @return  0 on success, -1 if self or stream is NULL
 *
 * This function prints a comprehensive view of the bptree structure including:
 * - File IO members (file pointer, buffer)
 * - File identification (version, is_lite flag, block_size)
 * - Tree structure (root_pointer, node_size, node boundaries)
 * - Data type information (key_type with name, value_size)
 * - Memory management (free_list head and size)
 * - Statistics (record_count, node_count, tree_height)
 */
int bptr_dump_handler(const struct bptree *self, FILE *stream);
/*--------------------------- Public Functions END ---------------------------*/

#endif
