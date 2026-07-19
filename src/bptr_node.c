/*----------------------------- Private Includes -----------------------------*/
#include "bptr_node.h"
#include "bptr_internal.h"
#include "bptr_io.h"
#include "bptr_cache.h"
#include "bptr_utils.h"
#include <string.h>
/*--------------------------- Private Includes END ---------------------------*/


/*------------------------------ Private Macro -------------------------------*/
#define iter_write(iter, src, size) do \
{ \
   memcpy((iter), (src), (size)); \
   (iter) = (char*)(iter) + (size); \
} while (0)

#define buf_tc_write(buf, val, type) do \
{ \
   type _val_tc = (val); \
   memcpy((buf), &_val_tc, sizeof(_val_tc)); \
} while (0)

#define iter_tc_write(iter, val, type) do \
{ \
   buf_tc_write((iter), (val), type); \
   (iter) = (char*)(iter) + sizeof(type); \
} while (0)


#define iter_read(iter, dst, size) do \
{ \
   memcpy((dst), (iter), (size)); \
   (iter) = (char*)(iter) + (size); \
} while (0)

#define buf_tc_read(buf, dst, type) do \
{ \
   type val; \
   memcpy(&val, (buf), sizeof(val)); \
   (dst) = val; \
} while (0)

#define iter_tc_read(iter, dst, type) do \
{ \
   buf_tc_read((iter), (dst), type); \
   (iter) += sizeof(type); \
} while (0)

#define _node_val_size(self, node) \
   ((node)->is_leaf ? (self)->value_size : \
                      ((self)->is_lite ? BPTR_LITE_PTR_BYTE : \
                                         BPTR_NORM_PTR_BYTE))
#define _node_val_arr_size(self, node) \
   ((node)->is_leaf ? \
      (self)->value_size * (node)->key_count : \
      ((self)->is_lite ? BPTR_LITE_PTR_BYTE : BPTR_NORM_PTR_BYTE) * \
         ((node)->key_count + 1))
/*---------------------------- Private Macro END -----------------------------*/


/*---------------------- Private Function Declarations -----------------------*/
/**
 * @brief   Marshal node data from struct to self->fbuf
 *
 * @param[in,out] self  bptr obj.
 * @param[in]     node  node obj. to be serialized.
 *
 * @note    Serialization should be a more accurate name, but marshal is
 *          adopted as it's shorter.
 */
static inline
void bptr_node_marshal(struct bptr *self, struct bptr_node *node);
/**
 * @brief   Serialize node data from self->fbuf to node struct.
 *
 * @param[in,out] self  bptr obj.
 * @param[out]    node  node obj. to hold the deserialized data
 *
 * @warning    This function allocates memory for node->keys and node->vals;
 *             thence, these two members should not have exclusive ownership
 *             on anything before a call to this function as their value will
 *             be overwritten.
 * @node       Serialization should be a more accurate name, but marshal is
 *             adopted as it's shorter.
 */
static inline
void bptr_node_unmarshal(struct bptr *self, struct bptr_node *node);
/**
 * @brief   preallocate node-sized space in file
 *
 * This function reserves a free node-sized block in self->file and returns a
 * node index (not direct file offset) referring to that block.
 *
 * @param   self  bptr obj.
 * @return  the node index that's allocated.
 * @retval  0     failure; bptr_errno is set.
 *
 * @remark  the return value is node indx. This means the actual file offset is
 *          calculated thru. `ret * self->node_size`.
 */
static inline
bptr_node_t bptr_node_prealloc(struct bptr *self);
/**
 * @brief   Vacate the file space occupied by the node.
 *
 * This function releases the file space allocated for the node (identified by
 * @c node->node_idx ) and returns it back to internal free list.
 *
 * @param[in,out] self     bptr obj. Only @c fbuf and @c free_list fields will
 *                         be modified.
 * @param[in]     node_idx node to which the file space is allocated
 * @return  status_code
 * @retval  0           success
 * @retval  non-0       failed during io flush. Returns what
 *                      @c bptr_io_flush_node returns.
 */
static inline
int bptr_node_vacate(struct bptr *self, bptr_node_t node_idx);
/**
 * @brief   Insert a key into keys
 *
 * @param[in]     self  @c bptr object.
 * @param[in,out] node  node
 * @param[in]     key   value of key to be copied into @c node->keys
 * @param[in]     idx   index at which the key is inserted
 *
 * @warning It's caller's responsibility to check it's valid to insert into the
 *          node.
 *
 * @remark  This function does not increment @c node->key_count . Caller should
 *          perform such operation themselves, if needed.
 * @note    This function assumes correct @c node->key_count . It causes
 *          undefined behavior if such assumption is not fulfilled.
 */
