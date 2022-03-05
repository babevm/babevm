# Threads

Key files:

| unit                                                                          | docs                                |
|-------------------------------------------------------------------------------|-------------------------------------|
| [thread.c](https://github.com/babevm/babevm/blob/master/src/c/thread.c){:target="_blank"} | [link](doxygen/html/thread_8c.html) |
| [exec.c](https://github.com/babevm/babevm/blob/master/src/c/exec.c){:target="_blank"}     | [link](doxygen/html/exec_8c.html)   |
| [frame.c](https://github.com/babevm/babevm/blob/master/src/c/frame.c){:target="_blank"}   | [link](doxygen/html/frame_8c.html)  |


The VM fully implements java threading semantics.  

Threading here is independent of any underlying threading services the platform operating system may provide.  This is to keep things as simple and portable as possible and provide threads to Java applications running on platforms that do not have threads.  Implementing threads above the OS (or 'green' threads as they are usually called) does provide consistent portability, however this comes at some internal complexity - especially around the VM method call stack, and also when debugging - debuggers always suspend and resume threads.  In a 'green' thread implementation this can cause a few thread 'gymnastics' occasionally.  Hopefully, the unit documentation helps. 

Daemon threads are also supported.  They will shut down when all non-daemon threads have terminated.  This provides a means to have (say) a background thread handling and enqueuing events from the operating system, but will not hold up VM shutdown.   

Greens thread do not permit calling Java methods from the VM C code.  This creates some complications at times when the VM *must* call Java methods - like when a thread `run()` method is called, or `Class.newInstance()` must have an object constructor called and so on.  Hopefully the documentation at each point helps the developer to understand the intention and the mechanism.

Although quite complete, the VM threading implementation is reasonably simple.  The scheduling algorithm is based on each thread having a number of bytecodes to execute before a thread 'context switch' takes place. The number of bytecodes each thread executes before being swapped is related to the thread's priority.  All very simple.  Additionally, thread groups are not supported - they do not seem to make too much sense in a small VM.

The 'bytecode count' for thread switching is the `BVM_THREAD_TIMESLICE` compiler define.  Note that when the dubugger code is compiled in the timeslice count is lower. 

All threads have their own call stacks but share the one interpreter loop.  Thread stacks start small then grow and shrink as required. That is, if additional stack space is needed it will be allocated it from the shared heap and, when no longer required, given back to the heap. A thread call stack is a linked list of stack 'segments'.

The docs link above provides a far more detailed explanation of the implementation.