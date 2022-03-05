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

#ifndef BVM_TRYCATCH_H_
#define BVM_TRYCATCH_H_

/**
  @file

  Constants/Macros/Functions/Types for try/catch support.

  @author Greg McCreath
  @since 0.0.10

*/

/**
 * A frame struct for keeping track of exception environments in nested try / catch
 * blocks.
 */
typedef struct _bvmexceptionstackframe {

	/** handle to ANSI C \c jmp_buf use to store env at entry of try block */
    jmp_buf *jmpenv;

    /** handle to the exception object thrown using #BVM_THROW */
    bvm_throwable_obj_t *exception;

    /** handle to the previous #bvm_exception_frame_t in the stack */
    struct _bvmexceptionstackframe *prev;
} bvm_exception_frame_t;

/* handle to top of exceptions frame stack */
extern bvm_exception_frame_t *bvm_gl_exception_stack;

/**
 * A handle to a jmp_buf created at VM startup - used to cause a jump to a specific place
 * on a #BVM_VM_EXIT.
 */
void* bvm_vm_exit_env;

/**
 * On a #BVM_VM_EXIT, an exit code may be specified.  It is held here afterwards.
 */
int bvm_gl_vm_exit_code;

/**
 * On a #BVM_VM_EXIT, an exit message may be specified.  It is held here afterwards.
 */
char *bvm_gl_vm_exit_msg;

#define BVM_TRY                                                     	\
    {                                                           	\
	    bvm_exception_frame_t exception_frame;                      	\
        bvm_bool_t exception_is_caught = BVM_FALSE;						\
        bvm_uint32_t transient_stack_mark = bvm_gl_gc_transient_roots_top;	\
        int exception_state;                                        \
        jmp_buf exception_jmpenv;                                   \
        exception_frame.prev = bvm_gl_exception_stack;              	\
        bvm_gl_exception_stack = &exception_frame;                  	\
        bvm_gl_exception_stack->jmpenv = &exception_jmpenv;             \
        if ((exception_state = setjmp(exception_jmpenv)) == 0) {

#define BVM_CATCH(__exception)                                    		\
        }                                                       	\
	    bvm_gl_gc_transient_roots_top = transient_stack_mark;			\
        if (exception_state != 0)  {                                \
            bvm_throwable_obj_t *__exception = bvm_gl_exception_stack->exception;		\
            bvm_gl_exception_stack = exception_frame.prev;              \
	        exception_is_caught = BVM_TRUE;

#define BVM_END_CATCH                                               	\
		}															\
	    if (!exception_is_caught) {									\
	    	bvm_gl_exception_stack = exception_frame.prev;         		\
	    }															\
	    bvm_gl_gc_transient_roots_top = transient_stack_mark;			\
    }

#define BVM_THROW(__exception)                                    		\
    {                                                           	\
		bvm_gl_exception_stack->exception = __exception;          		\
        longjmp(*((jmp_buf*)bvm_gl_exception_stack->jmpenv),1);        	\
    }

#define BVM_VM_TRY                                               		\
    {                                                          		\
        jmp_buf exception_jmpenv;                                   \
        bvm_vm_exit_env = &(exception_jmpenv);                          \
        if (setjmp(exception_jmpenv) == 0) {

#define BVM_VM_EXIT(__exit_code, __exit_msg) 						    \
		{                                      				   		\
			bvm_gl_vm_exit_code = __exit_code;                          \
			bvm_gl_vm_exit_msg  = __exit_msg;                           \
	        longjmp(*((jmp_buf*)bvm_vm_exit_env),1);				   	\
		}

#define BVM_VM_CATCH(__exit_code, __exit_msg)                          \
        } else {                                               		\
            int __exit_code = bvm_gl_vm_exit_code;						\
			char *__exit_msg = bvm_gl_vm_exit_msg;

#define BVM_VM_END_CATCH                                          		\
        }                                                      		\
    }

/* prototypes */

void bvm_throw_exception(const char* exception_clazz_name, const char *msg);
bvm_throwable_obj_t *bvm_create_exception_c(const char* exception_clazz_name, const char *msg);
bvm_throwable_obj_t *bvm_create_exception(bvm_clazz_t *clazz, const char* msg);

#endif /*BVM_TRYCATCH_H_*/
