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

#include "../h/bvm.h"

/**

 @file

 Stacktrace population and printing functions.  Effectively a C implementation of the java logic
 to print formatted stacktrace information.

 @author Greg McCreath
 @since 0.0.10

 */

/**
 *
 * Callback for populating a frame in an exception backtrace.
 *
 * @param frameinfo callback info about a frame on the stack
 * @param data the #bvm_stack_backtrace_t backtrace
 *
 * @return BVM_TRUE
 */
static bvm_bool_t populate_backtrace_callback(bvm_stack_frame_info_t *frameinfo, void *data) {

	int line_number = -1;  /* -1 is 'no line number, -2 is 'native'. */

	bvm_stack_backtrace_t *backtrace = data;

	bvm_method_t *method = frameinfo->method;
	bvm_instance_clazz_t *clazz = method->clazz;

	/* ignore all frames that are for any clazz that is a throwable class or subclass. */
	if ( !(bvm_clazz_is_assignable_from( (bvm_clazz_t *) clazz, (bvm_clazz_t *) BVM_THROWABLE_CLAZZ))) {

		bvm_stack_backtrace_frameinfo_t *frame = (bvm_stack_backtrace_frameinfo_t *) &backtrace->frames[backtrace->depth++];
		frame->clazz_name = clazz->name;
		frame->method_name = method->name;

#if (BVM_LINE_NUMBERS_ENABLE || BVM_DEBUGGER_ENABLE)
		line_number = (method->line_numbers == NULL) ? -1 : bvm_clazz_get_source_line(method, frameinfo->pc);
		frame->file_name = clazz->source_file_name;
#endif
		/* line number is defaulted to -1.  If native, make -2.  If lines are supported it will be
		 * the actual line number (if it was compiled into the class file).
		 */
		frame->line_number = BVM_METHOD_IsNative(method) ? -2 : line_number;
	}

	return BVM_TRUE;
}


/**
 * Create a stack backtrace structure for a given throwable object.  This
 * function allocates memory for a #bvm_stack_backtrace_t with enough room in its \c frames array
 * to hold a #bvm_stack_backtrace_frameinfo_t for each frame on the stack (starting
 * at the top - the current executing method).
 *
 * The backtrace ignores any methods on the stack that are from the \c java.lang.Throwable class or
 * any of its subclasses.  Why?  This backtrace is called during *construction* of an exception object
 * way up at the 'Throwable' class constructor.  So as we're executing this native code, the stack is actually
 * full of constructor methods for each Throwable subclass down to the actual thrown exception class'
 * constructor.  Simply put, we just ignore them all when creating the backtrace.
 *
 * The actual backtrace population is implemented as a stack visit callback - #populate_backtrace_callback.
 *
 * @param throwable_obj the throwable object
 */
void bvm_stacktrace_populate_backtrace(bvm_throwable_obj_t *throwable_obj) {

	int depth;
	bvm_stack_backtrace_t *backtrace;

	if (!bvm_gl_vm_is_initialised) return;

	/* determine the stack depth - this may be greater than actually required.  At this point we do not know how
	 * many of the stack frames are for throwable construction (which we will omit from the trace).  We'll
	 * allocate memory for the entire depth of the trace and probably not fill it all. */
	depth = bvm_stack_get_depth(bvm_gl_thread_current);

	/* create a backtrace to accommodate a frame info element for each relevant frame in the stack */
	backtrace = bvm_heap_calloc(sizeof(bvm_stack_backtrace_t) + (depth * (sizeof(bvm_stack_backtrace_frameinfo_t) )), BVM_ALLOC_TYPE_DATA );

	BVM_MAKE_TRANSIENT_ROOT(backtrace);

	throwable_obj->backtrace = backtrace;

	/* if the throwable already has a stack strace, lose it before creating a new backtrace.
	 * This situation probably means that someone created an exception and did not throw it
	 * immediately, then later re-populated the stacktrace before it was actually thrown.
	 * NULLing the stack_trace_elements will allow it to be GC'd if no other handles are
	 * kept to it. */
	throwable_obj->stack_trace_elements = NULL;

	/* visit the stack calling populate_backtrace_callback for each frame */
	bvm_stack_visit(bvm_gl_thread_current, 0, depth, NULL, populate_backtrace_callback, backtrace);
}