static inline
void _node_key_insert(struct bptr *self, struct bptr_node *node,
                      const void *key, uint_fast32_t idx);
/**
 * @brief   Insert a val into vals
 *
 * @param[in]     self  @c bptr object.
 * @param[in,out] node  node
 * @param[in]     val   value of val to be copied into @c node->vals
 * @param[in]     idx   index at which the val is inserted
 *
 * @warning It's caller's responsibility to check it's valid to insert into the
 *          node.
 *
 * @remark  This function does not increment @c node->key_count . Caller should
 *          perform such operation themselves, if needed.
 * @note    This function assumes correct @c node->key_count . It causes
 *          undefined behavior if such assumption is not fulfilled.
 */
static inline
void _node_val_insert(struct bptr *self, struct bptr_node *node,
                      const void *val, uint_fast32_t idx);
/**
 * @brief   Erase a Key from @c node->keys
 *
 * @param[in]     self  @c bptr object.
 * @param[in,out] node  node
 * @param[in]     idx   index of the key to be erased
 *
 * @warning It's caller's responsibility to check it's valid to insert into the
 *          node.
 *
 * @remark  This function does not modify @c node->key_count . Caller should
 *          perform such operation themselves, if needed.
 * @note    This function assumes correct @c node->key_count . It causes
 *          undefined behavior if such assumption is not fulfilled.
 */
static inline
void _node_key_erase(struct bptr *self, struct bptr_node *node,
                     uint_fast32_t idx);
/**
 * @brief   Erase a val from @c node->vals
 *
 * @param[in]     self  @c bptr object.
 * @param[in,out] node  node
 * @param[in]     idx   index of the val to be erased
 *
 * @warning It's caller's responsibility to check it's valid to erase from the
 *          node.
 *
 * @remark  This function does not modify @c node->key_count . Caller should
 *          perform such operation themselves, if needed.
 * @note    This function assumes correct @c node->key_count . It causes
 *          undefined behavior if such assumption is not fulfilled.
 */
static inline
void _node_val_erase(struct bptr *self, struct bptr_node *node,
                     uint_fast32_t idx);
/**
 * @brief   Search for a key within a node's key array
 *
 * Performs a binary search to locate the first key in @p node that is not less
 * than @p key . This index serves as the exact match or the potential insertion
 * point.
 *
 * @param[in]  self  B+Tree instance.
 * @param[in]  node  node to be searched.
 * @param[in]  key   target key to locate.
 *
 * @return  The index of the lower bound of @p key
 * @retval  index    If the key is found, this is the index of the match.
 * @retval  index    If not found, this is the insertion point (the index of the
 *                   smallest key greater than @p key , or @c node->key_count if
 *                   @p key is the largest).
 * @remark  If an exact match is not found, @c bptr_errno is set to @c -1 .
 *          Otherwise, @c bptr_errno is set to @c 0 .
 */
static inline
uint32_t _node_key_search(struct bptr *self, struct bptr_node *node,
                          const void *key);
/**
 * @brief   Promote a key-node(reference) pair into a parent node
 *
 * Inserts the promoted key and child pointer of @p prm_n into @p par_n .
 * If @p par_n is already full, a recursive split is triggered via
 * @c bptr_node_split .
 *
 * @param[in]     self  @c bptr object.
 * @param[in,out] par_n parent node to receive the promoted key and child.
 * @param[in]     prm_n node being promoted into @p par_n .
 * @param[in]     key   key to promote.
 *
 * @return  error code
 * @retval  0     success
 * @retval  200   parent node was full and split failed
 */
static inline
int _node_promote(struct bptr *self, struct bptr_node *par_n,
                  struct bptr_node *prm_n, const void *key);


/**
 * @brief   Free a node and Drop all changes
 *
 * @param[in,out] self  bptr obj.
 * @param[in.out] node  target node to be droped.
 *
 * @return  error code
 */
static inline
int _node_drop(struct bptr *self, struct bptr_node *node);


