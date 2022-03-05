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

  JDWP EventRequest commandset command handlers.

  @section eventdef-eventdefs Event Definitions

  As the debugger sends requests for debuggee events that it is interested in, they are processed and stored
  internally as "event definitions".

  An Event Definition (or 'eventdef') is a #bvmd_eventdef_t structure instance that holds information about a particular
  event request made by the debugger.  Each eventdef has a JDWP 'eventkind'.

  Each new eventdef is placed into a global linked list (#bvmd_gl_eventdefs).

  As each eventdef is received and processed, its eventkind is also registered into a single consolidated bvm_int32_t
  (#bvmd_gl_registered_events).  Each JDWP eventkind is translated to a 'DBG eventkind' that can be applied as a simple mask
  on the \c bvmd_gl_registered_events bvm_int32_t to set a bit and help quickly determine whether a given eventkind is registered.
  Notice each JDWP eventkind has a corresponding DBG eventkind.  The JDWP eventkinds numbers are not amenable to any kind
  of quick lookup structure, so this simple translation is effective.

  As eventdefs are cleared by the debugger, the \c bvmd_gl_registered_events bvm_int32_t is recalculated to make sure it only contains
  those eventkinds that are actually still registered.

  @section eventdef-modifiers Modifiers

  Each eventdef may contain a number of 'modifiers'.  Modifiers are actually 'filters' provided by the debugger with an event
  request to narrow down the applicability of a given eventdef. For example, a breakpoint eventdef has a modifier that
  restricts the location.

  As the event request is read and processed into its internal #bvmd_eventdef_t form, the modifiers are also read and
  processed and held with the eventdef as an array of #bvmd_event_modifier_t.

  There are a number of different modifier types and each modifier holds just on at a time.  The type
  of a modifier is defined by its 'modifier kind'.

  Notice the #bvmd_event_modifier_t structure is actually a union of smaller structures - one for each different modifier
  kind.  This provides a way to treat all modifiers in the same way.

  Each modifier holds just enough information to be useful for its kind.  Some modifiers compare against clazzes and
  methods and so on.  As usual, the debugger sends the ID of these with the modifier definition.  It is worth noting at
  this point that most of the modifiers that deal with comparisons against 'things with an ID' actually resolve
  the ID immediately and store a direct pointer in the internalised modifier.  The modifiers that do this are using those
  pointers strictly for equality *comparison* - nothing further - so if the pointer actually is no longer valid,
  no harm will be done.  Some other modifiers actually *use* the thing represented by the ID.  For example, the
  "exception only" modifier must test whether a thrown exception's clazz is a subclazz of the clazz represented by the
  modifier clazz ID. If the clazz has been GC'd, any bvm_clazz_t pointers will be invalid, and using it may
  cause things to blow up. For this reason .. some modifiers store IDs and perform dbg id map lookups each time.  If
  the ID is not in the map, the memory has been GC'd and thus the modifier test must fail.

  @section eventdef-clearing Clearing Event Definitions

  Some eventkinds require some extra care when setting and clearing.  When a breakpoint event request is received, the actual
  bytecode it refers to is replaced with an #OPCODE_breakpoint opcode - the execution loop uses this opcode to handle
  breakpoints.  Conversely, when a breakpoint is cleared the displaced opcode must be put back into its original position.

  Similarly, a single-step eventdef requires some state information being stored with the single-stepping thread, and
  being cleared when the single step eventdef is cleared.

  Event handling really has two faces - one side is the actual registration and clearing of event requests (and their modifiers)
  from a debugger (all handled in this unit), the other is the creation and sending to the debugger of events that meets the debuggers
  criteria.  This happens in \c debugger-cmd-64-event.c

  @author Greg McCreath
  @since 0.0.10

*/

#if BVM_DEBUGGER_ENABLE

/**
 * Global holder of registered event kinds.  One DBG event kind per bit is stored in this bvm_uint32_t.
 */
bvm_uint32_t bvmd_gl_registered_events = 0;

/** Head of a global linked list of #bvmd_eventdef_t event definitions */
bvmd_eventdef_t *bvmd_gl_eventdefs = NULL;

/** Head of a list of clazzes unloaded during GC awaiting their CLASS_UNLOAD event being sent. Clazzes
 * here are no longer in the clazz pool, and have had all their internal structures already freed.  Only
 * the bvm_clazz_t struct remains, and any pointers it may have into permanent things like the
 * uftstring pool. */
bvm_clazz_t *bvmd_gl_unloaded_clazzes = NULL;


/**
 * Frees an eventdef and releases its memory back to the heap.  Additionally, scans the eventdef's
 * modifiers for particular modifiers, that if present, have known to also have
 * allocated heap memory - the heap used by these modifier is also freed.
 *
 * @param eventdef the eventdef to free
 */
static void dbg_eventdef_free(bvmd_eventdef_t *eventdef) {

	if (eventdef->modifier_count > 0) {

		int i = eventdef->modifier_count;

		/* clean up modifiers first */
		while (i--) {

			bvmd_event_modifier_t *modifier = &(eventdef->modifiers[i]);
			bvm_uint8_t modkind = modifier->modkind;

			/* the following modifiers allocate heap memory for their string patterns */
			if ( (modkind == JDWP_EventRequest_Set_Out_modifiers_Modifier_ClassMatch) ||
				 (modkind == JDWP_EventRequest_Set_Out_modifiers_Modifier_ClassExclude) )  {

				if (modifier->u.classmatch.pattern.data != NULL)
					bvm_heap_free(modifier->u.classmatch.pattern.data);
			}
		}

		/* free the modifiers array */
		bvm_heap_free(eventdef->modifiers);
	}

	/* remove the event def from the ID cache */
	bvmd_id_remove_addr(eventdef);

	/* and finally free the eventdef */
	bvm_heap_free(eventdef);
}

/**
 * Set a breakpoint at the given location by replacing the opcode there with an
 * #OPCODE_breakpoint opcode.  The original opcode is returned.
 *
 * @param location a given location
 *
 * @return the opcode displaced by the breakpoint.
 */
static bvm_uint8_t dbg_breakpoint_set(bvmd_location_t *location) {

	/* get the method opcode at the given location */
	bvm_uint8_t opcode = *(location->method->code.bytecode + location->pc_index);

	/* substitute the BREAKPOINT opcode in its stead */
	location->method->code.bytecode[location->pc_index] = OPCODE_breakpoint;

	return opcode;
}

/**
 * For a given JDWP eventkind, translate it to a DBG eventkind and 'or' it in the
 * bvmd_gl_registered_events bitmap.
 *
 * @param jdwp_eventkind a given JDWP eventkind
 *
 * @return #BVM_TRUE if the JDWP eventkind is registered, #BVM_FALSE otherwise.
 */
static bvm_bool_t dbg_eventkind_register(bvm_uint8_t jdwp_eventkind) {

	bvm_int32_t dbg_eventkind = BVMD_EventKind_NONE;

	switch (jdwp_eventkind) {

		case (JDWP_EventKind_SINGLE_STEP):
			dbg_eventkind = BVMD_EventKind_SINGLE_STEP;
			break;
		case (JDWP_EventKind_BREAKPOINT):
			dbg_eventkind = BVMD_EventKind_BREAKPOINT;
			break;
		/*case (JDWP_EventKind_FRAME_POP):
			dbg_eventkind = BVMD_EventKind_FRAME_POP;
			break;*/
		case (JDWP_EventKind_EXCEPTION):
			dbg_eventkind = BVMD_EventKind_EXCEPTION;
			break;
		/*case (JDWP_EventKind_USER_DEFINED):
			dbg_eventkind = BVMD_EventKind_USER_DEFINED;
			break;*/
		case (JDWP_EventKind_THREAD_START):
			dbg_eventkind = BVMD_EventKind_THREAD_START;
			break;
		case (JDWP_EventKind_THREAD_DEATH):
			dbg_eventkind = BVMD_EventKind_THREAD_DEATH;
			break;
		case (JDWP_EventKind_CLASS_PREPARE):
			dbg_eventkind = BVMD_EventKind_CLASS_PREPARE;
			break;
		case (JDWP_EventKind_CLASS_UNLOAD):
			dbg_eventkind = BVMD_EventKind_CLASS_UNLOAD;
			break;
		case (JDWP_EventKind_CLASS_LOAD):
			dbg_eventkind = BVMD_EventKind_CLASS_LOAD;
			break;
		/*case (JDWP_EventKind_FIELD_ACCESS):
			dbg_eventkind = BVMD_EventKind_FIELD_ACCESS;
			break;
		case (JDWP_EventKind_FIELD_MODIFICATION):
			dbg_eventkind = BVMD_EventKind_FIELD_MODIFICATION;
			break;*/
		case (JDWP_EventKind_EXCEPTION_CATCH):
			dbg_eventkind = BVMD_EventKind_EXCEPTION_CATCH;
			break;
		/*case (JDWP_EventKind_METHOD_ENTRY):
			dbg_eventkind = BVMD_EventKind_METHOD_ENTRY;
			break;
		case (JDWP_EventKind_METHOD_EXIT):
			dbg_eventkind = BVMD_EventKind_METHOD_EXIT;
			break;
		case (JDWP_EventKind_METHOD_EXIT_WITH_RETURN_VALUE):
			dbg_eventkind = BVMD_EventKind_METHOD_EXIT_WITH_RETURN_VALUE;
			break;
		case (JDWP_EventKind_MONITOR_CONTENDED_ENTER):
			dbg_eventkind = BVMD_EventKind_MONITOR_CONTENDED_ENTER;
			break;
		case (JDWP_EventKind_MONITOR_CONTENDED_ENTERED):
			dbg_eventkind = BVMD_EventKind_MONITOR_CONTENDED_ENTERED;
			break;
		case (JDWP_EventKind_MONITOR_WAIT):
			dbg_eventkind = BVMD_EventKind_MONITOR_WAIT;
			break;
		case (JDWP_EventKind_MONITOR_WAITED):
			dbg_eventkind = BVMD_EventKind_MONITOR_WAITED;
			break;*/
		case (JDWP_EventKind_VM_START):
			dbg_eventkind = BVMD_EventKind_VM_START;
			break;
		case (JDWP_EventKind_VM_DEATH):
			dbg_eventkind = BVMD_EventKind_VM_DEATH;
			break;
		/*case (JDWP_EventKind_VM_DISCONNECTED):
			dbg_eventkind = BVMD_EventKind_VM_DISCONNECTED;
			break;*/
		default : {
			return BVM_FALSE;
		}
	}

	bvmd_gl_registered_events |= dbg_eventkind;

	return BVM_TRUE;
}

/**
 * Removes an eventdef from the global eventdefs list.  Actually serves two purposes:  the first
 * eventdef of the given \c eventkind or that has the given \c id is removed - then the
 * function returns.
 *
 * @param jdwp_eventkind the JDWP kind of event to remove; or
 * @param id the id of the eventdef to remove
 *
 * @return a handle to the removed eventdef, or \c NULL if one was not removed.
 */
static bvmd_eventdef_t *dbg_eventdef_remove(bvm_uint8_t jdwp_eventkind, bvm_int32_t id) {

	bvmd_eventdef_t *prev = NULL;
	bvmd_eventdef_t *def = bvmd_gl_eventdefs;

	/* traverse the eventdefs list and remove the first matching (either) id or eventkind */

	while (def != NULL) {

		/* if the id, or the jdwp eventkind matches, remove it */
		if ( (def->id == id) || (def->eventkind == jdwp_eventkind) ) {

			/* if head of list */
			if (prev == NULL) {
				bvmd_gl_eventdefs = def->next;
			} else {
				prev->next = def->next;
			}

			return def;
		}

		prev = def;
		def = def->next;

	}

	return NULL;
}

/**
 * Resets the global #bvmd_gl_registered_events bitmap to have only those eventkinds that exist in
 * the global #bvmd_gl_eventdefs eventdefs list.
 */
static void dbg_eventdef_refresh_registrations() {

	/* start searching at the eventdef list head */
	bvmd_eventdef_t *eventdef = bvmd_gl_eventdefs;

	bvmd_gl_registered_events = 0;

	/* no eventdefs registered?  Out. */
	if (eventdef == NULL) return;

	do {
		/* ignore all eventdefs that have been marked as not in use */
		if (!eventdef->in_use) continue;

		dbg_eventkind_register(eventdef->eventkind);

	} while ((eventdef = eventdef->next));
}

/**
 * JDWP Command handler for EventRequest/Set.
 *
 * Read a debugger 'event set' request and internalises it into an #bvmd_eventdef_t in the global eventdefs
 * list #bvmd_gl_eventdefs.  Each event modifier is internalised into an #bvmd_event_modifier_t housed in an
 * array of modifiers on the eventdef.
 *
 * Additionally, registers the eventkind in the global eventkind bitmap #bvmd_gl_registered_events.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_ER_Set(bvmd_instream_t* in, bvmd_outstream_t* out) {

	bvm_uint8_t eventkind = bvmd_in_readbyte(in);
	bvm_uint8_t suspend_policy = bvmd_in_readbyte(in);
	bvm_int32_t modifiers = bvmd_in_readint32(in);
	bvm_int32_t i;
	bvmd_eventdef_t *eventdef;

	/* not a valid eventkind or one that we are interested in ? */
	if (!dbg_eventkind_register(eventkind)) {
		out->error = JDWP_Error_INVALID_EVENT_TYPE;
		return;
	}

	eventdef = bvm_heap_calloc(sizeof(bvmd_eventdef_t), BVM_ALLOC_TYPE_STATIC);
	eventdef->id = 	bvmd_id_put(eventdef);
	eventdef->eventkind = eventkind;
	eventdef->modifier_count = modifiers;
	eventdef->suspend_policy = suspend_policy;
	eventdef->in_use = BVM_TRUE;

	/* if the eventdef has modifiers, create an array to house them */
	if (modifiers > 0)
		eventdef->modifiers = bvm_heap_calloc(modifiers * sizeof(bvmd_event_modifier_t), BVM_ALLOC_TYPE_STATIC);

	/* for each modifier */
	for (i=0; i < modifiers; i++) {

		bvmd_event_modifier_t *modifier = &(eventdef->modifiers[i]);

		modifier->modkind = bvmd_in_readbyte(in);
		modifier->in_use = BVM_TRUE;

		switch(modifier->modkind) {

			case (JDWP_EventRequest_Set_Out_modifiers_Modifier_Count):	{		/* 1 */
				modifier->u.count.remaining = bvmd_in_readint32(in);
				break;
			}
			case (JDWP_EventRequest_Set_Out_modifiers_Modifier_Conditional): {	/* 2 */
				/* not supporting this capability */
				modifier->in_use = BVM_FALSE;
				/* eat the unused bytes */
				bvmd_in_readint32(in);
				break;
			}
			case (JDWP_EventRequest_Set_Out_modifiers_Modifier_ThreadOnly):	{	/* 3 */
				/* get the thread for the given id.  bvmd_readcheck_threadref() will populate
				 * the error in the output stream if there is a problem */
				bvm_thread_obj_t *thread_obj;
				if ( (thread_obj = bvmd_readcheck_threadref(in, out)) )
					modifier->u.threadonly.vmthread = thread_obj->vmthread;
				break;
			}
			case (JDWP_EventRequest_Set_Out_modifiers_Modifier_ClassOnly):	{	/* 4 */
				/* get the id of the clazz.  Note we do not resolve the clazz pointer now with this
				 * modifier.  Each time we use the ID to look up the clazz.  No clazz found = clazz has \
				 * been GC'd.  We do not want to do operations on a clazz that has been GC'd.
				 */
				modifier->u.classonly.clazz_id = bvmd_in_readint32(in);
				break;
			}
			case (JDWP_EventRequest_Set_Out_modifiers_Modifier_ClassMatch):	{	/* 5 */
				/* read the class name utfstring and make it into an internalised class
				name by replacing all '.' with '/' */
				bvmd_in_readutfstring(in, &(modifier->u.classmatch.pattern) );
				bvm_str_replace_char( (char *) modifier->u.classmatch.pattern.data, modifier->u.classmatch.pattern.length,'.','/');
				break;
			}
			case (JDWP_EventRequest_Set_Out_modifiers_Modifier_ClassExclude):{	/* 6 */
				/* read the class name utfstring and make it into an internalised class
				name by replacing all '.' with '/' */
				bvmd_in_readutfstring(in, &(modifier->u.classexclude.pattern) );
				bvm_str_replace_char( (char *) modifier->u.classexclude.pattern.data, modifier->u.classexclude.pattern.length,'.','/');
				break;
			}
			case (JDWP_EventRequest_Set_Out_modifiers_Modifier_LocationOnly):	{ /* 7 */

				bvmd_location_t location;
				bvmd_in_readlocation(in, &location);

				if (location.clazz == NULL || location.method == NULL) {
					out->error = JDWP_Error_INVALID_LOCATION;
				} else {
					bvm_uint8_t unused;
					/* believe it or not, it is possible that a debugger sends the same
					 * breakpoint more than.  Ludicrous, yes.  Here, we make sire that only
					 * a single breakpoint is set for a given location - if there is
					 * already a breakpoint for the given location, the breakpoint is not
					 * set and this modifier is marked as not in use - so is the eventdef
					 */
					if (!bvmd_breakpoint_opcode_for_location(&location, &unused)) {
						modifier->u.locationonly = location;
						eventdef->breakpoint_opcode = dbg_breakpoint_set(&location);
					} else {
						eventdef->in_use = BVM_FALSE;
						modifier->in_use = BVM_FALSE;
					}
				}

				break;
			}
			case (JDWP_EventRequest_Set_Out_modifiers_Modifier_ExceptionOnly): {	/* 8 */
				/* get the id of the clazz.  Note we do not resolve the clazz pointer now with this
				 * modifier.  Each time we use the ID to look up the clazz.  No clazz found = clazz has \
				 * been GC'd.  We do not want to do operations on a clazz that has been GC'd.
				 */
				modifier->u.exceptiononly.clazz_id = bvmd_in_readint32(in);
				modifier->u.exceptiononly.caught = bvmd_in_readboolean(in);
				modifier->u.exceptiononly.uncaught = bvmd_in_readboolean(in);
				break;
			}
			case (JDWP_EventRequest_Set_Out_modifiers_Modifier_FieldOnly):	{	/* 9 */
				/* not supporting this capability as yet*/
				modifier->in_use = BVM_FALSE;
				/*
				modifier->u.fieldonly.clazz = bvmd_in_readobject(in);
				modifier->u.fieldonly.field = bvmd_in_readaddress(in);
				*/
				break;
			}
			case (JDWP_EventRequest_Set_Out_modifiers_Modifier_Step):	{		/* 10 */

				bvm_vmthread_t *vmthread;
				bvm_thread_obj_t *thread_obj;

				if ( (thread_obj = bvmd_readcheck_threadref(in, out)) ) {

					modifier->u.step.vmthread = vmthread= thread_obj->vmthread;
					modifier->u.step.size = bvmd_in_readint32(in);
					modifier->u.step.depth = bvmd_in_readint32(in);

					/* we are going to into step mode ... */
					vmthread->dbg_is_stepping = BVM_TRUE;

					/* ... about the type of step we are doing */
					vmthread->dbg_step.size = modifier->u.step.size;
					vmthread->dbg_step.depth = modifier->u.step.depth;

					/* .. now remember the current execution position so we may compare against it */

					bvm_thread_store_registers(bvm_gl_thread_current);

					/* the currently executing method */
					vmthread->dbg_step.method = vmthread->rx_method;

					/* if the step size is BVM_MIN (individual bytecode instruction), use the address of the byte code as the current
					 * position, otherwise, if the size is by LINE, use the current line number as the position.  If the class
					 * file has no line numbers, the line will be recorded as 0. */

					/* developer note:  The 'position' captured here would actually, really be best served by capturing
					 * a pc 'range' for the position - like the lower and upper pc for a given line number.  This would be
					 * the ideal. For BVM_MIN, the upper and lower pc would simply be the same number (the pc), and for LINE the lower
					 * and upper pc would be the pc bounds of a given line.  Sounds right doesn't it?  However, the line number
					 * attribute table contained in the class file does not structure itself well to finding this
					 * information simply.  As per the JVMS:
					 *
					 * "If LineNumberTable attributes are present in the attributes table of a given Code attribute, then they
					 * may appear in any order. Furthermore, multiple LineNumberTable attributes may together represent a
					 * given line of a source file; that is, LineNumberTable attributes need not be one-to-one
					 * with source lines".
					 *
					 * It is however, quite easy to find the line number for a given pc (but not the inverse "find the pc's for a
					 * given line number") - so we stick with using the line number as the position when using the LINE step
					 * size. */
					vmthread->dbg_step.position = bvmd_step_get_position(vmthread->dbg_step.size, vmthread->rx_method, vmthread->rx_pc);

					/* and record the stack depth also .. */
					vmthread->dbg_step.stack_depth = bvm_stack_get_depth(bvm_gl_thread_current);
				}

				break;
			}
			case (JDWP_EventRequest_Set_Out_modifiers_Modifier_InstanceOnly): {	/* 11 */
				/*
				 * TODO: "BreakpointRequest, MethodExitRequest, ExceptionRequest, StepRequest, WatchpointRequest and
				 *  MethodEntryRequest classes now have the capability of adding instance filters by using the
				 *  addInstanceFilter method. Instance filters restrict the events generated by a request to
				 *  those in which the currently executing instance is the object specified".
				 */
				/* not supporting this capability */
				modifier->in_use = BVM_FALSE;
				/* eat unused bytes */
				bvmd_in_readobject(in);
				break;
			}
			case (JDWP_EventRequest_Set_Out_modifiers_Modifier_SourceNameMatch): { /* 12 */
				/* not supporting this capability - only used for 'debug extension' stuff when
				 * the VM is running code that may be produced from another (non-java) language) */
				modifier->in_use = BVM_FALSE;
				/* eat unused bytes */
				bvmd_in_readbytes(in, NULL, bvmd_in_readint32(in) );
				break;
			}
			default : {
				/* dunno what it is, but we're not supporting it! */
				out->error = JDWP_Error_INVALID_EVENT_TYPE;
				break;
			}
		}
	}

	if (out->error == JDWP_Error_NONE) {

		/* place the new eventdef at the head of the events list */
		eventdef->next = bvmd_gl_eventdefs;
		bvmd_gl_eventdefs = eventdef;

		bvmd_out_writeint32(out, eventdef->id);
	} else {
		/* if an error occurred, wipe out the new eventdef */
		dbg_eventdef_free(eventdef);
		dbg_eventdef_refresh_registrations();
	}
}

/**
 * Clear the breakpoint for a given eventdef.  The locationonly modifier for the eventdef is assumed
 * to be the location of a breakpoint, and thus the location of a breakpoint opcode.  Only
 * the first locationonly modifier is processed - why would a breakpoint have two?  Dunno.
 *
 * @param eventdef a given breakpoint eventdef.
 */
static void dbg_breakpoint_clear(bvmd_eventdef_t *eventdef) {

	/* loop through the modifiers looking for the locationonly modifier.  God hope the
	 * debugger only ever sends one for a breakpoint.  It would seem ludicrous for a
	 * debugger to send two location modifiers for a breakpoint - they couldn't all be satisfied
	 * at the same time!
	 *
	 * Note also that duplicate breakpoints will have their in_use flag set to false.
	 */
	int i = eventdef->modifier_count;

	/* for each modifier */
	while (i--) {

		bvmd_event_modifier_t *modifier = &(eventdef->modifiers[i]);

		if ( (modifier->modkind == JDWP_EventRequest_Set_Out_modifiers_Modifier_LocationOnly) && (modifier->in_use) ) {

			/* grab the method and pc offset from the location */
			bvm_method_t *method = modifier->u.locationonly.method;
			bvm_uint32_t pc_index = modifier->u.locationonly.pc_index;

			/* restore the bytecode at the pc index position to the original bytecode.  Can't
			 * think  of a reason why it might *not* be a breakpoint, but being defensive ... */
			if (method->code.bytecode[pc_index] == OPCODE_breakpoint)
				method->code.bytecode[pc_index] = eventdef->breakpoint_opcode;

			break;
		}
	}
}

/**
 * Clear the single-step info on a thread for a given single-step eventdef.
 *
  * @param eventdef a given single-step eventdef.
 */
static void dbg_singlestep_clear(bvmd_eventdef_t *eventdef) {
	int i = eventdef->modifier_count;

	/* loop through modifiers looking for a single step modifier.  When found, reset the appropriate
	 * fields on it thread */

	while (i--) {
		bvmd_event_modifier_t *modifier = &(eventdef->modifiers[i]);

		/* if the modifier is a step, clear out the step info on the stepping thread */
		if (modifier->modkind == JDWP_EventRequest_Set_Out_modifiers_Modifier_Step) {
			modifier->u.step.vmthread->dbg_is_stepping = BVM_FALSE;
			memset(&(modifier->u.step.vmthread->dbg_step),0, sizeof(modifier->u.step.vmthread->dbg_step));
		}
	}
}

/**
 * Clear all registered events. After this function the global eventdefs list (#bvmd_gl_eventdefs) will
 * be \c NULL and the registered eventkind bitmap (#bvmd_gl_registered_events) will be zero.
 *
 * While clearing, if an eventdef is either a breakpoint, or a singlestep, specific processing is undertaken
 * to clear those properly.
 *
 */
void bvmd_eventdef_clearall() {

	/* start searching at the eventdef list head */
	bvmd_eventdef_t *eventdef = bvmd_gl_eventdefs;
	bvmd_eventdef_t *next;

	/* for each eventdef */
	while (eventdef != NULL) {

		next = eventdef->next;

		/* clean up breakpoints and single steps correctly */
		if (eventdef->eventkind == JDWP_EventKind_BREAKPOINT) {
			dbg_breakpoint_clear(eventdef);
		} else if (eventdef->eventkind == JDWP_EventKind_SINGLE_STEP) {
			dbg_singlestep_clear(eventdef);
		}

		/* clean up the memory used */
		dbg_eventdef_free(eventdef);

		eventdef = next;
	}

	/* NULL the head of the eventdef list */
	bvmd_gl_eventdefs = NULL;

	/* .. and make sure no eventkinds are registered anymore */
	bvmd_gl_registered_events = 0;
}

/**
 * JDWP Command handler for EventRequest/Clear.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_ER_Clear(bvmd_instream_t* in, bvmd_outstream_t* out) {

	bvm_int32_t id;
	bvmd_eventdef_t *eventdef;

	/* eat the eventkind byte .. not required */
	bvmd_in_readbyte(in);

	/* the event id */
	id = bvmd_in_readint32(in);

	/* remove it from the list */
	eventdef = dbg_eventdef_remove(JDWP_EventKind_NONE, id);

	/* it is was there .. */
	if (eventdef != NULL) {

		/* clear the breakpoint if it was so ... */
		if (eventdef->eventkind == JDWP_EventKind_BREAKPOINT) {
			dbg_breakpoint_clear(eventdef);
		} else if (eventdef->eventkind == JDWP_EventKind_SINGLE_STEP) {
			dbg_singlestep_clear(eventdef);
		}

		/* clean up the memory used */
		dbg_eventdef_free(eventdef);

		/* recalc which eventkinds are now registered after this removal */
		dbg_eventdef_refresh_registrations();
	}
}

/**
 * JDWP Command handler for EventRequest/ClearAllBreakpoints.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_ER_ClearAllBreakpoints(bvmd_instream_t* in, bvmd_outstream_t* out) {

	/* remove the first breakpoint from the list (if any) */
	bvmd_eventdef_t *eventdef = dbg_eventdef_remove(JDWP_EventKind_BREAKPOINT, -1);

	/* clear the breakpoint, and keep removing breakpoints from the eventdef list
	 * and clearing them */
	while (eventdef != NULL) {

		dbg_breakpoint_clear(eventdef);

		dbg_eventdef_free(eventdef);

		eventdef = dbg_eventdef_remove(JDWP_EventKind_BREAKPOINT, -1);
	}

	/* remove BREAKPOINT eventkind from registered event kinds */
	bvmd_gl_registered_events &= ~BVMD_EventKind_BREAKPOINT;
}

#endif
