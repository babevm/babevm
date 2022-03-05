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

  JDWP Event commandset functions.

  @author Greg McCreath
  @since 0.0.10

*/

#if BVM_DEBUGGER_ENABLE

/**
 * Returns the current step position inside of a given method.  The position is dependent on the JDWP
 * step size. A step size of BVM_MIN measure the position by bytecode offset into the method.  A step size
 * of LINE measure it by the current executing line number - which is uses the executing bytecode offset
 * to calculate.
 *
 * Note that if the setp size is LINE, and the given method has no line numbers (not compiled in),
 * the line number will always be zero.
 *
 * @param step_size the JDWP step size
 * @param method the method to location the position within
 * @param pc a pointer to the current executing bytecode within the method's bytecodes.
 *
 * @return the position as described above
 */
bvm_native_ulong_t bvmd_step_get_position(int step_size, bvm_method_t *method, bvm_uint8_t *pc) {

    bvm_native_ulong_t position = 0;

	 if (step_size == JDWP_StepSize_MIN) {
	     // cast rx_pc into an integer value
		 position = *(bvm_native_ulong_t *) bvm_gl_rx_pc;
	 }
	 else {
		 if (method->line_numbers != NULL) position = bvm_clazz_get_source_line(method, pc);
	 }

	return position;
}

/**
 * Creates and returns a new event context for a given eventkind and thread.
 *
 * @param eventkind a given JDWP event kind
 * @param vmthread a given thread
 *
 * @return a new bvmd_eventcontext_t allocated from the heap.
 */
bvmd_eventcontext_t *bvmd_eventdef_new_context(bvm_uint8_t eventkind, bvm_vmthread_t *vmthread) {

	bvmd_eventcontext_t *result = bvm_heap_calloc(sizeof(bvmd_eventcontext_t), BVM_ALLOC_TYPE_STATIC);
	result->eventkind = eventkind;
	result->vmthread = vmthread;

	return result;
}

/**
 * Free an event context and returns its memory back to the heap.
 */
void bvmd_eventdef_free_context(bvmd_eventcontext_t *context) {
	bvm_heap_free(context);
}

/**
 * Sends an event associated with a given event context.  Used for processing events that were created
 * while a thread was suspended.
 *
 * @param context the event context.
 */
static void dbg_send_event(bvmd_eventcontext_t *context) {

	switch(context->eventkind) {

	/*
		case (JDWP_EventKind_SINGLE_STEP):
			break;
		case (JDWP_EventKind_BREAKPOINT):
			break;
		case (JDWP_EventKind_FRAME_POP):
			break;
		case (JDWP_EventKind_EXCEPTION):
			break;
		case (JDWP_EventKind_USER_DEFINED):
			break;
			*/
		case (JDWP_EventKind_THREAD_START):
			bvmd_event_Thread(context);
			break;
		case (JDWP_EventKind_THREAD_DEATH):
			bvmd_event_Thread(context);
			break;
		case (JDWP_EventKind_CLASS_PREPARE):
			bvmd_event_ClassPrepare(context);
			break;
			/*
		case (JDWP_EventKind_CLASS_UNLOAD):
			break;
		case (JDWP_EventKind_CLASS_LOAD):
			break;
		case (JDWP_EventKind_FIELD_ACCESS):
			break;
		case (JDWP_EventKind_FIELD_MODIFICATION):
			break;
		case (JDWP_EventKind_EXCEPTION_CATCH):
			break;
		case (JDWP_EventKind_METHOD_ENTRY):
			break;
		case (JDWP_EventKind_METHOD_EXIT):
			break;
		case (JDWP_EventKind_METHOD_EXIT_WITH_RETURN_VALUE):
			break;
		case (JDWP_EventKind_MONITOR_CONTENDED_ENTER):
			break;
		case (JDWP_EventKind_MONITOR_CONTENDED_ENTERED):
			break;
		case (JDWP_EventKind_MONITOR_WAIT):
			break;
		case (JDWP_EventKind_MONITOR_WAITED):
			break;
		case (JDWP_EventKind_VM_START):
			break;
		case (JDWP_EventKind_VM_DEATH):
			break;
		case (JDWP_EventKind_VM_DISCONNECTED):
			break;*/
	}
}