/**
 * @brief   Split a full B+tree node around a new key–value pair
 *
 * Splits @p node (which must be at maximum capacity) into two nodes by
 * distributing its existing keys and values along with the incoming
 * @p key / @p val .  A new sibling node is allocated, the leaf-level
 * doubly-linked list is stitched to place it immediately after the original
 * node, and the separator key together with the new child pointer are
 * inserted into the parent.  If the parent is already full the split
 * cascades upward via a recursive call.
 *
 * When @p node has no parent (i.e., it is the root) a fresh parent is
 * created and @c self->height is incremented; the original node becomes
 * the leftmost child.
 *
 * **Leaf-split specifics.**
 * The new node receives @c leaf.up / 2 keys (the higher half), while the
 * original node keeps the lower half.  After the data are moved, the first
 * key of the new node is used as the separator and is promoted into the
 * parent together with the new node's pointer.  @c self->record_cnt is
 * incremented to account for the newly inserted record.
 *
 * **Internal-node specifics.**
 * One key is promoted to the parent via @c _node_promote ; the keys and
 * child pointers are then distributed so that the original node keeps the
 * keys strictly left of the promotion point and the new node receives the
 * keys strictly right of it.  After distribution every child pointer owned
 * by the new node has its @c parent field rewritten to point to the new
 * node.
 *
 * @param[in,out] self  bptr object.  @c node_cnt is incremented for the
 *                      new sibling (and for a new parent, if one is
 *                      created).  For leaf splits @c record_cnt is also
 *                      incremented.  @c height and @c root_idx may be
 *                      updated when splitting the root.
 * @param[in,out] node  the full node to split.  Its @c key_count , keys,
 *                      vals, @c next , and (if it was the root) @c parent
 *                      are modified.
 * @param[in]     key   new key to insert during the split.
 * @param[in]     val   new value to insert during the split.  For internal
 *                      nodes this is a child pointer.
 *
 * @return  node index of the newly created sibling.
 * @retval  >0   success; the returned value is the index of the new
 *               sibling node.
 * @retval  0    failure; @c bptr_errno is set and all side effects are
 *               rolled back — the new sibling (and any freshly created
 *               parent) are dropped, @c key_count is restored, and
 *               @c record_cnt / @c height / @c root_idx are reverted
 *               where applicable.
 *
 * @pre  @p node must be at maximum capacity (@c node->key_count equals
 *       the applicable @c node_bound up bound minus one).  Calling this
 *       function on a non-full node sets @c bptr_errno to
 *       @c BPTR_E_FN_INPUT and returns 0.
 * @pre  @p key must not already exist in @p node ; a duplicate key causes
 *       the same early-exit behaviour.
 *
 * @note  The forward declaration carries @c BPTR_STATIC because
 *        @c _node_promote (defined earlier) may call this function
 *        recursively, creating a mutual recursion between the two.
 */
BPTR_STATIC
bptr_node_t bptr_node_split(struct bptr *self, struct bptr_node *node,
                            const void *key, const void *val);
/*-------------------- Private Function Declarations END ---------------------*/


/*----------------------------- Public Functions -----------------------------*/
// node->{prev, next} are left uninitialized; the
// caller is responsible to write that
// if parent == 0, self->height is incremented
// checksum is also uninitialized
// self->node_cnt is NOT updated
struct bptr_node *bptr_node_new
 (struct bptr *self, bptr_node_t parent)
{
   _Bool has_set_err = 0;
   bptr_node_t file_slot;
   struct bptr_node *node, *parent_n;

   file_slot = bptr_node_prealloc(self);
   if (file_slot == 0) goto PREALLOC_ERR;

   node = bptr_cache_alloc(self, file_slot);
   if (node == NULL) goto CACHE_ALLOC_ERR;

   if (parent)
    {
      parent_n = bptr_node_fetch(self, parent);
      if (parent_n == NULL) goto PARENT_LOAD_ERR;
      node->level = parent_n->level - 1;
      bptr_node_unload(self, parent_n);
    }
   else
      // if parent == 0, root split
      node->level = self->height++;

   node->flags = BPTR_NODE_FLAG_VALID;
   if (node->level == 0)
    {
      node->flags |= BPTR_NODE_FLAG_LEAF;
      node->is_leaf = 1;
      node->vals = node->keys + (self->node_bound.leaf.up - 1) * self->key_size;
    }
   else
    {
      node->is_leaf = 0;
      node->vals = node->keys + (self->node_bound.brch.up - 1) * self->key_size;
    }
   node->is_dirty = 1;
   node->key_count = 0;
   node->parent = parent;
   node->node_idx = file_slot;

   return node;

   /*-------------------------- Error Handling Zone --------------------------*/
   int err_code;

PARENT_LOAD_ERR:  _set_err_code(bptr_errno);
bptr_cache_reclaim(self, node);
CACHE_ALLOC_ERR:  _set_err_code(bptr_errno);
                  if (bptr_node_vacate(self, file_slot))
                     perror("`bptr_node_new' error zone: `bptr_node_vacate': flush failure");
PREALLOC_ERR:  _set_err_code(bptr_errno);

               bptr_errno = err_code;
   return NULL;
}


