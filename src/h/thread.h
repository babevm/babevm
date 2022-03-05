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

#ifndef BVM_THREADS_H_
#define BVM_THREADS_H_

/**
  @file

  Constants/Macros/Functions/Types for thread support.

  @author Greg McCreath
  @since 0.0.10

*/

#if BVM_DEBUGGER_ENABLE

struct _bvmdeventcontextstruct;
typedef struct _bvmdeventcontextstruct bvmd_eventcontext_t;

#endif

/**
 * A Java \c java.lang.Thread object.
 */
typedef struct _bvmthreadobjstruct {

	/** standard structure for any object */
	BVM_COMMON_OBJ_INFO

	/** Runnable object target (if any) of thread execution */
	bvm_obj_t *target;

	/** Thread name */
	bvm_string_obj_t *name;

	/** VM internal #bvm_vmthread_t structure */
	struct _bvmthreadstruct *vmthread;

	/** Thread id */
	bvm_cell_t id;

	/** Thread priority - standard Java priorities */
    bvm_cell_t priority;

	/** is a daemon thread */
    bvm_cell_t is_daemon;

	/** the inherited access control context, if any  */
	bvm_accesscontrolcontext_obj_t *inherited_context;

} bvm_thread_obj_t;

typedef struct bvm_find_exception_callback_data {
	bvm_throwable_obj_t *throwable;
	bvm_uint16_t depth;
	bvm_method_t *method;
	bvm_uint16_t handler_pc;
	bvm_bool_t caught;
} bvm_exception_location_data_t;


/**
 * A VM thread (not a java Thread).  This structure is the internal VM structure used to manage
 * a java thread.   The Java Thread class is not a final class and can be extended, so this is
 * where we put all our thread management stuff.
 */
typedef struct _bvmthreadstruct {

	/** The current state of the thread.  Note that BLOCKED may be or'd with WAITING and TIMED_WAITING to
	 * provide more info about the type of suspension */
	enum {
		BVM_THREAD_STATUS_NEW   	     = 1,
		BVM_THREAD_STATUS_RUNNABLE       = 2,
		BVM_THREAD_STATUS_BLOCKED        = 4,
		BVM_THREAD_STATUS_WAITING        = 8,
		BVM_THREAD_STATUS_TIMED_WAITING  = 16,
		BVM_THREAD_STATUS_DBG_SUSPENDED  = 32,
		BVM_THREAD_STATUS_TERMINATED     = 64
	} status;

	/** The Java Thread object being managed by this VM thread */
	bvm_thread_obj_t *thread_obj;

	/** The head of the stack list for this thread */
	bvm_stacksegment_t *stack_list;

	/** The stack register - the current stack segment */
	bvm_stacksegment_t *rx_stack;

	/** The stack pointer register - the position in the current stack that represents the top */
	bvm_cell_t *rx_sp;

	/** The program counter register - a pointer to the current thread bytecode */
	bvm_uint8_t   *rx_pc;

	/** The locals register - a pointer to the base of the current frame - also the same as where the
	 * frame local variable start */
	bvm_cell_t *rx_locals;

	/** The method register - the current executing method */
	bvm_method_t *rx_method;

	/** The number of bytecodes to execute for this thread before a thread switch occurs. */
	bvm_uint32_t timeslice;

	/** The Java object this vmthread is waiting on (if it is waiting).  May be \c NULL.  */
	// TODO: this is likely just the monitor, not the object - they seem to ber synonymous
	bvm_obj_t *waiting_on_object;

	/** When put into a WAITING state, this is the current lock depth here */
	bvm_uint16_t lock_depth;

	/** When in the timed callback list, this is the time to wake up */
	bvm_int64_t time_to_awake;

	/** Interruption flag as per the JVMS */
	bvm_bool_t is_interrupted;

	/** A pending exception, if any, as per the JVMS. \c NULL if there is no pending exception. */
	bvm_throwable_obj_t *pending_exception;

	/** A callback function pointer for when the thread exits the callback list */
	void (*callback)(struct _bvmthreadstruct *);

	/** The next thread in the global thread list #bvm_gl_threads. \c NULL if this is the last
	 * thread in the list. */
	struct _bvmthreadstruct *next;

	/** The next thread in whichever list this thread is in at the time, if any.  Can only be
	 * in one list at a time (runnable or callback) */
	struct _bvmthreadstruct *next_in_list;

	/** The next thread in whichever monitor queue this thread is in, if any.  Can only be
	 * in one queue at a time (lock or wait).  \c NULL if in no queue. */
	struct _bvmthreadstruct *next_in_queue;

	/** the found location (if any) of a thrown exception */
	bvm_exception_location_data_t exception_location;

#if BVM_DEBUGGER_ENABLE

	/** The number of times the debugger has suspended this thread */
	bvm_int16_t dbg_suspend_count;

	/** Whether the thread is at a breakpoint */
	bvm_bool_t dbg_is_at_breakpoint;

	/** If the thread is at a breakpoint, this is the opcode to execute when it resumes - bvm_int16_t so we can store a negative -1. */
	bvm_uint8_t dbg_breakpoint_opcode;

	/** List of event contexts to be sent when the thread resumes. \c NULL is none. */
	bvmd_eventcontext_t *dbg_parked_events;

	/** Whether the thread is single stepping*/
	bvm_bool_t dbg_is_stepping;

	/**
	 * Positional info about the thread when it went into step mode.
	 */
	struct {

		/** the stack depth at the time */
		bvm_uint16_t stack_depth;

		/** the executing method at the time */
		bvm_method_t *method;

		/** line or pc (depending of step size) (if class has no line numbers, will be 0)*/
		bvm_native_ulong_t position;

		/** JDWP size of step. BVM_MIN or LINE */
		bvm_uint32_t size;

		/** JDWP type of step - INTO, OVER, OUT */
		bvm_uint32_t depth;

	} dbg_step;


	bvm_bool_t is_exception_breakpoint;

#endif

} bvm_vmthread_t;