/**
 * Processes an event context.  It the context's thread is suspended in any way the context will be
 * parked on the thread at the end of its 'parked events' list.  If the thread is running the event
 * will be handled immediately.
 *
 * @param context a given event context.
 */
void bvmd_do_event(bvmd_eventcontext_t *context) {

	bvm_vmthread_t *vmthread = context->vmthread;

	/* if the thread is not suspended, just send the event, otherwise, park the event on the thread
	 * so that when it next wakes up, it will be sent then. */
	if (vmthread->status == BVM_THREAD_STATUS_RUNNABLE) {
		dbg_send_event(context);
		bvmd_eventdef_free_context(context);
	} else {

		if (vmthread->dbg_parked_events == NULL) {

			/* head of list */
			vmthread->dbg_parked_events = context;

		} else {

			bvmd_eventcontext_t *list = vmthread->dbg_parked_events;

			/* put at end of list */
			while (list->next != NULL) list=list->next;

			list->next = context;
		}
	}
}

/**
 * Attempt to send all the parked events for a given thread.  The events will only be sent if the thread is
 * not suspended in any way.  After each event sent the thread's status is re-checked - if it has become
 * suspended as a result of sending an event no further parked events will be sent and the
 * function will return.
 *
 * @param vmthread a given thread.
 */
void bvmd_send_parked_events(bvm_vmthread_t *vmthread) {

	bvmd_eventcontext_t *context = vmthread->dbg_parked_events;

	while ( (vmthread->status == BVM_THREAD_STATUS_RUNNABLE) && (context != NULL) ) {

		dbg_send_event(context);
		vmthread->dbg_parked_events = context->next;
		bvmd_eventdef_free_context(context);
		context = vmthread->dbg_parked_events;
	}
}

/**
 * Creates a new output stream configured as a debuggee command for #JDWP_Event and
 * #JDWP_Event_Composite as long as the debugger has registered for events of the given
 * event kind.
 *
 * @param eventkind a given eventkind.
 */
static bvmd_packetstream_t *dbg_commandstream_new(bvm_uint32_t eventkind) {

	if (!BVMD_IS_EVENTKIND_REGISTERED(eventkind) || !bvmd_is_session_open())
		return NULL;

	return bvmd_new_commandstream(JDWP_Event, JDWP_Event_Composite);
}

/**
 * Reports whether a given char string matches a given JDWP pattern.  JDWP pattern only either start with '*' or end
 * with '*', but not both at the same time.  Nothing more complex than that.
 *
 * @param string a char string to test the pattern against.
 * @param slen the length of the above \c string.
 * @param pattern the pattern to test against \c string.
 * @param plen the length of the \c pattern.
 *
 * @return #BVM_TRUE of the pattern matches, #BVM_FALSE otherwise.
 */
static bvm_bool_t dbg_match_pattern(char *string, int slen, char *pattern, int plen) {

	char *sstart = string;
	char *pstart = pattern;

	bvm_bool_t result = BVM_TRUE;

	if ((pattern[0] == '*') || (pattern[plen - 1] == '*')) {

		plen--; /* length of pattern less the '*' */

		/* if the pattern length exceeds the string length, no can do ... */
		if (plen > slen)
			return BVM_FALSE;

		if (pattern[0] == '*') {

			/* anything that ends with the pattern (less the '*') */

			pstart++; /* pattern starts with first char (after '*') */

			/* move starting position to the end, less the size of the pattern */
			sstart = string + (slen - plen);
		}

		result = (strncmp(sstart, pstart, plen) == 0);

	} else {
		result = (strcmp(string, pattern) == 0);
	}

	return result;
}

/**
 * Reports whether the modifiers for a given event definition are valid against a given
 * event context.
 *
 * @param eventdef a given event definition.
 * @param context a given event context.
 *
 * @return #BVM_TRUE of the eventdef's modifiers are valid, #BVM_FALSE otherwise.
 */
