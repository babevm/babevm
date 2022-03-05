# The Interpreter

Key files:

| unit                                                                        | docs                          |
|-----------------------------------------------------------------------------|-------------------------------|
| [exec.c](https://github.com/babevm/babevm/blob/master/src/c/exec.c){:target="_blank"}   | [link](doxygen/html/exec_8c.html) |
| [frame.c](https://github.com/babevm/babevm/blob/master/src/c/frame.c){:target="_blank"} | [link](doxygen/html/frame_8c.html) |

The interpreter is a 'classic' large switch statement - albeit with some things to help speed things up.  

When the VM starts it performs its initialisation, finds the starting Java 'main' method and then launches into the 'interp' loop .... and there it stays until the VM exits.  All threads share this one running loop and are swapped in and out (see [Threading](threads.md))

Some non-standard Java opcodes exist within the 'switch'.  Mostly, these are 'fast' implementations of a number of standard opcodes. Often, when a given opcode within a given method is executed, a number of checks are performed and pointers are resolved and so on.  These are not required to be re-checked or re-resolved on subsequent executions of that particular bytecode. In this case, the VM substitutes the bytecode for another 'fast' version of the same functionality.  The next time the bytecode is to be executed, the faster version is executed instead.

If the VM is compiled with debugger support using `BVM_DEBUGGER_ENABLE`, this 'fast' bytecode substitution does not take place.  The debugger facilities need to perform its own bytecode substitution for 'breakpoint' and this is incompatible with the VM swapping stuff about.  

For the GCC toolchain (and possibly others), the compiler define `BVM_DIRECT_THREADING_ENABLE` will enable the use of GCC 'direct threading' for a boost in bytecode dispatch speed.  If not defined, a normal 'switch' block is used in code.



