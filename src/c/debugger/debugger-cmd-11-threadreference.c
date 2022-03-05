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

  JDWP ThreadReference commandset command handlers.

  @author Greg McCreath
  @since 0.0.10

*/

#if BVM_DEBUGGER_ENABLE

/**
 * JDWP Command handler for ThreadReference/Name.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_TR_Name(bvmd_instream_t* in, bvmd_outstream_t* out) {
	bvm_thread_obj_t *thread_obj;

	if ( (thread_obj = bvmd_readcheck_threadref(in, out)) == NULL) return;

	bvmd_out_writestringobjectvalue(out, thread_obj->name);
}

/**
 * JDWP Command handler for ThreadReference/Suspend.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_TR_Suspend(bvmd_instream_t* in, bvmd_outstream_t* out) {
	bvm_thread_obj_t *thread_obj;

	if ( (thread_obj = bvmd_readcheck_threadref(in, out)) == NULL) return;

	bvmd_thread_suspend(thread_obj->vmthread);
}

/**
 * JDWP Command handler for ThreadReference/Resume.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_TR_Resume(bvmd_instream_t* in, bvmd_outstream_t* out) {
	bvm_thread_obj_t *thread_obj;

	if ( (thread_obj = bvmd_readcheck_threadref(in, out)) == NULL) return;

	bvmd_thread_resume(thread_obj->vmthread);
}

/**
 * Logic to determine the JDWP status of a given thread.
 *
 * Note: this finds what the status of a thread is ignoring the debugger suspend status.  This is not explained
 * very well in the JDWP specs.  Here, the debugger is asking what the thread status would be if
 * the debugger was <i>not</i> interfering.  That is, even though the debugger may have suspended a
 * thread, if it was RUNNABLE before being suspended by the debugger, then the debugger wants to know
 * exactly that.
 *
 * @param vmthread a given thread
 *
 * @return the JDWP thread status
 */
static bvm_int32_t dbg_thread_jdwp_status(bvm_vmthread_t *vmthread) {

	bvm_int32_t thstatus;
	bvm_int32_t dbg_status;

	/* easy, if the thread is dead, debugger thinks 'zombie' */
	if (!bvm_thread_is_alive(vmthread)) return JDWP_ThreadStatus_ZOMBIE;

	thstatus = vmthread->status;

	if (thstatus == BVM_THREAD_STATUS_RUNNABLE) {
		dbg_status = JDWP_ThreadStatus_RUNNING;
	} else {

		/* remove the DBG suspend modifier if is there to help see other status more clearly */
		thstatus &= ~BVM_THREAD_STATUS_DBG_SUSPENDED;

		if (thstatus == BVM_THREAD_STATUS_BLOCKED) {

			/* if a thread is 'vanilla' suspended and is not actually waiting on an object, it must have been
			 * suspended by the debugger when it was happily running.  */
			dbg_status = (vmthread->waiting_on_object == NULL) ? JDWP_ThreadStatus_RUNNING : JDWP_ThreadStatus_MONITOR;

		} else {

			/* so, not runnable, not vanilla blocked - must be sleeping or waiting */

			if (thstatus & BVM_THREAD_STATUS_WAITING) {
				dbg_status = JDWP_ThreadStatus_WAIT;
			} else {
				/* could be sleeping, or could be waiting with
				 * wait(x) or join(x). */
				dbg_status = JDWP_ThreadStatus_SLEEPING;
			}
		}
	}

	return dbg_status;
}

/**
 * JDWP Command handler for ThreadReference/Status.
 *
 * @param in the request input stream
 * @param out the response output stream
 *
 * @see #dbg_thread_jdwp_status()
 */
void bvmd_ch_TR_Status(bvmd_instream_t* in, bvmd_outstream_t* out) {

	bvm_thread_obj_t *thread_obj;
	bvm_vmthread_t *vmthread;
	bvm_int32_t thstatus;

	if ( (thread_obj = bvmd_readcheck_threadref(in, out)) == NULL) return;

	thstatus = thread_obj->vmthread->status;
	vmthread = thread_obj->vmthread;

	if (bvm_thread_is_alive(vmthread)) {

		bvm_int32_t suspend_status = 0;

		/* if thread is suspended ... */
		if (thstatus != BVM_THREAD_STATUS_RUNNABLE)
			suspend_status = JDWP_SuspendStatus_SUSPEND_STATUS_SUSPENDED;

		bvmd_out_writeint32(out, dbg_thread_jdwp_status(vmthread));
		bvmd_out_writeint32(out, suspend_status);
	} else {
		bvmd_out_writeint32(out, JDWP_ThreadStatus_ZOMBIE);
		bvmd_out_writeint32(out, 0);
	}
}