static bvm_bool_t dbg_are_modifiers_valid(bvmd_eventdef_t *eventdef, bvmd_eventcontext_t *context) {

	int i;
	int mods = eventdef->modifier_count;

	/* for each event modifier */
	for (i = 0; i < mods; i++) {

		bvmd_event_modifier_t *modifier = &(eventdef->modifiers[i]);

		/* ignore modifers that are not in use */
		if (!modifier->in_use)
			continue;

		switch (modifier->modkind) {

			case (JDWP_EventRequest_Set_Out_modifiers_Modifier_Count): {
				/* count has not reached zero yet?  No good. */
				if (modifier->u.count.remaining-- > 1) return BVM_FALSE;

				/* okay, count has reached zero, so mark the eventdef as not in use - it will no
				 * doubt get used this time, but will not the next time. */
				eventdef->in_use = BVM_FALSE;
				break;
			}
			case (JDWP_EventRequest_Set_Out_modifiers_Modifier_Conditional): {
				/* not supported */
				break;
			}
			case (JDWP_EventRequest_Set_Out_modifiers_Modifier_ThreadOnly): { /* 3 */
				if (context->vmthread != modifier->u.threadonly.vmthread)
					return BVM_FALSE;
				break;
			}
			case (JDWP_EventRequest_Set_Out_modifiers_Modifier_ClassOnly): { /* 4 */

				bvm_clazz_t *clazz = bvmd_id_get_by_id(modifier->u.classonly.clazz_id);

				if ( (clazz == NULL) ||
					 (!bvm_clazz_is_assignable_from(context->location.clazz, clazz))) return BVM_FALSE;

				break;
			}
			case (JDWP_EventRequest_Set_Out_modifiers_Modifier_ClassMatch): { /* 5 */
				if (!dbg_match_pattern((char *) context->location.clazz->name->data, context->location.clazz->name->length,
						(char *) modifier->u.classmatch.pattern.data, modifier->u.classmatch.pattern.length))
					return BVM_FALSE;
				break;
			}
			case (JDWP_EventRequest_Set_Out_modifiers_Modifier_ClassExclude): { /* 6 */
				if (dbg_match_pattern((char *) context->location.clazz->name->data, context->location.clazz->name->length,
						(char *) modifier->u.classexclude.pattern.data, modifier->u.classexclude.pattern.length))
					return BVM_FALSE;
				break;
			}
			case (JDWP_EventRequest_Set_Out_modifiers_Modifier_LocationOnly): { /* 7 */
				if ( (context->location.clazz != modifier->u.locationonly.clazz) ||
					 (context->location.method != modifier->u.locationonly.method) ||
					 (context->location.pc_index != modifier->u.locationonly.pc_index) )
				return BVM_FALSE;
				break;
			}
			case (JDWP_EventRequest_Set_Out_modifiers_Modifier_ExceptionOnly): { /* 8 */

				bvmd_exceptiononlymod_t eonly = modifier->u.exceptiononly;

				/* a non-zero clazz id means restrict checking to a clazz and its subtypes.  A NULL class means all
				 * exception clazzes are valid. */
				if (eonly.clazz_id != 0) {
					bvm_clazz_t *clazz = bvmd_id_get_by_id(eonly.clazz_id);

					/* if the clazz is not lopnger loaded ... must be wrong */
					if (clazz == NULL) return BVM_FALSE;

					/* otherwise, if the clazz is not a subtype or is not the same, bad */
					if (!bvm_clazz_is_subclass_of( (bvm_clazz_t *) context->exception.throwable->clazz, clazz))
						return BVM_FALSE;
				}

				/* read this a few times ... it says "only report 'caught' if 'caught' is asked for, and only
				 * report 'uncaught' if 'uncaught' is asked for */
				if ( ( (context->exception.caught && !eonly.caught) ||
					   (!context->exception.caught && !eonly.uncaught)) ) return BVM_FALSE;

				break;
			}
			case (JDWP_EventRequest_Set_Out_modifiers_Modifier_FieldOnly): { /* 9 */
				/* for the moment, unused */
				/*if ( (context->location.clazz != (bvm_clazz_t *) modifier->u.fieldonly.clazz) ||
					 (context->field != modifier->u.fieldonly.field)
				) return BVM_FALSE;*/
				break;
			}
			case (JDWP_EventRequest_Set_Out_modifiers_Modifier_Step): { /* 10 */
				if ( (context->step.vmthread != modifier->u.step.vmthread) ||
					 (context->step.size != modifier->u.step.size) ||
					 (context->step.depth != modifier->u.step.depth)
				) return BVM_FALSE;

				break;
			}
			case (JDWP_EventRequest_Set_Out_modifiers_Modifier_InstanceOnly): { /* 11 */
				/* not supported */
				break;
			}
			case (JDWP_EventRequest_Set_Out_modifiers_Modifier_SourceNameMatch): { /* 12 */
				/* not supported - only used for 'non-java language support'. */
				break;
			}
		}
	}

	return BVM_TRUE;
}

