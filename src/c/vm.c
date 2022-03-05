/*******************************************************************
*
* Copyright 2022 Montera Pty Ltd
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*    http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
*******************************************************************/

/**
 * @file
 *
 * VM startup, command line, and shutdown functions.
 *
 */

/**

  @mainpage

  The Babe Virtual Machine.

  @section vm-welcome Welcome to the Machine.

  First, Oracle and Java are registered trademarks of Oracle and/or its affiliates.  This VM has no association
  with Oracle and does not claim to be Java or 'Java compatible'.

  The Babe Virtual Machine (aka BVM) is an implementation of the Oracle Java Virtual Machine Specification (JVMS) 1.6.
  This VM executes Java bytecode according to the JVMS and has a supporting runtime support library of classes
  to support the operational VM.

  The BVM is intended for small environments that require a small and fast means of running Java bytecode - albeit
  without skimping on the good bits.

  The overall objectives of the BVM implementation are to provide a fast, small, full-featured Java Virtual
  Machine and associated core classes.

  The target environment is small secure devices like a payments terminal.  Indeed, many of the decisions
  regarding the VM have taken this environment into account - for example - in the name of closing security doors, class
  files are not read at runtime by java classes and handed to the VM.  In this case, the VM handles
  all class file reading so no developer can create or load classes at runtime that are not deployed with
  the application.  Reflection is not available.  Additionally, the notion of runtime protection of code
  is implemented.

  An overview of the features of the Babe VM.

  @li Java 6.  Runs code compiled with a Java 6 compiler, or a compiler configured to produce Java 6 classes.
  @li 32 bit and 64 bit.
  @li small - about 150k or so.
  @li Every JVMS opcode is implemented.  Doubles, longs, thread monitors etc.
  @li threads - a complete (green) thread implementation with all locking / synchronisation.
  @li memory efficient - it is used very sparingly ...
  @li performant - it ain't a slug.
  @li portable -  intended to be portable across OSs and architectures.  It is developed in ANSI C (90) with
  very few standard lib functions used.
  @li garbage collected - a full GC implementation (except for Java finalizers).  Collection of Class objects
  and Classloader objects is supported.
  @li unicode - from the ground up.
  @li longs - Native 64 bits longs are supported - even on 32 bit platforms. Java longs can be emulated on small
  architectures that do not support 64 bit integers.
  @li stack traces - Familiar feeling stack-traces are implemented.
  @li Native Interface - a 'JNI' implementation similar to that of the Java Native Interface albeit particular to this VM.
  @li jars - support for reading class files from jars.
  @li classLoaders - loaded java classes are namespaced by their class loader as per the JVMS.
  @li unified memory management - the running VM uses the same memory primitives as the Java bytecode and
  also the same Garbage Collector.  The 'C' VM is garbage collected just like Java is (and at the same time).  The memory
  allocator is a "coallescing best fit" algorithm.
  @li intern strings - supported.
  @li class/member access visibility - class and member visibility semantics are fully implemented (some tiny VMs treat everything as 'public').
  @li class assignment - all JVMS class assignment rules are implemented - some small VMs skip here
  @li limited class verification implemented - focusing around language security, not class file correctness.
  @li dynamic thread stacks - thread stacks start as small as they can be and grow and shrink as required.  A thread
  stack can be as little as 128 bytes and still be useful.
  @li exceptions - full support for try/catch/finally and thread uncaught exception handlers.  VM exceptions and
  bytecode exceptions use the same handling code.
  @li weakReferences - support as per CLDC 1.1
  @li 'direct threading' - when using GCC, the interpreter switch use the GCC 'labels as values' to speed up opcode
  dispatch.

  Last but not least is that the Babe VM also implements remote debugging and so it may be debugged using a standard
  development IDE like Intellij or VSCode or Eclipse.  Remote debugging is by way of a Java Debug Wire Protocol (JDWP)
  implementation.  The JDWP is a standard part of the Java Platform Debug Architecture (JPDA).  This VM implements all
  sensible parts of it for a small VM.

  Some noteable Limitations:

  @li 16mb heap limit max.  The memory allocator uses 24 bits to express the size of a memory chunk.  Each memory chunk
  has a single 32bit header of which 24bits represent the size of the memory chunk.  Given the target
  environment, it is rare that this VM will have 16m allocated to it.
  @li class loaders are not responsible for reading class file bytes.  They serve primarily as
  a namespace for loaded classes and thus 'application' separation.  All class file reading is performed by the VM.
  @li class loading is restricted to files that can be found either on the class path or are contained within a jar
  file on the class path.  Jar files must end with a lowercase ".jar".
  @li the heap does not grow or shrink - the heap size is declared on startup and its size remains constant.

  @section requirements Requirements

  The Babe VM has the following requirements:

  @li the OS platform must be able to allocate dynamic memory.
  @li a file system of some sort must be available - supports an ANSI C file interface, but others can be
  implemented.
  @li the platform C compiler/libraries must support the ANSI C \c setjmp() and \c longjmp() functions.
  @li at least 128k of memory available for the running VM heap.  It is certainly possible to run programs in less
  than this, but they will be (no doubt) trivial.  Memory usage is larger for 64 bit.
  @li a 32 bit integer type. Signed and unsigned.  For 64 bit same but with more bits ...
  @li a clock preferably one that gives millisecond timing.  The ability to (somehow) calculate the number of
  milliseconds since Jan 1 1970 must be available.

  @section vm-architecture-overview Architecture Overview

  The VM is, like all other Javas VMs, a 'Java bytecode interpreter'.  Given the 'small device' target
  environment, it has had to strike a balance between speed, size, and memory usage, and complexity to achieve what
  it does.  A key design goal was to keep the VM and core classes to about 100k each and yet provide as complete an
  implementation of the JVMS as possible .  Ultimately, software this small is a collection of compromises made to
  balance each of the competing concerns above.  There is no point in creating a platform that is very small, but
  very slow, or hyper-quick, but too large, or will not run on a 'standard' machine.  Also, there is no point is
  creating a system so complex and clever that no-one will ever be able to understand the code nor help maintain it.

  To this end, the key overriding design goal of this VM is simplicity.  Simple is small, often quick, and easy to
  understand. Having said that, a VM is quite often not a simple piece of software.  In this implementation there a few areas
  that seem complex, but hopefully, the unit documentation provides enough detail for a developer to understand the mechanisms
  and the reasoning behind them.

  In point form, what follows is a small discussion on a number of the key architectural aspects of the Babe VM.

  @subsection vm-intro-memory Memory management.

  The VM performs all its own memory management, including garbage collection.  At startup, the VM heap is allocated from
  the OS and it is never expanded nor shrunk during execution.  The memory allocator (found in heap.c) is meant for
  managing a small linear address space.  It assumes that issues such as locality-of-reference or paging are not actually
  present (as is often the case in smaller platforms).

  Once allocated, memory does not move - it is not compacted or reshuffled.   This means all memory references can be
  direct references and performance does not suffer from pointer indirections.  The allocator algorithm is coalescing.
  That is, adjacent free memory blocks are coalesced into larger ones. In this manner, fragmentation is greatly minimised.

  The memory allocator is shared between the VM, for any of its operations, and Java.  Therefore all
  internal 'C' memory usage and Java memory usage is unified. The memory used by the VM is garbage collected in the same
  way the memory the Java programs it runs is.  In this sense, the VM memory usage is "unified".

  The VM allocator imposes a 16mb heap maximum.  This is to cut down on the overhead associated with tracking memory
  allocations, including Java objects.  The heap allocator has an overhead of 4 bytes per memory allocation, and 24 of
  those 32 bits is used to specify the size of an allocation.  2^24 is 16mb.

  @subsection vm-intro-bvm_gc Garbage Collection.

  The garbage collector (GC) is a mark-and-sweep collector.  Its role is to determine what memory is no longer used and
  return it to the VM heap.  The GC uses a tricolour scheme to mark references grey while marking, then black.
  Unmarked references are left as white and freed.  When memory is requested from the allocator, a 'type' is specified.  The
  type is used by the garbage collector to determine how to scan the memory for other memory references.  Actually,
  the GC mechanisms are pretty straight forward.  More interesting is how the GC determines its roots for reference marking.

  Besides each thread's method call stack (which are 'root' sources of references) the VM maintains two stacks used for
  'permanent' and 'transient' root references.  During the GC mark phase, each of these stacks is traversed and each
  reference they contain is scanned and marked.  The permanent root stack contains memory references that are unchanging during
  the lifetime of the VM - like some pre-built structures the VM uses during execution.

  The transient root stack contains transient references that are momentary in existence.  A transient reference is valid during a
  block of code called a 'transient block'. After a transient code block has exited, any transient references created within
  it are no longer considered as 'roots' and are eligible for GC.  Thus, the size of the transient stack grows and shrinks as
  these transient blocks are entered and exited.

  WeakReference objects are supported as per the CLDC 1.1 specification.

  The size of the permanent and transient stacks can be set at VM startup.

  @subsection vm-intro-trycatch Try / Catch and Exceptions

  Augmenting the usage of transient blocks is a 'try/catch' mechanism with exceptions.  In VM C code, exceptions are
  thrown in much the same way as java exceptions are.  They can also be caught in VM code, or in Java code.  The exceptions
  thrown in the VM code are actually Java exception objects.

  An exception thrown in C will propagate up to the Java code level if it is uncaught in the VM.  In this way, the VM can treat
  its own errors as Java errors and they will be presented to the Java layer if the C code does not deal with them.  For example,
  when executing (say) the bytecode to put a value into an array index, if the array object is \c NULL the VM code can throw a
  Java \c NullPointerException and, if uncaught in the VM, that exception will be thrown at the java layer as same exception.

  Using a try / catch mechanism at the VM level has obviated the need for the usual development pattern in C of using return
  codes to indicate errors and using pointer arguments to pass back return values.  In this VM, generally, functions return
  properly typed meaningful values, and not return codes.  If something goes wrong, an exception is thrown.  It is simple (ish)
  in its implementation and works surprisingly well.  It does mean the compiler and target environment must support the
  ANSI C \c setjmp() and \c longjmp() functions.

  Usually, one of the key reasons for using the traditional 'functions return codes for errors' development pattern is not
  to alter the program flow unexpectedly and thus give the developer an opportunity to 'clean up' when things go
  wrong. 'Clean up' normally means 'memory clean up' to avoid leaks.  With this VM, the memory cleanup is automatic so there
  are no leaks.

  How? When a exception is thrown all transient references since the last 'try' are discarded and become eligible for
  garbage collection.  In reality, a try / catch is like a transient block.

  What this ultimately adds up to is a much less code for handling errors conditions.  The VM C code is written in such a
  manner as to assume that everything will always work as expected, and leaves it to the exception handing mechanism to deal
  with problems.  This would not have been possible without the VM being garbage collected like Java and the use of
  'transient references'.

  @subsection vm-intro-interp The Interpreter

  The interpreter is a classic large switch statement.  When the VM starts it performs its initialisation, finds the starting
  Java 'main' method and then launches into the 'interp' loop .... and there is stays until the VM exits.  All threads share this
  one running loop and are swapped in and out.

  Some non-standard Java opcodes exist within the 'switch'.  Mostly, these are 'fast' implementations of a number of standard opcodes.
  Often, when a given opcode within a given method is executed, a number of checks are performed and pointers are resolved and so
  on.  These are not required to be re-checked or re-resolved on subsequent executions of that particular bytecode. In this case,
  the VM substitutes the bytecode for another 'fast' version of the same functionality.  The next time the bytecode is to be
  executed, the faster version is executed instead.

  If the VM is compiled with debugger support, this 'fast' bytecode substitution does not take place.  The debugger facilities
  needs to perform its own bytecode substitution for 'breakpoint'.

  @subsection vm-intro-threads Threads

  The VM fully implements java threading.  Threading here is independent from any underlying threading services the
  platform operating system may provide.  This is to keep things as simple and portable as possible and provide threads to
  Java applications running on platforms that do not have threads.  Implementing threads above the OS (or 'green' threads as they
  are usually called) does provide consistent portability, however this comes at some internal complexity - especially around
  the VM method call stack, and especially when debugging - debuggers always suspend and resume threads.  In a 'green'
  thread implementation this can cause a few thread 'gymnastics' occasionally.

  Greens thread do not permit calling Java methods from the VM C code.  This creates some complications at times when the VM
  *must* call Java methods - like when a thread \c run() method is called, or \c Class.newInstance() must have an object constructor
  called and so on.  Hopefully the documentation at each point helps the developer to understand the intention and the mechanism.

  The limitation that VM code cannot call a Java method also means that a full Java Native Interface (JNI) implementation is
  not possible.

  Although complete, the VM threading implementation is reasonably simple.  The scheduling algorithm is based on each thread having a
  number of bytecodes to execute before a thread 'context switch' takes place. The number of bytecodes each thread executes before
  being swapped is related to the thread's priority.  All very simple.

  All threads have their own call stacks but share the one interpreter loop.  Threads stacks start small then grow and shrink as
  required. That is, if additional stack space is needed it will be allocated it from the shared heap and, when no longer required,
  given back to the heap. A thread call stack is a linked list of stack 'segments'.

  @subsection vm-intro-pool Pooling Resources

  A number of 'pools' exist within the VM.  A pool is actually a cache in the form of a hash map structure.  There is no generic hash
  map structure - each pooled 'thing' has a 'next' member of its structure to help with pooling.  This makes pooling much smaller and
  much faster at the possible expense of a some code redundancy here and there (but not much).  The VM pools classes, strings, and
  native methods definitions among other things.

  @subsection vm-intro-classLoading Class Loading

  In Java, a class is namespaced by the ClassLoader object that loaded it.  Two identically named classes loaded with different loaders are
  considered by the VM to be different classes.  This can be used to prevent class naming collisions between two running applications.  Just
  like Java SE, classloaders here form a hierarchy, and each time a class is to be loaded the parentage gets first go.

  Babe VM Classloaders use a classpath.  Classpaths may include '.jar' archive files.

  There are two distinct VM classpaths.  One is the 'boot' classpath.  The 'bootstrap' class loader is responsible for loading the core
  'java' and VM runtime classes.  Classes loaded on the boot classpath are trusted.

  The second classpath is the 'user' classpath.  This is the classpath for application and framework classes.  No class on the user
  classpath may belong to a package that begins with 'java/' or 'babe/'.  This ensures that no application or framework class can
  pretend to be a system class.

  Unlike Java SE class loading, a class loader here does not have the capability of loading a stream of bytes and handing that off to the
  VM for class loading.  In this VM, only the VM itself can load class files.  The classloaders serve as a means of specifying a
  classpath for the VM to search on - and as a container for a list of Class objects it has loaded - to stop them being GC'd.  We
  get the benefit of classloader namespacing, but the security of knowing that developers cannot manipulate (possibly certified)
  class files on-the-fly.

  Like Java SE, classloaders also serve as a mean to stop Class objects being GC'd.  According to the JVMS, classes may be unloaded when
  their classloader is unreachable and is eligible for GC.  This is exactly how it happen in this VM.  Classes and their
  associated internals class structures may be GC'd, but only in those prescribed circumstances.  This gives the VM the ability to completely
  unload an 'application' and all its classes and free up the memory for other 'applications'.

  The JVMS describes two types of relationships a classloader may have with a class.  A classloader may be the 'defining' loader for a given
  class (meaning it was the one that found the class in its classpath and actually loaded it), and/or it may be an 'initiating' classloader
  for a class (meaning it either loaded it, or attempted to load it but could not so it delegated it).  This VM does not track the 'initiating'
  relationship -  only the defining class loader relationship is maintained.

  @subsection vm-intro-jarfiles Jar Files.

  Class loading from jar files is supported.  The classloader's classpath may include files that end in lowercase '.jar' (case sensitive).  The
  inflate code is very small and (no doubt) not as efficient as some implementations, but it adds only 2.5k to the VM.  This is less than 10% of
  the size of most standard inflate libs such as zlib.

  The VM implements a very small directory cache for each jar it opens.  The entire directory is not cached,
  just some small parts of it that help the VM to locate files more quickly, for example, rather than cache file names in the jar, just the
  hash of the name is, and an offset into the directory.  In that way when looking up an entry only the cache may be scanned for matching
  hashes, and on a match, further inspection to see it if really is the requested file.  If it is, the directory offset gives where to get the
  rest of the file information.

  Jar files are not closed by the VM, once on the classpath, they are open for the duration of the running VM, and their cached
  directories are also.

  @subsection vm-intro-longs Longs

  The 64 bit java 'long' primitive type is supported both in emulated form as well as native form.  That is, if the target platform has a
  native 64 bit integer type then Babe can use it.  Otherwise, 64 bit integer support is emulated using two 32 bit integers and
  'arbitrary precision' mathematics.

  When using float support, there is no choice - native 64bit long usage is a prerequisite.

  @subsection nativeInterface The Native Interface

  A native interface (NI) API is provided that is modelled on the java Native Interface (JNI).  I say 'modelled' because it is not possible
  to provide a complete JNI implementation on such a small VM.  The JNI *assumes* dynamically loadable libraries such as DLLs on windows or
  shared objects on Linux.  The Babe VM assumes these facilities are not present and that all libraries are statically linked.

  Additionally, the greens threads implementation means that java methods cannot be called from C code.  This precludes a portion of the
  JNI specification.  However, having said that, the Babe native interface provides *most* of the JNI API.  Indeed, the method
  comments on the NI ape the specification.

  The Babe NI has no notion of an 'environment' like the JNI does.  In JNI the 'environment' is passed to all functions - it is
  a C interface struct containing method pointers.  The Babe NI has no requirement for such an interface.  Additionally, the Babe
  NI provides no facilities for 'global' memory allocations.

  Like the JNI, and unlike the rest of the Babe VM, Babe NI functions have return values and do not throw exceptions.  Error codes
  must be checked and exceptions must be handled manually.  This is a break from Babe VM convention, but it is as per the JNI
  specification.

  @subsection vm-intro-debugger Debugging

  An implementation of the Java Debug Wire Protocol is included.  Almost all standard facilities are there, as well as some optional
  ones (like inspecting thread monitors and getting raw bytecodes).  Including JDWP means standard debuggers such as Intellij, VSCode or
  Eclipse can attach to a running Babe VM instance (or vice-versa) to perform source level debugging with all breakpoint
  and variable inspection facilities.  Additionally, initial support for JSR045 "Debugging Other Languages" is included, so
  that a debugger can debug bytecodes / source written in a non-Java language.

  The JDWP debigger-support code can be optionally compiled in/out on the VM.  Depending on the compiler, the size of this code
  may vary but the debugger support code is fairly substantial.  Consider excluding it for production builds - the VM will be smaller
  and will run faster - some performance optimisations are not possible when debugger code is included.

  The transport layer of the debugger support is abstracted so that any transport may be implemented.  A sockets implementation is
  provided, but (in theory) the transport could be anything from shared-memory to dial-up.  Both client and server modes are supported
  by the transport abstraction meaning that the VM (in client mode) can 'attach' to a running debugger that is waiting for a incoming
  connection, or (in server mode) listen for a debugger to connect to it.  The attach and listen addresses can be set using command
  line arguments.

  The debugger code does complicate things sometimes.  In a VM with native threads the interaction with a debugger might be simpler in
  the sense when the debugger suspends a thread it really is suspended.  The whole running java/C thread is suspended.  With green
  threads it is a little more complex and sometimes the VM has to inspect a thread's 'suspend' status after an interaction with the debugger
  to determine what it should do next.  This can cause some complexity, but hopefully the code comments describe what is going on well enough.

  Developers should note that debuggers interact really quite a lot with running VMs.  Lots of small packets go between debugger
  and VM - often multiple times for exactly the same thing.  Before attempting to provide 'on-platform' debugger communications it may be
  worth considering whether 'off-platform' debugging provides what is needed.

  Refer : @ref debug-ov.

 */

