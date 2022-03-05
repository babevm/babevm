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
  @file

  JVMS Constants/Macros/Functions/Types and errors for the VM.

  @author Greg McCreath
  @since 0.0.10

*/

#ifndef BVM_VM_H_
#define BVM_VM_H_

/* handles to throwable class names used throughout the VM */

extern const char *BVM_ERR_ABSTRACT_METHOD_ERROR;
extern const char *BVM_ERR_ARITHMETIC_EXCEPTION;
extern const char *BVM_ERR_ARRAY_INDEX_OUT_OF_BOUNDS_EXCEPTION;
extern const char *BVM_ERR_ARRAYSTORE_EXCEPTION;
extern const char *BVM_ERR_CLASS_CAST_EXCEPTION;
extern const char *BVM_ERR_CLASS_CIRCULARITY_ERROR;
extern const char *BVM_ERR_CLASS_FORMAT_ERROR;
extern const char *BVM_ERR_CLASS_NOT_FOUND_EXCEPTION;
extern const char *BVM_ERR_CLONE_NOT_SUPPORTED_EXCEPTION;
extern const char *BVM_ERR_ILLEGAL_ACCESS_ERROR;
extern const char *BVM_ERR_ILLEGAL_ACCESS_EXCEPTION;
extern const char *BVM_ERR_ILLEGAL_THREAD_STATE_EXCEPTION;
extern const char *BVM_ERR_ILLEGAL_MONITOR_STATE_EXCEPTION;
extern const char *BVM_ERR_ILLEGAL_ARGUMENT_EXCEPTION;
extern const char *BVM_ERR_INTERRUPTED_EXCEPTION;
extern const char *BVM_ERR_INCOMPATIBLE_CLASS_CHANGE_ERROR;
extern const char *BVM_ERR_INDEX_OUT_OF_BOUNDS_EXCEPTION;
extern const char *BVM_ERR_STRING_INDEX_OUT_OF_BOUNDS_EXCEPTION;
extern const char *BVM_ERR_INSTANTIATION_ERROR;
extern const char *BVM_ERR_INSTANTIATION_EXCEPTION;
extern const char *BVM_ERR_INTERNAL_ERROR;
extern const char *BVM_ERR_LINKAGE_ERROR;
extern const char *BVM_ERR_NEGATIVE_ARRAY_SIZE_EXCEPTION;
extern const char *BVM_ERR_NO_CLASS_DEF_FOUND_ERROR;
extern const char *BVM_ERR_NO_SUCH_FIELD_ERROR;
extern const char *BVM_ERR_NO_SUCH_METHOD_ERROR;
extern const char *BVM_ERR_NULL_POINTER_EXCEPTION;
extern const char *BVM_ERR_RUNTIME_EXCEPTION;
extern const char *BVM_ERR_THROWABLE;
extern const char *BVM_ERR_ERROR;
extern const char *BVM_ERR_UNSATISFIED_LINK_ERROR;
extern const char *BVM_ERR_OUT_OF_MEMORY_ERROR;
extern const char *BVM_ERR_VIRTUAL_MACHINE_ERROR;
extern const char *BVM_ERR_VERIFY_ERROR;
extern const char *BVM_ERR_UNKNOWN_ERROR;
extern const char *BVM_ERR_STACK_OVERFLOW_ERROR;
extern const char *BVM_ERR_EXCEPTION_IN_INITIALIZER_ERROR;
extern const char *BVM_ERR_UNSUPPORTED_OPERATION_EXCEPTION;
extern const char *BVM_ERR_IO_EXCEPTION;
extern const char *BVM_ERR_FILE_NOT_FOUND_EXCEPTION;

#if (BVM_CONSOLE_ENABLE || BVM_DEBUGGER_ENABLE)

extern const char *BVM_VM_VERSION;
extern const char *BVM_VM_DESC;
extern const char *BVM_VM_NAME;
extern const char *BVM_VM_COPYRIGHT;

#endif

extern bvm_bool_t bvm_gl_vm_is_initialised;
extern bvm_uint32_t bvm_gl_stack_height;

extern char *bvm_gl_user_classpath;
extern char *bvm_gl_boot_classpath;

/* an array of char* that points to the path elements of the class path held
 * in #bvm_gl_boot_classpath. */
extern char *bvm_gl_boot_classpath_segments[];
extern char *bvm_gl_user_classpath_segments[];

extern bvm_bool_t bvm_gl_assertions_enabled;

extern char *bvm_gl_home_path;

/* BVM_MAX and BVM_MIN values for INT */
#define BVM_MAX_INT           ((bvm_int32_t)0x7FFFFFFF)
#define BVM_MIN_INT           ((bvm_int32_t)0x80000000)

#define BVM_MIN(a, b) ((a) < (b) ? (a) : (b))
#define BVM_MAX(a, b) ((a) > (b) ? (a) : (b))