static bvm_bool_t dbg_get_filtered_eventdefs(bvmd_eventcontext_t *context, bvm_uint8_t *suspendPolicy, int *listcount, bvmd_eventdef_t **head) {

	bvmd_eventdef_t *eventdef_list = NULL;

	/* start searching at the eventdef list head */
	bvmd_eventdef_t *eventdef = bvmd_gl_eventdefs;

	/* default hit-counter to zero */
	*listcount = 0;

	/* no eventdefs registered?  Out. */
	if (eventdef == NULL)
		return BVM_FALSE;

	do {

		/* the 'nextvalid' field of the eventdef type is used to link together those eventdefs that satisfy
		 * the filters for the given context.  As we go though the eventdefs we'll NULL this field here and, when
		 * we have an eventdef that satisfies its filters, we'll link it into the list.
		 */
		eventdef->nextvalid = NULL;

		/* ignore all eventdefs that has been marked as not in use */
		if (!eventdef->in_use)
			continue;

		/* we only match on eventkind */
		if (eventdef->eventkind == context->eventkind) {

			/* no modifiers, or all modifiers satisfied mean 'cool - we found a matching eventdef */
			if ((eventdef->modifier_count == 0) || dbg_are_modifiers_valid(eventdef, context)) {

				/* inform the calling function that we have found one (by way of incrementing this
				 * passed-in counter) */
				(*listcount)++;

				/* keep the highest suspend policy we come across for matching eventdefs.  Multiple matching
				 * eventdefs may have different suspend policies - we go for the top */
				if (eventdef->suspend_policy > *suspendPolicy) {
					*suspendPolicy = eventdef->suspend_policy;
				}

				/* link this eventdefs into the list of found eventdefs. */
				if (eventdef_list == NULL) {
					/* first eventdef in chain */
					*head = eventdef;
					eventdef_list = eventdef;
				} else {
					/* not the first in the chain */
					eventdef_list->nextvalid = eventdef;
					eventdef_list = eventdef;
				}
			}
		}

	} while ((eventdef = eventdef->next));

	return (eventdef_list != NULL);
}

static void dbg_do_suspend(bvm_uint8_t policy, bvm_vmthread_t *vmthread) {

	switch (policy) {
		case (JDWP_SuspendPolicy_NONE):
			break;
		case (JDWP_SuspendPolicy_EVENT_THREAD): {
			bvmd_thread_suspend(vmthread);
			break;
		}
		case (JDWP_SuspendPolicy_ALL):
			bvmd_thread_suspend_all();
	}
}

