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

  JDWP ReferenceType commandset command handlers.

  @author Greg McCreath
  @since 0.0.10

*/

#if BVM_DEBUGGER_ENABLE

/**
 * JDWP Command handler for ObjectReference/ReferenceType.
 *
 * Note: if the object ID asked for is actually any of the hard-coded ThreadGroup objects IDs
 * (#BVMD_THREADGROUP_SYSTEM_ID, #BVMD_THREADGROUP_MAIN_ID) - the reference type is the Object
 * clazz.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_OR_ReferenceType(bvmd_instream_t* in, bvmd_outstream_t* out) {

	bvm_uint8_t reftype;
	bvm_clazz_t *clazz;
	bvm_int32_t id = bvmd_in_readint32(in);

	/* special hard-coded case for the threadgroups - we fool a debugger into thinking there are some. */
	if ( (id == BVMD_THREADGROUP_SYSTEM_ID) || (id == BVMD_THREADGROUP_MAIN_ID) ) {
		reftype = JDWP_TagType_CLASS;
		clazz = (bvm_clazz_t *) BVM_OBJECT_CLAZZ;
	} else {

		bvm_obj_t *obj = bvmd_id_get_by_id(id);

		if (obj == NULL) {
			out->error = JDWP_Error_INVALID_OBJECT;
			return;
		}

		clazz = obj->clazz;
		reftype = bvmd_clazz_reftype(clazz);
	}

	bvmd_out_writebyte(out, reftype);
	bvmd_out_writeobject(out, clazz);
}

/**
 * Internal command handler logic for ObjectReference/GetValues and ObjectReference/SetValues - both are
 * largely the same.
 *
 * Note:  'Native' fields (those that hold a pointer on behalf of the VM) are output as INT.  When attempting to set the
 * value of a native field, the set operation is ignored.
 *
 * @param in the request input stream
 * @param out the response output stream
 * @param do_set if BVM_TRUE, the function will set values with an object, if BVM_FALSE function will output values
 * to the output stream.
 */
void dbg_ObjectReference_GetSetValues(bvmd_instream_t* in, bvmd_outstream_t* out, bvm_bool_t do_set) {

	bvm_obj_t *obj = bvmd_in_readobject(in);
	bvm_uint32_t fields_nr, i;
	bvm_uint8_t tagtype;
	bvm_cell_t *value_cell;

	if (obj == NULL) {
		out->error = JDWP_Error_INVALID_OBJECT;
		return;
	}

	/* the number of field values requested ... */
	fields_nr = bvmd_in_readint32(in);

	/* ... number of field values go out is the same value */
	if (!do_set)
		bvmd_out_writeint32(out, fields_nr);

	/* for each field requested */
	for (i = 0; i < fields_nr; i++) {

		/* get the field from the (instance) clazz */
		bvm_field_t *field = bvmd_in_readaddress(in);

		/* just in case, can't imagine it happening though ... */
		if (field == NULL) {
			out->error = JDWP_Error_INVALID_FIELDID;
			return;
		}

		/* get a pointer to the bvm_cell_t that holds the field's value */
		value_cell = &(obj->fields[field->value.offset]);

		/* if native, treat value as an INT */
		if (BVM_FIELD_IsNative(field)) {
			tagtype = JDWP_Tag_INT;
		} else {
			/* otherwise test for primitive and array ... */
			tagtype = bvmd_get_jdwptag_from_char(field->jni_signature->data[0]);
		}

		/* lastly if not primitive or array (or native), get field tag type from the cell ref value */
		if (!tagtype) tagtype = bvmd_get_jdwptag_from_object(value_cell->ref_value);

		if (do_set) {
			/* only modify field if it is not native */
			if (!BVM_FIELD_IsNative(field))
				bvmd_in_readcell(in, tagtype, value_cell);
			else
				out->error = JDWP_Error_INVALID_FIELDID;
		} else {
			bvmd_out_writecell(out, tagtype, value_cell);
		}
	}
}