/**
 * JDWP Command handler for ThreadReference/ThreadGroup.
 *
 * Note: all threads in Babe belong to the magical hard-coded #BVMD_THREADGROUP_MAIN_ID thread group.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_TR_ThreadGroup(bvmd_instream_t* in, bvmd_outstream_t* out) {

	if ( bvmd_readcheck_threadref(in, out) == NULL) return;

	/* all threads are in the 'main' threadgroup. */
	bvmd_out_writeint32(out, BVMD_THREADGROUP_MAIN_ID);
}

/**
 * A stack visit callback to output a frame to an output stream.
 *
 * @param stack_info info about a stack frame
 * @param data the response output stream.
 *
 * @return always BVM_TRUE
 */
static bvm_bool_t frames_callback(bvm_stack_frame_info_t *stack_info, void *data) {

	bvmd_outstream_t *out = data;

	bvmd_location_t location;

	/* address of the frame locals if the frameID */
	bvmd_out_writeaddress(out, stack_info->locals);

	location.clazz = (bvm_clazz_t *) stack_info->method->clazz;
	location.method = stack_info->method;

	/* the pc index is the different between the current PC and the start of the method's bytecode */
	location.pc_index = stack_info->pc - stack_info->method->code.bytecode;

	bvmd_out_writelocation(out, &location);

	return BVM_TRUE;
}

/**
 * JDWP Command handler for ThreadReference/Frames.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_TR_Frames(bvmd_instream_t* in, bvmd_outstream_t* out) {

	bvm_thread_obj_t *thread_obj;
	bvmd_streampos_t count_pos;
    int frames_nr;

    bvm_int32_t startframe;	/* The index of the first frame to retrieve. */
    bvm_int32_t length;	/* The count of frames to retrieve (-1 means all remaining). */

	if ( (thread_obj = bvmd_readcheck_threadref(in, out)) == NULL) return;

	/* JDWP spec says thread must be suspended, but does not actually say an error should be
	 * returned - we will. */
	if (thread_obj->vmthread->status == BVM_THREAD_STATUS_RUNNABLE) {
		out->error = JDWP_Error_THREAD_NOT_SUSPENDED;
		return;
	}

	/* write out a zero counter and remember the place in the stream where it was written */
	count_pos = bvmd_out_writeint32(out, 0);

	startframe = bvmd_in_readint32(in);
	length = bvmd_in_readint32(in);

	/* visit the stack and use a callback to write out the stack frame info as each frame is visited -
	 * note that we are only interested in the 'length' frames starting at 'startframe'. */
	bvm_stack_visit(thread_obj->vmthread, startframe, length, &frames_nr, frames_callback, out);

	/* rewrite the count if there is one */
    bvmd_out_rewriteint32(&count_pos, (bvm_int32_t) frames_nr);
}

/**
 * JDWP Command handler for ThreadReference/OwnedMonitors.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_TR_OwnedMonitors(bvmd_instream_t* in, bvmd_outstream_t* out) {

	bvmd_streampos_t count_pos;
	bvm_thread_obj_t *thread_obj;
	bvm_monitor_t *monitor;
    bvm_int32_t counter = 0;

	if ( (thread_obj = bvmd_readcheck_threadref(in, out)) == NULL) return;

	/* JDWP spec says thread must be suspended, but does not actually say an error should be
	 * returned - we will. */
	if (thread_obj->vmthread->status == BVM_THREAD_STATUS_RUNNABLE) {
		out->error = JDWP_Error_THREAD_NOT_SUSPENDED;
		return;
	}

	/* assume a count of zero - that we'll rewrite later if it is different */
	count_pos = bvmd_out_writeint32(out, 0);

	/* get the head of the thread monitor list */
	monitor = bvm_gl_thread_monitor_list;

	/* for each monitor, check if the owned thread is the given thread.  */
	while (monitor != NULL) {

		if ( (monitor->in_use) && (monitor->owner_thread == thread_obj->vmthread)) {
			counter++;
			bvmd_out_writebyte(out, bvmd_get_jdwptag_from_object(monitor->owner_object));
			bvmd_out_writeobject(out, monitor->owner_object);
		}

		monitor = monitor->next;
	}

	/* rewrite count if different */
	if (counter) bvmd_out_rewriteint32(&count_pos, counter);
}