bvm_bool_t bvmd_breakpoint_opcode_for_location(bvmd_location_t *location, bvm_uint8_t *opcode) {
	bvmd_eventdef_t *eventdef = bvmd_gl_eventdefs;

	if (eventdef == NULL)
		return BVM_FALSE;

	do {

		/* note that we are searching also for event eventdefs that are in_use = false.  Why?  If a breakpoint
		 * has a counter, we mark it as not in use when the counter is reached - but we still want to
		 * find it. */

		/* we only match on breakpoint eventkind */
		if (eventdef->eventkind == JDWP_EventKind_BREAKPOINT) {

			/* ... now, get the locationonly modifier from the breakpoint */

			int i = eventdef->modifier_count;

			/* for each eventdef modifier */
			while (i--) {

				bvmd_event_modifier_t *modifier = &(eventdef->modifiers[i]);

				if (modifier->modkind == JDWP_EventRequest_Set_Out_modifiers_Modifier_LocationOnly) {

					if ( (location->clazz == modifier->u.locationonly.clazz) &&
						 (location->method == modifier->u.locationonly.method) &&
						 (location->pc_index == modifier->u.locationonly.pc_index) ) {

						*opcode = eventdef->breakpoint_opcode;

						return BVM_TRUE;
					}
				}
			}
		}

	} while ((eventdef = eventdef->next));

	return BVM_FALSE;
}


void bvmd_event_Breakpoint(bvmd_location_t *location) {

	bvmd_packetstream_t *out;
	bvmd_eventdef_t *eventdef = NULL;
	int listcount = 0;
	bvm_uint8_t suspendpolicy = JDWP_SuspendPolicy_NONE;
	bvmd_eventcontext_t context;

	if ((out = dbg_commandstream_new(BVMD_EventKind_BREAKPOINT)) == NULL) return;

	memset(&context, 0, sizeof(context));
	context.vmthread = bvm_gl_thread_current;
	context.eventkind = JDWP_EventKind_BREAKPOINT;
	context.location = *location;

	if (dbg_get_filtered_eventdefs(&context, &suspendpolicy, &listcount, &eventdef)) {

		bvmd_out_writebyte(out, suspendpolicy);
		bvmd_out_writeint32(out, listcount);

		while (eventdef != NULL) {

			bvmd_out_writebyte(out, JDWP_EventKind_BREAKPOINT);
			bvmd_out_writeint32(out, eventdef->id);
			bvmd_out_writeobject(out, bvm_gl_thread_current->thread_obj);
			bvmd_out_writelocation(out, location);

			eventdef = eventdef->nextvalid;
		}

		bvmd_sendstream(out);
		dbg_do_suspend(suspendpolicy, bvm_gl_thread_current);
	}
}

void bvmd_event_Exception(bvmd_eventcontext_t *context) {

	bvmd_packetstream_t *out;
	bvmd_eventdef_t *eventdef = NULL;
	int listcount = 0;
	bvm_uint8_t suspendpolicy = JDWP_SuspendPolicy_NONE;

	if ((out = dbg_commandstream_new(BVMD_EventKind_EXCEPTION)) == NULL)
		return;

	if (dbg_get_filtered_eventdefs(context, &suspendpolicy, &listcount, &eventdef)) {

		bvmd_out_writebyte(out, suspendpolicy);
		bvmd_out_writeint32(out, listcount);

		while (eventdef != NULL) {

			bvmd_out_writebyte(out, JDWP_EventKind_EXCEPTION);

			bvmd_out_writeint32(out, eventdef->id); 							/* request id */
			bvmd_out_writeobject(out, context->vmthread->thread_obj); 			/* thread id */
			bvmd_out_writelocation(out, &(context->location)); 				/* exception location */
			bvmd_out_writebyte(out, JDWP_Tag_OBJECT); 						/* ref tag type*/
			bvmd_out_writeobject(out, context->exception.throwable); 			/* exception object */
			bvmd_out_writelocation(out, &(context->catch_location)); 		/* exception catch location */

			eventdef = eventdef->nextvalid;
		}

		bvmd_sendstream(out);
		dbg_do_suspend(suspendpolicy, context->vmthread);
	}
}