enum BVM_FATAL_ERRS {
	BVM_FATAL_ERR_UNCAUGHT_EXCEPTION = 100,
	BVM_FATAL_ERR_NOT_ENOUGH_PARAMS = 101,							/* not enough command line params to executable */
	BVM_FATAL_ERR_NO_MAIN_METHOD = 102,								/* class specified as main has no static void main method */
	BVM_FATAL_ERR_FILE_HANDLES_EXCEEDED = 103,
	BVM_FATAL_ERR_MAX_CLASSPATH_SEGMENTS_EXCEEDED = 104,
	BVM_FATAL_ERR_NO_BOOT_CLASSPATH_SPECIFIED = 105,
	BVM_FATAL_ERR_NO_USER_CLASSPATH_SPECIFIED = 106,
	BVM_FATAL_ERR_HEAP_SIZE_LT_THAN_ALLOWABLE_MIN = 107,
	BVM_FATAL_ERR_HEAP_SIZE_GT_THAN_ALLOWABLE_MAX = 108,
	BVM_FATAL_ERR_HEAP_LEAK_DETECTED_ON_VM_EXIT = 109,				/* in debug, as the VM was closed down it found it could not free all memory */
	BVM_FATAL_ERR_INVALID_MEMORY_CHUNK = 110,						/* chunk does not pass muster when being freed */
	BVM_FATAL_ERR_ATTEMPT_TO_ALLOC_LARGER_THAN_HEAP = 111,			/* silly attempt to grab a heap chunk larger than the heap */
	BVM_FATAL_ERR_OUT_OF_MEMORY = 112,								/* unable to allocate more memory, even after a GC - and the exception is uncaught */
	BVM_FATAL_ERR_CANNOT_ALLOCATE_HEAP = 113,						/* VM was unable to create the initial heap */
	BVM_FATAL_ERR_TRANSIENT_ROOTS_EXHAUSTED = 114,					/* there is no more space in the transient root stack */
	BVM_FATAL_ERR_PERMANENT_ROOTS_EXHAUSTED = 115,					/* there is no more space in the permanent root stack */
	BVM_FATAL_ERR_NO_RUNNABLE_OR_WAITING_THREADS = 116,				/* VM has encountered a situation where there are no threads to run */
	BVM_FATAL_ERR_UNABLE_TO_REPORT_CLASS_LOADING_FAILURE = 117,		/* most likely that system classes cannot be found */
	BVM_FATAL_ERR_UNABLE_TO_ATTACH_TO_DEBUGGER = 118,				/* cannot attach to debugger */
	BVM_FATAL_ERR_INVALID_BREAKPOINT_ENCOUNTERED = 119,				/* a breakpoint was encountered and the VM was not connected to a debugger */
	BVM_FATAL_ERR_NO_BREAKPOINT_FOUND = 120,						/* the VM expected to locate a breakpoint eventdef, but it found none. */
	BVM_FATAL_ERR_FLOAT_NOT_SUPPORTED = 121,						/* A class has float constant or float bytecodes and the platform does not support float*/
	BVM_FATAL_ERR_INCORRECT_UINT8_SIZE = 122,						/* incorrect type size */
	BVM_FATAL_ERR_INCORRECT_INT8_SIZE = 123,						/* incorrect type size */
    BVM_FATAL_ERR_INCORRECT_UINT16_SIZE = 124,						/* incorrect type size */
    BVM_FATAL_ERR_INCORRECT_INT16_SIZE = 125,						/* incorrect type size */
	BVM_FATAL_ERR_INCORRECT_UINT32_SIZE = 126,						/* incorrect type size */
	BVM_FATAL_ERR_INCORRECT_INT32_SIZE = 127,						/* incorrect type size */
	BVM_FATAL_ERR_INCORRECT_FLOAT_SIZE = 128,						/* incorrect type size */
	BVM_FATAL_ERR_INCORRECT_DOUBLE_SIZE	= 129,						/* incorrect type size */
	BVM_FATAL_ERR_INCORRECT_NATIVE_LONG_SIZE = 130,					/* incorrect type size */
	BVM_FATAL_ERR_INCORRECT_CELL_SIZE = 131,					    /* incorrect type size */
	BVM_FATAL_ERR_INFLATE_FAILED = 132,					            /* jar inflation failed */
	BVM_FATAL_ERR_INFLATE_NOT_ENABLED = 133,					    /* jar inflation not enabled */
	BVM_FATAL_ERR_UNKNOWN_COMPRESSION_METHOD = 133				    /* unknown compression method used in jar */
};

int bvm_main(int argc, char *argv[]);

#if BVM_CONSOLE_ENABLE
void bvm_show_version_and_copyright();
void bvm_show_usage();
#endif



#endif /*BVM_VM_H_*/