/**
 * JDWP Command handler for ObjectReference/GetValues.
 *
 * @param in the request input stream
 * @param out the response output stream
 *
 * @see #dbg_ObjectReference_GetSetValues()
 */
void bvmd_ch_OR_GetValues(bvmd_instream_t* in, bvmd_outstream_t* out) {
	dbg_ObjectReference_GetSetValues(in, out, BVM_FALSE);
}

/**
 * JDWP Command handler for ObjectReference/SetValues.
 *
 * @param in the request input stream
 * @param out the response output stream
 *
 * @see #dbg_ObjectReference_GetSetValues()
 */
void bvmd_ch_OR_SetValues(bvmd_instream_t* in, bvmd_outstream_t* out) {
	dbg_ObjectReference_GetSetValues(in, out, BVM_TRUE);
}

/**
 * JDWP Command handler for ObjectReference/MonitorInfo.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_OR_MonitorInfo(bvmd_instream_t* in, bvmd_outstream_t* out) {

	bvm_monitor_t *monitor;
	bvmd_streampos_t count_pos;
	bvm_obj_t *obj = bvmd_in_readobject(in);

	if (obj == NULL) {
		out->error = JDWP_Error_INVALID_OBJECT;
		return;
	}

	/* JDWP spec says all threads must be suspended, but does not say which error to return.  This will
	 * have to do. */
	if (!bvmd_is_vm_suspended()) {
		out->error = JDWP_Error_THREAD_NOT_SUSPENDED;
		return;
	}

	monitor = get_monitor_for_obj(obj);

	if (monitor != NULL) {

		/* we have a monitor, output its details and then examine the wait and lock queues
		 * for threads. */
		bvm_vmthread_t *vmthread = monitor->owner_thread;
		bvmd_out_writeobject(out, (vmthread != NULL)? vmthread->thread_obj : NULL);
		bvmd_out_writeint32(out, monitor->lock_depth);
		count_pos = bvmd_out_writeint32(out, 0);

		if (vmthread != NULL) {

            bvm_int32_t counter = 0;
			bvm_vmthread_t *waiting = monitor->wait_queue;

			/* traverse the monitor's wait queue for waiting threads */
			while (waiting != NULL) {
				counter++;
				bvmd_out_writeobject(out, waiting->thread_obj);
				waiting = waiting->next_in_queue;
			}

			/* traverse the monitor's lock queue for threads waiting on a sync */
			waiting = monitor->lock_queue;
			while (waiting != NULL) {
				counter++;
				bvmd_out_writeobject(out, waiting->thread_obj);
				waiting = waiting->next_in_queue;
			}

			/* rewrite the thread count if there are any */
			if (counter) bvmd_out_rewriteint32(&count_pos, counter);
		}
	} else {
		/* no monitor, easy */
		bvmd_out_writeobject(out, NULL);
		bvmd_out_writeint32(out, 0);
		bvmd_out_writeint32(out, 0);
	}
}

#if 0

typedef struct _invokemethodcallbackdata {
	int request_id;
	bvm_uint8_t return_value_tag;
	int suspend_opts;
	bvm_vmthread_t *vmthread;
	bvm_bool_t is_at_breakpoint;
	bvm_uint8_t breakpoint_opcode;
} dbg_invokemethod_callback_data_t;

bvm_uint8_t get_method_returntype(bvm_method_t *method) {

	char *ch = strchr( (char *) method->desc->data, ')') + 1;

	return (*ch == 'L') ? JDWP_Tag_OBJECT : bvmd_get_jdwptag_from_char(*ch);
}