/**
 * Given an #bvm_throwable_obj_t with a populated backtrace, create its array of \c java.lang.StackTraceElement
 * objects.  This function will have no effect if the array is already populated, or the throwable has no
 * backtrace.
 *
 * After creating the array, the throwable's backtrace is NULLed and the memory freed.
 *
 * @param throwable_obj the throwable to populate.
 */
void bvm_stacktrace_populate(bvm_throwable_obj_t *throwable_obj) {

	bvm_instance_array_obj_t *array;
	bvm_instance_clazz_t *element_clazz;
	bvm_stack_backtrace_frameinfo_t *frameinfo;
	bvm_string_obj_t *temp_str;
	int depth;
	int i;

	array = throwable_obj->stack_trace_elements;

	if ( (array == NULL) &&  (throwable_obj->backtrace != NULL) ) {

		element_clazz = (bvm_instance_clazz_t *) bvm_clazz_get_c(BVM_BOOTSTRAP_CLASSLOADER_OBJ, "java/lang/StackTraceElement");

		depth = throwable_obj->backtrace->depth;

		BVM_BEGIN_TRANSIENT_BLOCK {

			/* create the new StackTraceElement array */
			array = bvm_object_alloc_array_reference(depth, (bvm_clazz_t *) element_clazz);

			/* and keep a handle to it */
			BVM_MAKE_TRANSIENT_ROOT(array);

			/* for each frame element in the backtrace, create a new StackTraceElement object and populate it.
			 * This will involve creating strings based on the class and method names among other things.
			 */
			for (i=0;i<depth;i++) {

				/* create the StackTraceElement object */
				bvm_stack_frame_element_obj_t *element = (bvm_stack_frame_element_obj_t *) bvm_object_alloc(element_clazz);

				/* assign to the array .. this will also stop it getting GC'd */
				array->data[i] = (bvm_obj_t *) element;

				/* get a handle to a stack frame info object from the backtrace */
				frameinfo = &throwable_obj->backtrace->frames[i];

				/* get a String object for the class name - either from the interned string pool, or
				 * by creating it (then adding it to the pool).  Assign it to the element (that will
				 * also stop it being GC'd). */
				temp_str = (bvm_string_obj_t *) bvm_internstring_pool_get(frameinfo->clazz_name, BVM_TRUE);
				element->class_name = temp_str;

				/* .. and the same for the method name */
				temp_str = (bvm_string_obj_t *) bvm_internstring_pool_get(frameinfo->method_name, BVM_TRUE);
				element->method_name = temp_str;

				/* and the line number ..*/
				element->line_number.int_value = frameinfo->line_number;

#if (BVM_LINE_NUMBERS_ENABLE || BVM_DEBUGGER_ENABLE)

				/* .. create String from the source file name, if any */
				if (frameinfo->file_name != NULL) {
					temp_str = (bvm_string_obj_t *) bvm_internstring_pool_get(frameinfo->file_name, BVM_TRUE);
					element->file_name = temp_str;
				}
#endif
			}

			/* ... and assign the new array to the throwable object */
			throwable_obj->stack_trace_elements = array;

			/* free up the backtrace .. it is no longer needed */
			bvm_heap_free(throwable_obj->backtrace);
			throwable_obj->backtrace = NULL;

		} BVM_END_TRANSIENT_BLOCK
	}
}

#if BVM_CONSOLE_ENABLE

/**
 * Prints a formatted stack frame to the console.
 *
 * @param frameinfo the frame to print
 */
