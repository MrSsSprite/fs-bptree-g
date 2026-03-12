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

/*------------------------------ Public Structs ------------------------------*/
/* Large key/value types for testing with larger data sizes */
typedef struct { uint32_t v[4]; } key16_t;
typedef struct { uint32_t v[8]; } key32_t;
typedef enum
{
   CHAR, INT16_T, INT32_T, INT64_T,
   KEY16_T, KEY32_T,
} data_type;
typedef struct
{
   const char *f_nm;
   _Bool is_lite, is_leaf;
   uint32_t node_sz;
   data_type key_type, val_type;
   int (*compare)(const void*, const void*);
} bptr_config;
/*---------------------------- Public Structs END ----------------------------*/

/*----------------------- Public Function Declarations -----------------------*/
int cmp_i(const void *lhs, const void *rhs);
int cmp_u64(const void *lhs, const void *rhs);
int cmp_key32(const void *lhs, const void *rhs);
static inline
uint16_t data_size(data_type type)
{
   switch (type)
    {
   case CHAR:     return sizeof(char);
   case INT16_T:	return sizeof(int16_t);
   case INT32_T:	return sizeof(int32_t);
   case INT64_T:	return sizeof(int64_t);

   case KEY16_T:	return sizeof(key16_t);
   case KEY32_T:	return sizeof(key32_t);
    }
   exit(BPTR_E_UNREACHABLE);
}
/*--------------------- Public Function Declarations END ---------------------*/

#endif
