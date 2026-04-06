# Node Binary Layout
This file documents binary layout of node on disk (file).

## Metadata
Every node (valid or invalid, leaf or internal) has a metadata (header) of size
64-byte. The main storage will begin at offset 64 bytes.

## Flags
Both valid and invalid node always start with `flags`.
However, in an **invalid node**, ***only the valid-bit*** (LSB, i.e.,
0x0001/`BPTR_NODE_FLAG_VALID`) is meaningful, and all other bits are undefined.

| Offset | Field | Description |
| ------ | ----- | ----------- |
| 0      | valid | 0: unoccupied; 1: valid node |
| 1      | leaf  | 0: internal node, 1: leaf node |

## Invalid Node (Vacant in free list)
| Offset | Type | Field     | Description |
| ------ | ---- | --------- | ----------- |
| 0      | u16  | flags     | [binary flags](#flags) |
| 2      | uptr | next free | Index of next node in free list |
| 6/10   | N/A  | The End   | The end of meaningful data (remaining bytes undefined) |

## Valid Node
| Offset | Type | Field     | Description |
| ------ | ---- | --------- | ----------- |
| 0      | u16  | flags     | [binary flags](#flags) |
| 2      | u16  | level     | height of node in the tree (0: leaf) |
| 4      | u32  | key count | number of keys stored in the node |
| 8      | u32  | checksum  | (not yet implemented) |
| 12     | uptr | parent    | index of parent node (0: no parent) |
| 16/20  | uptr | next      | right sibling index  (0: non-existent) |
| 20/28  | uptr | prev      | left sibling index  (0: non-existent) |
| 24/36  | N/A  | The End   | The end of meaningful metadata (remaining bytes in metadata undefined) |

### Size of Node Index (`uptr`)
The size of node index (`uptr`) is 32-bit on lite tree, or 64-bit on normal tree.

Corresponding macros:
- Lite: `BPTR_LITE_PTR_BYTE`, `BPTR_LITE_PTR_TYPE`
- Normal: `BPTR_NORM_PTR_BYTE`, `BPTR_NORM_PTR_TYPE`
