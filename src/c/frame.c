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

  Frame and VM Register Management.

  @section frame-overview Frames and Stacks

  A <i>frame</i> contains information about the execution of a Java method.  A <i>stack</i> is where these frames
  are held by the VM during their execution.  A stack is made of a one-way linked list of
  stack segments (#bvm_stacksegment_t).

  Every thread has a stack.  Frames are pushed onto a thread's stack before method invocation and popped from the stack
  on return from the method.  Each frame stores info that needs to be restored when it is
  popped.  Perhaps, unlike some frame implementations (I think) which detail the method that is <i>currently</i>
  executing, these frames tell about the method that <i>was</i> executing before the currently executing
  method.

  The information about the <i>currently</i> executing method is held in (for lack of a better term)
  global VM 'registers'.  These are global variables all prefixed with \c 'gl_rx_'.  The VM
  registers store information about the execution state of the currently executing method.  When a method is
  being called the current VM registers are pushed onto the stack so that they may be restored when
  the new method is popped after it is finished.

  Using global registers for state of the current executing method is a performance decision.  Accessing method execution
  state like the current instruction pointer ('program counter' - PC) or the current stack pointer are operations that
  happen a <em>LOT</em>.  Keeping these high-access variables as globals rather than on the stack means they can be accessed
  directly, rather than via pointer indirection - access is reduced from two instructions to one instruction - and
  given the amount of time they are accessed, this hopefully can make a difference.

  Likewise, as a thread is 'switched' away from, the current global registers are stored into the thread's state,
  and the switched-to thread has its registers restored to the global set.

  Basically, the global registers are the only ones that are actually used at any time.  They are swapped in and out on
  thread switch and on frame push/pop.

  The registers are as follows.

  frame locals: (#bvm_gl_rx_locals) A pointer to the base #bvm_cell_t of the part of the stack used by the currently
  executing method. This is a pointer to the first storage cell for method-local variables.  For non-static
  methods, this will also be a pointer to the object that the method is being called upon
  (In Java, 'this' is always the first local variable of a method).

  current method: (#bvm_gl_rx_method) A pointer to the #bvm_method_t method description of the currently executing method.

  program counter: (#bvm_gl_rx_pc) Often also called the instruction pointer, this is a pointer to the currently
  executing opcode of the currently executing method.

  previous program counter: (#bvm_gl_rx_ppc) not so easy to explain but here goes:  As each frame is pushed the
  program counter is advanced to point to the instruction that will be executed when method execution resumes
  again after a frame pop.  It is 'moved on' before the push takes place.  We do this because when a frame is pushed
  we know where we want it to be when it resumes execution, but we do not have the same contextual information on
  a pop.  However, the danger with pre-calculating the to-execute-next instruction on a push is that exception handling
  cannot back-calculate to determine the *exact* bytecode the method was executing when it was pushed.  The obvious answer
  is to 'move on' the program counter *after* the pop - then we do not have to keep a PC and a PPC.  *BUT*, the PC is not
  moved by the same amount every time a method invocation takes place - an 'invokeinterface' method call moves it on 5,
  all other method invocation move it on 3.  So we would need to keep that information somewhere to know how to move the PC
  on afterwards.  Same situation then. May as well just store the PPC.

  The exception handling needs to know the 'previous bytecode' in order to correctly determine which try/catch/finally
  clause it may appear in, if any.

  Using the 'moved on' #bvm_gl_rx_pc for exception handling will actually work in most instances - but it is not
  exact.  Imagine a scenario where the current executing bytecode is a method invocation and it is the last bytecode
  within a try/catch block.  When we invoke the method, if we pushed just the next bytecode to execute it would fall outside
  the try/catch block and exception handling would fail to determine that the previous pushed frame's bytecode was within
  a try/catch.  That is why we must keep the *last* executed bytecode also.

  stack pointer: (#bvm_gl_rx_sp) a pointer to the #bvm_cell_t 'top' of the current stack.  The top of the stack is
  where the next pushed cell will be written - it points to 'one further' than the topmost currently used stack
  cell.

  stack segment:  (#bvm_gl_rx_stack) A pointer to the current #bvm_stacksegment_t stack segment in use.  Thread stacks
  dynamically grow and shrink as required.  The segments can be different sizes so a handle to the current one
  is necessary so we can determine how much space is left in it.

  sync obj: holds the sync object for the *currently executing method* if the method is synchronized.
  Unlike all other elements in a pushed frame which refer to the *calling* method, the sync object is for
  the currently executing method (the 'called' method).

  * a note about \c bvm_gl_rx_clazz:  This global register is not stored on the stack.  It can be
  derived easily from \c bvm_gl_rx_method each swap in/out of global registers.

  @section frame-stacks Stacks

  Each thread has a stack.  Each stack may have one or more stack segments. Each frame push may cause a new
  heap-allocated stack segment to be appended to the current stack.  Each segment has a link to the next stack
  segment in the list (if any).  If the size requirements of a to-be-pushed frame exceed the remaining capacity
  of a stack segment, then the frame-push logic takes one of two approaches:

  @li If there is no 'next' stack segment a new one is created that is the larger of the default
  stack segment size, or the new frame's size requirements.

  @li If there is already a 'next' stack segment, that stack segment will be used if
  (and only if) it can accommodate the new frame's size requirements - if it can not, a new stack segment
  is created that will accommodate the frame and is made to be the 'next' stack segment of the current stack segment.
  This has the effect of severing the existing 'next' link in the list.  All those severed segments will now be
  unreachable and eligible for GC.

  @section frame-structure Frame structure

  The size of each frame is calculated as such:

 @verbatim
    method->max_stack + method->max_locals + BVM_STACK_RETURN_FRAME_SIZE;
 @endverbatim

 Each Java method describes its max stack requirements and the max local values it has - both of these values are provided
 by the compiler.  In addition to that is the 'return frame data'.  As each new frame is created the current registers
 are place at the bottom of the frame.  This is essentially to store them so that when the new frame eventually is
 popped off the stack, those registers can be restored.

 Above that 'return frame data' is the area used by the running exec loop

  @section frame-cells Cells

  The basic unit of storage on the stack, and indeed for object and class fields, is the 'cell' as defined by #bvm_cell_t.  A cell
  is a union of the different types of values that may be held in the stack or field.  A thread stack is therefore a stack
  of cells.  Fields values are also held in cells.

  @author Greg McCreath
  @since 0.0.10
*/

/** Global 'stack pointer' register */
bvm_cell_t *bvm_gl_rx_sp = NULL;

/** Global 'program counter' register */
bvm_uint8_t *bvm_gl_rx_pc = NULL;

/** Global 'previous program counter' register */
bvm_uint8_t *bvm_gl_rx_ppc = NULL; // may not be needed anymore

/** Global 'current clazz' register */
bvm_instance_clazz_t *bvm_gl_rx_clazz = NULL;

/** Global 'current executing method' register */
bvm_method_t *bvm_gl_rx_method = NULL;

/** Global 'element zero of the method-local variables' register */
bvm_cell_t *bvm_gl_rx_locals = NULL;

/** Global 'current stack segment' register */
bvm_stacksegment_t *bvm_gl_rx_stack = NULL;

/**
 * Push a frame onto the current stack segment.  If the current stack segment is too small this
 * may have the effect of allocating a new stack segment from the heap and attaching it to the
 * current one.
 *
 * @param method - the method that is about to be executed.  This method will become the
 * new #bvm_gl_rx_method.
 * @param current_sp - a pointer to a place in the stack that will become the #bvm_gl_rx_sp when the pushed
 * frame is popped.
 * @param current_pc - a pointer to the current executing instruction.  If an exception is thrown, this PC will
 * be used to determine exactly where the method was when the exception was raised - search for try/finally/catch blocks
 * uses this PC.
 * @param next_pc - a pointer to the instruction to execute when the pushed method is popped and resumes
 * execution.
 * @param sync_obj - a pointer to the object (if any) that is the synchronisation object for the being-called method.
 */
void bvm_frame_push(bvm_method_t *method, bvm_cell_t *current_sp, bvm_uint8_t *current_pc, bvm_uint8_t *next_pc, bvm_obj_t *sync_obj) {

	bvm_uint32_t required_size;
	bvm_stacksegment_t *current_stack;
	bvm_cell_t *frame_cell_ptr;

	current_stack = bvm_gl_rx_stack;

	/* Check for stack segment overflow (= cannot fit the next frame + method locals and op-stack).
	 * If all this cannot fit, use the next one if it exists and will fit it.  If no next
	 * one exists or it simply isn't big enough, allocate a new one.  */

	required_size = method->max_stack + method->max_locals + BVM_STACK_RETURN_FRAME_SIZE;

	/* calc the starting address for the next frame.  It will be after the current frame.  Note if the
	 * bvm_gl_rx_method is NULL then there is no current method executing .. that is, the VM is
	 * booting.  Shame we have to check every time for it ... */
	frame_cell_ptr = (bvm_gl_rx_method == NULL) ? bvm_gl_rx_sp : bvm_gl_rx_locals + bvm_gl_rx_method->max_stack + bvm_gl_rx_method->max_locals;

	/* test to see if the size requirements of the new frame will allow it to fit into the current stack */
	if ((frame_cell_ptr + required_size) >= bvm_gl_rx_stack->top ) {

		/* no next one, or it will not fit */
		if ( (bvm_gl_rx_stack->next == NULL) ||
		     ( (bvm_gl_rx_stack->next != NULL) && (bvm_gl_rx_stack->next->height < required_size))) {

			/* the new size is the larger of the required size or the default */
			bvm_uint32_t real_size = (required_size > bvm_gl_stack_height) ? required_size : bvm_gl_stack_height;

			/* ... so create it then ..*/
			bvm_gl_rx_stack->next = bvm_thread_create_stack(real_size);

			/* make it the current stack */
			bvm_gl_rx_stack = bvm_gl_rx_stack->next;

			/* reset temp stack start pointer to reference the new stack */
			frame_cell_ptr = bvm_gl_rx_stack->body;

		} else {

			/* just use the next one */
			bvm_gl_rx_stack = bvm_gl_rx_stack->next;

			/* reset temp stack start pointer to reference the new stack */
			frame_cell_ptr = bvm_gl_rx_stack->body;
		}
	}

	/* load up the global registers.  TODO Nice if this could be reduced to a single operation - might be faster ?*/

	/* Push current locals pointer onto stack.  Ends up being at BVM_FRAME_LOCALS_OFFSET */
	(frame_cell_ptr++)->ptr_value 	= bvm_gl_rx_locals;

	/* Push current stack pointer onto the stack. Ends up being at BVM_FRAME_SP_OFFSET */
	(frame_cell_ptr++)->ptr_value 	= current_sp;

	/* Push current PC onto the stack. Ends up being at BVM_FRAME_PPC_OFFSET */
	(frame_cell_ptr++)->ptr_value 	= current_pc;

	/* Push next PC onto the stack. Ends up being at BVM_FRAME_PC_OFFSET */
	(frame_cell_ptr++)->ptr_value 	= next_pc;

	/* Push current method pointer onto the stack. Ends up being at BVM_FRAME_METHOD_OFFSET */
	(frame_cell_ptr++)->ptr_value 	= bvm_gl_rx_method;

	/* Push current stack segment pointer onto the stack. Ends up being at BVM_FRAME_STACK_OFFSET */
	(frame_cell_ptr++)->ptr_value 	= current_stack;

	/* Push sync object onto the stack.  Ends up being at BVM_FRAME_SYNCOBJ_OFFSET */
	(frame_cell_ptr++)->ptr_value 	= sync_obj;

	/* set the global registers to their new values for the being-called clazz/method */
	bvm_gl_rx_method = method;
	bvm_gl_rx_clazz  = method->clazz;

    // frame_cell_ptr is now above the 'frame return info' data. This is where locals start.
	bvm_gl_rx_locals = frame_cell_ptr;

	bvm_gl_rx_sp = bvm_gl_rx_locals + method->max_locals;
	bvm_gl_rx_pc = method->code.bytecode;
}

/**
 * Pop a frame from the current stack and store the return frame data to the global registers.
 */
void bvm_frame_pop() {

	/* NOTE: bvm_gl_rx_locals[BVM_FRAME_SYNCOBJ_OFFSET].ptr_value is used to store the sync
	 * object - there is no global register for that */

	/* TODO - nice if this could be a single memory operation - might be faster ?*/
	/*gl_sync_obj  = bvm_gl_rx_locals[BVM_FRAME_SYNCOBJ_OFFSET].ptr_value;*/
	bvm_gl_rx_stack  = bvm_gl_rx_locals[BVM_FRAME_STACK_OFFSET].ptr_value;
	bvm_gl_rx_method = bvm_gl_rx_locals[BVM_FRAME_METHOD_OFFSET].ptr_value;
	bvm_gl_rx_pc 	 = bvm_gl_rx_locals[BVM_FRAME_PC_OFFSET].ptr_value;
	bvm_gl_rx_ppc 	 = bvm_gl_rx_locals[BVM_FRAME_PPC_OFFSET].ptr_value;
	bvm_gl_rx_sp 	 = bvm_gl_rx_locals[BVM_FRAME_SP_OFFSET].ptr_value;
	bvm_gl_rx_locals = bvm_gl_rx_locals[BVM_FRAME_LOCALS_OFFSET].ptr_value;

	bvm_gl_rx_clazz = (bvm_gl_rx_method == NULL) ? NULL : bvm_gl_rx_method->clazz;
}


