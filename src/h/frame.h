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

#ifndef BVM_FRAME_H_
#define BVM_FRAME_H_

/**
  @file

  Constants/Macros/Functions/Types for stack frames.

  @author Greg McCreath
  @since 0.0.10

*/

/**
 * A thread stack segment.  Threads have a list of one or more these.
 */
typedef struct _bvmstacksegmentstruct {

	/** Height of this stack segment in cells (not bytes). */
	size_t height;

	/** Pointer to the #bvm_cell_t *past* the top of the stack body (as represented by the #body field of
	 * this structure). */
	bvm_cell_t *top;

	/** Pointer to the next #bvm_stacksegment_t in the list. If \c NULL, this is the last segment in the
	 * list.  */
	struct _bvmstacksegmentstruct *next;

	/** Pointer to the first #bvm_cell_t of the stack.  The actual length of each segment is determined
	 * at runtime. The default stack size (in cells, not in bytes) is given in the global #bvm_gl_stack_height,
	 * which is defaulted from the compile-time define #BVM_THREAD_STACK_HEIGHT. */
	bvm_cell_t body[1];

} bvm_stacksegment_t;

/* externs for global registers */
extern bvm_cell_t 			*bvm_gl_rx_sp;
extern bvm_uint8_t   		*bvm_gl_rx_pc;
extern bvm_uint8_t   		*bvm_gl_rx_ppc;
extern bvm_instance_clazz_t *bvm_gl_rx_clazz;
extern bvm_method_t 		*bvm_gl_rx_method;
extern bvm_cell_t 			*bvm_gl_rx_locals;
extern bvm_stacksegment_t 	*bvm_gl_rx_stack;
/*extern bvm_obj_t  		*bvm_gl_sync_obj;*/

/*extern bvm_uint32_t bvm_gl_stack_depth;*/

extern bvm_method_t *BVM_METHOD_NOOP;
extern bvm_method_t *BVM_METHOD_NOOP_RET;
extern bvm_method_t *BVM_METHOD_CALLBACKWEDGE;

#define BVM_FRAME_SYNCOBJ_OFFSET  	-1
#define BVM_FRAME_STACK_OFFSET  	-2
#define BVM_FRAME_METHOD_OFFSET 	-3
#define BVM_FRAME_PC_OFFSET     	-4
#define BVM_FRAME_PPC_OFFSET    	-5
#define BVM_FRAME_SP_OFFSET 		-6
#define BVM_FRAME_LOCALS_OFFSET 	-7

#define BVM_STACK_RETURN_FRAME_SIZE  7

#define BVM_STACK_GetCallingClass()       ( (bvm_instance_clazz_t *) ((bvm_method_t *) bvm_gl_rx_locals[BVM_FRAME_METHOD_OFFSET].ptr_value)->clazz)
#define BVM_STACK_ExecMethodSyncObject()  (bvm_gl_rx_locals[BVM_FRAME_SYNCOBJ_OFFSET].ref_value)

void bvm_frame_pop();
void bvm_frame_push(bvm_method_t *method, bvm_cell_t *returnsp, bvm_uint8_t *ppc, bvm_uint8_t *returnpc, bvm_obj_t *sync_obj);

#endif /*BVM_FRAME_H_*/