void bptr_node_unload(struct bptr *self, struct bptr_node *node)
{ bptr_cache_release(self, node); }


int bptr_node_load
 (struct bptr *self, bptr_node_t node_idx, struct bptr_node *node)
{
   _Bool has_set_err = 0;

   if (node == NULL)
      goto INVALID_NODE;

   // Read into fbuf
   if (bptr_io_fread_node(self, node_idx))
      goto FREAD_NODE_ERR;

   bptr_node_unmarshal(self, node);
   node->node_idx = node_idx;

   return 0;

   /*-------------------------- Error Handling Area --------------------------*/
   int err_code;

FREAD_NODE_ERR:   _set_err_code(BPTR_E_FACCESS);
INVALID_NODE:     _set_err_code(BPTR_E_FN_INPUT);
   return err_code;
}


int bptr_node_flush(struct bptr *self, struct bptr_node *node)
{
   _Bool has_set_err = 0;
   int fn_err;

   bptr_node_marshal(self, node);
   fn_err = bptr_io_flush_node(self, node->node_idx);
   if (fn_err) goto FLUSH_ERR;
   node->is_dirty = 0;

   return 0;

   /*-------------------------- Error Handling Area --------------------------*/
   int err_code;

FLUSH_ERR:  _set_err_code(fn_err);
   return err_code;
}


int bptr_node_insert
 (struct bptr *self, struct bptr_node *node, bptr_node_ki_t idx,
  const void *key, const void *value)
{
   // node full
   if (++node->key_count == self->node_bound.leaf.up)
    {
      node->key_count--;
      if (bptr_node_split(self, node, key, value) == 0)
         return bptr_errno;   // split should set bptr_errno
      return 0;
    }

   _node_key_insert(self, node, key, idx);
   _node_val_insert(self, node, value, idx);
   node->key_count++;
   self->record_cnt++;
   return 0;
}


int bptr_node_root_drop(struct bptr *self, struct bptr_node *node)
{
   int err_code = _node_drop(self, node);
   if (err_code) return err_code;
   self->root_idx = 0;
   self->height = self->record_cnt = self->node_cnt = 0;
   return 0;
}
/*--------------------------- Public Functions END ---------------------------*/



/*---------------------------- Private Functions -----------------------------*/
static inline
void bptr_node_marshal(struct bptr *self, struct bptr_node *node)
{
   void *buf_it = self->fbuf;

   iter_write(buf_it, &node->flags, 2);
   iter_write(buf_it, &node->level, 2);
   iter_write(buf_it, &node->key_count, 4);
   iter_write(buf_it, &node->checksum, 4);
#define _WRITE_FIELDS(type) do { \
      iter_tc_write(buf_it, node->parent, type); \
      iter_tc_write(buf_it, node->next, type); \
      iter_tc_write(buf_it, node->prev, type); \
} while (0)
   if (self->is_lite)   _WRITE_FIELDS(BPTR_LITE_PTR_TYPE);
   else                 _WRITE_FIELDS(BPTR_NORM_PTR_TYPE);
#undef _WRITE_FIELDS
   buf_it = (char*)self->fbuf + BPTR_NODE_METADATA_BYTE;

   iter_write(buf_it, node->keys,
             self->key_size * node->key_count);
   iter_write(buf_it, node->vals, _node_val_arr_size(self, node));
}


static inline
void bptr_node_unmarshal(struct bptr *self, struct bptr_node *node)
{
   void *buf_it = self->fbuf;

   iter_read(buf_it, &node->flags, 2);
   iter_read(buf_it, &node->level, 2);
   iter_read(buf_it, &node->key_count, 4);
   iter_read(buf_it, &node->checksum, 4);
#define _READ_FIELDS(type) do { \
      iter_tc_read(buf_it, node->parent, type); \
      iter_tc_read(buf_it, node->next, type); \
      iter_tc_read(buf_it, node->prev, type); \
} while (0)
   if (self->is_lite)   _READ_FIELDS(uint32_t);
   else                 _READ_FIELDS(uint64_t);
#undef _READ_FIELDS
   buf_it = (char*)self->fbuf + BPTR_NODE_METADATA_BYTE;

   if (node->level == 0)
    {
      node->is_leaf = 1;
      node->vals = node->keys + (self->node_bound.leaf.up - 1) * self->key_size;
    }
   else
    {
      node->is_leaf = 0;
      node->vals = node->keys + (self->node_bound.brch.up - 1) * self->key_size;
    }
   node->is_dirty = 0;
   iter_read(buf_it, node->keys,
            self->key_size * node->key_count);
   iter_read(buf_it, node->vals, _node_val_arr_size(self, node));
}

