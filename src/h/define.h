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

#ifndef BVM_DEFINE_H_
#define BVM_DEFINE_H_

/**

 @file

 Compile time definitions.

 @author Greg McCreath
 @since 0.0.10

 */

/**
 * When set, the VM performs a GC before every memory allocation.
 *
 * Default is disabled.
 *
 * @note This is pretty severe stuff - it causes a complete garbage collection cycle before *every* memory
 * allocation - thus if there are to be memory issues or GC issues, this will help find them.
 */
#ifndef BVM_DEBUG_HEAP_GC_ON_ALLOC
#define BVM_DEBUG_HEAP_GC_ON_ALLOC 0
#endif


/**
 * When set, memory chunks are filled with zeros as the are freed.  For debugging.
 */
#ifndef BVM_DEBUG_HEAP_ZERO_ON_FREE
#define BVM_DEBUG_HEAP_ZERO_ON_FREE 0
#endif

/**
 * When set, on VM exit, clear the heap of all known stuff and see if anything is left over - to check the
 * allocator.  For debugging.
 */
#ifndef BVM_DEBUG_CLEAR_HEAP_ON_EXIT
#define BVM_DEBUG_CLEAR_HEAP_ON_EXIT 0
#endif

/**
 * Enables platform support for the #bvm_pd_console_out() function.
 */
#ifndef BVM_CONSOLE_ENABLE
#define BVM_CONSOLE_ENABLE 1
#endif

/**
 * Enables platform support for floating point.  The underlying platform/compiler must provide support for a
 * 32 bit single precision float type and a 64 bit double precision float type.  Additionally, float support is
 * only possible when native 64 bit integer is also supported - see #BVM_NATIVE_INT64_ENABLE.
 */
#ifndef BVM_FLOAT_ENABLE
#define BVM_FLOAT_ENABLE 1
#endif

/**
 * Enables support for line number within class files.  Line numbers will also be supported
 * if #BVM_DEBUGGER_ENABLE is enabled.
 *
 * @note Line number support consumes heap and can add considerably to the size of a VM-loaded class.  Line
 * numbers are really only used for debugging and stack trace.  In a production environment,
 * consider disabling this.
 */
#ifndef BVM_LINE_NUMBERS_ENABLE
#define BVM_LINE_NUMBERS_ENABLE 1
#endif

/**
 * Enables support for providing stack trace information to the console (if supported,
 * refer #BVM_CONSOLE_ENABLE) and to the java level.
 */
#ifndef BVM_STACKTRACE_ENABLE
#define BVM_STACKTRACE_ENABLE 1
#endif

/**
 * Enables support for allowing a Java program to exit using \c System.exit().
 */
#ifndef BVM_RUNTIME_EXIT_ENABLE
#define BVM_RUNTIME_EXIT_ENABLE 1
#endif

/**
 * Enables support for JDPA / JDWP runtime debugging.
 */
#ifndef BVM_DEBUGGER_ENABLE
#define BVM_DEBUGGER_ENABLE 1
#endif

/**
 * Enables debugger support for JDWP canGetBytecodes capability.  Only valid if debugger support
 * is also enabled - refer #BVM_DEBUGGER_ENABLE.
 */
#ifndef BVM_DEBUGGER_BYTECODES_ENABLE
#define BVM_DEBUGGER_BYTECODES_ENABLE 0
#endif

/**
 * Enables debugger support for JDWP monitor information capabilities. Only valid if debugger
 * support is also enabled - refer #BVM_DEBUGGER_ENABLE.
 */
#ifndef BVM_DEBUGGER_MONITORINFO_ENABLE
#define BVM_DEBUGGER_MONITORINFO_ENABLE 0
#endif

/**
 * Enables debugger support for JSR045 "Debugging Other Language".  In JDWP terms, this means
 * provide support for the \c canSetDefaultStratum and \c canGetSourceDebugExtension new capabilities.
 *
 * @note The JSR045 is very vague on exactly what a VM might do to support JSR045 - there is no actual
 * definition of what effect canSetDefaultStratum has and what happens when it is set.  Weird.
 */
#ifndef BVM_DEBUGGER_JSR045_ENABLE
#define BVM_DEBUGGER_JSR045_ENABLE 1
#endif

