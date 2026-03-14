#ifndef TEST_NODE_INSERT_H
#define TEST_NODE_INSERT_H

/*----------------------------- Public Includes ------------------------------*/
#include <stdio.h>
#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

#include "../src/bptr_internal.h"
#include "../bptree.h"
/*--------------------------- Public Includes END ----------------------------*/

/*------------------------------ Public Macros -------------------------------*/
/*---------------------------- Public Macros END -----------------------------*/
/* Helper macro for creating key32_t from a single uint32_t value */
#define KEY32(val) ((key32_t){{ (val), 0, 0, 0, 0, 0, 0, 0 }})
#define BPTR_LITE_PRI_TYPE       PRIu32
#define BPTR_NORM_PRI_TYPE       PRIu64

/*------------------------------ Public Structs ------------------------------*/
/* Large key/value types for testing with larger data sizes */
typedef struct { uint32_t v[4]; } key16_t;
typedef struct { uint32_t v[8]; } key32_t;
typedef struct
{
   const char *f_nm;
   _Bool is_lite, is_leaf;
   uint32_t node_sz;
} bptr_config;
/*---------------------------- Public Structs END ----------------------------*/

/*----------------------- Public Function Declarations -----------------------*/
int cmp_i(const void *lhs, const void *rhs);
int cmp_i32(const void *lhs, const void *rhs);
int cmp_u64(const void *lhs, const void *rhs);
int cmp_key32(const void *lhs, const void *rhs);
/*--------------------- Public Function Declarations END ---------------------*/

#endif
