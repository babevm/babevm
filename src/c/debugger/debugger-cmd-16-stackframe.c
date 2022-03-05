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

#include "../../h/bvm.h"

/**
  @file

  JDWP StackFrame commandset command handlers.

  @author Greg McCreath
  @since 0.0.10

*/

#if BVM_DEBUGGER_ENABLE

/**
 * Internal command handler logic for StackFrame/GetValues and StackFrame/SetValues - they are both basically
 * the same.
 *
 * @param in the request input stream
 * @param out the response output stream
 * @param do_set if BVM_TRUE values will be read from the input stream and set into the frame, if
 * BVM_FALSE, the values will be read from a stack frame and written to the output stream.
 */
static void dbg_frame_getset_values(bvmd_instream_t* in, bvmd_outstream_t* out, bvm_bool_t do_set) {

	bvm_thread_obj_t *thread_obj;
	bvm_cell_t *frame_locals;
	int slots;

	if ( (thread_obj = bvmd_readcheck_threadref(in, out)) == NULL) return;

	frame_locals = bvmd_in_readaddress(in);
	slots = bvmd_in_readint32(in);

	if (!do_set)
		bvmd_out_writeint32(out, slots);

	while (slots--) {
		int slot = bvmd_in_readint32(in);
		bvm_uint8_t tag = bvmd_in_readbyte(in);

		if (do_set) {
			bvmd_in_readcell(in, tag, (frame_locals + slot) );
		}
		else {
			bvmd_out_writecell(out, tag, (frame_locals + slot) );
		}
	}
}

/**
 * JDWP Command handler for StackFrame/GetValues.
 *
 * @param in the request input stream
 * @param out the response output stream
 *
 * @see #dbg_frame_getset_values()
 */
void bvmd_ch_SF_GetValues(bvmd_instream_t* in, bvmd_outstream_t* out) {
	dbg_frame_getset_values(in, out, BVM_FALSE);
}

/**
 * JDWP Command handler for StackFrame/SetValues.
 *
 * @param in the request input stream
 * @param out the response output stream
 *
 * @see #dbg_frame_getset_values()
 */
void bvmd_ch_SF_SetValues(bvmd_instream_t* in, bvmd_outstream_t* out) {
	dbg_frame_getset_values(in, out, BVM_TRUE);
}

/**
 * A callback struct for help in finding a given 'this' object in a thread stack.
 */
typedef struct _thisobjcallbackstruct {

	/** The response output stream */
	bvmd_outstream_t *out;

	/** a pointer to a "frame ID" (which is really a pointer to the frame locals */
	bvm_cell_t *locals;

	/** Report if the given frame id was found in the stack. */
	bvm_bool_t found_frame;

} thisobj_data_t;

/**
 * Stack visitation callback to locate a given 'this' object in the stack.
 *
 * @param stack_info stack frame callback info
 * @param data a #thisobj_data_t struct of callback data.
 */
static bvm_bool_t thisobject_callback(bvm_stack_frame_info_t *stack_info, void *data) {

	thisobj_data_t *objdata = data;
	bvmd_outstream_t *out = objdata->out;

	if (objdata->locals == stack_info->locals) {

		if (BVM_METHOD_IsStatic(stack_info->method) || BVM_METHOD_IsNative(stack_info->method)) {
			bvmd_out_writebyte(out, JDWP_Tag_OBJECT);
			bvmd_out_writeobject(out, NULL);
		} else {
			bvm_obj_t *obj = stack_info->locals[0].ref_value;
			bvmd_out_writebyte(out, bvmd_get_jdwptag_from_object(obj));
			bvmd_out_writeobject(out, obj);
		}

		objdata->found_frame = BVM_TRUE;

		return BVM_FALSE;
	}

	return BVM_TRUE;
}

/**
 * JDWP Command handler for StackFrame/ThisObject.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_SF_ThisObject(bvmd_instream_t* in, bvmd_outstream_t* out) {

	bvm_thread_obj_t *thread_obj;
	int unused;
	thisobj_data_t objdata;

	if ( (thread_obj = bvmd_readcheck_threadref(in, out)) == NULL) return;

	bvm_thread_store_registers(bvm_gl_thread_current);

	objdata.locals = bvmd_in_readaddress(in);
	objdata.out = out;
	objdata.found_frame = BVM_FALSE;

	/* visit stack, looking for frame with locals pointer that matches the requested frame locals pointer */
	bvm_stack_visit(thread_obj->vmthread, 0, -1, &unused, thisobject_callback, &objdata);

	/* if we didn't find one ... must have a bad frame ID ... */
	if (!objdata.found_frame) {
		out->error = JDWP_Error_INVALID_FRAMEID;
	}
}

#endif