/**
 * Enables debugger support for the 'Signature' class file attributes for fields, methods, and
 * classes.
 *
 * @note This adds a utfstring pointer to every field, method and class and also creates a utfstring
 * for each signature encountered.  So be wary, it may consume quite some heap.  Included for compliance
 * with the Java 6 enhancements to JDWP - although it seems that successful debugging can be carried
 * on without it.  Might depend on the debugger.
 */
#ifndef BVM_DEBUGGER_SIGNATURES_ENABLE
#define BVM_DEBUGGER_SIGNATURES_ENABLE 1
#endif

/**
 * The number of thread switches to do before checking if any data is waiting from the debugger.
 */
#ifndef BVM_DEBUGGER_INTERACTION_COUNT
#define BVM_DEBUGGER_INTERACTION_COUNT 1
#endif

/**
 * Enables use of the native 64 bit integer type.  If disabled, the VM will emulate 64
 * bits using two 32 bit numbers.
 *
 * @note A 64 bit type is required if #BVM_FLOAT_ENABLE is enabled.
 */
#ifndef BVM_NATIVE_INT64_ENABLE
#define BVM_NATIVE_INT64_ENABLE 1
#endif

/**
 * Enables Java assertions.  The response to Java \c Class.desiredAssertionStatus().  Can be set using command
 * line option \c -ea.  The #bvm_gl_assertions_enabled global variable will be defaulted to true if defined.
 */
#ifndef BVM_ASSERTIONS_ENABLE
#define BVM_ASSERTIONS_ENABLE 0
#endif

/**
 * When set, the VM will exit with \c BVM_FATAL_ERR_UNCAUGHT_EXCEPTION if an exception is uncaught.  Normal behaviour is
 * uncaught exceptions are passed to the current thread's uncaught exception handler for processing.
 */
#ifndef BVM_EXIT_ON_UNCAUGHT_EXCEPTION
#define BVM_EXIT_ON_UNCAUGHT_EXCEPTION 0
#endif

/**
 * When set, VM will perform extra checking on heap chunks for correctness on #bvm_heap_free and #bvm_heap_clone.  For
 * debugging purposes.
 */
#ifndef BVM_DEBUG_HEAP_CHECK_CHUNKS
#define BVM_DEBUG_HEAP_CHECK_CHUNKS 0
#endif

/**
 * Enables big endian support.
 */
#ifndef BVM_BIG_ENDIAN_ENABLE
#define BVM_BIG_ENDIAN_ENABLE 0
#endif

/**
 * Enables support for runtime binary compatibility checking in classes.
 */
#ifndef BVM_BINARY_COMPAT_CHECKING_ENABLE
#define BVM_BINARY_COMPAT_CHECKING_ENABLE 1
#endif

/**
 * Enables support for compression in jar files.
 */
#ifndef BVM_JAR_INFLATE_ENABLE
#define BVM_JAR_INFLATE_ENABLE 1
#endif

/**
 * Enables support for ANSI C console.
 */
#ifndef BVM_ANSI_CONSOLE_ENABLE
#define BVM_ANSI_CONSOLE_ENABLE 1
#endif

/**
 * Enables support for ANSI C file operations.
 */
#ifndef BVM_ANSI_FILE_ENABLE
#define BVM_ANSI_FILE_ENABLE 1
#endif

/**
 * Enables support for ANSI C heap.
 */
#ifndef BVM_ANSI_MEMORY_ENABLE
#define BVM_ANSI_MEMORY_ENABLE 1
#endif

/**
 * Enables support for sockets.
 */
#ifndef BVM_SOCKETS_ENABLE
#define BVM_SOCKETS_ENABLE 1
#endif

/**
+ * Enables BSD sockets - assumes \c BVM_SOCKETS_ENABLE is also enabled.
+ */
#ifndef BVM_BSD_SOCKETS_ENABLE
#define BVM_BSD_SOCKETS_ENABLE 1
#endif

/*
 * Let the platform override anything it needs to, or whatever.  Notice that all the defines above this line
 * are of a boolean on/off nature.  All defines below this line will set a default if no value has been given.
 */