/**

 @file

 @author Greg McCreath
 @since 0.0.10

 VM initialisation, startup and shutdown.

 */

#include "../h/bvm.h"

#if (BVM_CONSOLE_ENABLE || BVM_DEBUGGER_ENABLE )
const char *BVM_VM_VERSION = "0.6.0";
const char *BVM_VM_DESC = "Babe VM";
const char *BVM_VM_NAME = "BabeVM";
const char *BVM_VM_COPYRIGHT = "Copyright 2022 Montera Pty Ltd.";
#endif

/** Handle to the command line boot classpath with default. */
#if BVM_PLATFORM_FILE_SEPARATOR == '/'
char *bvm_gl_boot_classpath = "./rt.jar:../lib/rt.jar";
#else
char *bvm_gl_boot_classpath = ".\\rt.jar;..\\lib\\rt.jar";
#endif

/** Handle to the command line user classpath.  Default is the current directory as represented by ".". */
char *bvm_gl_user_classpath = ".";

/** An array used to hold the segments of the command line boot classpath (as delimited by
 * a #BVM_PLATFORM_PATH_SEPARATOR character). The length is #BVM_MAX_CLASSPATH_SEGMENTS. Populated during VM initialisation.
 * If less than \c BVM_MAX_CLASSPATH_SEGMENTS elements are in the array, the final element will be \c NULL - this can be used
 * when traversing to identify the end of the segments list. */
char *bvm_gl_boot_classpath_segments[BVM_MAX_CLASSPATH_SEGMENTS];

/** An array used to hold the segments of the command line user classpath (as delimited by
 * a #BVM_PLATFORM_PATH_SEPARATOR character). The length is #BVM_MAX_CLASSPATH_SEGMENTS. Populated during VM initialisation.  If less
 * than \c BVM_MAX_CLASSPATH_SEGMENTS elements are in the array, the final element will be \c NULL - this can be used
 * when traversing to identify the end of the segments list. */
