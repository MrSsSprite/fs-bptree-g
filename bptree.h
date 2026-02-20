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

/*----------------------- Public Variable Declaration ------------------------*/
extern int bptr_errno;
/*--------------------- Public Variable Declaration END ----------------------*/
/*----------------------------- Public Functions -----------------------------*/
struct bptree *bptr_init
(
   const char *filename,
   _Bool is_lite,
   uint32_t node_size,
   uint16_t key_size,
   uint16_t value_size
);
struct bptree *bptr_load(const char *filename);
int bptr_insert(struct bptree *self, const void *kv);
int bptr_erase(struct bptree *self, const void *key);
const void *bptr_find(struct bptree *self, const void *key);
int bptr_find_range(struct bptree *self, const void *bg, const void *ed,
                    void **res_it);
int bptr_destroy(struct bptree *self);
/*--------------------------- Public Functions END ---------------------------*/

#endif
