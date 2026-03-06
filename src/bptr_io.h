#ifndef BPTR_IO_H
#define BPTR_IO_H

/*----------------------------- Public Includes ------------------------------*/
#include "bptr_internal.h"
/*--------------------------- Public Includes END ----------------------------*/

/*------------------------------ Public Macros -------------------------------*/
#define _FILE_OFFSET_BITS 64  /* Ensures off_t is 64-bit on Linux/Unix */
#include <stdio.h>
#include <stdint.h>

/* Simple cross-platform wrapper for 64-bit seeking */
#ifdef _WIN32
    #define fseek64 _fseeki64
    #define ftell64 _ftelli64
    typedef __int64 bptr_off_t;
#else
    #define fseek64 fseeko
    #define ftell64 ftello
    typedef off_t bptr_off_t;
#endif
/*---------------------------- Public Macros END -----------------------------*/

/*----------------------------- Public Functions -----------------------------*/
/**
 * @brief   create and initialize a new file for a new B+tree
 * @return  0 on success. Otherwise, the error code is returned.
 */
int bptr_io_fcreat(struct bptr *self, const char *filename);
/**
 * @brief   load an existing B+tree from a file
 * @return  0 on success. Otherwise, the error code is returned.
 */
int bptr_io_fload(struct bptr *self, const char *filename);
/**
 * @brief   close the file and clean up file-related resources
 * @return  0 on success. Otherwise, the error code is returned.
 */
int bptr_io_fclose(struct bptr *self);
int bptr_io_fread_node(struct bptr *self, bptr_node_t node_idx);
bptr_node_t bptr_io_flush_node(struct bptr *self, bptr_node_t node_idx);
/*--------------------------- Public Functions END ---------------------------*/

#endif