char *bvm_gl_user_classpath_segments[BVM_MAX_CLASSPATH_SEGMENTS];

/**
 * Home file path of the VM - may be \c NULL (the default).  If not \c NULL, all file opening performed by the VM, including
 * the java.io.File class, will have this home path prepended to it before a file open is attempted.  Effectively,
 * this provides a mean of giving the VM a 'root' directory.
 */
char *bvm_gl_home_path = NULL;

/**
 * Reports if the VM has finished initialising.  Will be #BVM_TRUE only when the VM has completed startup
 * and initialisation.
 */
bvm_bool_t bvm_gl_vm_is_initialised = BVM_FALSE;

bvm_method_t *BVM_METHOD_NOOP =  NULL;
bvm_method_t *BVM_METHOD_NOOP_RET  =  NULL;
bvm_method_t *BVM_METHOD_CALLBACKWEDGE = NULL;

/**
 * Default stack size in cells.  Defaults to #BVM_THREAD_STACK_HEIGHT.
 */
bvm_uint32_t bvm_gl_stack_height = BVM_THREAD_STACK_HEIGHT;

/**
 * Determines if java language assertions are enabled.  If #BVM_ASSERTIONS_ENABLE is defined, defaults
 * to #BVM_TRUE.  Can be set on command line by using \c -ea.
 */
#if BVM_ASSERTIONS_ENABLE
bvm_bool_t bvm_gl_assertions_enabled = BVM_TRUE;
#else
bvm_bool_t bvm_gl_assertions_enabled = BVM_FALSE;
#endif

/*
 * Class names of Java exceptions thrown or used by the VM.
 */
const char *BVM_ERR_ABSTRACT_METHOD_ERROR 					= "java/lang/AbstractMethodError";
const char *BVM_ERR_ARITHMETIC_EXCEPTION 					= "java/lang/ArithmeticException";
const char *BVM_ERR_ARRAY_INDEX_OUT_OF_BOUNDS_EXCEPTION 	= "java/lang/ArrayIndexOutOfBoundsException";
const char *BVM_ERR_ARRAYSTORE_EXCEPTION 					= "java/lang/ArrayStoreException";
const char *BVM_ERR_CLASS_CAST_EXCEPTION 					= "java/lang/ClassCastException";
const char *BVM_ERR_CLASS_CIRCULARITY_ERROR 				= "java/lang/ClassCircularityError";
const char *BVM_ERR_CLASS_FORMAT_ERROR 						= "java/lang/ClassFormatError";
const char *BVM_ERR_CLASS_NOT_FOUND_EXCEPTION 				= "java/lang/ClassNotFoundException";
const char *BVM_ERR_CLONE_NOT_SUPPORTED_EXCEPTION       	= "java/lang/CloneNotSupportedException";
const char *BVM_ERR_ERROR 									= "java/lang/Error";
const char *BVM_ERR_EXCEPTION_IN_INITIALIZER_ERROR 			= "java/lang/ExceptionInInitializerError";
const char *BVM_ERR_ILLEGAL_ACCESS_EXCEPTION 				= "java/lang/IllegalAccessException";
const char *BVM_ERR_ILLEGAL_ACCESS_ERROR 					= "java/lang/IllegalAccessError";
const char *BVM_ERR_ILLEGAL_THREAD_STATE_EXCEPTION      	= "java/lang/IllegalThreadStateException";
const char *BVM_ERR_ILLEGAL_MONITOR_STATE_EXCEPTION     	= "java/lang/IllegalMonitorStateException";
const char *BVM_ERR_ILLEGAL_ARGUMENT_EXCEPTION          	= "java/lang/IllegalArgumentException";
const char *BVM_ERR_INTERRUPTED_EXCEPTION               	= "java/lang/InterruptedException";
const char *BVM_ERR_INCOMPATIBLE_CLASS_CHANGE_ERROR 		= "java/lang/IncompatibleClassChangeError";
const char *BVM_ERR_INDEX_OUT_OF_BOUNDS_EXCEPTION 			= "java/lang/IndexOutOfBoundsException";
const char *BVM_ERR_STRING_INDEX_OUT_OF_BOUNDS_EXCEPTION 	= "java/lang/StringIndexOutOfBoundsException";
const char *BVM_ERR_INSTANTIATION_ERROR 				 	= "java/lang/InstantiationError";
const char *BVM_ERR_INSTANTIATION_EXCEPTION 			 	= "java/lang/InstantiationException";
const char *BVM_ERR_INTERNAL_ERROR 							= "java/lang/InternalError";
const char *BVM_ERR_NEGATIVE_ARRAY_SIZE_EXCEPTION 			= "java/lang/NegativeArraySizeException";
const char *BVM_ERR_NO_CLASS_DEF_FOUND_ERROR 				= "java/lang/NoClassDefFoundError";
const char *BVM_ERR_NO_SUCH_FIELD_ERROR 					= "java/lang/NoSuchFieldError";
const char *BVM_ERR_NO_SUCH_METHOD_ERROR 					= "java/lang/NoSuchMethodError";
const char *BVM_ERR_NULL_POINTER_EXCEPTION 					= "java/lang/NullPointerException";
const char *BVM_ERR_UNSATISFIED_LINK_ERROR 					= "java/lang/UnsatisfiedLinkError";
const char *BVM_ERR_OUT_OF_MEMORY_ERROR 					= "java/lang/OutOfMemoryError";
const char *BVM_ERR_VIRTUAL_MACHINE_ERROR 					= "java/lang/VirtualMachineError";
const char *BVM_ERR_VERIFY_ERROR 							= "java/lang/VerifyError";
const char *BVM_ERR_STACK_OVERFLOW_ERROR 					= "java/lang/StackOverflowError";
const char *BVM_ERR_UNSUPPORTED_OPERATION_EXCEPTION 		= "java/lang/UnsupportedOperationException";
const char *BVM_ERR_IO_EXCEPTION 							= "java/io/IOException";
const char *BVM_ERR_FILE_NOT_FOUND_EXCEPTION 				= "java/io/FileNotFoundException";

/* The count of the number of system properties in the command line. */
static int cmdline_nr_system_properties = 0;

/* An array of pointers to the command line system properties. This will only get a value if there
 * are actually system properties */
static char **cmdline_system_properties = NULL;

#if BVM_CONSOLE_ENABLE

/**
 * Print version and copyright information to the console.
 */
void bvm_show_version_and_copyright() {
    char *bits = (sizeof(void *) == 4) ? "32bit" : "64bit";
	bvm_pd_console_out("%s, version %s - %s.  %s\n", BVM_VM_NAME, BVM_VM_VERSION, bits, BVM_VM_COPYRIGHT);
}

#endif

/**
 * During VM init, when loading the constants for classes *before* the \c java/lang/String class is loaded, the
 * #BVM_STRING_CLAZZ global var it not yet set.  We use \c BVM_STRING_CLAZZ for speedy direct access to the String
 * clazz rather than doing a class pool lookup.  The \c Object, \c Class, and \c String classes have string constants within
 * them. During constant pool loading of these classes, like any other class, we are creating interned \c String objects
 * for those constant strings.  The trouble is, with #BVM_STRING_CLAZZ not yet being populated, all the interned Strings
 * for these classes (Object/Class/String) have their class set to something that does not yet exist.
 *
 * So, this function is called after the Object/Class/String classes have been loaded and is intended to correct the
 * above situation.
 *
 * Here we'll loop through the constants of a given class and find the String constants (they will all
 * be interned) and set their class to #BVM_STRING_CLAZZ, and also to correct the clazz of its associated
 * char[] field (which also had not been set when these classes where created).
 *
 * Yes, as the name says, this is an annoying fudge, but to be honest, the speed advantage of directly accessing
 * these commonly used clazzes outweighs the clunkiness  of this little kludge - frankly.
 *
 */
static void vm_annoying_fudge_to_populate_class_String_interns(bvm_instance_clazz_t *clazz) {

	bvm_clazzconstant_t *constant;

	/* get the number of constants - this is actually the first constant in the
	 * clazz constant pool array */
	bvm_uint16_t count = (bvm_uint16_t) clazz->constant_pool[0].data.value.int_value;

	/* for each constant ... */
	while (count--) {

		/* retrieve it */
		constant = &clazz->constant_pool[count];

		/* if it is a String constant .. do the magic */
		if (BVM_CONSTANT_Tag(clazz,count) == BVM_CONSTANT_String) {

			/* get the interned string */
			bvm_internstring_obj_t *str = (bvm_internstring_obj_t *) constant->data.value.ptr_value;

			/* correct its class */
			str->clazz = (bvm_clazz_t *) BVM_STRING_CLAZZ;

			/* and also correct the class of its char array */
			str->chars->clazz = bvm_gl_type_array_info[BVM_T_CHAR].primitive_array_clazz;;
		}
	}
}

#if BVM_DEBUGGER_ENABLE

/**
 * Flag a given field for a given class as 'native'.  Only used when debugging.  Marking a field native is
 * a signal to the VM debugger code that the field is intended for use by the VM and should not be
 * accessible or changeable by a debugger.
 *
 * The field is marked by introducing the #BVM_FIELD_ACCESS_FLAG_NATIVE flag to its flags member.
 *
 * @param clazz a given clazz
 * @param fieldname the name of the field to mark
 */
static void vm_mark_field_as_native(bvm_instance_clazz_t *clazz, char *fieldname) {

	int i = clazz->fields_count;

	while (i--) {

		bvm_field_t *field = &(clazz->fields[i]);

		if (strcmp(fieldname, (char *) field->name->data) == 0) {
			field->access_flags |= BVM_FIELD_ACCESS_FLAG_NATIVE;
			return;
		}
	}
}

#endif

/**
 * As part of VM init a number of clazzes are pre-loaded.  This is to save clazz lookup effort later on.  Mostly, these clazzes will
 * have direct handles to them stored somewhere so they do not need to be looked up.  For example, why look up the \c Object class time
 * and time again when life would be so much faster if we could just store a direct handle to it.  Handles are global handles.
 *
 * Information about array clazzes are initialised and stored into an array of #bvm_array_typeinfo_t
 * called #bvm_gl_type_array_info.  The #bvm_array_typeinfo_t structure stores the byte size of each element of its component type
 * as well as (for primitive array clazzes) a direct handle to the #bvm_clazz_t for the array class.
 *
 * All primitive array clazzes are initialised at startup and belong to the bootstrap class loader.
 *
 * The #bvm_gl_type_array_info array is indexed by the #bvm_jtype_t type.  So, retrieving  the element size of (say) a long array is
 * \c bvm_gl_type_array_info[BVM_T_LONG].element_size.  The clazz of same can be got by
 * \c bvm_gl_type_array_info[BVM_T_LONG].primitive_array_clazz.
 *
 * We're careful here with the order that clazzes are loaded.
 *
 */
