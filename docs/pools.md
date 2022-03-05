### Pooling Resources

Key files:

| unit                                                                                                | docs                                           |
|-----------------------------------------------------------------------------------------------------|------------------------------------------------|
| [pool_clazz.c](https://github.com/babevm/babevm/blob/master/src/c/pool_clazz.c){:target="_blank"}               | [link](doxygen/html/pool_clazz_8c.html)        |
| [pool_internstring.c](https://github.com/babevm/babevm/blob/master/src/c/pool_internstring.c){:target="_blank"} | [link](doxygen/html/pool_internstring_8c.html) |
| [pool_nativemethod.c](https://github.com/babevm/babevm/blob/master/src/c/pool_nativemethod.c){:target="_blank"} | [link](doxygen/html/pool_nativemethod_8c.html) |
| [pool_utfstring.c](https://github.com/babevm/babevm/blob/master/src/c/pool_utfstring.c){:target="_blank"}       | [link](doxygen/html/pool_utfstring_8c.html)    |

A number of 'pools' exist within the VM.  A pool is actually a cache in the form of a hash map structure.  There is no generic hash map structure - each pooled 'thing' has a 'next' member of its structure to help with pooling.  This makes pooling much smaller and much faster at the possible expense of a some code redundancy here and there (but not much).  The VM pools classes, strings, and native methods definitions among other things.

