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

#ifndef BVM_STACKTRACE_H_
#define BVM_STACKTRACE_H_

/**
  @file

  Constants/Macros/Functions/Types for the stack traces.

  @author Greg McCreath
  @since 0.0.10

*/

/**
 * A summary of a single stack frame used by an exception object to create Java \c java.lang.StackTraceElement
 * object for the \c Thread.printStackTrace() method.
 */
typedef struct _bvmstackbacktracesegmentstruct {

	/** The class name the stack frame's method belongs to */
	bvm_utfstring_t *clazz_name;

	/** The method that was executing at the frame */
	bvm_utfstring_t *method_name;

	/** The name of the source file containing the executing class/method */
	bvm_utfstring_t *file_name;

	/** The line number within the source file where the frame is at */
	bvm_int32_t	 line_number;

} bvm_stack_backtrace_frameinfo_t;

/**
 * A holder of stack frame information used by exception to generate a stack trace using the
 * \c Thread.printStackTrace() method.
 */
struct _bvmstackbacktracestruct {

	/** the number of frames described in #frames */
	bvm_int32_t depth;

	/** An array of #bvm_stack_backtrace_frameinfo_t - one for each frame that was on the stack when
	 * the exception occurred */
	bvm_stack_backtrace_frameinfo_t frames[1];
};

/**
 * A Java \c java.lang.StackStraceElement object.
 */
typedef struct _bvmstackframelementstruct {
	BVM_COMMON_OBJ_INFO

	/** The String class name of the class whose method occupies this stack frame */
	bvm_string_obj_t *class_name;

	/** the String name of the method that was executing at this stack frame */
	bvm_string_obj_t *method_name;

	/** The String name of the source file (if any) of the executing method.  The compile-time-option
	 * #BVM_LINE_NUMBERS_ENABLE must be \c 1 for this to have any other value except \c NULL.  Additionally, the java class
	 * file must have been compiled with the java option to include the file name in the class.
	 */
	bvm_string_obj_t *file_name;

	/** The line number within the source file of the current executing line within the frame segment.  The compile-time-option
	 * #BVM_LINE_NUMBERS_ENABLE must be \c 1 for this to have any other value except \c -2 (if the method is native), or \c -1
	 * (which represents no line number).
	 */
	bvm_cell_t line_number;

} bvm_stack_frame_element_obj_t;

void bvm_stacktrace_populate_backtrace(bvm_throwable_obj_t *throwable_obj);
void bvm_stacktrace_populate(bvm_throwable_obj_t *throwable_obj);

#if BVM_CONSOLE_ENABLE
void bvm_stacktrace_print_to_console(bvm_throwable_obj_t *throwable_obj);
#endif

#endif /* BVM_STACKTRACE_H_ */