static void stacktrace_print_frameinfo(bvm_stack_backtrace_frameinfo_t *frameinfo) {

	/* output class name and method name - these will never be NULL */
	if (frameinfo->clazz_name != NULL) bvm_pd_console_out("\tat %s.%s ", frameinfo->clazz_name->data, frameinfo->method_name->data);

	if (frameinfo->line_number == -2) {
		bvm_pd_console_out("(Native Method)\n");
	} else {

		if ( (frameinfo->file_name != NULL) && (frameinfo->line_number >= 0)) {
			bvm_pd_console_out("(%s:%u)\n", frameinfo->file_name->data, (int) frameinfo->line_number);
		} else {

			if (frameinfo->file_name != NULL) {
				bvm_pd_console_out("(%s)\n", frameinfo->file_name->data);
			} else {
				bvm_pd_console_out("(Unknown Source)\n");
			}
		}
	}
}


/**
 * Tests whether two frameinfos are for the same stack frame.  Two frames are considered equal if the clazz_name,
 * method_name, file_name, and line_number are exactly the same.
 *
 * @param frame1 the first frame to compare
 * @param frame2 the second frame to compare
 *
 * @return \c BVM_TRUE if the frames are the same, \c BVM_FALSE otherwise.
 */
static bvm_bool_t frames_are_equals(bvm_stack_backtrace_frameinfo_t *frame1, bvm_stack_backtrace_frameinfo_t* frame2) {

	return ( (frame1->clazz_name == frame2->clazz_name) &&
			 (frame1->method_name == frame2->method_name) &&
			 (frame1->file_name == frame2->file_name) &&
			 (frame1->line_number == frame2->line_number) );
}

/**
 * Prints a stacktrace of the cause of an throwable showing "caused by ..."
 *
 * @param throwable_obj
 * @param caused_backtrace
 */
static void stacktrace_print_as_cause(bvm_throwable_obj_t *throwable_obj, bvm_stack_backtrace_t *caused_backtrace) {

	bvm_uint32_t m, n, i, frames_in_common;
	bvm_stack_backtrace_t *backtrace = throwable_obj->backtrace;

	if (backtrace != NULL) {

		m = backtrace->depth-1;
		n = caused_backtrace->depth-1;

		for (; m >= 0 && n >= 0 && frames_are_equals(&backtrace->frames[m], &caused_backtrace->frames[n]) ; m--, n--);

		frames_in_common = backtrace->depth - 1 - m;

		bvm_pd_console_out("Caused by: %s ", throwable_obj->clazz->name->data);
		if (throwable_obj->message != NULL) {
			bvm_string_print_to_console(throwable_obj->message, NULL);
		}

		bvm_pd_console_out("\n");

		for (i=0; i <= m; i++) {
			stacktrace_print_frameinfo(&backtrace->frames[i]);
		}

		if (frames_in_common > 0)
			bvm_pd_console_out("\t... %u more \n", frames_in_common);

		if (throwable_obj->cause != NULL)
			stacktrace_print_as_cause(throwable_obj->cause, backtrace);
	}
}

/**
 * Prints a throwable's stack trace to the console.  If the throwable has no backtrace data this function
 * has no effect.  As the java level stack trace printing removes a throwable's backtrace - this function only does
 * a stacktrace if the throwable has not printed a stack trace at the java level.
 *
 * The logic here is effectively a re-implementation of the java level stacktrace.
 *
 * @param throwable_obj the throwable to print the trace of.
 */
void bvm_stacktrace_print_to_console(bvm_throwable_obj_t *throwable_obj) {

	int i;
	bvm_stack_backtrace_t *backtrace;

	if (bvm_gl_thread_current->thread_obj->name != NULL) {
		bvm_string_print_to_console(bvm_gl_thread_current->thread_obj->name, "Exception in thread \"%s\"");
	}

	bvm_pd_console_out(" %s: ", throwable_obj->clazz->name->data);
	if (throwable_obj->message != NULL) {
		bvm_string_print_to_console(throwable_obj->message, NULL);
	}

	bvm_pd_console_out("\n");

	backtrace = throwable_obj->backtrace;

	/* if this throwable has backtrace, use it */
	if (backtrace != NULL) {

		for (i=0; i < backtrace->depth; i++) {
			stacktrace_print_frameinfo(&backtrace->frames[i]);
		}

		/* if the throwable has a cause output it */
		if (throwable_obj->cause != NULL)
			stacktrace_print_as_cause(throwable_obj->cause, backtrace);
	}
}

#endif