static void vm_init_bootstrap_clazzes() {

	{
		bvm_gl_type_array_info[0].element_size          = 0;				/* unused */
		bvm_gl_type_array_info[BVM_T_OBJECT].element_size   = sizeof(bvm_obj_t *);
		bvm_gl_type_array_info[BVM_T_ARRAY].element_size 	= sizeof(bvm_obj_t *);
		bvm_gl_type_array_info[3].element_size 		    = 0;				/* unused */
		bvm_gl_type_array_info[BVM_T_BOOLEAN].element_size  = sizeof(bvm_uint8_t);
		bvm_gl_type_array_info[BVM_T_CHAR].element_size 	= sizeof(bvm_uint16_t);
		bvm_gl_type_array_info[BVM_T_FLOAT].element_size 	= sizeof(bvm_int32_t);
		bvm_gl_type_array_info[BVM_T_DOUBLE].element_size   = sizeof(bvm_int64_t);
		bvm_gl_type_array_info[BVM_T_BYTE].element_size 	= sizeof(bvm_uint8_t);
		bvm_gl_type_array_info[BVM_T_SHORT].element_size 	= sizeof(bvm_int16_t);
		bvm_gl_type_array_info[BVM_T_INT].element_size 	    = sizeof(bvm_int32_t);
		bvm_gl_type_array_info[BVM_T_LONG].element_size     = sizeof(bvm_int64_t);
	}

	/* pre-load some often-used clazzes and give them handles so we get 'em quick.  These
	 * are done in a specific order (eg, the interned string in Class requires the BVM_STRING_CLAZZ
	 * to be populated. */
	BVM_BOOTSTRAP_CLASSLOADER_OBJ = NULL;
	BVM_OBJECT_CLAZZ = (bvm_instance_clazz_t *) bvm_clazz_get_c(BVM_BOOTSTRAP_CLASSLOADER_OBJ, "java/lang/Object");

	/* populate an array of primitive clazzes at start up.  This array gives us speedy access to
	 * classes for primitive type based on the JVMS enum for types (BVM_T_OBJECT, BVM_T_CHAR, etc etc) */
	{
		bvm_gl_type_array_info[0].primitive_array_clazz              = NULL;
		bvm_gl_type_array_info[BVM_T_OBJECT].primitive_array_clazz   = NULL;
		bvm_gl_type_array_info[BVM_T_ARRAY].primitive_array_clazz    = NULL;
		bvm_gl_type_array_info[3].primitive_array_clazz 		 	 = NULL;
		bvm_gl_type_array_info[BVM_T_BOOLEAN].primitive_array_clazz  = (bvm_array_clazz_t *) bvm_clazz_get_c(BVM_BOOTSTRAP_CLASSLOADER_OBJ, "[Z");
		bvm_gl_type_array_info[BVM_T_CHAR].primitive_array_clazz 	 = (bvm_array_clazz_t *) bvm_clazz_get_c(BVM_BOOTSTRAP_CLASSLOADER_OBJ, "[C");
		bvm_gl_type_array_info[BVM_T_FLOAT].primitive_array_clazz    = (bvm_array_clazz_t *) bvm_clazz_get_c(BVM_BOOTSTRAP_CLASSLOADER_OBJ, "[F");
		bvm_gl_type_array_info[BVM_T_DOUBLE].primitive_array_clazz 	 = (bvm_array_clazz_t *) bvm_clazz_get_c(BVM_BOOTSTRAP_CLASSLOADER_OBJ, "[D");
		bvm_gl_type_array_info[BVM_T_BYTE].primitive_array_clazz 	 = (bvm_array_clazz_t *) bvm_clazz_get_c(BVM_BOOTSTRAP_CLASSLOADER_OBJ, "[B");
		bvm_gl_type_array_info[BVM_T_SHORT].primitive_array_clazz    = (bvm_array_clazz_t *) bvm_clazz_get_c(BVM_BOOTSTRAP_CLASSLOADER_OBJ, "[S");
		bvm_gl_type_array_info[BVM_T_INT].primitive_array_clazz 	 = (bvm_array_clazz_t *) bvm_clazz_get_c(BVM_BOOTSTRAP_CLASSLOADER_OBJ, "[I");
		bvm_gl_type_array_info[BVM_T_LONG].primitive_array_clazz 	 = (bvm_array_clazz_t *) bvm_clazz_get_c(BVM_BOOTSTRAP_CLASSLOADER_OBJ, "[J");
	}

	/* Preload the Class class - actually, it will already be loaded as a consequence of loading the Object class
	 * this call should just retrieve it from the clazz pool. */
	BVM_CLASS_CLAZZ = (bvm_instance_clazz_t *) bvm_clazz_get_c(BVM_BOOTSTRAP_CLASSLOADER_OBJ, "java/lang/Class");

#if BVM_DEBUGGER_ENABLE
	/* the 'vm_clazz' field in the Class class is described in Java code as an 'Object'.  Here, it is not. */
	vm_mark_field_as_native(BVM_CLASS_CLAZZ, "vm_clazz");
#endif

	/* preload the String class */
	BVM_STRING_CLAZZ = (bvm_instance_clazz_t *) bvm_clazz_get_c(BVM_BOOTSTRAP_CLASSLOADER_OBJ, "java/lang/String");

	/* see comments on annoying_fudge_to_populate_class_String_interns() function */
	vm_annoying_fudge_to_populate_class_String_interns(BVM_OBJECT_CLAZZ);
	vm_annoying_fudge_to_populate_class_String_interns(BVM_CLASS_CLAZZ);
	vm_annoying_fudge_to_populate_class_String_interns(BVM_STRING_CLAZZ);

	/* preload the System class */
	BVM_SYSTEM_CLAZZ = (bvm_instance_clazz_t *) bvm_clazz_get_c(BVM_BOOTSTRAP_CLASSLOADER_OBJ, "java/lang/System");

	/* a handle to a method that does nothing and returns void */
	BVM_METHOD_NOOP = bvm_clazz_method_get(BVM_SYSTEM_CLAZZ, bvm_utfstring_pool_get_c("noop", BVM_TRUE),
			                         		  bvm_utfstring_pool_get_c("()V", BVM_TRUE),
			                         		  BVM_METHOD_SEARCH_CLAZZ);

	/* the BVM_METHOD_NOOP_RET stuff is really a bit of a hack - notice the method desc
	 * actually does not have a return.  It is only used with newInstance() to fool the stack.  I
	 * hate it and hope I find a better way of doing this in the future  */
	BVM_METHOD_NOOP_RET = bvm_clazz_method_get(BVM_SYSTEM_CLAZZ, bvm_utfstring_pool_get_c("noop_ret", BVM_TRUE),
			                         			  bvm_utfstring_pool_get_c("()V", BVM_TRUE),
			                         			  BVM_METHOD_SEARCH_CLAZZ);

	BVM_METHOD_NOOP_RET->returns_value = 1;
	BVM_METHOD_NOOP_RET->max_stack = 1;

	/* the callback wedge is used as a marker in the stack to call a callback when
	 * we pop down to the frame that contains it.  Used for class init and thread termination */
	BVM_METHOD_CALLBACKWEDGE = bvm_clazz_method_get(BVM_SYSTEM_CLAZZ, bvm_utfstring_pool_get_c("callbackwedge", BVM_TRUE),
			                         			       bvm_utfstring_pool_get_c("(Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;)V", BVM_TRUE),
			                         			       BVM_METHOD_SEARCH_CLAZZ);

	/* preload the ClassLoader class */
	BVM_CLASSLOADER_CLAZZ = (bvm_instance_clazz_t *) bvm_clazz_get_c(BVM_BOOTSTRAP_CLASSLOADER_OBJ, "java/lang/ClassLoader");

	/* preload the Thread class */
	BVM_THREAD_CLAZZ = (bvm_instance_clazz_t *) bvm_clazz_get_c(BVM_BOOTSTRAP_CLASSLOADER_OBJ, "java/lang/Thread");

#if BVM_DEBUGGER_ENABLE
	/* the 'vm_thread' field in the thread class is described in Java code as an 'Object'.  Here, it is not. */
	vm_mark_field_as_native(BVM_THREAD_CLAZZ, "vm_thread");
#endif

	BVM_THROWABLE_CLAZZ = (bvm_instance_clazz_t *) bvm_clazz_get_c(BVM_BOOTSTRAP_CLASSLOADER_OBJ, "java/lang/Throwable");

#if BVM_DEBUGGER_ENABLE
	/* the '_backtrace' field in the Throwable class is described in Java code as an 'Object'.  It is not. */
	vm_mark_field_as_native(BVM_THROWABLE_CLAZZ, "_backtrace");
#endif
}

/**
 * During VM init, create a number of objects that the VM uses.
 */
static void vm_init_bootstrap_objects() {

	int i;

	bvm_instance_array_obj_t *patharray;
	bvm_instance_array_obj_t *classesarray;

	bvm_utfstring_t tempstr;

	/* pre-create an "out of memory" exception.  We'll have trouble making one of these if we
	 * actually *are* out of memory! */
	bvm_gl_out_of_memory_err_obj  = bvm_create_exception_c(BVM_ERR_OUT_OF_MEMORY_ERROR, NULL);
	BVM_MAKE_PERMANENT_ROOT(bvm_gl_out_of_memory_err_obj);

	/* create the class 'loader' for the main application.  In a Java SE VM this would be the
	 * 'system' class loader.  We cannot call java code from C in this VM, so the following code effectively
	 * instantiates the loader and performs the operations that would be in the <init> constructor (without
	 * actually calling the constructor) */
	BVM_SYSTEM_CLASSLOADER_OBJ = (bvm_classloader_obj_t *) bvm_object_alloc(BVM_CLASSLOADER_CLAZZ);
	BVM_MAKE_PERMANENT_ROOT(BVM_SYSTEM_CLASSLOADER_OBJ);

	BVM_BEGIN_TRANSIENT_BLOCK {

		bvm_protectiondomain_obj_t *pd;
		bvm_codesource_obj_t *cs;

		/* now that we have the system class loader, we'll populate its classpath with the global "user
		 * classpath" variable". */

		/* create a String array */
		patharray = (bvm_instance_array_obj_t *) bvm_object_alloc_array_reference(BVM_MAX_CLASSPATH_SEGMENTS, (bvm_clazz_t *) BVM_STRING_CLAZZ);
		BVM_MAKE_TRANSIENT_ROOT(patharray);

		/* for each user classpath segment create a String object and put it into the system
		 * classloader classpath array */
		for (i=0; (i < BVM_MAX_CLASSPATH_SEGMENTS); i++) {
			char *segment;
			if ( (segment = bvm_gl_user_classpath_segments[i]) == NULL) break;
			tempstr = bvm_str_wrap_utfstring(segment);
			patharray->data[i] = (bvm_obj_t *) bvm_string_create_from_utfstring(&tempstr, BVM_FALSE);
		}

		/* associate the path String[] with the system classloader */
		BVM_SYSTEM_CLASSLOADER_OBJ->paths_array = patharray;

		/* now we'll create an empty classes array.  It is an array of Class. We'll default the length to 3.
		 * This object is used by the classloader to keep track of the classes it has loaded - primarily serves the
		 * purpose of stopping Class objects from getting GC'd */
		classesarray = (bvm_instance_array_obj_t *) bvm_object_alloc_array_reference(3, (bvm_clazz_t *) BVM_CLASS_CLAZZ);

		/* ... and store it into the class loader ...*/
		BVM_SYSTEM_CLASSLOADER_OBJ->class_array = (bvm_class_instance_array_obj_t *) classesarray;

		/* lastly, we need to give this new classloader a ProtectionDomain with a CodeSource - note we assign no static
		 * permissions to the codesource (not required), and no certificates - not implemented yet! */
		pd = (bvm_protectiondomain_obj_t *) bvm_object_alloc( (bvm_instance_clazz_t *) bvm_clazz_get_c(BVM_BOOTSTRAP_CLASSLOADER_OBJ, "java/security/ProtectionDomain"));
		BVM_MAKE_TRANSIENT_ROOT(pd);

		cs = (bvm_codesource_obj_t *) bvm_object_alloc( (bvm_instance_clazz_t *) bvm_clazz_get_c(BVM_BOOTSTRAP_CLASSLOADER_OBJ, "java/security/CodeSource"));
		BVM_MAKE_TRANSIENT_ROOT(cs);

		/* give this codesource an alias of "main" - that name is 'reserved' by the runtime classes */
		cs->location = (bvm_string_obj_t *) bvm_string_create_from_cstring("main");
		pd->codesource = cs;

		BVM_SYSTEM_CLASSLOADER_OBJ->protection_domain = pd;

	} BVM_END_TRANSIENT_BLOCK
	/* **************************************************************** */
}

