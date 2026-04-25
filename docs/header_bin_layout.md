# B+Tree Header Binary Layout
This file documents binary layout of B+Tree header (metadata) on disk (file).

## Layout
| Offset | Type | Field           | Description |
| ------ | ---- | --------------- | ----------- |
| 0      | u32  | Magic String    | Magic String used to verify correctness of file type. (value `"BPTR"`) |
| 4      | u32  | version         | Version # of the tree. The MSB (`0x80`) is an `is_lite` flag (i.e., set if the tree is lite, unset otherwise) |
| 8      | u32  | node size       | Node size in **bytes** |
| 12     | u16  | key size        | Key size in **bytes** |
| 14     | u16  | value size      | Value size in **bytes** |
| 16     | u64  | record count    | Total number of records in the tree |
| 24     | u32  | tree height     | Tree height |
| 28     | uptr | root index      | Index of root node |
| 32/36  | uptr | free list head  | Index of the head of free list |
| 36/44  | uptr | free list count | Number of nodes in free list |
| 40/52  | uptr | node count      | Number of valid nodes in the entire tree |

### Size of Node Index (`uptr`)
The size of node index (`uptr`) is 32-bit on lite tree, or 64-bit on normal tree.

Corresponding macros:
- Lite: `BPTR_LITE_PTR_BYTE`, `BPTR_LITE_PTR_TYPE`
- Normal: `BPTR_NORM_PTR_BYTE`, `BPTR_NORM_PTR_TYPE`