/**
 * JDWP Command handler for ThreadReference/CurrentContendedMonitor.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_TR_CurrentContendedMonitor(bvmd_instream_t* in, bvmd_outstream_t* out) {

	bvm_thread_obj_t *thread_obj;

	if ( (thread_obj = bvmd_readcheck_threadref(in, out)) == NULL) return;

	/* JDWP spec says thread must be suspended, but does not actually say an error should be
	 * returned - we will. */
	if (thread_obj->vmthread->status == BVM_THREAD_STATUS_RUNNABLE) {
		out->error = JDWP_Error_THREAD_NOT_SUSPENDED;
		return;
	}

	if (thread_obj->vmthread->waiting_on_object != NULL) {

		/* easy, we know the object it is 'wait'ing on .. if a thread is waiting on an object the
		 * VM impl will always set the 'waiting_on_object' value - not so for a thread waiting on a
		 * sync lock. */
		bvmd_out_writebyte(out, bvmd_get_jdwptag_from_object(thread_obj->vmthread->waiting_on_object));
		bvmd_out_writeobject(out, thread_obj->vmthread->waiting_on_object);
		return;

	} else {

		/* traverse the lock queues of all in-use monitors looking for this thread */
		bvm_monitor_t *monitor = bvm_gl_thread_monitor_list;
		bvm_vmthread_t *locked_thread;

		while (monitor != NULL) {

			if ( (monitor->in_use) && ( (locked_thread = monitor->lock_queue) != NULL) ) {
				do {

					/* if we've found it, output the monitor's owned object */
					if (thread_obj->vmthread == locked_thread) {
						bvmd_out_writebyte(out, bvmd_get_jdwptag_from_object(monitor->owner_object));
						bvmd_out_writeobject(out, monitor->owner_object);
						return;
					}
				} while ( (locked_thread = locked_thread->next_in_queue) != NULL );
			}

			monitor = monitor->next;
		}
	}

	/* none */
	bvmd_out_writebyte(out, JDWP_Tag_OBJECT);
	bvmd_out_writeobject(out, NULL);
}

/**
 * JDWP Command handler for ThreadReference/FrameCount.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_TR_FrameCount(bvmd_instream_t* in, bvmd_outstream_t* out) {

	bvm_thread_obj_t *thread_obj;

	if ( (thread_obj = bvmd_readcheck_threadref(in, out)) == NULL) return;

	/* easy, we know the object it is 'wait'ing on .. if a thread is waiting on an object the
	 * VM impl will always set the 'waiting_on_object' value - not so for a thread waiting on a
	 * sync lock. */
	if (thread_obj->vmthread->status == BVM_THREAD_STATUS_RUNNABLE) {
		out->error = JDWP_Error_THREAD_NOT_SUSPENDED;
		return;
	}

	bvmd_out_writeint32(out, bvm_stack_get_depth(thread_obj->vmthread));
}

/**
 * JDWP Command handler for ThreadReference/Interrupt.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_TR_Interrupt(bvmd_instream_t* in, bvmd_outstream_t* out) {
	bvm_thread_obj_t *thread_obj;

	if ( (thread_obj = bvmd_readcheck_threadref(in, out)) == NULL) return;

	bvm_thread_interrupt(thread_obj->vmthread);
}

/**
 * JDWP Command handler for ThreadReference/SuspendCount.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_TR_SuspendCount(bvmd_instream_t* in, bvmd_outstream_t* out) {
	bvm_thread_obj_t *thread_obj;

	if ( (thread_obj = bvmd_readcheck_threadref(in, out)) == NULL) return;

	bvmd_out_writeint32(out, thread_obj->vmthread->dbg_suspend_count);
}

#endif
