# Memory management.

Key files:

| unit                                                                                 | docs                                  |
|--------------------------------------------------------------------------------------|---------------------------------------|
| [heap.c](https://github.com/babevm/babevm/blob/master/src/c/heap.c){:target="_blank"}            | [link](doxygen/html/heap_8c.html)     |
| [collector.c](https://github.com/babevm/babevm/blob/master/src/c/collector.c){:target="_blank"}  | [link](doxygen/html/collector_8c.html) |

## Allocation

The VM performs all its own memory management, including garbage collection.  At startup, the VM heap is allocated from the OS and is never expanded nor shrunk during execution.  The memory allocator (found in heap.c) is meant for managing a small linear address space.  It assumes that issues such as locality-of-reference or paging are not actually present (as is often the case in smaller platforms).

Once allocated, memory does not move - it is not compacted or reshuffled.   This means all memory references can be direct references and performance does not suffer from pointer indirections.  The allocator algorithm is coalescing. That is, adjacent free memory blocks are coalesced into larger ones. In this manner, fragmentation is greatly minimised.

The memory allocator is shared between the VM, for any of its operations, and Java.  Therefore, all internal 'C' memory usage and Java memory usage is 'unified'. The memory used by the VM is garbage collected in the same way at the same time as the memory used by running Java code.

The VM allocator imposes a 16mb heap maximum.  This is to cut down on the overhead associated with tracking memory allocations, including Java objects.  The heap allocator has an overhead of 4 bytes per memory allocation, and 24 of those 32 bits is used to specify the size of an allocation.  2^24 is 16mb.

Each request for memory has an 'allocation type' associated with it.  Allocation types help the garbage collector know how to scan the memory looking for references.  The allocator also assists the collector by marking each memory allocation as 'white' for the three-color collection scheme.  See [Garbage Collection](garbage_collection.md).

The `heap.c` unit documentation has a very in-depth explanation of how the memory allocator works.  Down to the bit level. 