void bvmd_event_Thread(bvmd_eventcontext_t *context) {

	bvmd_packetstream_t *out;
	bvmd_eventdef_t *eventdef = NULL;
	int listcount = 0;
	bvm_uint8_t suspendpolicy = JDWP_SuspendPolicy_NONE;
	int dbgeventkind;

	dbgeventkind = (context->eventkind == JDWP_EventKind_THREAD_START) ? BVMD_EventKind_THREAD_START : BVMD_EventKind_THREAD_DEATH;

	if ((out = dbg_commandstream_new(dbgeventkind)) == NULL)
		return;

	if (dbg_get_filtered_eventdefs(context, &suspendpolicy, &listcount, &eventdef)) {

		bvmd_out_writebyte(out, suspendpolicy);
		bvmd_out_writeint32(out, listcount);

		while (eventdef != NULL) {
			bvmd_out_writebyte(out, context->eventkind);
			bvmd_out_writeint32(out, eventdef->id);
			bvmd_out_writeobject(out, context->vmthread->thread_obj);

			eventdef = eventdef->nextvalid;
		}

		bvmd_sendstream(out);
		dbg_do_suspend(suspendpolicy, context->vmthread);
	}
}

void bvmd_event_ClassPrepare(bvmd_eventcontext_t *context) {

	bvmd_packetstream_t *out;
	bvmd_eventdef_t *eventdef = NULL;
	int listcount = 0;
	bvm_uint8_t suspendpolicy = JDWP_SuspendPolicy_NONE;

	bvm_clazz_t *clazz = context->location.clazz;

	if (BVM_CLAZZ_IsPrimitiveClazz(clazz) || (clazz->state < BVM_CLAZZ_STATE_LOADED))
		return;

	if ((out = dbg_commandstream_new(BVMD_EventKind_CLASS_PREPARE)) == NULL)
		return;

	if (dbg_get_filtered_eventdefs(context, &suspendpolicy, &listcount, &eventdef)) {

		bvmd_out_writebyte(out, suspendpolicy);
		bvmd_out_writeint32(out, listcount);

		while (eventdef != NULL) {

			bvmd_out_writebyte(out, JDWP_EventKind_CLASS_PREPARE);

			bvmd_out_writeint32(out, eventdef->id); 					/* request id */
			bvmd_out_writeobject(out, context->vmthread->thread_obj); 	/* thread id */
			bvmd_out_writebyte(out, bvmd_clazz_reftype(clazz)); 		/* ref tag type*/
			bvmd_out_writeobject(out, clazz); 							/* referencetype id */
			bvmd_out_writeutfstring(out, clazz->jni_signature); 	/* jni signature */
			bvmd_out_writeint32(out, bvmd_clazz_status(clazz)); 		/* status */

			eventdef = eventdef->nextvalid;
		}

		bvmd_sendstream(out);
		dbg_do_suspend(suspendpolicy, context->vmthread);
	}
}

/**
 * Do a SingleStep event - and optionally also do a breakpoint.  Any single-step event and breakpoint events will
 * be send as a consolidated command to the debugger.
 *
 * @param context the event context
 * @param do_breakpoint if true, look for breakpoint events that satisfy the same context as
 * the single step.
 *
 */