static void bvm_show_sizes() {
    bvm_pd_console_out("\n");
    bvm_pd_console_out("size void*                  : %d \n", sizeof(void*));
    bvm_pd_console_out("size size_t                 : %d \n", sizeof(size_t));
    bvm_pd_console_out("size char                   : %d \n", sizeof(char));
    bvm_pd_console_out("size int                    : %d \n", sizeof(int));
    bvm_pd_console_out("size short                  : %d \n", sizeof(short));
    bvm_pd_console_out("size long                   : %d \n", sizeof(long));
    bvm_pd_console_out("size long long              : %d \n", sizeof(long long));
#if BVM_FLOAT_ENABLE
    bvm_pd_console_out("size float                  : %d \n", sizeof(float));
    bvm_pd_console_out("size double                 : %d \n", sizeof(double));
    bvm_pd_console_out("size long double            : %d \n", sizeof(long double));
#endif
    bvm_pd_console_out("\n");
    bvm_pd_console_out("size bvm_uint8_t            : %d \n", sizeof(bvm_uint8_t));
    bvm_pd_console_out("size bvm_uint32_t           : %d \n", sizeof(bvm_uint32_t));
    bvm_pd_console_out("size bvm_int32_t            : %d \n", sizeof(bvm_int32_t));
    bvm_pd_console_out("size bvm_uint64_t           : %d \n", sizeof(bvm_uint64_t));
    bvm_pd_console_out("size bvm_int64_t            : %d \n", sizeof(bvm_int64_t));
    bvm_pd_console_out("size bvm_float_t            : %d \n", sizeof(bvm_float_t));
    bvm_pd_console_out("size bvm_double_t           : %d \n", sizeof(bvm_double_t));
    bvm_pd_console_out("size bvm_native_long_t      : %d \n", sizeof(bvm_native_long_t));
    bvm_pd_console_out("size bvm_native_ulong_t     : %d \n", sizeof(bvm_native_ulong_t));
    bvm_pd_console_out("\n");
    bvm_pd_console_out("size cell_t                 : %d \n", sizeof(bvm_cell_t));
	bvm_pd_console_out("size string_obj_t           : %d \n", sizeof(bvm_string_obj_t));
	bvm_pd_console_out("size internstring_obj_t     : %d \n", sizeof(bvm_internstring_obj_t));
	bvm_pd_console_out("size clazz_t                : %d \n", sizeof(bvm_clazz_t));
	bvm_pd_console_out("size instance_clazz_t       : %d \n", sizeof(bvm_instance_clazz_t));
	bvm_pd_console_out("size array_clazz_t          : %d \n", sizeof(bvm_array_clazz_t));
	bvm_pd_console_out("size primitive_clazz_t      : %d \n", sizeof(bvm_primitive_clazz_t));
    bvm_pd_console_out("\n");
	bvm_pd_console_out("size instance_t             : %d \n", sizeof(bvm_obj_t));
	bvm_pd_console_out("size array_obj_t            : %d \n", sizeof(bvm_jarray_obj_t));
	bvm_pd_console_out("size jchar_array_obj_t      : %d \n", sizeof(bvm_jchar_array_obj_t));
	bvm_pd_console_out("size jbyte_array_obj_t      : %d \n", sizeof(bvm_jbyte_array_obj_t));
	bvm_pd_console_out("size jint_array_obj_t       : %d \n", sizeof(bvm_jint_array_obj_t));
	bvm_pd_console_out("size jshort_array_obj_t     : %d \n", sizeof(bvm_jshort_array_obj_t));
#if BVM_FLOAT_ENABLE
	bvm_pd_console_out("size jfloat_array_obj_t     : %d \n", sizeof(bvm_jfloat_array_obj_t));
	bvm_pd_console_out("size jdouble_array_obj_t    : %d \n", sizeof(bvm_jdouble_array_obj_t));
#endif
	bvm_pd_console_out("size clazzconstant_t        : %d \n", sizeof(bvm_clazzconstant_t));
	bvm_pd_console_out("size field_t                : %d \n", sizeof(bvm_field_t));
	bvm_pd_console_out("size method_t               : %d \n", sizeof(bvm_method_t));
	bvm_pd_console_out("size utfstring_t            : %d \n", sizeof(bvm_utfstring_t));
	bvm_pd_console_out("size exception_t            : %d \n", sizeof(bvm_exception_t));
	bvm_pd_console_out("size linenumber_t           : %d \n", sizeof(bvm_linenumber_t));
	bvm_pd_console_out("size native_method_t        : %d \n", sizeof(bvm_native_method_t));
	bvm_pd_console_out("size native_method_desc_t   : %d \n", sizeof(bvm_native_method_desc_t));
	bvm_pd_console_out("size classloader_obj_t      : %d \n", sizeof(bvm_classloader_obj_t));
	bvm_pd_console_out("size class_obj_t            : %d \n", sizeof(bvm_class_obj_t));
	bvm_pd_console_out("size filebuffer_t           : %d \n", sizeof(bvm_filebuffer_t));
	bvm_pd_console_out("size throwable_instance_t   : %d \n", sizeof(bvm_throwable_obj_t));
	bvm_pd_console_out("size exception_frame_t      : %d \n", sizeof(bvm_exception_frame_t));
	bvm_pd_console_out("size vmthread_t             : %d \n", sizeof(bvm_vmthread_t));
	bvm_pd_console_out("size thread_obj_t           : %d \n", sizeof(bvm_thread_obj_t));
	bvm_pd_console_out("size threadstack_t          : %d \n", sizeof(bvm_stacksegment_t));
	bvm_pd_console_out("size bvm_chunk_t            : %d \n", sizeof(bvm_chunk_t));
    bvm_pd_console_out("\n");
}

/**
 * Given a \c NULL terminated classpath of paths segments separated by #BVM_PLATFORM_PATH_SEPARATOR chars
 * populate the given segments array with pointers to the start of each path segment.
 *
 * The \c segments array will be populated with pointers to each classpath segment found within
 * the \c classpath string.
 *
 * @param classpath the null-terminated C string containing the classpath to parse.  Each path in the class
 * path is to be separated by a #BVM_PLATFORM_PATH_SEPARATOR char.
 *
 * @param segments a handle to an array that will be populated with pointers to the classpath segments.
 *
 * @throws VM fatal error #BVM_FATAL_ERR_MAX_CLASSPATH_SEGMENTS_EXCEEDED if the given classpath has
 * more segments than the VM can accommodate (as defined by #BVM_MAX_CLASSPATH_SEGMENTS).
 */
static void vm_parse_classpath(char *classpath, char *segments[]) {

    size_t i;
	int pos = 0;
	char *mark = classpath;
	char *seg;
	int size;
	size_t len = strlen(classpath);

	/* loop through each character of the classpath looking for #BVM_PLATFORM_PATH_SEPARATOR
	 * characters.  Replace each character with a NULL and place a pointer to the start of
	 * the classpath segment in the segments array */

	for (i=0;i<len;i++) {

		if (classpath[i] == BVM_PLATFORM_PATH_SEPARATOR) {

			if (pos == BVM_MAX_CLASSPATH_SEGMENTS)
				BVM_VM_EXIT(BVM_FATAL_ERR_MAX_CLASSPATH_SEGMENTS_EXCEEDED, NULL);

			size = (&classpath[i] - mark) + 1;
			seg = bvm_heap_alloc(size, BVM_ALLOC_TYPE_STATIC);
			memcpy(seg, mark, size-1);
			seg[size-1] = '\0';

			segments[pos++] = seg;
			mark = &classpath[i+1];
		}

	}

	/* ... and get the tail ... */
	segments[pos++] = mark;

	/* set the last segment in the segment array to NULL so when traversing the array we know when
	 * we have reached the end - no need to store the length anywhere then.*/
	if ( pos < BVM_MAX_CLASSPATH_SEGMENTS)
		segments[pos] = NULL;
}

/**
 * Perform initialisation of the java classes required by the VM. Before doing so, the
 * #bvm_gl_boot_classpath or #bvm_gl_user_classpath global variables are parsed respectively
 * into #bvm_gl_boot_classpath_segments and #bvm_gl_user_classpath_segments.
 *
 * @throws BVM_FATAL_ERR_NO_BOOT_CLASSPATH_SPECIFIED if #bvm_gl_boot_classpath is not specified.
 * @throws BVM_FATAL_ERR_NO_USER_CLASSPATH_SPECIFIED if #bvm_gl_user_classpath is not specified.
 */
static void vm_init_classpaths() {

	/* no paths established for class loading?  Bang out of VM in a nasty way */
	if (bvm_gl_user_classpath == NULL) BVM_VM_EXIT(BVM_FATAL_ERR_NO_USER_CLASSPATH_SPECIFIED, NULL);
	if (bvm_gl_boot_classpath == NULL) BVM_VM_EXIT(BVM_FATAL_ERR_NO_BOOT_CLASSPATH_SPECIFIED, NULL);

	/* parse the given classpaths into segments */
	vm_parse_classpath(bvm_gl_boot_classpath, bvm_gl_boot_classpath_segments);
	vm_parse_classpath(bvm_gl_user_classpath, bvm_gl_user_classpath_segments);
}

