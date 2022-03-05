# Portability Requirements

The Babe VM has the following platform requirements for portability:

* ANSI C (90) compiler and libs.  Though - not many std functions are used.
* C compiler/libraries must support the ANSI C `setjmp()` and `longjmp()` functions.
* a file system of some sort must be available - supports an ANSI C file interface, but others can be implemented, or emulated.
* at least 128k of memory available for the running VM heap.  It is certainly possible to run programs in less than this, but they will be (no doubt) trivial.  Memory usage is larger for 64 bit.
* a 32 bit integer type. Signed and unsigned.  For 64 bit platforms, the same but (erm) with 64 bits ...
* a clock - preferably one that gives millisecond timing.  The ability to (somehow) calculate the number of milliseconds since Jan 1 1970 must be available.

In Babe, functions that begin with `bvm_pd_` are machine dependant and intended to be provided on a per-platform basis. The prototypes to be implemented are in:

Key files:

| unit                                                                                                 | docs                                    | Description                                                                                                                                                                                                                                       |
|------------------------------------------------------------------------------------------------------|-----------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| [md_console.h](https://github.com/babevm/babevm/blob/master/src/c/md_console.h){:target="_blank"}                | [link](doxygen/html/md_console_8h.html) | to output text to a console.  The compiler define `BVM_CONSOLE_ENABLE` is unset, console support will be disabled. An ANSI C version is in [md_console.c](https://github.com/babevm/babevm/blob/master/src/platform/ansi/md_console.c){:target="_blank"}      |
| [md_file.h](https://github.com/babevm/babevm/blob/master/src/c/md_file.h){:target="_blank"}                      | [link](doxygen/html/md_file_8h.html)    | file handling functions. An ANSI C version is in [md_file.c](https://github.com/babevm/babevm/blob/master/src/platform/ansi/md_file.c){:target="_blank"}                                                                                                      |                                                  
| [md_memory.h](https://github.com/babevm/babevm/blob/master/src/c/md_memory.h){:target="_blank"}                  | [link](doxygen/html/md_memory_8h.html)  | memory allocation and free. An ANSI C version is in [md_memory.c](https://github.com/babevm/babevm/blob/master/src/platform/ansi/md_memory.c){:target="_blank"}                                                                                               |
| [md_system.h](https://github.com/babevm/babevm/blob/master/src/c/md_system.h){:target="_blank"}                  | [link](doxygen/html/md_system_8h.html)  | exiting the VM, getting the time, and build OS name / version                                                                                                                                                                                     |
| [md_bsd_win_socket.h](https://github.com/babevm/babevm/blob/master/src/c/md_bsd_win_socket.h){:target="_blank"}  | [link](doxygen/html/md_bsd_win_socket_8h.html)  | sockets. At this socket are not exposed to Java classes - only used by the debugger.  There is a shared windows/linux impl in [md_bsd_win_socket.c](https://github.com/babevm/babevm/blob/master/src/platform/sockets/md_bsd_win_socket.c){:target="_blank"}  |

For each target platform the following types need to be defined:

| name                 | type                                             |
|----------------------|--------------------------------------------------|
| `bvm_uint8_t`        | unsigned 8 bit                                   |
| `bvm_int8_t`         | signed 8 bit                                     |
| `bvm_uint16_t`       | unsigned 16 bit                                  |
| `bvm_int16_t`        | signed 16 bit                                    |
| `bvm_uint32_t`       | unsigned 32 bit                                  |
| `bvm_int32_t`        | signed 32 bit                                    |
| `bvm_float_t`        | float type                                       |
| `bvm_double_t`       | double type                                      |
| `bvm_native_ulong_t` | unsigned long the same size as `sizeof(void *)`  |
| `bvm_native_long_t`  | signed long the same size as `sizeof(void *)`    |

And when native 64 bit integer is used the following must also be defined:

| name                 | type                                            |
|----------------------|-------------------------------------------------|
| `bvm_int64_t`        | unsigned 64 bit                                 |
| `bvm_uint64_t`       | signed 64 bit                                   |

When native 64 bit is not available (and emulated by Babe - `BVM_NATIVE_INT64_ENABLE` not defined), the `bvm_int64_t` and `bvm_uint64_t` types are pre-defined by the VM as a struct of two `bvm_uint32_t` in `int64_emulated.h`.  An example of usage is commented out in the linux pd header here: ([bvm_pd.h](https://github.com/babevm/babevm/blob/master/src/platform/linux/h/bvm_pd.h){:target="_blank"})[]   