BPTR_STATIC
bptr_node_t bptr_node_prealloc (struct bptr *self)
{
   bptr_node_t ret;

   if (self->free_list.cnt)
    {
      ret = self->free_list.head;
      if (bptr_io_fread_node(self, self->free_list.head))
       { bptr_errno = -1; return 0; }
      if (self->is_lite)
         buf_tc_read((char*)self->fbuf + 2, self->free_list.head,
                     BPTR_LITE_PTR_TYPE);
      else
         buf_tc_read((char*)self->fbuf + 2, self->free_list.head,
                     BPTR_NORM_PTR_TYPE);
      self->free_list.cnt--;
    }
   else
    {
      static const char dummy = 0;
      bptr_off_t offset;

      if (fseek64(self->file, 0, SEEK_END))
       { bptr_errno = -1; return 0; }
      offset = ftell64(self->file);
      if (offset == -1)
       { bptr_errno = -1; return 0; }
      if (fseek64(self->file, self->node_size - 1, SEEK_CUR))
       { bptr_errno = -1; return 0; }
      if (fwrite(&dummy, 1, 1, self->file) != 1)
       { bptr_errno = -1; return 0; }
      if (fflush(self->file))
       { bptr_errno = -1; return 0; }
      ret = offset / self->node_size;
    }
   return ret;
}


static inline
int bptr_node_vacate(struct bptr *self, bptr_node_t node_idx)
#define _WRITE_FL_HEAD(T) do \
{ \
      T head = self->free_list.head; \
      memcpy(buf_it, &head, sizeof(head)); \
} while (0)
{
   uint16_t flags = 0;
   void *buf_it = self->fbuf;
   int fn_err;

   iter_write(buf_it, &flags, 2);

   if (self->is_lite)
      _WRITE_FL_HEAD(BPTR_LITE_PTR_TYPE);
   else
      _WRITE_FL_HEAD(BPTR_NORM_PTR_TYPE);
#undef _WRITE_FL_HEAD

   fn_err = bptr_io_flush_node(self, node_idx);
   if (fn_err) return fn_err;

   self->free_list.head = node_idx;
   self->free_list.cnt++;
   return 0;
}


static inline
void _node_key_insert(struct bptr *self, struct bptr_node *node,
                      const void *key, uint_fast32_t idx)
{
   uint_fast32_t idx_plus1 = idx + 1,
                 /* up is max_SIZE + 1; max_INDEX is max_SIZE - 1 */
                 ed = (node->is_leaf ? self->node_bound.leaf.up :
                                       self->node_bound.brch.up) - 1;
   char *tar_p = (char*)node->keys + idx * self->key_size;

   // insert idx is not last element
   if (idx_plus1 != ed)
      // reserve slot
      memmove(tar_p + self->key_size, tar_p,
              (node->key_count - idx) * self->key_size);
   // copy into slot
   memcpy(tar_p, key, self->key_size);
}


static inline
void _node_val_insert(struct bptr *self, struct bptr_node *node,
                      const void *val, uint_fast32_t idx)
{
   uint_fast32_t idx_plus1 = idx + 1,
                 /* up is max_SIZE + 1; max_INDEX is max_SIZE - 1
                  * leaf: val_cnt == key_cnt; branch: val_cnt == key_cnt + 1 */
                 ed = (node->is_leaf ? self->node_bound.leaf.up :
                                       self->node_bound.brch.up + 1) - 1;
   uint_fast16_t val_sz = _node_val_size(self, node);
   char *tar_p = (char*)node->vals + idx * val_sz;

   // insert idx is not last element
   if (idx_plus1 != ed)
      // reserve slot
      memmove(tar_p + val_sz, tar_p,
              (_node_val_cnt(node) - idx) * val_sz);
   // copy into slot
   memcpy(tar_p, val, val_sz);
}


static inline
void _node_child_insert(struct bptr *self, struct bptr_node *parent_n,
                        bptr_node_t child_ptr, uint_fast32_t idx)
{
   if (self->is_lite)
    {
      BPTR_LITE_PTR_TYPE n_idx = child_ptr;
      _node_val_insert(self, parent_n, &n_idx, idx);
    }
   else
    {
      BPTR_NORM_PTR_TYPE n_idx = child_ptr;
      _node_val_insert(self, parent_n, &n_idx, idx);
    }
}