/**
 * The default listen port when debug listening on TCP transport in server mode.  When debug attaching to
 * a debugger this is the default port to attach to on the default host name of "localhost". Only valid
 * if debugger support is also enabled - refer #BVM_DEBUGGER_ENABLE.  Overridden with the \c
 * address=xxxx setting the \c -Xrunjdwp command line option.
 *
 * Default is 8000.
 */
#ifndef BVM_DEBUGGER_SOCKET_PORT
#define BVM_DEBUGGER_SOCKET_PORT 	8000
#endif

/**
 * Default debugger connection attach/accept/handshake timeout in millseconds.  Can be set with the \c timeout=xxx
 * setting in the =Xrunjdwp command line option.  Only valid if debugger support is also enabled - refer
 * #BVM_DEBUGGER_ENABLE.  The #bvmd_gl_timeout global variable will be set to #BVM_DEBUGGER_TIMEOUT if no
 * command line value is given.
 *
 * Default is 60000 milliseconds.
 */
#ifndef BVM_DEBUGGER_TIMEOUT
#define BVM_DEBUGGER_TIMEOUT 		60000
#endif

/**
 * Data packet size in bytes for reading/writing JDWP debug packets.   Only valid if debugger support is also
 * enabled - refer #BVM_DEBUGGER_ENABLE.
 *
 * Default is 128 bytes.
 */
#ifndef BVM_DEBUGGER_PACKETDATA_SIZE
#define BVM_DEBUGGER_PACKETDATA_SIZE 		128
#endif

/**
 * Platform path separator.
 *
 * Default is ':'.
 */
#ifndef BVM_PLATFORM_PATH_SEPARATOR
#define BVM_PLATFORM_PATH_SEPARATOR 	':'
#endif

/**
 * Platform directory separator.
 *
 * Default is '/'.
 */
#ifndef BVM_PLATFORM_FILE_SEPARATOR
#define BVM_PLATFORM_FILE_SEPARATOR 	'/'
#endif

/**
 * Maximum number of #BVM_PLATFORM_PATH_SEPARATOR separated segments in a classpath.
 *
 * Default is 5.
 */
#ifndef BVM_MAX_CLASSPATH_SEGMENTS
#define BVM_MAX_CLASSPATH_SEGMENTS		5
#endif

/**
 * Initial thread stack height (in cells - see #bvm_cell_t).  Can be set using command line
 * option \c -stack.  The #bvm_gl_stack_height global variable will be set to #BVM_THREAD_STACK_HEIGHT if
 * the value is not specified on the command line.
 *
 * Default is 256 cells.
 */
#ifndef BVM_THREAD_STACK_HEIGHT
#define BVM_THREAD_STACK_HEIGHT 256
#endif

/**
 * Default heap size in bytes.  Heap size can set using command line option \c -heap and specifying
 * bytes, kb, or Mb.  The #bvm_gl_heap_size global variable will be set to #BVM_HEAP_SIZE if the value is
 * not specified on the command line.
 *
 * Default is 256.
 */
#ifndef BVM_HEAP_SIZE
#ifdef BVM_DEBUGGER_ENABLE
#define BVM_HEAP_SIZE          (256 * BVM_KB)
#else
#define BVM_HEAP_SIZE          (256 * BVM_KB)
#endif
#endif

/**
 * Height of transient root stack in cells (refer #bvm_cell_t).  Can be set using command line option \c -tr.
 * The #bvm_gl_gc_transient_roots_depth global variable will be set to #BVM_GC_TRANSIENT_ROOTS_DEPTH if the value is
 * not specified on the command line.
 *
 * Default is 50 cells.
 */
#ifndef BVM_GC_TRANSIENT_ROOTS_DEPTH
#define BVM_GC_TRANSIENT_ROOTS_DEPTH   		50
#endif

/**
 * Height of permanent root stack in cells (refer #bvm_cell_t).  Can be set using command line option \c -sr. Has
 * the effect of setting the #bvm_gl_gc_permanent_roots_depth global variable.
 *
 * Default is 100 cells.
 */
#ifndef BVM_GC_PERMANENT_ROOTS_DEPTH
#define BVM_GC_PERMANENT_ROOTS_DEPTH   		100
#endif