bvm_bool_t bvmd_event_SingleStep(bvmd_eventcontext_t *context, bvm_bool_t do_breakpoint) {

	bvmd_packetstream_t *out;
	bvmd_eventdef_t *eventdef = NULL;
    int listcount = 0;
    int totalcount = 0;
	bvm_uint8_t suspendpolicy = JDWP_SuspendPolicy_NONE;
	bvm_bool_t is_step_header_written = BVM_FALSE;
	bvmd_eventcontext_t bp_context;

	bvmd_streampos_t sp_pos;
	bvmd_streampos_t lc_pos;

	if (!bvmd_is_session_open()) return BVM_FALSE;

	/* one output stream for both events, if any */
	out = bvmd_new_commandstream(JDWP_Event, JDWP_Event_Composite);

	if (BVMD_IS_EVENTKIND_REGISTERED(BVMD_EventKind_SINGLE_STEP)) {

		/* check whether any step event match the given context */
		if (dbg_get_filtered_eventdefs(context, &suspendpolicy, &listcount, &eventdef)) {

			/* write out the suspend policy and initial event count - but remember the write positions, if we
			 * also write out a breakpoint these will need to be re-written */
			sp_pos = bvmd_out_writebyte(out, suspendpolicy);
			lc_pos = bvmd_out_writeint32(out, listcount);

			/* flag to indicate that we have written a single step event(s) */
			is_step_header_written = BVM_TRUE;

			/* .. and we're keeping a tally of all events sent out */
			totalcount = listcount;

			/* write out all the single step events (normally, this would be just one */
			while (eventdef != NULL) {

				bvmd_out_writebyte(out, JDWP_EventKind_SINGLE_STEP);

				bvmd_out_writeint32(out, eventdef->id); 						/* request id */
				bvmd_out_writeobject(out, context->vmthread->thread_obj); 		/* thread id */
				bvmd_out_writelocation(out, &(context->location)); 			/* step location */

				eventdef = eventdef->nextvalid;
			}
		}
	}

	if (do_breakpoint) {

		if (BVMD_IS_EVENTKIND_REGISTERED(BVMD_EventKind_BREAKPOINT)) {

			/* copy some details from the single step context into a another context structure for breakpoints */
			memset(&bp_context, 0, sizeof(bp_context));
			bp_context.vmthread = context->vmthread;
			bp_context.eventkind = JDWP_EventKind_BREAKPOINT;
			bp_context.location = context->location;
			eventdef = NULL;

			/* and go looking for breakpoints */
			if (dbg_get_filtered_eventdefs(&bp_context, &suspendpolicy, &listcount, &eventdef)) {

				if (!is_step_header_written) {
					bvmd_out_writebyte(out, suspendpolicy);
					bvmd_out_writeint32(out, listcount);
				}

				/* and update the tally ... */
				totalcount += listcount;

				while (eventdef != NULL) {

					bvmd_out_writebyte(out, JDWP_EventKind_BREAKPOINT);
					bvmd_out_writeint32(out, eventdef->id);
					bvmd_out_writeobject(out, bvm_gl_thread_current->thread_obj);
					bvmd_out_writelocation(out, &(bp_context.location));

					eventdef = eventdef->nextvalid;
				}
			}
		}
	}

	if (totalcount > 0) {

		/* some notes : notice we only suspend threads once - we get the most severe policy from any of the
		 * eventdefs that we have sent and use that policy. */

		if (is_step_header_written && do_breakpoint) {
			bvmd_out_rewritebytes(&sp_pos, &suspendpolicy, sizeof(suspendpolicy));
            bvmd_out_rewriteint32(&lc_pos, (bvm_int32_t) totalcount);
		}

		bvmd_sendstream(out); // also frees the packetstream
		dbg_do_suspend(suspendpolicy, context->vmthread);
	} else {
        dbg_free_packetstream(out);
	}

	return (totalcount > 0);
}

static void dbg_event_ClassUnload(bvmd_eventcontext_t *context) {

	bvmd_packetstream_t *out;
	bvmd_eventdef_t *eventdef = NULL;
	int listcount = 0;
	bvm_uint8_t suspendpolicy = JDWP_SuspendPolicy_NONE;

	if 	(context->location.clazz->state < BVM_CLAZZ_STATE_LOADED) return;

	if ((out = dbg_commandstream_new(BVMD_EventKind_CLASS_UNLOAD)) == NULL)
		return;

	if (dbg_get_filtered_eventdefs(context, &suspendpolicy, &listcount, &eventdef)) {

		bvmd_out_writebyte(out, suspendpolicy);
		bvmd_out_writeint32(out, listcount);

		while (eventdef != NULL) {

			bvmd_out_writebyte(out, JDWP_EventKind_CLASS_UNLOAD);

			bvmd_out_writeint32(out, eventdef->id); 								/* request id */
			bvmd_out_writeutfstring(out, context->location.clazz->jni_signature); 	/* clazz jni signature */

			eventdef = eventdef->nextvalid;
		}

		bvmd_sendstream(out);
		dbg_do_suspend(suspendpolicy, bvm_gl_thread_current);
	}
}