/**
 * Perform VM initialisation.  All internal structures and VM-required memory
 * are allocated during VM initialisation.  After successful initialisation the #bvm_gl_vm_is_initialised
 * will be \c BVM_TRUE.
 *
 * This function is the kickstart for the entire VM initialisation processes.  All VM related memory pool
 * and arrays are created and assigned during execution of this function.
 */
static void vm_init() {

	/* initialize the heap to a given size */
	bvm_heap_init(bvm_gl_heap_size);

	/* create an array used for the clazz pool */
	bvm_gl_clazz_pool = bvm_heap_calloc(bvm_gl_clazz_pool_bucketcount * sizeof(bvm_clazz_t*), BVM_ALLOC_TYPE_STATIC);

	/* create the array used for the utf string pool.  The string pool is an
	 * array of bvm_utfstring_t pointers */
	bvm_gl_utfstring_pool = bvm_heap_calloc(bvm_gl_utfstring_pool_bucketcount * sizeof(bvm_utfstring_t*), BVM_ALLOC_TYPE_STATIC);

	/* create the array used for the intern string pool.  The string pool is an
	 * array of bvm_internstring_obj_t pointers */
	bvm_gl_internstring_pool = bvm_heap_calloc(bvm_gl_internstring_pool_bucketcount * sizeof(bvm_internstring_obj_t*), BVM_ALLOC_TYPE_STATIC );

	/* create space for the native method pool */
	bvm_gl_native_method_pool = bvm_heap_calloc(bvm_gl_native_method_pool_bucketcount * sizeof(bvm_native_method_desc_t*), BVM_ALLOC_TYPE_STATIC );

	/* create space for the transient GC roots and init the top of the stack */
	bvm_gl_gc_transient_roots = bvm_heap_calloc(bvm_gl_gc_transient_roots_depth * sizeof(bvm_cell_t), BVM_ALLOC_TYPE_STATIC );
	bvm_gl_gc_transient_roots_top = 0;

	/* create space for the permanent GC roots and init the top of the stack */
	bvm_gl_gc_permanent_roots = bvm_heap_calloc(bvm_gl_gc_permanent_roots_depth * sizeof(bvm_cell_t), BVM_ALLOC_TYPE_STATIC );
	bvm_gl_gc_permanent_roots_top = 0;

	/* do any initialisation required for the native part of the VM */
    bvm_init_native();

	/* Initialise the VM file handling facilities */
    bvm_init_io();

	/* initialise java classes and objects used by the VM */
    vm_init_classpaths();

    /* load a number of important or often-used classes at bootstrap and have 'em all ready */
    vm_init_bootstrap_clazzes();

    /* create a number of objects that are used by the startup process on the VM */
    vm_init_bootstrap_objects();

    /* init the VM thread and create the bootstrap thread */
    bvm_init_threading();

#if BVM_SOCKETS_ENABLE
	bvm_pd_socket_init();
#endif

	/* all good .  Finished .... */
	bvm_gl_vm_is_initialised = BVM_TRUE;
}


/**
 * Perform any finalisation necessary of the heap before VM exit.
 */
static void bvm_finalise() {
	bvm_heap_release();
}

/**
 * Given a char pointer, read the numeric memory representation it provides. The given char string must start
 * with numerics and may end with \c NULL, or 'k', 'K', 'm', or 'M' suffix to denote the size is expressed in
 * kilobytes or megabytes respectively.  By default, a size without a suffix is in kilobytes.
 *
 * If the char string is not terminated with any of these characters, the return value will be 1.
 *
 * @param c the char string
 *
 * @return the number of bytes the chat string represents.
 */
static long parse_mem(char *c) {
	char *end_arg;
	bvm_uint32_t mem_size = strtol(c, &end_arg, 10);
	switch (*end_arg) {
	case '\0':
	case 'k':
	case 'K':
		mem_size *= BVM_KB;
		break;
	case 'm':
	case 'M':
		mem_size *= BVM_MB;
		break;
	default:
        bvm_pd_system_exit(1);
	}

	return mem_size;
}

/**
 * Given a string char, convert it to a long.
 *
 * @param c the char string.
 *
 * @return a \c long
 */
static long parse_num(char *c) {
	char *end_arg;
	bvm_uint32_t num = strtol(c, &end_arg, 10);
	switch (*end_arg) {
	case '\0':
		break;
	default:
        bvm_pd_system_exit(1);
	}

	return num;
}

#if BVM_CONSOLE_ENABLE

void bvm_show_usage() {

	bvm_show_version_and_copyright();

	bvm_pd_console_out("\n");

	bvm_pd_console_out("Usage: babe [-options] class [args ...]\n\n");
	bvm_pd_console_out("where options include:\n");
	bvm_pd_console_out("\t-cp \t<search path> search path for system/application level classes \n\t\tand jars.\n");
	bvm_pd_console_out("\t-classpath \tSame as -cp.\n");
	bvm_pd_console_out("\t-Xbootclasspath \t<search path> search path for boot classes and jars.\n");
	bvm_pd_console_out("\t-home \t<path> 'root' path for relative path resolution.\n");
	bvm_pd_console_out("\t-heap \t<xxx> the heap size (or xxxM, or xxxK).\n");
	bvm_pd_console_out("\t-stack \t<xxx> thread stack segment size in bytes (or xxxM, or xxxK).\n");
	bvm_pd_console_out("\t-tr \t<xxx> The height of the transient root stack.\n");
	bvm_pd_console_out("\t-sr \t<xxx> The height of the system root stack.\n");
	bvm_pd_console_out("\t-file \t<xxx> The max number of open file handles.\n");
	bvm_pd_console_out("\t-utfb \t<xxx> The number of buckets for the intern utfstring hash pool.\n");
	bvm_pd_console_out("\t-strb \t<xxx> The number of buckets for the intern String hash pool.\n");
	bvm_pd_console_out("\t-clazzb <xxx> The number of buckets for the class hash pool.\n");
	bvm_pd_console_out("\t-ea \tEnable assertions.\n");
#if BVM_CONSOLE_ENABLE
	bvm_pd_console_out("\t-version \tPrint version and exit.\n");
	bvm_pd_console_out("\t-usage \tPrint this text and exit.\n");
	bvm_pd_console_out("\t-sizes \tPrint bytes sizes of internal stuff and exit.\n");
#endif
	bvm_pd_console_out("\t-Dprop=value \tSet System Property \"prop\" to \"value\". \n");

#if BVM_DEBUGGER_ENABLE
	bvm_pd_console_out("\n");
	bvm_pd_console_out("JDPA Debugger support options: \n");
	bvm_pd_console_out("\t-Xdebug \tEnable JPDA debugging.\n");
	bvm_pd_console_out("\t-Xrunjdwp: \tSpecify JDWP transport properties (transport/server/suspend/address/timeout).\n");
#endif

	bvm_pd_console_out("\n");
}

#endif

#if BVM_DEBUGGER_ENABLE

/**
 * Parses the \c Xrunjdwp command line option.  For format of this command line option is as per JDWP specs at:
 *
 * http://java.sun.com/javase/6/docs/technotes/guides/jpda/conninv.html#jdwpoptions
 *
 * Except that only transport, server, suspend, address and timeout are parsed.  Of these, the 'transport' is
 * parsed but presently not stored anywhere or used.
 *
 * @param arg
 */
static void parse_runjdwp(char *arg) {

	/* start at the colon in the argument */
	char *ch = strchr(arg, ':');

	while (ch != NULL) {

		ch++;

		if (strncmp(ch,"transport=", 10) == 0) {
			ch += 10;
			/* TODO */
			ch = strchr(ch, ',');
		} else if (strncmp(ch,"server=", 7) == 0) {
			ch += 7;
			bvmd_gl_server = (*ch == 'y');
			ch = strchr(ch, ',');
		} else if (strncmp(ch,"suspend=", 8) == 0) {
			ch += 8;
			bvmd_gl_suspendonstart = (*ch == 'y');
			ch = strchr(ch, ',');
		} else if (strncmp(ch,"address=", 8) == 0) {
			char *pos;
			ch += 8;
			pos = strchr(ch, ',');
			if (pos == NULL) {
				bvmd_gl_address = ch;
			} else {
				int size = (pos - ch) + 1;
				bvmd_gl_address = bvm_pd_memory_alloc(size);
				memcpy(bvmd_gl_address, ch, size);
				bvmd_gl_address[size-1] = '\0';
			}
			ch = pos;
		} else if (strncmp(ch,"timeout=", 8) == 0) {
			char *pos;
			ch += 8;
			pos = strchr(ch, ',');
			if (pos == NULL) {
				bvmd_gl_timeout = parse_num(ch);
			} else {
				char nums[11];
				int size = BVM_MIN(((pos - ch) + 1), 10);
				memcpy(nums, ch, size);
				nums[size-1] = '\0';
				bvmd_gl_timeout = parse_num(nums);
			}
			ch = pos;
		}
	}
}
#endif

#define BVM_ECHO_COMMAND_LINE 0

#if BVM_ECHO_COMMAND_LINE
static void echo_argument_value(char *value) {
		bvm_pd_console_out("argument value: %s\n", value);
}
#else
#define echo_argument_value(x)
#endif

/**
 * Read the command line arguments and populate their respective global properties.  System properties (beginning
 * with `-D`) get some special handling for later parsing.
 *
 * If no arguments are given, usage info is output to console.
 *
 * @param ac
 * @param av
 */
