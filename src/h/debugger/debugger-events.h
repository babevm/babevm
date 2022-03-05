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

#ifndef BVM_DEBUGGEREVENTS_H_
#define BVM_DEBUGGEREVENTS_H_

/**

 @file

 Definitions for JDWP event handling.

 @author Greg McCreath
 @since 0.0.10

*/

extern bvm_uint32_t bvmd_gl_registered_events;

extern bvm_clazz_t *bvmd_gl_unloaded_clazzes;

/**
 * Macro to determine if a given DBG eventkind is registered.
 */
#define BVMD_IS_EVENTKIND_REGISTERED(e) ( ( bvmd_gl_registered_events & (e) ) == (e) )

/**
 * JDWP Count modifier.
 */
typedef struct _bvmdcountmodstruct {
	bvm_int32_t remaining;
} bvmd_countmod_t;

/**
 * JDWP Thread-only modifier.
 */
typedef struct _bvmdthreadonlymodstruct {
	bvm_vmthread_t *vmthread;
} bvmd_threadonlymod_t;

/**
 * JDWP Class-only modifier.
 */
typedef struct _bvmdclassonlymodstruct {
	bvm_int32_t clazz_id;
} bvmd_classonlymod_t;

/**
 * JDWP Class-match modifier.
 */
typedef struct _bvmdclassmatchmodstruct {
	bvm_utfstring_t pattern;
} bvmd_classmatchmod_t;

/**
 * JDWP Class-exclude modifier.
 */
typedef struct _bvmdclassexcludemodstruct {
	bvm_utfstring_t pattern;
} bvmd_classexcludemod_t;

/**
 * JDWP Location-only modifier.
 */
typedef struct _bvmdlocationonlymodstruct {
	bvmd_location_t location;
} bvmd_locationonlymod_t;

/**
 * JDWP Exception-only modifier.
 */
typedef struct _bvmdexceptiononlymodstruct {
	bvm_int32_t clazz_id;
	bvm_bool_t caught;
	bvm_bool_t uncaught;
} bvmd_exceptiononlymod_t;

/*
typedef struct _bvmdfieldonlymodstruct {
	bvm_instance_clazz_t *clazz;
	bvm_field_t *field;
} bvmd_fieldonlymod_t;
*/

/**
 * JDWP Step modifier.
 */
typedef struct _bvmdstepmodstruct {
	bvm_vmthread_t *vmthread;
	bvm_int32_t size;
	bvm_int32_t depth;
} bvmd_stepmod_t;

/*
typedef struct _dbginstanceonlystruct {
	bvm_int32_t objectid;
} dbg_instanceonlymod_t;
*/

/**
 * A JDWP event modifier described as a union of modifiers types.
 */
typedef struct _bvmdmodifierstruct {
	bvm_uint8_t modkind;
	bvm_bool_t in_use;
	union {
		bvmd_countmod_t count;
		bvmd_threadonlymod_t threadonly;
		bvmd_classonlymod_t classonly;
		bvmd_classmatchmod_t classmatch;
		bvmd_classexcludemod_t classexclude;
		bvmd_location_t locationonly;
		bvmd_exceptiononlymod_t exceptiononly;
		/*dbg_fieldonlymod_t fieldonly;*/
		bvmd_stepmod_t step;
		/*dbg_instanceonlymod_t instanceonly;*/
	} u;

} bvmd_event_modifier_t;


/**
 * Contextual bag of data for a JDWP event.  Different fields are populated depending on needs of
 * the event.
 */
struct _bvmdeventcontextstruct {
	bvm_vmthread_t *vmthread;
	bvm_uint8_t eventkind;
	bvmd_location_t location;
	bvmd_location_t catch_location;
	bvmd_stepmod_t step;
	struct {
		bvm_throwable_obj_t *throwable;
		bvm_bool_t caught;
	} exception;
	bvm_field_t *field;
	struct _bvmdeventcontextstruct *next;
} /* bvmd_eventcontext_t*/ ;  /* forward declared in bvm.h */