static inline
void _node_key_erase(struct bptr *self, struct bptr_node *node,
                     uint_fast32_t idx)
{
   uint_fast32_t idx_plus1 = idx + 1;

   memmove(node->keys + idx * self->key_size,
           node->keys + idx_plus1 * self->key_size,
           (node->key_count - idx_plus1) * self->key_size);
}


static inline
void _node_val_erase(struct bptr *self, struct bptr_node *node,
                     uint_fast32_t idx)
{
   uint_fast32_t idx_plus1 = idx + 1,
                 val_cnt = _node_val_cnt(node),
                 val_size = _node_val_size(self, node);

   memmove(node->vals + idx * val_size,
           node->vals + idx_plus1 * val_size,
           (val_cnt - idx_plus1) * val_size);
}


static inline
uint32_t _node_key_search(struct bptr *self, struct bptr_node *node,
                               const void *key)
{
   uint_fast32_t lo, md, hi;
   // Edge Case: empty (i.e., key_count == 0)
   int cmp_res = -1;

   for (lo = 0, hi = node->key_count, md = hi / 2;
        lo != hi; md = lo + (hi - lo) / 2)
    {
      cmp_res = self->compare(key, node->keys + md * self->key_size);
      if (cmp_res < 0)
         hi = md;
      else if (cmp_res > 0)
         lo = md + 1;
      else
         break;
    }

   if (cmp_res == 0) bptr_errno = 0;
   else              bptr_errno = -1;
   return md;
}


