# The Native Interface

Key files:

| unit                                                                                   | docs                          |
|----------------------------------------------------------------------------------------|-------------------------------|
| [ni.c](https://github.com/babevm/babevm/blob/master/src/c/int64.c){:target="_blank"}               | [link](doxygen/html/ni_8c.html) |
| [native.c](https://github.com/babevm/babevm/blob/master/src/thirdparty/xp/xp.c){:target="_blank"}  | [link](doxygen/html/native_8c.html) |

A native interface (NI) API is provided that is modelled on the java Native Interface (JNI).  I say 'modelled' because it is not possible to provide a complete JNI implementation on such a small VM.  The JNI *assumes* dynamically loadable libraries such as DLLs on windows or shared objects on Linux.  The Babe VM assumes these facilities are not present and that all libraries are statically linked.

Additionally, the green threads implementation means that java methods cannot be called from C code.  This precludes a portion of the JNI specification.  However, having said that, the Babe native interface implements most of the JNI API.  

The Babe NI removes the notion of a JNI-like 'environment'.  In JNI, the 'environment' is passed to all functions - it is a C interface struct containing method pointers.  The Babe NI has no requirement for such an interface.

However, like the JNI, and unlike the rest of the Babe VM, Babe NI functions have return values and do not throw exceptions.  Error codes must be checked and exceptions must be handled manually.  This is a break from Babe VM convention, but it is as per the JNI specification.

The `native.c` unit contains the C implementations of the native methods in the Babe core lib.