static void parse_command_line(int *ac, char ***av) {

	int argc = *ac;
	char **argv = *av;

	/* No arguments?  If console is supported show a usage screen
	 * and exit */
	if (argc == 0) {

#if BVM_CONSOLE_ENABLE
		bvm_show_usage();
#endif
        bvm_pd_system_exit(0);
	}

	while (argc > 0) {

#if BVM_ECHO_COMMAND_LINE
		bvm_pd_console_out("argument: %s\n", argv[0]);
#endif

		if ( (strcmp(argv[0], "-cp") == 0) || (strcmp(argv[0], "-classpath") == 0) ) {
			bvm_gl_user_classpath = argv[1];
			echo_argument_value(argv[1]);
			argv+=2;
			argc-=2;
		}

		else if (strcmp(argv[0], "-Xbootclasspath") == 0) {
            bvm_gl_boot_classpath = argv[1];
            echo_argument_value(argv[1]);
            argv+=2;
            argc-=2;
		}

		else if (strcmp(argv[0], "-home") == 0) {
			bvm_gl_home_path = argv[1];
			echo_argument_value(argv[1]);
			argv+=2;
			argc-=2;
		}

		else if (strcmp(argv[0], "-heap") == 0) {
			bvm_gl_heap_size = parse_mem(argv[1]);

			echo_argument_value(argv[1]);

			/* min */
			if (bvm_gl_heap_size < BVM_HEAP_MIN_SIZE) {
				bvm_gl_heap_size = BVM_HEAP_MIN_SIZE;
			}

			/* max */
			if (bvm_gl_heap_size > BVM_HEAP_MAX_SIZE) {
				bvm_gl_heap_size = BVM_HEAP_MAX_SIZE;
			}

			/* make sure heap size is a multiple of the alignment factor */
			bvm_gl_heap_size -= bvm_gl_heap_size % BVM_CHUNK_ALIGN_SIZE;

			argv+=2;
			argc-=2;
		}

		else if (strcmp(argv[0], "-stack") == 0) {
			bvm_uint32_t mem = parse_mem(argv[1]);

			echo_argument_value(argv[1]);

			bvm_gl_stack_height = mem / sizeof(bvm_cell_t);

			/* Seems reasonable */
			if (bvm_gl_stack_height < 128) {
				bvm_gl_stack_height = 128;
			}

			/* pretty arbitrary really, just to stop bad params */
			if (bvm_gl_stack_height > 16 * BVM_KB) {
				bvm_gl_stack_height = 16 * BVM_KB;
			}

			argv+=2;
			argc-=2;
		}

		else if (strcmp(argv[0], "-tr") == 0) {
			bvm_gl_gc_transient_roots_depth = parse_num(argv[1]);

			echo_argument_value(argv[1]);

			/* Seems reasonable */
			if (bvm_gl_gc_transient_roots_depth < 50) {
				bvm_gl_gc_transient_roots_depth = 50;
			}

			/* pretty arbitrary really, just to stop bad params */
			if (bvm_gl_gc_transient_roots_depth > 5000) {
				bvm_gl_gc_transient_roots_depth = 5000;
			}

			argv+=2;
			argc-=2;
		}

		else if (strcmp(argv[0], "-sr") == 0) {
			bvm_gl_gc_permanent_roots_depth = parse_num(argv[1]);

			echo_argument_value(argv[1]);

			/* Must be large enough to hold all classes in java packages + a
			 * few objects */
			if (bvm_gl_gc_permanent_roots_depth < 100) {
				bvm_gl_gc_permanent_roots_depth = 100;
			}

			/* pretty arbitrary really, just to stop bad params */
			if (bvm_gl_gc_permanent_roots_depth > 500) {
				bvm_gl_gc_permanent_roots_depth = 500;
			}

			argv+=2;
			argc-=2;
		}

		else if (strcmp(argv[0], "-files") == 0) {
			bvm_gl_max_file_handles = parse_num(argv[1]);
			echo_argument_value(argv[1]);
			argv+=2;
			argc-=2;
		}

		else if (strcmp(argv[0], "-utfb") == 0) {
			bvm_gl_utfstring_pool_bucketcount = parse_num(argv[1]);
			echo_argument_value(argv[1]);
			argv+=2;
			argc-=2;
		}

		else if (strcmp(argv[0], "-strb") == 0) {
			bvm_gl_internstring_pool_bucketcount = parse_num(argv[1]);
			echo_argument_value(argv[1]);
			argv+=2;
			argc-=2;
		}

		else if (strcmp(argv[0], "-clazzb") == 0) {
			bvm_gl_clazz_pool_bucketcount = parse_num(argv[1]);
			echo_argument_value(argv[1]);
			argv+=2;
			argc-=2;
		}

		else if (strcmp(argv[0], "-ea") == 0) {
			bvm_gl_assertions_enabled = BVM_TRUE;
			argv+=1;
			argc-=1;
		}

		else if (strncmp(argv[0], "-D", 2) == 0) {
			/* for now, ignore system params - the System class will post process these
			 * on VM startup. */
			if (cmdline_nr_system_properties % 5 == 0) {
				char **old_array = cmdline_system_properties;
				int size = (cmdline_nr_system_properties + 5) * sizeof(char *);
				cmdline_system_properties = bvm_pd_memory_alloc(size);
				memset(cmdline_system_properties,0,size);

				/* if the old array was not empty, copy its contents into the new one */
				if (old_array != NULL) {
					memcpy(cmdline_system_properties, old_array, cmdline_nr_system_properties * sizeof(char *) );
				}
			}

			cmdline_system_properties[cmdline_nr_system_properties++] = argv[0];

			argv+=1;
			argc-=1;
		}

#if BVM_CONSOLE_ENABLE

		else if (strcmp(argv[0], "-version") == 0) {
			bvm_show_version_and_copyright();
            bvm_pd_system_exit(0);
		}

		else if (strcmp(argv[0], "-usage") == 0) {
			bvm_show_usage();
            bvm_pd_system_exit(0);
		}

		else if (strcmp(argv[0], "-sizes") == 0) {
            bvm_show_sizes();
            bvm_pd_system_exit(0);
		}

#endif

#if BVM_DEBUGGER_ENABLE

		else if (strcmp(argv[0], "-Xdebug") == 0) {
			bvmd_gl_enabledebug = BVM_TRUE;
			argv+=1;
			argc-=1;
		}

		else if (strncmp(argv[0], "-Xrunjdwp:", 10) == 0) {
			parse_runjdwp(argv[0]);
			argv+=1;
			argc-=1;
		}

#endif

		else if (strncmp(argv[0], "-X", 2) == 0) {
			/* ignore all unknown -X arguments. */
			argv+=1;
			argc-=1;
		}

		else if (*(argv[0]) == '-') {

#if BVM_CONSOLE_ENABLE
			bvm_pd_console_out("Invalid command line argument '%s'.", argv[0]);
#endif

            bvm_pd_system_exit(500);
		}
		else {
			break;
		}
	}

	*ac = argc;
	*av = argv;
}

/**
 * Reads the command lines system properties (those that begin with -D) into the first field on the runtime System
 * class ('_cmdLine').  The System class uses that to expose system properties as a Map.
 */
static void set_system_class_properties() {

	bvm_instance_clazz_t *clazz;
	bvm_instance_array_obj_t *args_array_obj;
	bvm_utfstring_t temp_utfstring;
	int lc;

	if (cmdline_nr_system_properties > 0) {

		BVM_BEGIN_TRANSIENT_BLOCK {

			/* Get the system clazz */
			temp_utfstring = bvm_str_wrap_utfstring("java/lang/System");
			clazz = (bvm_instance_clazz_t *) bvm_clazz_get( (bvm_classloader_obj_t *) BVM_SYSTEM_CLASSLOADER_OBJ, &temp_utfstring);

			/* create an array of string objects */
			args_array_obj = bvm_object_alloc_array_reference(cmdline_nr_system_properties, (bvm_clazz_t *) BVM_STRING_CLAZZ);

			/* make sure our new array does not disappear if following allocations cause a GC */
			BVM_MAKE_TRANSIENT_ROOT(args_array_obj);

			/* set the first field '_cmdLine' of the System class to the new String object array - it will be
			 * populated in the loop below. */
			clazz->fields[0].value.static_value.ref_value = (bvm_obj_t *) args_array_obj;

			/* for each System property set on the command line */
			for (lc = cmdline_nr_system_properties; lc--;) {

				/* get the command line system prop */
				char *sysprop = cmdline_system_properties[lc] + 2;  /* the +2 moves past the '-D' */

				/* create a String object from it and populate it into the String array */
				temp_utfstring = bvm_str_wrap_utfstring(sysprop);
				args_array_obj->data[lc] = (bvm_obj_t *) bvm_string_create_from_utfstring(&temp_utfstring, BVM_FALSE);
			}

		} BVM_END_TRANSIENT_BLOCK
	}
}

/**
 * Some sanity checking on the sizes of platform types.
 */
static void sanity_check_sizes() {
    if (sizeof(bvm_int8_t) != 1) bvm_pd_system_exit(BVM_FATAL_ERR_INCORRECT_UINT8_SIZE);
    if (sizeof(bvm_uint8_t) != 1) bvm_pd_system_exit(BVM_FATAL_ERR_INCORRECT_UINT8_SIZE);

    if (sizeof(bvm_uint16_t) != 2) bvm_pd_system_exit(BVM_FATAL_ERR_INCORRECT_UINT16_SIZE);
    if (sizeof(bvm_int16_t) != 2) bvm_pd_system_exit(BVM_FATAL_ERR_INCORRECT_INT16_SIZE);

    if (sizeof(bvm_uint32_t) != 4) bvm_pd_system_exit(BVM_FATAL_ERR_INCORRECT_UINT32_SIZE);
    if (sizeof(bvm_int32_t) != 4) bvm_pd_system_exit(BVM_FATAL_ERR_INCORRECT_INT32_SIZE);

    if (sizeof(bvm_float_t) != 4) bvm_pd_system_exit(BVM_FATAL_ERR_INCORRECT_FLOAT_SIZE);
    if (sizeof(bvm_double_t) != 8) bvm_pd_system_exit(BVM_FATAL_ERR_INCORRECT_DOUBLE_SIZE);

    if (sizeof(bvm_native_long_t) != sizeof(void *)) bvm_pd_system_exit(BVM_FATAL_ERR_INCORRECT_NATIVE_LONG_SIZE);
    if (sizeof(bvm_native_ulong_t) != sizeof(void *)) bvm_pd_system_exit(BVM_FATAL_ERR_INCORRECT_NATIVE_LONG_SIZE);

//    if (sizeof(bvm_native_long_t) != sizeof(void *)) bvm_pd_system_exit(BVM_FATAL_ERR_INCORRECT_NATIVE_LONG_SIZE);
//    if (sizeof(bvm_native_ulong_t) != sizeof(void *)) bvm_pd_system_exit(BVM_FATAL_ERR_INCORRECT_NATIVE_LONG_SIZE);

    if (sizeof(bvm_cell_t) != sizeof(size_t)) bvm_pd_system_exit(BVM_FATAL_ERR_INCORRECT_CELL_SIZE);
}


