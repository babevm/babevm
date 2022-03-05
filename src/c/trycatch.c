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

 Try / catch and exceptions

 @author Greg McCreath
 @since 0.0.10

 @section trycatch-ov Overview

 The Babe try/catch exception handling mechanism is a means to use a java-like try/catch coding style at the
 C level.

 The usage of BVM_TRY/BVM_CATCH at the C level obviates the requirement for the typical C idiom of returning error
 codes from functions and having the caller of a function check for errors.  Just like Java, this VM relies on
 exception handling to keep its functions meaningful and uncluttered with error return codes.  In Babe, function
 return real values, not error codes.

 Exception handling uses the ANSI C \c setjmp() and \c longjmp() functionality.  The usage of which is hidden
 in the #BVM_TRY and #BVM_CATCH and #BVM_END_CATCH macros.

 It is best to think of the BVM_TRY/BVM_CATCH implementation as being a stack.  In reality it is a linked list with the
 list linkage pointers going from the last element back to the first so it can act like a stack.  Each time a
 'try' is performed a new #bvm_exception_frame_t is pushed on the stack (added to the end with a backward
 reference created to the previous frame).  The #bvm_exception_frame_t holds the backwards handle and also
 an ANSI C \c jmp_buf variable.

 The #BVM_THROW macro takes a Java \c Throwable object and does an ANSI C \c longjmp().  The \c longjmp will use
 the \c jmp_buf of the last exception frame on the try/catch stack.  Execution will re-start at the last
 #BVM_TRY macro and, because of the nature of the setjmp/longjmp usage, it will go to the code block defined by the
 try's adjacent #BVM_CATCH macro.

 In this manner, no matter where a #BVM_THROW is used, it will always go back to the last BVM_TRY used within the
 C execution stack.

 Now there's a thing:  The exception stack is actually contained within the C execution stack.  All
 information for a global reference to the exception stack top, is held in local
 variables within normal C methods - they are hidden within the BVM_TRY/BVM_CATCH/BVM_END_CATCH macros.

 @section trycatch-st Exceptions and the Transient Root Stack.

 In addition to providing BVM_TRY/BVM_CATCH services the exception handling mechanism also tracks the top of
 the transient root stack.  As each BVM_TRY block is entered the top of the global transient root stack is
 captured with the new exception frame information.  When an exception is caught or the BVM_END_CATCH is
 passed with no exception the top of the transient root stack is restored to where is was at the entry to
 the BVM_TRY/BVM_CATCH.

 In many ways a BVM_TRY/BVM_CATCH is just like a #BVM_BEGIN_TRANSIENT_BLOCK / #BVM_END_TRANSIENT_BLOCK
 combination - they both remember the top of the transient root stack when they are entered and restore it
 when they are exited.

 @section trycatch-jni.

 Now, after having extolled the virtues of exception handling at the C level - the Java Native Interface (JNI)
 specification does not use exception handling - it uses return codes.  For this reason, you'll will not find
 exception handling in the Babe Native Interface - it follows the JNI/C idiom of using error return codes.

 @section trycatch-vmexit

 There are actually two distinct forms of BVM_TRY/BVM_CATCH in the VM.  There is the (usual) BVM_TRY/BVM_CATCH
 described above and there is also a BVM_VM_TRY/BVM_VM_CATCH variant that is used to signal that the VM is going to
 exit because of a nasty error.  All operations in the VM are within a global outer BVM_VM_TRY/BVM_VM_CATCH block.

 Basically, the BVM_VM_TRY/BVM_VM_CATCH is for VM related exit conditions and there is only one.  In practice, just
 the BVM_TRY/BVM_CATCH need using for VM dev.

 Unlike the BVM_TRY/BVM_CATCH blocks the BVM_VM_TRY/BVM_VM_CATCH blocks do not deal with exception objects - just
 an error code and a message.

 There is, and can only be, one BVM_VM_TRY/BVM_VM_CATCH block in the entire VM.  The BVM_VM_TRY/BVM_VM_CATCH blocks
 do not operate with a linked list in the C stack like normal BVM_TRY/BVM_CATCH does - it just uses a few globals.
 */

/**
 * Storage for the base frame of the exception frame stack.
 */
static bvm_exception_frame_t _throwable_scope_struct = { NULL, NULL, NULL };

/**
 * Current top of the exceptions frame stack
 */
bvm_exception_frame_t *bvm_gl_exception_stack = &_throwable_scope_struct;

/**
 * Create an exception object of the given class.  The pointer to the message char array is not held.  A String
 * object is created from it (meaning the data is copied as UTF into the new String).
 *
 * Note this method is not used by running java bytecode, only by the executing VM for
 * its own purposes.  Throwable objects produced here have their \c needs_native_try_reset field set
 * the true.
 *
 * @param clazz the class of the new exception
 * @param msg the null-terminated UTF-8 encoded message for the exception.  May be \c NULL.
 *
 * @return a new throwable object
 *
 * @throw any exception to do with class loading and object creation.
 */
bvm_throwable_obj_t *bvm_create_exception(bvm_clazz_t *clazz, const char *msg) {

	bvm_throwable_obj_t *instance;

	/* create the new exception object */
	instance = (bvm_throwable_obj_t *) bvm_object_alloc( (bvm_instance_clazz_t *) clazz);

	/* mark it as a native exception */
	instance->needs_native_try_reset.int_value = BVM_TRUE;

#if BVM_STACKTRACE_ENABLE

	BVM_BEGIN_TRANSIENT_BLOCK {

		BVM_MAKE_TRANSIENT_ROOT(instance);

		/* populate the stack backtrace in case a stack trace is later required */
		bvm_stacktrace_populate_backtrace(instance);

	} BVM_END_TRANSIENT_BLOCK

#endif

	/* create a string object from the given message and store it in the
	 * exception as the 'message' field. */
    if (msg != NULL) {

		bvm_string_obj_t *message;

    	BVM_BEGIN_TRANSIENT_BLOCK {
    		BVM_MAKE_TRANSIENT_ROOT(instance);
	        message = bvm_string_create_from_cstring(msg);
	        instance->message = message;
		} BVM_END_TRANSIENT_BLOCK

    }

    return instance;
}

/**
 * Throws a native exception of the given class name using the bootstrap class loader.
 *
 * This function does not return.
 *
 * @param exception_clazz_name the class name of the exception to create.
 * @param msg the message text for the exception.  May be \c NULL.
 *
 * @see BVM_THROW
 * @see bvm_create_exception()
 */
void bvm_throw_exception(const char* exception_clazz_name, const char *msg) {
	bvm_clazz_t *clazz = bvm_clazz_get_c(BVM_BOOTSTRAP_CLASSLOADER_OBJ, exception_clazz_name);
	BVM_THROW(bvm_create_exception(clazz, msg));
}

/**
 * Creates a native exception object of the given class using the bootstrap
 * classloader.
 *
 * @param exception_clazz_name the class name of the exception to create.
 * @param msg the message text for the exception.  May be \c NULL.
 *
 * @return a new throwable object
 *
 * @throw any exception to do with class loading and object creation.
 *
 * @see #bvm_create_exception()
 */
bvm_throwable_obj_t *bvm_create_exception_c(const char* exception_clazz_name, const char *msg) {
	bvm_clazz_t *clazz = bvm_clazz_get_c(BVM_BOOTSTRAP_CLASSLOADER_OBJ, exception_clazz_name);
	return bvm_create_exception(clazz, msg);
}

