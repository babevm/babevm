# Babe VM documentation

Here, you'll find explanations inner workings of the VM with links to relevant sources files and other documentation.

If you're looking to get going, look here: [Quickstart](./quickstart.md).

For other build-related info, look here: [Building](./build.md).

If you're looking for the code (doxygen) docs, look here: [Doxygen](./doxygen/html/index.html) 

If you're looking for the runtime classes, look over here: [Runtime](https://github.com/babevm/runtime)

## The project

First, some notes about the actual code and project layout:

* All functions, global stuff and macros are prefixed with `bvm` or `BVM`.  
* things related to JDWP debugging are prefixed with `bvm_d` or `BVM_D`
* things related to platform dependant code are prefixed with `bvm_pd` or `BVM_PD` 
* global vars will have `_gl_` in their name. 

The layout of the project src folder is as such:

* `c` folder - as you'd expect
* `h` folder - as you'd expect
* `platform` folder - one folder for each platform. Consider the `hyc_t4200` and `osx_ppc` folders pretty sketchy. The `hyc_t4200` was a test port to a rel payment device (with remote debugging), and `osx_ppc` 
* `thirdparty` folder - other libs (and licences) used in this build. 

The platform folder is as such:

* `ansi` - an ANSI C impl of console, memory and file handling.
* `linux` - linux specifics.  Actually, the linux port uses ANSI C stuff plus the BSD sockets impl, so very little is in here apart from the specific of getting a timestamp.
* `osx_ppc` - config and specifics for mac OSX running powerpc (big endian) architecture.  Big endian is 'supported' but has not been exercised in a while. Refer [Endianess](./endian.md).
* `sockets` - a BSD sockets impl for windows and linux/osx. Used as the transport for the JDWP capability.
* `winos` - Windows specifics.  At this point, only Windows 64 bit has been well exercised since porting to 64 bit.  

The code has a lot of annotations that hopefully help devs to understand what is going on.  Sometimes the annotations also describe what the JVMS details, and the approach taken.  

## data types

ANSI C defines standard data types, but, their size is not constant between platforms.  Java requires a series of datatypes that are of a known size.  To that end, part of the platform porting process is to provide platform specific types that match the Java types.  The VM also uses those types in it own code.  You'll not see `int` or `long` types in the code but their VM / Java types.  Refer to [portability](./portability.md) for more info.

## The runtime Java lib

Associated with this VM code is the core runtime classes.  These are Java classes in the [https://github.com/babevm/runtime](https://github.com/babevm/runtime) repo.  Babe VM needs these classes to run.  Those classes are specific to this VM and no other.  There are many touchpoints between a VM and its runtime classes where the VM knows some stuff about the classes, and the classes know stuff about the VM - they are bound.  This is normal - even the current Java open JDK does the same - the VM and the classes work together.

In order to be able to run this VM, the runtime ('rt') repo will need to be pulled and built (using the maven build provided in that repo).  The build produces a `rt-<version>.jar` file as well as an uncompressed folder of compiled classes.

When running the VM, the absolute path to the jar or classes folder is required as `-Xbootclasspath`.  Alternatively, if the `rt-<version>.jar` file is in the same folder as the VM executable when it runs the `-Xbootclasspath` is not required.

## Building

Babe VM and the core classes are distributed as source.  At this time there are no pre-built binaries.  Both the VM code and the core runtime classes ('rt') code are required to get up and going.

Have a look at [Building](./build.md) for more detail.

## Some FAQ ...

Here  ... [FAQ](./faq.md)

## How stuff works

The Java Virtual Machine Specification is an astounding piece of work.  From 1996 to now, the same fundamentals apply to the latest JVMs - just look at the performance of OpenJDK and other modern JVMs - running on largely the same spec as bnack then.  Quite incredible. Maybe just one (two?) new opcodes have been added in 25 years.

It has a lot to teach us.

To this end, the key overriding design goal of this VM is simplicity.  Simple is small, often quick, and easier to understand, and easier to document.  This is a very complete implementation, and religious coding style arguments aside, hopefully offers developers a readable and understandable view into how a JVMS and a JDWP implementation do their jobs.

Having said that, a VM is quite often not a simple piece of software.  In this implementation there a few areas that seem complex, but hopefully, the unit documentation provides enough detail for developers to understand the mechanisms and the reasoning behind them.  If that is not the case, then let's fix it.

The VM, like all other Javas VMs, is a 'Java bytecode interpreter'.  Given the 'small device' target environment, it has to strike a balance between speed, size, and memory usage, and complexity to achieve what it does.  A key design goal was to keep the VM and core classes to about 300k total and yet provide as complete an implementation of the JVMS as possible .  Ultimately, software this small is a collection of compromises made to balance each of the competing concerns above.  Hopefully, the documentation helps.

The code documentation does not replicate the JVMS document and assumes the developer has given it some attention at least.  Though, the annotations do provide some insight into the JVMS and why things are the way they are.

Each of the following links will provide more information on how some aspect of the VM works, and from there will also link to relevant source code and 'doxygen' pages.

In no particular order:

* [Memory Management](./memory.md)
* [Class Loading](./class_loading.md)
* [Execution](./interpreter.md)
* [Threading](./threads.md)
* [Try Catch](./trycatch.md)
* [Jar file support](./jarfiles.md)
* [Remote Debugging](./debugger.md)
* [Longs](./longs.md)
* [Resource Pools](./pools.md)
* [Native Interface](./native_interface.md)
* [Portability](./portability.md)
* [Security Permissions](./permissions.md)
* [File Handling](./file_handling.md)
* [Endianess](./endian.md)
* [Limitations](./limitations.md)

## What is left to do?

A few quick thoughts on things that can be better, should be done, or could just be done anyways:

* the maximum permissible heap size could easily be extended to a multiple or 4, 8 or 16 on what it is now.  Presently, a memory chunk size is expressed in bytes, but this could fairly easily be changed to be a multiple of bytes. 
* some additional JDWP debugger commands could likely be supported with a little effort.
* the heap is not segmented in any way (to keep it simple), but, a 'generational' memory scheme could be implemented to reduce the effort for many GC cycles.  Additionally, a tri colour marking scheme is already in use so soe groundwork for 'incremental' collection has also been laid. 
* class loaders could feed the VM the raw bytes of classes and not rely solely on the VM finding classes on the classpath.  The permissions system can control whose is allowed to do so.
* parsing doubles/floats from Strings is not yet implemented.
* not all relevant runtime classes use SecurityManager to protect themselves as yet.
* a default policy loader implementation could be supplied. 
* exposing file handling to at the Java layer would likely be handy 