/**
 * The VM startup method.  Platform implementations are expected to call this method after they
 * have done their own startup thing (command line based or otherwise).
 *
 * @section args Arguments
 *
 * There are a number of standard arguments accepted by this function - just like it was being executed
 * from the command line.  They are:
 *
 * @li \c -Xbootclasspath : the system classpath.  This is a path to the runtime classes (rt.jar) used by the VM.
 * @li \c -cp / -classpath : the user classpath.  This is a path to the classes used by the running application.
 * @li \c -home : a home directory considered as a root for file opening.
 * @li \c -heap : the size of the heap.  Kilobytes can also be expressed by appending 'k' to the given
 * number ('K' will also do).  For megabytes append either an 'm' or an 'M'.
 * @li \c -stack : the size of a stack segment in bytes - see notes for expressing memory sizes on the 'heap' argument.
 * @li \c -tr : the size of the stack for transient GC roots - expressed in terms of its height, not size in
 * bytes.  Just an int. Min is 50 - Max is 5000.
 * @li \c -sr : the size of the stack for permanent system GC root - expressed in terms of its height, not size in
 * bytes.  Just an int. Min is 100 - Max is 500.
 * @li \c -files : max number of open files.
 * @li \c -utfb : number of hash buckets for the utf string pool.
 * @li \c -strb : number of hash buckets for the interned String pool.
 * @li \c -clazzb : number of hash buckets for the class pool.
 * @li \c -ea : globally enable java asserts.  Java assertions are off by default.
 * @li \c -version : output version information and exit.  Not this is only available if the platform support a console.
 * @li \c -Dprop=value : Set a System property called "prop" to "value".
 *
 * Following all that <i>must</i> be the class name of the java class containing a static void main method for
 * execution.  This is mandatory.
 *
 * Following that are the arguments for the java class, in any.
 *
 * Any command line arguments that begin with '-' and do not match any of the above will cause the VM to exit
 * immediately with a return code of \c 500.
 *
 * @section notes notes:
 *
 * This function loads and initialises the 'command line' java class, finds the "public static void main(String[])"
 * method of that class and executes it.  The VM will exit with code #BVM_FATAL_ERR_NO_MAIN_METHOD if no method of
 * that signature can be found.
 *
 * The arguments to 'main' are created here as a populated \c String[] and passed to the 'main' method
 * when it is executed.
 *
 * The running VM is 'protected' (for lack of a better word) by two nested BVM_TRY/BVM_CATCH blocks.  The outer block is
 * to catch exceptions that are otherwise uncaught.  The inner is a #BVM_VM_TRY / #BVM_VM_CATCH to catch
 * when a #BVM_VM_EXIT is executed.  A #BVM_VM_EXIT will arrive back here for a bit of post processing before
 * the VM really does exit.
 *
 * The interpreter loop is started here.
 *
 * @param argc The number of java command line arguments - must be at least one (the java class name).
 *
 * @param argv the arguments.   Argument 0 is the full name of the java class to execute start.  Subsequent
 * arguments (if any) are converted to java String objects and placed into a String array and passed to the java
 * main() method.  There must be only 'argc' number of arguments.
 *
 * @return the exit code for the terminated VM.  0 if the VM terminated ok, #BVM_FATAL_ERR_UNCAUGHT_EXCEPTION
 * if an exception was thrown that was not caught by the exception handling mechanism.  All other exit
 * codes that the VM uses are defined in the enum #BVM_FATAL_ERRS.  NI methods may cause a VM exit but they can
 * only specify an exit message, not an exit code (as per JNI specifications).
 *
 * @throws BVM_FATAL_ERR_NOT_ENOUGH_PARAMS if no arguments are passed in.
 * @throws BVM_FATAL_ERR_NO_MAIN_METHOD if the main class has no 'main' with the the correct method
 * signature.
 *
 * @throws any exception for VM initialisation and class loading.
 */

int bvm_main(int argc, char *argv[]) {

	int exit_code = 0;
	char *exit_msg  = NULL;

	int ac = argc;
	char **av = argv;

	sanity_check_sizes();

	/* parse and process the command line arguments.  The addresses of ac/av are passed in by reference
	 * and after function execution ac > 0 and av == java main class name. */
	parse_command_line(&ac, &av);

	BVM_TRY { /* to catch BVM_EXIT */

		BVM_VM_TRY {  /* to catch uncaught exceptions */

			bvm_instance_clazz_t *clazz;
			bvm_instance_array_obj_t *args_array_obj;
			bvm_method_t *method;
			bvm_utfstring_t temp_utfstring;

			int lc;

			/* init the VM */
			vm_init();

			/* .. not even a startup class mentioned ? Bang out. */
			if (ac == 0) BVM_VM_EXIT(BVM_FATAL_ERR_NOT_ENOUGH_PARAMS, NULL);

#if BVM_CONSOLE_ENABLE
			/* just to keep the punters straight, output a copyright notice. */
			bvm_show_version_and_copyright();
#endif

#if BVM_DEBUGGER_ENABLE

			if (bvmd_gl_enabledebug) {

				/* initialise the debugger stuff */
				bvmd_init();

				if (bvmd_is_session_open()) {

					/* wait for that first command ... which will be 'sizes' */
					bvmd_interact(BVMD_TIMEOUT_FOREVER);

					/* send 'VM start' event */
					bvmd_event_VMStart();

					/* if the start mode is to suspend everything, spin on the debugger until
					 * it resumes stuff ... */
					if (bvmd_gl_suspendonstart)
						bvmd_spin_on_debugger();

				}
			}
#endif

#if BVM_DEBUGGER_ENABLE

			/* if debugging, inform the debugger that the system thread is starting */
			if (bvmd_is_session_open()) {

				/* create a new event context for the 'thread start' event type */
				bvmd_eventcontext_t *context = bvmd_eventdef_new_context(JDWP_EventKind_THREAD_START, bvm_gl_thread_current);

				/* and let the debugger know that the system thread has started - bvmd_do_event() will free the new context */
				bvmd_do_event(context);
			}
#endif
			BVM_BEGIN_TRANSIENT_BLOCK {

				/* create a String[] to be used as the parameters for the main method. */
				args_array_obj = bvm_object_alloc_array_reference(ac-1, (bvm_clazz_t *) BVM_STRING_CLAZZ);

				/* make sure our new array does not disappear if following allocations cause a GC */
				BVM_MAKE_TRANSIENT_ROOT(args_array_obj);

				/* for each param create a String object and place it in the String array. */
				for (lc=1; lc < ac; lc++)  {
					temp_utfstring = bvm_str_wrap_utfstring(av[lc]);
					args_array_obj->data[lc-1] = (bvm_obj_t *) bvm_string_create_from_utfstring(&temp_utfstring, BVM_FALSE);
				}

                // TODO: check the class file name does not already have slashes.
				/* make the command line class name into an internal class name (replace dots with forward
				 * slashes)*/
                bvm_str_replace_char(av[0], (int) strlen(av[0]),'.','/');

				/* .. and find the vm startup class to execute - use the system classloader  */
				temp_utfstring = bvm_str_wrap_utfstring(av[0]);
				clazz = (bvm_instance_clazz_t *) bvm_clazz_get( (bvm_classloader_obj_t *) BVM_SYSTEM_CLASSLOADER_OBJ, &temp_utfstring);

				/* find the void static "main" method for the startup class that has an array of
				 * Strings as an argument - only search the given class, not its supers or interfaces.
				 * We do need to be very specific here because 'main()', like any other method name can be
				 * overloaded - so we have to get the right one */
				method = bvm_clazz_method_get(clazz, bvm_utfstring_pool_get_c("main", BVM_TRUE),
										   bvm_utfstring_pool_get_c("([Ljava/lang/String;)V", BVM_TRUE),
										   BVM_METHOD_SEARCH_CLAZZ);

				/* If no "main" method can be found, or it is not both public and static, bang out */
				if ( (method == NULL) || !(BVM_METHOD_IsPublic(method) && BVM_METHOD_IsStatic(method) ))
					BVM_VM_EXIT(BVM_FATAL_ERR_NO_MAIN_METHOD, NULL);

				/* push the "main" method onto the stack making sure to capture the sync object if required -
				 * Yes, it is legal in Java to have a synchronised main method - it is just another
				 * method like any other.  We set the return pc to the magic BVM_THREAD_KILL_PC to let us know
				 * this is the base of the thread */
				bvm_frame_push(method, bvm_gl_rx_sp, bvm_gl_rx_pc, BVM_THREAD_KILL_PC, BVM_METHOD_IsSynchronized(method) ? (bvm_obj_t *) method->clazz->class_obj : NULL);

				/* set the argument of "main" to our newly created String array of command
				 * line arguments */
				bvm_gl_rx_locals[0].ref_value = (bvm_obj_t *) args_array_obj;

				/* (JVMS 5.5) VM startup class is initialised before use.  All classes, before being use are
				 * 'initialized'.  This is a fairly complex process to get a class internalised and set up
				 * for running - most of the complexity arises from the fact that more than single thread
				 * may be attempting to initialise the class at the same time - so locking etc has
				 * to be done.  */
				bvm_clazz_initialise(clazz);

				/* also, make sure the thread class is initialised before we used it - we have
				 * already instantiated one before we get the interp loop running, so put it on top
				 * here to make sure it is correctly initialised.  Normally, class initialisation happens
				 * as part of the interp loop - but we're not there yet, so we push it manually. */
				bvm_clazz_initialise(BVM_THREAD_CLAZZ);

				/* Set System class properties */
                    set_system_class_properties();

			} BVM_END_TRANSIENT_BLOCK

			/* and finally, start the interpreter loop for the virtual machine */
			bvm_exec_run();

		} BVM_VM_CATCH(code, msg) {

			/* nasty - if we arrive here an BVM_VM_EXIT has been performed somewhere */
			exit_code = code;
			exit_msg = msg;

		} BVM_VM_END_CATCH

	}
	BVM_CATCH(e) {

#if BVM_CONSOLE_ENABLE

		/* if we arrive here then an exception was raised that was not caught - let's stack trace it.  Stack
		 * tracing may cause a GC as the stack trace elements are created, so even though we are about to exit
		 * the VM we'll still make the exception a transient root to make sure a GC does not collect it while we're
		 * using it. */

		/* TODO - it is possible to arrive here before initialisation has completed - in other
		 *  words there may be no memory, and no current thread.  Check for `bvm_gl_vm_is_initialised` */
		BVM_BEGIN_TRANSIENT_BLOCK {
			BVM_MAKE_TRANSIENT_ROOT(e);
			bvm_stacktrace_print_to_console(e);
		} BVM_END_TRANSIENT_BLOCK

#endif

		/* if the uncaught exception is actually an out of memory error we'll use a special
		 * exit code to provide a little more information.
		 */
		if (e == bvm_gl_out_of_memory_err_obj) {
			exit_code = BVM_FATAL_ERR_OUT_OF_MEMORY;
		} else {
			exit_code = BVM_FATAL_ERR_UNCAUGHT_EXCEPTION;
		}

	} BVM_END_CATCH

#if BVM_DEBUGGER_ENABLE
	/* if we're debugging and a session is open, then inform the debugger and close it */
	if (bvmd_gl_enabledebug) {
		if (bvmd_is_session_open()) {
            bvmd_event_VMDeath();
        }
		bvmd_shutdown();
	}
#endif

#if BVM_CONSOLE_ENABLE
    /* on exit, report the exit code and associated message (if any) to the console. */
	if (bvm_gl_vm_is_initialised) {
		if (exit_msg == NULL) {
			bvm_pd_console_out("VM exit code: %d\n", exit_code);
		} else {
			bvm_pd_console_out("VM exit code: %d - %s\n", exit_code, exit_msg);
		}
	}
#endif

#if BVM_SOCKETS_ENABLE
	/* close down the sockets */
	bvm_pd_socket_finalise();
#endif

	/* shut down file system access */
	bvm_file_finalise();

#if BVM_DEBUG_CLEAR_HEAP_ON_EXIT
	bvm_debug_clear_heap();
#endif

	/* clean up before we leave */
	bvm_finalise();

	return exit_code;
}