typedef struct _dbgeventdefstruct {
	bvm_int32_t id;
	bvm_uint8_t eventkind;
	bvm_uint8_t suspend_policy;
	bvm_int32_t modifier_count;
	bvm_bool_t in_use;
	bvmd_event_modifier_t *modifiers;
	bvm_uint8_t breakpoint_opcode;
	struct _dbgeventdefstruct *next;
	struct _dbgeventdefstruct *nextvalid;
} bvmd_eventdef_t;

extern bvmd_eventdef_t *bvmd_gl_eventdefs;

#define BVMD_EventKind_ANY								0xFFFFFF
#define BVMD_EventKind_NONE								0x000000
#define BVMD_EventKind_SINGLE_STEP						0x000001
#define BVMD_EventKind_BREAKPOINT						0x000002
#define BVMD_EventKind_FRAME_POP						0x000004
#define BVMD_EventKind_EXCEPTION						0x000008
#define BVMD_EventKind_USER_DEFINED						0x000010
#define BVMD_EventKind_THREAD_START						0x000020
#define BVMD_EventKind_THREAD_DEATH						0x000040
#define BVMD_EventKind_CLASS_PREPARE					0x000080
#define BVMD_EventKind_CLASS_UNLOAD						0x000100
#define BVMD_EventKind_CLASS_LOAD						0x000200
#define BVMD_EventKind_FIELD_ACCESS						0x000400
#define BVMD_EventKind_FIELD_MODIFICATION				0x000800
#define BVMD_EventKind_EXCEPTION_CATCH					0x001000
#define BVMD_EventKind_METHOD_ENTRY						0x002000
#define BVMD_EventKind_METHOD_EXIT						0x004000
#define BVMD_EventKind_METHOD_EXIT_WITH_RETURN_VALUE	0x008000
#define BVMD_EventKind_MONITOR_CONTENDED_ENTER			0x010000
#define BVMD_EventKind_MONITOR_CONTENDED_ENTERED		0x020000
#define BVMD_EventKind_MONITOR_WAIT						0x040000
#define BVMD_EventKind_MONITOR_WAITED					0x080000
#define BVMD_EventKind_VM_START							0x100000
#define BVMD_EventKind_VM_DEATH							0x200000
#define BVMD_EventKind_VM_DISCONNECTED					0x400000

void bvmd_event_Exception(bvmd_eventcontext_t *context);
void bvmd_event_Breakpoint(bvmd_location_t *location);
void bvmd_event_Thread(bvmd_eventcontext_t *context);
void bvmd_event_ClassPrepare(bvmd_eventcontext_t *context);
bvm_bool_t bvmd_event_SingleStep(bvmd_eventcontext_t *context, bvm_bool_t do_breakpoint);

/*void dbg_event_ClassUnload(bvmd_eventcontext_t context);*/

void bvmd_event_VMStart();
void bvmd_event_VMDeath();

void bvmd_eventdef_clearall();
bvmd_eventcontext_t *bvmd_eventdef_new_context(bvm_uint8_t eventkind, bvm_vmthread_t *vmthread);
void bvmd_eventdef_free_context(bvmd_eventcontext_t *context);
void bvmd_do_event(bvmd_eventcontext_t *context);
void bvmd_send_parked_events(bvm_vmthread_t *vmthread);

bvm_native_ulong_t bvmd_step_get_position(int size, bvm_method_t *method, bvm_uint8_t *pc);

bvm_bool_t bvmd_breakpoint_opcode_for_location(bvmd_location_t *location, bvm_uint8_t *opcode);

void bvmd_event_park_unloaded_clazz(bvm_clazz_t *clazz);
void bvmd_event_send_parked_clazz_unloads();


#endif /* BVM_DEBUGGEREVENTS_H_ */
