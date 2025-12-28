#ifndef BPTREE_H
#define BPTREE_H

/*------------------------------ Public Include ------------------------------*/
#include <stdint.h>
/*---------------------------- Public Include END ----------------------------*/

/*------------------------------ Public Define -------------------------------*/
#define BPTR_CURRENT_VERSION (0u)
/*---------------------------- Public Define END -----------------------------*/

/*------------------------ Public Struct Declaration -------------------------*/
struct bptree;
/*---------------------- Public Struct Declaration END -----------------------*/

/*------------------------------- Public Enums -------------------------------*/
enum bptr_key_type
{
   BPTR_KYTP_U8 = 1, BPTR_KYTP_I8,
   BPTR_KYTP_U16, BPTR_KYTP_I16,
   BPTR_KYTP_U32, BPTR_KYTP_I32,
   BPTR_KYTP_U64, BPTR_KYTP_I64,
};
/*----------------------------- Public Enums END -----------------------------*/

/*----------------------- Public Variable Declaration ------------------------*/
extern int bptr_errno;
/*--------------------- Public Variable Declaration END ----------------------*/
/*----------------------------- Public Functions -----------------------------*/
struct bptree *bptr_init
(
   const char *filename,
   int is_lite,
   uint32_t node_size,
   uint8_t key_type,
   uint16_t value_size
);
struct bptree *bptr_load(const char *filename);
int bptr_insert(struct bptree *this, const void *kv);
int bptr_erase(struct bptree *this, const void *key);
const void *bptr_find(struct bptree *this, const void *key);
int bptr_find_range(struct bptree *this, const void *bg, const void *ed,
                    void **res_it);
int bptr_destroy(struct bptree *this);
/*--------------------------- Public Functions END ---------------------------*/

#endif