bvm_obj_t *dbg_invokemethod_callback(bvm_cell_t *res1, bvm_cell_t *res2, bvm_bool_t is_exception, void *data) {

	bvm_cell_t values[2];
	dbg_invokemethod_callback_data_t *callback_data = data;

	bvmd_outstream_t* out = bvmd_new_packetstream();

	/* the id of the reply is the id of the request command */
	out->packet->type.reply.id = callback_data->request_id;

	/* the reply flags are set to show it as a 'reply' */
	out->packet->type.reply.flags = BVMD_TRANSPORT_FLAGS_REPLY;

	if (!is_exception) {

		bvm_uint8_t tag = callback_data->return_value_tag;

		values[0] = *res1;
		values[1] = *res2;

		bvmd_out_writecell(out, tag, values);
		bvmd_out_writebyte(out, JDWP_Tag_OBJECT);
		bvmd_out_writeobject(out, NULL);
	} else {
		bvmd_out_writebyte(out, JDWP_Tag_OBJECT);
		bvmd_out_writeobject(out, NULL);
		bvmd_out_writebyte(out, JDWP_Tag_OBJECT);
		bvmd_out_writeobject(out, res1.ref_value);
	}

	bvmd_sendstream(out);

	bvm_heap_free(callback_data);

	callback_data->vmthread->dbg_is_at_breakpoint = callback_data->is_at_breakpoint;
	callback_data->vmthread->dbg_breakpoint_opcode = callback_data->breakpoint_opcode;

	if (callback_data->suspend_opts & JDWP_InvokeOptions_INVOKE_SINGLE_THREADED)
		bvmd_thread_suspend(callback_data->vmthread);
	else
		bvmd_thread_suspend_all();

	return NULL;
}

void dbg_cmdhandler_InvokeMethod(bvmd_instream_t* in, bvmd_outstream_t* out, bvm_bool_t is_static) {

	bvm_obj_t *this_obj = NULL;
	bvm_thread_obj_t *thread_obj;
	bvm_method_t *method;
	bvm_instance_clazz_t *clazz;
	dbg_invokemethod_callback_data_t *callback_data;
	bvm_vmthread_t *vmthread;

	int args_nr;
	int args_posn = 0;
	int opts;

	if (!is_static) {

		/* the 'this' for the invocation */
		this_obj = bvmd_in_readobject(in);

		if (this_obj == NULL) {
			out->error = JDWP_Error_INVALID_OBJECT;
			return;
		}
	}

	/* the thread Object */
	if ( (thread_obj = bvmd_readcheck_threadref(in, out)) == NULL) return;
	vmthread = thread_obj->vmthread;

	if (! (vmthread->status & BVM_THREAD_STATUS_DBG_SUSPENDED) ) {
		out->error = JDWP_Error_THREAD_NOT_SUSPENDED;
		return;
	}

	/* the class ID */
	if ( (clazz = (bvm_instance_clazz_t *) bvmd_readcheck_reftype(in, out)) == NULL) return;

	/* the method */
	method = bvmd_in_readaddress(in);

	/* the number of arguments */
	args_nr = bvmd_in_readint32(in);

	/* the options */
	opts = bvmd_in_readint32(in);

	/* push the method onto the thread's stack.  First we make sure the current thread's registers are
	 * saved, then swap them to the given thread's.  We push the method - with a callback.
	 * Then for each given argument, push that onto the locals area of the frame.
	 */

	bvm_thread_store_registers(bvm_gl_thread_current);
	bvm_thread_load_registers(thread_obj->vmthread);

	/* note: callbackdata memory is freed in dbg_invokemethod_callback */
	callback_data = bvm_heap_alloc(sizeof(callback_data), BVM_ALLOC_TYPE_STATIC);
	callback_data->request_id = in->packet->type.cmd.id;
	callback_data->return_value_tag = get_method_returntype(method);
	callback_data->suspend_opts = opts;
	callback_data->vmthread = thread_obj->vmthread;

	if (thread_obj->vmthread->dbg_is_at_breakpoint) {
		callback_data->is_at_breakpoint = thread_obj->vmthread->dbg_is_at_breakpoint;
		callback_data->breakpoint_opcode = thread_obj->vmthread->dbg_breakpoint_opcode;
		thread_obj->vmthread->dbg_is_at_breakpoint = BVM_FALSE;
	}

	bvm_frame_push(BVM_METHOD_CALLBACKWEDGE, bvm_gl_rx_sp, bvm_gl_rx_pc, bvm_gl_rx_pc, NULL);
	bvm_gl_rx_locals[0].ref_value = NULL;
	bvm_gl_rx_locals[1].callback = dbg_invokemethod_callback;
	bvm_gl_rx_locals[2].ptr_value = callback_data;

	bvm_frame_push(method, bvm_gl_rx_sp, bvm_gl_rx_pc, bvm_gl_rx_pc, NULL);
	if (!is_static)
		bvm_gl_rx_locals[args_posn++].ref_value = this_obj;

	while (args_nr--) {
		bvm_gl_rx_locals[args_posn++].ref_value = NULL;
	}

	/* next, if the class needs initialising, request it to do so.  That may push yet
	 * another frame onto the stack for <clinit> Note that we are only interested in
	 * initialising instance clazzes.  Primitive and array clazzes are initialized when
	 * they are created.
	 */
	if ( (!BVM_CLAZZ_IsInitialised(clazz)) && (BVM_CLAZZ_IsInstanceClazz(clazz)) )
		bvm_clazz_initialise( (bvm_instance_clazz_t *) clazz);

	bvm_thread_store_registers(thread_obj->vmthread);
	bvm_thread_load_registers(bvm_gl_thread_current);

	if (opts & JDWP_InvokeOptions_INVOKE_SINGLE_THREADED)
		bvmd_thread_resume(thread_obj->vmthread);
	else
		bvmd_thread_resume_all(BVM_FALSE, BVM_FALSE);

	out->ignore_send = BVM_TRUE;
}