/**
 * A monitor structure for thread lock and wait management.
 */
typedef struct _bvmmonitorstruct {

	/** The object this monitor belongs to */
	bvm_obj_t *owner_object;

	/** The #bvm_vmthread_t that currently owns this monitor (\c NULL if none) */
	bvm_vmthread_t *owner_thread;

	/** The number of locks held on this monitor */
	bvm_uint32_t lock_depth;

	/** Head of a list of #bvm_vmthread_t locked on this monitor, if any. (\c NULL if none)*/
	bvm_vmthread_t *lock_queue;

	/** Head of a list of #bvm_vmthread_t waiting on this monitor, if any. (\c NULL if none) */
	bvm_vmthread_t *wait_queue;

	/** if the monitor is in use */
	bvm_bool_t in_use;

	/** Next entry in unused monitor cache, if any. (\c NULL if none)*/
	struct _bvmmonitorstruct *next;

} bvm_monitor_t;

extern bvm_vmthread_t *bvm_gl_threads;

extern bvm_vmthread_t *bvm_gl_thread_current;

extern bvm_uint32_t bvm_gl_thread_nondaemon_count;

extern bvm_int32_t bvm_gl_thread_timeslice_counter;

extern bvm_monitor_t *bvm_gl_thread_monitor_list;

extern bvm_uint32_t bvm_gl_thread_active_count;

/**
 * A structure passed to a #bvm_stack_visit_callback_t while traversing a thread's stack.  The information contained in
 * the struct is relevent to a single stack frame.
 */
typedef struct _bvmstackinfostruct {

	/** The #bvm_vmthread_t that owns the  stack */
	bvm_vmthread_t *vmthread;

	/** The method being executing by the frame */
	bvm_method_t *method;

	/** The program counter pointer for the frame */
	bvm_uint8_t *pc;

	/** The previous program counter for the frame */
	bvm_uint8_t *ppc;

	/** Pointer to frame 'locals' */
	bvm_cell_t *locals;

	/** the zero-based depth of the frame in the stack */
	int depth;

} bvm_stack_frame_info_t;

/** A function pointer type for a callback - used during thread stack visitation */
typedef bvm_bool_t (*bvm_stack_visit_callback_t)(bvm_stack_frame_info_t *, void *);


/* Thread priorities */

/** Java Thread priority MINIMUM. */
#define BVM_THREAD_PRIORITY_MIN     1
/** Java Thread priority NORMAL. */
#define BVM_THREAD_PRIORITY_NORM    5
/** Java Thread priority MAXIMUM. */
#define BVM_THREAD_PRIORITY_MAX     10

/**
 * A special program counter used to help determine when an executing thread has
 * reached the bottom of its execution stack - and thus finished executing.  Also
 * used when visiting a stack to know we've hit the bottom.
 */
#define BVM_THREAD_KILL_PC ((void *)1)

bvm_vmthread_t *bvm_thread_create_vmthread();
void bvm_thread_start(bvm_vmthread_t *thread_obj, bvm_bool_t push_run_method);
bvm_uint32_t bvm_next_thread_id();
void bvm_init_threading();

void bvm_thread_switch();
void bvm_do_thread_interrupt();
void bvm_thread_calc_timeslice(bvm_vmthread_t *vmthread);
bvm_bool_t bvm_thread_monitor_acquire(bvm_obj_t *obj, bvm_vmthread_t *vmthread);
void bvm_thread_wait(bvm_obj_t *obj, bvm_int64_t wait_time);
void bvm_thread_monitor_release(bvm_obj_t *obj);
void bvm_thread_notify(bvm_monitor_t *monitor, bvm_bool_t do_all);
void bvm_thread_interrupt(bvm_vmthread_t *vmthread);
bvm_uint32_t bvm_thread_get_next_id();
bvm_bool_t bvm_thread_is_alive(bvm_vmthread_t *vmthread);
void bvm_thread_sleep(bvm_int64_t wait_time);
bvm_obj_t *bvm_thread_terminated_callback(bvm_cell_t *res1, bvm_cell_t *res2, bvm_bool_t is_exception, void *data);
bvm_stacksegment_t *bvm_thread_create_stack(bvm_uint32_t height);
void bvm_thread_push_exceptionhandler(bvm_throwable_obj_t *throwable);
void bvm_thread_store_registers(bvm_vmthread_t *vmthread);
void bvm_thread_load_registers(bvm_vmthread_t *vmthread);
bvm_monitor_t *get_monitor_for_obj(bvm_obj_t *obj);

#if BVM_DEBUGGER_ENABLE

bvm_bool_t bvmd_check_thread_suspensions();

void bvmd_thread_suspend(bvm_vmthread_t *thread);
void bvmd_thread_resume(bvm_vmthread_t *thread);

void bvmd_thread_suspend_all();
void bvmd_thread_resume_all(bvm_bool_t force, bvm_bool_t clear_parked);

#endif

int bvm_stack_get_depth(bvm_vmthread_t *vmthread);
void bvm_stack_visit(bvm_vmthread_t *vmthread, int startframe, int endframe, int *counter, bvm_stack_visit_callback_t callback, void *data);

#endif /*BVM_THREADS_H_*/
