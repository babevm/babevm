# The BabeVM Virtual Machine

## Welcome to the Machine

First, Oracle and Java are registered trademarks of Oracle and/or its affiliates.  This software and associated bodies of work such as the runtime class library have no association with Oracle and does not claim to be Java.

The Babe VM (aka 'BVM', or often just 'Babe' in the docs) is a faithful implementation of the Java Virtual Machine Specification (JVMS book, edition 2) at 1.6 level. Babe executes bytecode according to the JVMS and has an associated library of core classes to support the operational VM.  It is released under the Apache 2.0 license (refer the included `LICENCE` file).

The license choice is intended to be make this code 'commercial friendly' so it can be incorporated into commercial offerings and derivative works.  As per the license, it is offered 'as-is', but, in the spirit of how it is offered, if you do find bugs, or improve this base code, then please consider getting a PR in.  This body of work has taken countless hours of effort of development and testing (and is pretty tight), but bugs may still exist.  So, do be a good soul and help out.  

Also, if you do use the code in any way, also please consider dropping a note at this project.  We love knowing that stuff. 

Babe is simple C and thus very portable and could largely run anywhere (32/64 bit, big/little endian), though it incorporates features that make it suitable for small secure devices like a payments terminal.  

Indeed, many of the decisions regarding the VM have taken this environment into account - for example - in the name of closing security doors, class files are not read at runtime by classes, but are handed to the VM for loading.  In this case, the VM handles all class file reading so no developer can create or load classes at runtime that are not deployed with the application.  Reflection is not available.  Additionally, the notion of runtime protection of code and resources is implemented as per Java SE such that, say, code may access only permitted peripherals or files, and then, only to read, or to write etc.

Babe can run standalone, or can be embedded into other works.

Babe is very small and is intended for small environments, though, it does not skimp on features.  For the techies, an overview of some features of the Babe VM.

* **Java 6**.  Runs code compiled with a Java 6 compiler, or a compiler configured to produce Java 6 classes.
* **Complete language support**.  All JVMS opcodes are implemented so the language support is very complete - Doubles, longs, thread monitors, exceptions etc.
* **Threading** - a complete (green) threading implementation with all locking / synchronisation semantics.  Daemon threads are supported.
* **32/64 bit**.  Runs on latest OS versions, or older 32bit architectures. 
* **Portable** - intended to be portable across OSs and architectures.  It is developed in 100% ANSI C (90) with very few standard lib functions used and no assembly used at all.  The 'C' is fairly textbook - not too much 'clever stuff' - if any.  
* **Garbage collection** - a full GC implementation.  Collection of Class objects and Classloader objects is supported.  
* **Weak references** - supported here as per CLDC 1.1.
* **Optional Floats** - for small environments that do not support (or need) Java `float` and `double` all floating point stuff can be excluded from the VM build.
* **Emulated 64 bit longs** - Uses native 64bit integers for Java longs, but they can be emulated on smaller architectures that do not have native 64 bit integers.  
* **Big/Little endian** - support for both architecture types.  Though, to be honest, big-endian is pretty un-exercised. See [Endianess](docs/endian.md).
* **Native Interface** - a native implementation similar to that of the Java Native Interface (JNI) albeit particular to this small VM.  A native interface means Java classes can incorporate calls to custom C code, and that C code can use VM facilities via the documented 'NI' abstraction.
* **Jar files** - support for reading class files from jars.
* **Class Loaders** - loaded java classes are namespaced by their class loader as per the JVMS.
* **Class/member access visibility** - class and member visibility semantics are fully implemented (some tiny VMs treat everything as 'public' - not very secure).
* **Class assignment** - all JVMS class assignment rules are obeyed. 
* **Unified memory management** - the running VM uses the same memory primitives as the Java bytecode and also the same Garbage Collector.  The 'C' VM is garbage collected just like Java is (and at the same time).  The memory allocator is a "coalescing best fit" algorithm.  Memory de-fragmentation is a natural part of the allocator, not a separate thing.
* **Exceptions** - full support for try/catch/finally and thread uncaught exception handlers.  VM 'C' exceptions and bytecode exceptions use the same mechanisms.
* **Stack traces** - familiar feeling console stack-traces are implemented.
* **Code/resource permissions** - A small implementation of the Java SE permissions model including `SecurityManager`, `AccessControlContext`, `ProtectionDomain`s and so on.
* **Dynamic stacks** - the VM thread stacks grow and shrink as required - no predefined max stack height. 
* **Remote Debugging** - running code can be debugged using a standard development IDE like Intellij or VSCode or Eclipse.  Remote debugging is supported via an implementation of the Java Debug Wire Protocol (JDWP).  Debugger support can be excluded from the build to create a smaller, faster runtime.  
* **Documented** - the code has a lot of documentation to help developers understand how it all hangs together. The code documentation format is the standard 'doxygen' format. 
* **Performant** - Some _very_ unscientific performance testing show it to be somewhat faster than python (circa 50% faster on a recursive Fibonacci algorithm)
* **Embeddable** - All functions and global vars and externs etc are prefixed with `bvm_` so you'll not get any name clashes with your own code.  

For more detailed documentation, check out the docs pages: [https://babevm.github.io/babevm/](https://babevm.github.io/babevm/) 