#endif

/**
 * JDWP Command handler for ObjectReference/InvokeMethod.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_OR_InvokeMethod(bvmd_instream_t* in, bvmd_outstream_t* out) {
/*	dbg_cmdhandler_InvokeMethod(in, out, BVM_FALSE);*/
/*	bvmd_cmdhandler_default(in, out);*/
#if 1
	bvm_method_t *method;

	bvmd_in_readobject(in);			    /* the object ID */
	bvmd_in_readobject(in);			    /* the thread ID */
	bvmd_in_readobject(in);			    /* the class ID */
	method = bvmd_in_readaddress(in);	/* the method ID */
	bvmd_in_readint32(in);				/* the number of arguments */
	bvmd_in_readint32(in);				/* the options */

	/* the debugger seems to call 'toString()' a lot.  Rather than feeding back
	 * errors all the time, we'll just give it a dummy string.*/
	if (strcmp( (char *) method->name->data, "toString") == 0) {
		bvmd_out_writebyte(out, JDWP_Tag_STRING);
		bvmd_out_writeobject(out, bvmd_nosupp_tostring_obj);
		bvmd_out_writebyte(out, JDWP_Tag_OBJECT);
		bvmd_out_writeobject(out, NULL);
	} else {
		bvmd_cmdhandler_default(in, out);
	}
#endif
}

/**
 * JDWP Command handler for ObjectReference/DisableCollection.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_OR_DisableCollection(bvmd_instream_t* in, bvmd_outstream_t* out) {
	bvm_obj_t *obj = bvmd_in_readobject(in);

	if (obj == NULL) {
		out->error = JDWP_Error_INVALID_OBJECT;
	} else {
		bvmd_root_put(obj);
	}
}

/**
 * JDWP Command handler for ObjectReference/EnableCollection.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_OR_EnableCollection(bvmd_instream_t* in, bvmd_outstream_t* out) {
	bvm_obj_t *obj = bvmd_in_readobject(in);

	if (obj != NULL) bvmd_root_remove(obj);
}

/**
 * JDWP Command handler for ObjectReference/IsCollected.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_OR_IsCollected(bvmd_instream_t* in, bvmd_outstream_t* out) {
	bvmd_out_writeboolean(out, (bvmd_in_readobject(in) == NULL) ? BVM_TRUE : BVM_FALSE);
}

#endif
