# Longs

Key files:

| unit                                                                               | docs                               |
|------------------------------------------------------------------------------------|------------------------------------|
| [int64.c](https://github.com/babevm/babevm/blob/master/src/c/int64.c){:target="_blank"}        | [link](doxygen/html/int64_8c.html) |
| [int64.h](https://github.com/babevm/babevm/blob/master/src/h/int64.h){:target="_blank"}        | [link](doxygen/html/int64_8h.html) |
| [xp.c](https://github.com/babevm/babevm/blob/master/src/thirdparty/xp/xp.c){:target="_blank"}  | [link](doxygen/html/xp_8c.html)    |

The 64 bit java 'long' primitive type is supported both in a native for as well as an emulated form (by using two 32bit integers).

If the target platform has a native 64 bit integer type then Babe can use it - the compiler define `BVM_NATIVE_INT64_ENABLE` set to '1' (the default) controls it.  Set it to '0' to emulate 64 bit integers using the logic in `xp.c`.    

The target platform must have at least 32 bit integer types.  In code, you'll see operations on those using normal C operations for addition, subtraction and so on.  However, as Java longs could either be a native 64 bit type, or (for this VM) a struct of two 32 bits for emulation, all operations on 64 bit types are done via functions and macros.  For example, to negate a value the `BVM_INT64_negate(x)` macro would be used, where `x` is `bvm_uint_64` - this would translate to `-x` for a native 64 bit type. All 64 bit operations and comparisons macros begin with `BVM_INT64_`.  There are two set of these macros, one to support native 64 bit integers, and the other for emulated 64 bit.     

When using float/double support, there is no choice - native 64bit long usage is a prerequisite.

As a side note, it is worth noting that in Java bytecode, 64 bit integers and doubles are actually represented as 2 32 bit integers.