/**
 * Default number of hash buckets in the intern string pool.  Can be set using command line option \c -strb.
 * The #bvm_gl_internstring_pool_bucketcount global variable will be set to #BVM_INTERNSTRING_POOL_BUCKETCOUNT if
 * no command line value is given.
 *
 * Default is 64 buckets.
 */
#ifndef BVM_INTERNSTRING_POOL_BUCKETCOUNT
#define BVM_INTERNSTRING_POOL_BUCKETCOUNT  	64
#endif

/**
 * Default number of hash buckets in the utfstring pool.  Can be set using command line option \c -utfb.
 * The #bvm_gl_utfstring_pool_bucketcount global variable will be set to #BVM_UTFSTRING_POOL_BUCKETCOUNT if no
 * command line value is given.
 *
 * Default is 64 buckets.
 */
#ifndef BVM_UTFSTRING_POOL_BUCKETCOUNT
#define BVM_UTFSTRING_POOL_BUCKETCOUNT 		64
#endif

/**
 * Default number of hash buckets in the native method pool.  Unlike other pool sizes this is cannot be
 * set from the command line.
 *
 * Default is 16 buckets.
 */
#ifndef BVM_NATIVEMETHOD_POOL_BUCKETCOUNT
#define BVM_NATIVEMETHOD_POOL_BUCKETCOUNT 	16
#endif

/**
 * Default number of hash buckets in the clazz pool.  Can be set using command line option \c -clazzb.  The
 * #bvm_gl_clazz_pool_bucketcount global variable will be set to #BVM_CLAZZ_POOL_BUCKETCOUNT if no command line
 * value is given.
 *
 * Default is 64 buckets.
 */
#ifndef BVM_CLAZZ_POOL_BUCKETCOUNT
#define BVM_CLAZZ_POOL_BUCKETCOUNT 	64
#endif

/**
 * Default max file handles.  Can be set using command line option \c -files.  The #bvm_gl_max_file_handles
 * global variable will be set to #BVM_MAX_FILE_HANDLES if no command line value is given.
 *
 * Default is 32 files.
 */
#ifndef BVM_MAX_FILE_HANDLES
#define BVM_MAX_FILE_HANDLES 		32
#endif

/**
 * Number of opcodes to use when calculating a thread's timeslice.  Default is 300 opcodes normally, or 100
 * opcodes if debugging is enabled.
 */
#ifndef BVM_THREAD_TIMESLICE
#if BVM_DEBUGGER_ENABLE
#define BVM_THREAD_TIMESLICE 		100
#else
#define BVM_THREAD_TIMESLICE 		300
#endif
#endif

/* Sanity check - float support requires native 64 bit integer support */
#if BVM_FLOAT_ENABLE
#if (!BVM_NATIVE_INT64_ENABLE)
#error "Floating point support requires BVM_NATIVE_INT64_ENABLE to be 1"
#endif
#endif

#ifndef BVM_32BIT_ENABLE
#define BVM_32BIT_ENABLE 0
#if (!BVM_NATIVE_INT64_ENABLE)
#error "64 bit systems must not have BVM_NATIVE_INT64_ENABLE as 0"
#endif
#endif

/* Sanity check - X86 endian-ness */
//#ifdef BVM_CPU_X86
//#undef BVM_BIG_ENDIAN_ENABLE
//#define BVM_BIG_ENDIAN_ENABLE 0
//#endif

/**
 * "Direct Threading" takes advantage of the GCC ability to treat labels as first class values and are
 * used with "goto" to speed up a 'switch'-like statement.  If \c __GNUC__ is defined we'll define
 * \c BVM_DIRECT_THREADING_ENABLE=1 so that the interp dispatch can be sped up.  Probably by 5% to 10%.
 *
 * On GNUC, direct threading will be enabled by default, but can be turned off by
 * defining \c BVM_DIRECT_THREADING_ENABLE=0.
 *
 * @note The use of direct threading avoids a few extra machine instructions that a 'switch'
 * statement generates to range check.  If GCC, unless you're having issues, direct threading is
 * recommended.
 *
 */
#ifndef BVM_DIRECT_THREADING_ENABLE
#if (defined(__GNUC__))
#define BVM_DIRECT_THREADING_ENABLE 1
#else
#define BVM_DIRECT_THREADING_ENABLE 0
#endif
#endif

#endif /*BVM_DEFINE_H_*/
