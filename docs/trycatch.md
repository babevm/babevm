# Try / Catch and Exceptions

Key files:

| unit                                                                               | docs                                  |
|------------------------------------------------------------------------------------|---------------------------------------|
| [trycatch.c](https://github.com/babevm/babevm/blob/master/src/c/trycatch.c){:target="_blank"}  | [link](doxygen/html/trycatch_8c.html) |
| [trycatch.h](https://github.com/babevm/babevm/blob/master/src/h/trycatch.h){:target="_blank"}  | [link](doxygen/html/trycatch_8h.html) |

All Java try/catch semantics are implemented. 

The VM C code uses exceptions in the same way as does Java.  An exception thrown in C will propagate up to the Java code level if it is uncaught in the VM.  In this way, the VM can treat its own errors as Java errors and will present them to the Java layer if the C code does not deal with them.  For example, when executing (say) the bytecode to put a value into an array index, if the array object is `NULL` the VM code can throw a Java `NullPointerException` and, if uncaught in the VM, that exception will be thrown at the java layer as exactly the same exception object. and then Java try/catch semantics apply. 

Using a try / catch mechanism at the VM level has obviated the need for the usual development pattern in C of using return codes to indicate errors and using pointer arguments to pass back return values.  In this VM, generally, functions return properly typed meaningful values, and not return codes.  If something goes wrong, an exception is thrown.  It is simple (ish) in its implementation and works surprisingly well.  It does mean the compiler and target environment must support the ANSI C `setjmp()` and `longjmp()` functions.  It also makes the code very clean and much easier to read.

Usually, one of the key reasons for using the traditional 'functions return codes for errors' development pattern is not to alter the program flow unexpectedly and thus give the developer an opportunity to 'clean up' when things go wrong. 'Clean up' normally means 'memory clean up' to avoid leaks.  With this VM, the memory cleanup is automatic so there are no leaks on exceptions.

How? It ties in with the memory allocator ([memory](memory.md)) and its notions of the 'transient stack'.   When an exception is thrown all transient memory references since the last 'try' are discarded and become eligible for garbage collection.  In reality, a try / catch is like a transient block.  

Try/catch blocks in C code can be nested, just like Java, and conceptually the same except for some small differences - at a C code 'catch' there is no notion of only catching a particular exception type - everything is caught - much like catching `Throwable` in Java.  Additionally, there is no 'finally'.

But, what this ultimately adds up to is a much less code for handling errors conditions.  Like Java, the VM C code is generally written in such a manner as to assume that everything will always work as expected, and leaves it to the exception handing mechanism to deal with problems.  This would not have been possible without the VM being garbage collected like Java and the use of 'transient references'.