BPTR_STATIC
bptr_node_t bptr_node_split(struct bptr *self, struct bptr_node *node,
                            const void *key, const void *val)
{
   _Bool has_set_err = 0;
   // TODO: replace error codes magic # with manifest constant
   struct bptr_node *new_n, *parent_n, *next_n;
   _Bool has_new_parent;
   uint_fast32_t max_sz = (node->is_leaf ? self->node_bound.leaf.up :
                                           self->node_bound.brch.up) - 1,
                 new_elem_idx;
   bptr_node_t ret;

   // Check full; error if not already full
   if (node->key_count != max_sz) goto PRE_WORK_ERR;

   /*{--------------- Pre-Work: Find low bound of new element ----------------*/
   new_elem_idx = _node_key_search(self, node, key);
   // newly inserted element should not match with another existing element
   if (bptr_errno == 0) goto PRE_WORK_ERR;
   /*}--------------- Pre-Work: Find low bound of new element ----------------*/

   /*{--------------------- Pre-Work: Load Parent Node -----------------------*/
   if (node->parent == 0)
    {
      parent_n = bptr_node_new(self, 0);
      if (parent_n == NULL) goto PAR_N_LOAD_ERR;

      has_new_parent = 1;
      // node was root as it has no parent
      parent_n->prev = parent_n->next = 0;
      // add orig. node into parent_n->vals in advance
      _node_child_insert(self, parent_n, node->node_idx, 0);
      node->parent = parent_n->node_idx;
      self->root_idx = parent_n->node_idx;
      self->node_cnt++;
    }
   else
    {
      has_new_parent = 0;
      parent_n = bptr_node_fetch(self, node->parent);
      if (parent_n == NULL) goto PAR_N_LOAD_ERR;
    }
   /*}--------------------- Pre-Work: Load Parent Node -----------------------*/

   /*{-------------------- Pre-work: Init new empty node ---------------------*/
   new_n = bptr_node_new(self, node->parent);
   if (new_n == NULL) goto NEW_N_MALLOC_ERR;
   /*}-------------------- Pre-work: Init new empty node ---------------------*/

   /*{--------------------- Pre-Work: Update prev, next ----------------------*/
   // update new_n->next->prev
   if (node->next)
    {
      next_n = bptr_node_fetch(self, node->next);
      if (next_n == NULL) goto NEXT_N_UPDATE_ERROR;
      next_n->prev = new_n->node_idx;
      next_n->is_dirty = 1;
      bptr_node_unload(self, next_n);
    }

   new_n->next = node->next;
   new_n->is_dirty = 1;
   new_n->prev = node->node_idx;
   node->next = new_n->node_idx;
   node->is_dirty = 1;
   /*}--------------------- Pre-Work: Update prev, next ----------------------*/

   /*{------------------------ Main-Work: Split node -------------------------*/
   if (node->is_leaf)
    {
      // new node gets up_bound / 2 keys; orig node retains other
      new_n->key_count = self->node_bound.leaf.up / 2;
      // total # of elem == up because one new elem is adding in
      node->key_count = self->node_bound.leaf.up - new_n->key_count;
      self->record_cnt++;

      // move elems to new node
      // because is_leaf, symmetric structure on keys & vals
      if (new_elem_idx < node->key_count)
       {
         // src_st = orig_n->key_cnt - 1
         memcpy(new_n->keys,
                (char*)node->keys + self->key_size * (node->key_count - 1),
                (size_t)self->key_size * new_n->key_count);
         memcpy(new_n->vals,
                (char*)node->vals + self->value_size * (node->key_count - 1),
                (size_t)self->value_size * new_n->key_count);

         node->key_count--;
         _node_key_insert(self, node, key, new_elem_idx);
         _node_val_insert(self, node, val, new_elem_idx);
         node->key_count++;
       }
      else  // new elem in right part
       {
         uint_fast32_t cnt = new_elem_idx - node->key_count;
         // before new elem
         memcpy(new_n->keys,
                (char*)node->keys + self->key_size * node->key_count,
                (size_t)self->key_size * cnt);
         memcpy(new_n->vals,
                (char*)node->vals + self->value_size * node->key_count,
                (size_t)self->value_size * cnt);
         // new elem
         memcpy((char*)new_n->keys + self->key_size * cnt,
                key, self->key_size);
         memcpy((char*)new_n->vals + self->value_size * cnt,
                val, self->value_size);
         cnt++;
         // after new elem
         memcpy((char*)new_n->keys + self->key_size * cnt,
                (char*)node->keys + self->key_size * new_elem_idx,
                (size_t)self->key_size * (max_sz - new_elem_idx));
         memcpy((char*)new_n->vals + self->value_size * cnt,
                (char*)node->vals + self->value_size * new_elem_idx,
                (size_t)self->value_size * (max_sz - new_elem_idx));
       }

      // Update parent
      if (parent_n->key_count == self->node_bound.brch.up - 1)
       { // Split parent if it's already full
         if (self->is_lite)
          {
            BPTR_LITE_PTR_TYPE n_idx = new_n->node_idx;
            if (bptr_node_split(self, parent_n, new_n->keys, &n_idx) == 0)
               goto PAR_SPLIT_ERR;
          }
         else
          {
            BPTR_NORM_PTR_TYPE n_idx = new_n->node_idx;
            if (bptr_node_split(self, parent_n, new_n->keys, &n_idx) == 0)
               goto PAR_SPLIT_ERR;
          }
       }
      else
       {
         uint32_t idx = _node_key_search(self, parent_n, new_n->keys);
         _node_key_insert(self, parent_n, new_n->keys, idx);
         _node_child_insert(self, parent_n, new_n->node_idx, idx + 1);
         parent_n->key_count++;
       }
    }
   else  // internal node
    {
      node->key_count = self->node_bound.brch.up / 2;
      new_n->key_count = self->node_bound.brch.up - node->key_count - 1;

      if (new_elem_idx < node->key_count)
       {
         memcpy(new_n->keys,
                (char*)node->keys + self->key_size * node->key_count,
                (size_t)self->key_size * new_n->key_count);
         // cp (up - new_val_cnt), nvc
         // nvc = new_key_cnt + 1 = (up - old_key_cnt - 1) + 1 = up - okc
         // => okc = up - nvc
         // cp okc, nvc
         memcpy(new_n->vals,
                (char*)node->vals + BPTR_PTR_SIZE * node->key_count,
                (size_t)BPTR_PTR_SIZE * (new_n->key_count + 1));

         if (_node_promote(self, parent_n, new_n,
               (char*)node->keys + self->key_size * (node->key_count - 1)))
            goto PAR_SPLIT_ERR;

         node->key_count--;
         _node_key_insert(self, node, key, new_elem_idx);
         _node_val_insert(self, node, val, new_elem_idx + 1);
         node->key_count++;
       }
      else if (new_elem_idx > node->key_count)
       {
         char *kdst = new_n->keys, *vdst = new_n->vals;
         size_t cnt, cnt_b, offset, offset_b;

         if (_node_promote(self, parent_n, new_n,
               (char*)node->keys + self->key_size * node->key_count))
            goto PAR_SPLIT_ERR;

         // Before new elem
         offset = node->key_count + 1;
         cnt = new_elem_idx - offset;
         offset_b = offset * self->key_size;
         cnt_b = cnt * self->key_size;
         memcpy(kdst, (char*)node->keys + offset_b, cnt_b);
         kdst += cnt_b;
         // offset unchanged as keys has prm as buffer
         offset_b = offset * BPTR_PTR_SIZE;
         // cnt_val = (nei + 1) - (okc + 1) = cnt_key + 1
         cnt_b = ++cnt * BPTR_PTR_SIZE;
         memcpy(vdst, (char*)node->vals + offset_b, cnt_b);
         vdst += cnt_b;

         // New elem
         memcpy(kdst, key, self->key_size);
         kdst += self->key_size;
         memcpy(vdst, val, BPTR_PTR_SIZE);
         vdst += BPTR_PTR_SIZE;

         // After new elem
         offset = new_elem_idx;
         cnt = max_sz - offset;
         offset_b = offset * self->key_size;
         cnt_b = cnt * self->key_size;
         memcpy(kdst, (char*)node->keys + offset_b, cnt_b);
         // nei_v = nei_k + 1
         offset = new_elem_idx + 1;
         // cnt_v = mx_v - offset = (mx_k + 1) - (nei_k + 1) = mx - nei
         cnt = max_sz - new_elem_idx;
         offset_b = offset * BPTR_PTR_SIZE;
         cnt_b = cnt * BPTR_PTR_SIZE;
         memcpy(vdst, (char*)node->vals + offset_b, cnt_b);
       }
      else
       { // new_elem_idx == okc
         if (_node_promote(self, parent_n, new_n, key)) goto PAR_SPLIT_ERR;

         memcpy(new_n->keys,
                (char*)node->keys + new_elem_idx * self->key_size,
                (size_t)self->key_size * new_n->key_count);

         memcpy(new_n->vals, val, BPTR_PTR_SIZE);
         // nei_v = nei_k + 1
         memcpy((char*)new_n->vals + BPTR_PTR_SIZE,
                (char*)node->vals + (new_elem_idx + 1) * BPTR_PTR_SIZE,
                (size_t)BPTR_PTR_SIZE * new_n->key_count);
       }

      // update parent member of children that are assigned new_n
      for (uint32_t new_i = 0; new_i <= new_n->key_count; new_i++)
       {
         struct bptr_node *child_n =
            bptr_node_fetch(self, _node_brch_vals_get(self, new_n, new_i));
         if (child_n == NULL) goto CHILD_N_UPDATE_ERR;
         child_n->parent = new_n->node_idx;
         child_n->is_dirty = 1;
         bptr_node_unload(self, child_n);
       }
    }
   /*}------------------------ Main-Work: Split node -------------------------*/

   bptr_node_unload(self, parent_n);
   ret = new_n->node_idx;
   bptr_node_unload(self, new_n);
   self->node_cnt++;
   return ret;

/*--------------------------- Error Handling Zone ----------------------------*/
// restore back to the state before fn call on error
CHILD_N_UPDATE_ERR:  has_set_err = 1;
   // TODO: children point back to old node
PAR_SPLIT_ERR:       has_set_err = 1;
   if (node->is_leaf) self->record_cnt--;
   node->key_count = max_sz;
NEXT_N_UPDATE_ERROR: has_set_err = 1;
   _node_drop(self, new_n);
NEW_N_MALLOC_ERR:    has_set_err = 1;
   if (has_new_parent)
    {
      _node_drop(self, parent_n);
      self->height--;
      self->node_cnt--;
      node->parent = 0;
      self->root_idx = node->node_idx;
    }
   else
      bptr_node_unload(self, parent_n);
PAR_N_LOAD_ERR:   has_set_err = 1;
PRE_WORK_ERR:     _set_errno(BPTR_E_FN_INPUT);
   return 0;
}


static inline
int _node_promote(struct bptr *self, struct bptr_node *par_n,
                  struct bptr_node *prm_n, const void *key)
{
   if (par_n->key_count == self->node_bound.brch.up - 1)
    {
#define _node_prm_split_par(type) do \
{ \
   type n_idx = prm_n->node_idx; \
   if (bptr_node_split(self, par_n, key, &n_idx) == 0) return bptr_errno; \
} while (0)

      if (self->is_lite)
         _node_prm_split_par(BPTR_LITE_PTR_TYPE);
      else
         _node_prm_split_par(BPTR_NORM_PTR_TYPE);

#undef _node_prm_split_par
    }
   else
    {
      // find insertion point
      uint32_t idx = _node_key_search(self, par_n, key);
      _node_key_insert(self, par_n, key, idx);
      _node_child_insert(self, par_n, prm_n->node_idx, idx + 1);
      par_n->key_count++;
    }
   return 0;
}


static inline
int _node_drop(struct bptr *self, struct bptr_node *node)
{
   int err_code = bptr_node_vacate(self, node->node_idx);
   bptr_cache_reclaim(self, node);
   return err_code;
}
/*-------------------------- Private Functions END ---------------------------*/
