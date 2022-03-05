# Garbage Collection.

Key files:

| unit                                                                                | docs                                  |
|-------------------------------------------------------------------------------------|---------------------------------------|
| [heap.c](https://github.com/babevm/babevm/blob/master/src/c/heap.c){:target="_blank"}           | [link](doxygen/html/heap_8c.html)     |
| [collector.c](https://github.com/babevm/babevm/blob/master/src/c/collector.c){:target="_blank"} | [link](doxygen/html/collector_8c.html) |

The garbage collector (GC) is a mark-and-sweep collector.  Its role is to determine what memory is no longer used and return it to the VM heap.  The GC uses a tricolour scheme to mark references grey while marking, then black. Unmarked references are left as white and freed.  When memory is requested from the allocator, a 'type' is specified.  The type is used by the garbage collector to determine how to scan the memory for other memory references.  The memory returned by the allocator is marked 'white'. 

The GC mechanisms are pretty straight forward one the 'allocate type' is known.  More interesting is how the GC determines its roots for reference marking.

Along with each thread's method call stack (which are 'root' sources of references) the VM maintains two stacks used for 'permanent' and 'transient' root references.  During the GC mark phase, each of these stacks is traversed and each reference they contain is scanned and marked.  The permanent root stack contains memory references that are unchanging during the lifetime of the VM - like some pre-built structures the VM uses during execution.

The transient root stack contains transient references that are momentary in existence.  A transient reference is valid during a block of code called a 'transient block'. After a transient code block has exited, any transient references created within it are no longer considered as 'roots' and are eligible for GC.  Thus, the size of the transient stack grows and shrinks as these transient blocks are entered and exited.

Weak reference objects are supported as per the CLDC 1.1 specification.

The sizes of the permanent and transient stacks can be set at VM startup as command line options.  To see command line options, run the vm executable with no arguments, or the `-usage` argument. 