/**
 * Park a given to-be-unloaded clazz struct onto the #bvmd_gl_unloaded_clazzes list.
 *
 * @param clazz the given clazz
 */
void bvmd_event_park_unloaded_clazz(bvm_clazz_t *clazz) {

	clazz->next = NULL;

	if (bvmd_gl_unloaded_clazzes == NULL) {
		bvmd_gl_unloaded_clazzes = clazz;
	} else {

		/* find the end and add it there */
		bvm_clazz_t *current = bvmd_gl_unloaded_clazzes;

		while (current->next != NULL)
			current = current->next;

		current->next = clazz;
	}
}

/**
 * Send events for all parked clazz unloads.
 */
void bvmd_event_send_parked_clazz_unloads() {

	/* Send off events for all parked clazzes.  We have to be quite careful here, as
	 * sending the events may cause a GC, which may in turn attempt to add more clazzes to the end of
	 * the list.
	 *
	 * Another note:  The heap for a clazz is marked as BVM_ALLOC_TYPE_STATIC by the collector when
	 * it parked it for clazz unload.  This ensures that the clazz, though not yet free, will
	 * not be GC'd again, and end up in the list again.
	 *
	 * Freeing the clazz is now our responsibility.
	 */

	/* I really have no idea of whether a debugger will attempt a suspend on a class unload, it seems
	 * stupid, but who knows.  The class unload is the only event that does not specify a thread ID, so it is
	 * highly unlikely that a debugger will attempt a thread suspend, but I guess it *could* suspend the VM
	 * while (erm) something is configured or whatever.  At any rate, I have not seen this behaviour, but
	 * just being defensive.
	 */

	bvm_clazz_t *next;
	bvmd_eventcontext_t context;

	memset(&context, 0, sizeof(context));
	context.vmthread = bvm_gl_thread_current;
	context.eventkind = JDWP_EventKind_CLASS_UNLOAD;

	/* keep moving the head of the list while processing .. */
	while ( (bvmd_gl_unloaded_clazzes != NULL) && (!bvmd_is_vm_suspended() ) ) {

		context.location.clazz = bvmd_gl_unloaded_clazzes;

		/* unload the head of the list */
		dbg_event_ClassUnload(&context);

		/* remember the 'next' before freeing the head */
		next = bvmd_gl_unloaded_clazzes->next;

		/* free the head */
		bvm_heap_free(bvmd_gl_unloaded_clazzes);

		/* and move the list head on */
		bvmd_gl_unloaded_clazzes = next;
	}
}

void bvmd_event_VMStart() {

	bvmd_packetstream_t *out;
	bvm_uint8_t suspendpolicy;

	if (!bvmd_is_session_open()) return;

	out = bvmd_new_commandstream(JDWP_Event, JDWP_Event_Composite);

	suspendpolicy = bvmd_gl_suspendonstart ? JDWP_SuspendPolicy_ALL : JDWP_SuspendPolicy_NONE;

	bvmd_out_writebyte(out, suspendpolicy);
	bvmd_out_writeint32(out, 1); /* just the one command */
	bvmd_out_writebyte(out, JDWP_EventKind_VM_START);
	bvmd_out_writeint32(out, 0); /* no request id */
	bvmd_out_writeobject(out, bvm_gl_thread_current->thread_obj); /* the initial thread */

	bvmd_sendstream(out);
	dbg_do_suspend(suspendpolicy, bvm_gl_thread_current);
}

void bvmd_event_VMDeath() {

	bvmd_packetstream_t *out;

	if (!bvmd_is_session_open()) return;

	out = bvmd_new_commandstream(JDWP_Event, JDWP_Event_Composite);

	bvmd_out_writebyte(out, JDWP_SuspendPolicy_NONE);
	bvmd_out_writeint32(out, 1); /* just the one command */
	bvmd_out_writebyte(out, JDWP_EventKind_VM_DEATH);
	bvmd_out_writeint32(out, 0); /* no request id */

	bvmd_sendstream(out);
}

#endif
