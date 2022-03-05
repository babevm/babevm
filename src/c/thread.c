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

  Everything to do with threads for the VM.

  @author Greg McCreath
  @since 0.0.10

  @section threads-overview Overview

  The VM has a full threads implementation including all locking and waiting semantics as per the JVMS.

  The threads implementation does not use native threads.  It is a preemptive 'green' threads implementation
  that operates above the OS - here, VM threads are independent of the underlying operating system threads even
  if it supports them.

  All threading is managed within the VM.  In the 'keep it simple' philosophy, this removes the need to port a threading
  implementations to for each platform and makes it all a lot easier to understand.

  Internally, the VM uses no native threads at all.   That is not to say that on some platform
  ports that (say) printing may not use a native thread to do its thing asynchronously.  The important thing
  is that to the java layer, it all looks the same.  Keep it simple.

  Not all of the Java threads API is implemented - however, most of it is there.  At this point, thread groups
  are not supported - they seems to offer nothing for a small platform.  Methods that specify nano-seconds are also
  not supported.  I imagine for most of the platforms this VM is ported to we'll be lucky to get a
  resolution less than a second.

  The notable omissions from standard Java threads are :

  @li ThreadGroups
  @li Reflective stuff like getting a list of all threads.
  @li All deprecated (and dangerous) stuff like stop/suspend/resume.

  ... but apart from that it's all to the letter of the JVMS/JDK.

  Each Java thread object has a handle to an internal VM thread (#bvm_vmthread_t) and vice-versa.  So when a
  Java Thread method is called that is native, the VM can translate the call from a Java thread
  object to a VM thread.  All VM operations concerning threads deal with the internal #bvm_vmthread_t objects.

  In this code unit, the only references to java thread objects are 1) to create a VM thread for a
  given Java Thread object, 2) to start a Java Thread object (we need to get its class and find
  the 'run' method), and 3) to create the bootstrap VM thread and its associated Thread Java object.

  In all instances, to find out how threads work in this VM at a Java level, you may refer to Java
  documentation for threading.

  @section thread-lifecycle Thread creation, startup, and termination

  VM threads are not created in isolation.  Every VM thread needs a Java thread as its buddy.  With the sole exception of
  the bootstrap thread, the VM never creates a VM thread to do something outside of Java.  After all, a VM thread is
  really just a means for a Java thread to execute bytecode.

  Of course, the reverse is not true, an executing Java program can create as many thread objects as it cares to.
  Actually, the VM does not care )or know) how many it <i>creates</i> - that is just Java bytecode.  The VM only cares
  when Java cares to <i>start</i> one. Starting a Java thread is when the VM becomes involved with it.

  Before any Java thread object can start executing bytecode it must have a VM thread associated with it.  VM
  threads manage the execution of the Java thread bytecode.

  All threads have a stack.  At thread startup, all threads have their 'run' method pushed onto the thread's
  stack.  This is the starting point for thread bytecode execution.  The 'run' method is chosen carefully.  If a
  thread has been instantiated with a target \c java.lang.Runnable object, the \c run() method of the \c Runnable
  object is pushed, otherwise the run method of the Thread (or subclass thereof) is pushed.  Special
  care is required here as the 'run' method itself may be synchronised.  If so, the VM must attempt to
  lock the about-to-run thread object's monitor (more later on this).  If it cannot, the thread is blocked and
  added to the thread object's monitor lock queue (more later ...) and will be dealt with like all other threads
  waiting on a monitor lock queue.

  The VM must know when a thread has finished executing.  And when it does it has to take action to clean it
  up and change status etc.  To do this, at thread startup the VM places a 'callback wedge' stack frame as the
  first frame of the thread's execution stack.  When the execution thread drops down to this special frame it
  calls the callback method to clean up the thread etc etc.  Bingo, it gets terminated and is no
  longer considered for selection for execution.

  A terminated thread has its stack memory freed straight away and does not wait for the next GC.  No real
  reasonfopr that - just that we know it is memory that is no longer used.

  For more information on how frames and thread startup works, see the frames documentation at \ref frame-overview.

  @section threads-bootstrap Bootstrap

  This is nice and simple.  At boot, the VM creates a single Thread object and (like every other thread) has
  the 'run' method pushed onto its execution stack.  Remember, the 'run' method of the Java Thread class actually does
  nothing.  But ... on top of that, the static 'main' method is then pushed.  Additionally, the Thread class
  initialiser method \c &lt;clinit&gt; is then pushed.

  VM execution thus begins with the Thread class initialisation, then 'main' method.

  After the 'main' method has finished, the frame-pop reinstates the 'run' method - which does nothing so it is also popped.
  Finally after that is the thread callback wedge frame as described above.

  See the frames documentation at \ref frame-overview.

  @section threads-monitors Object Monitors

  A monitor (#bvm_monitor_t) is used when an object becomes involved in synchronised activity.
  The monitor is the central control for things waiting on an object - be it for a sync lock, or for a wait.

  Whenever an object is the target of a sync block or synchronised method it will have a monitor allocated
  for it from the heap.

  The monitor contains important information.  Firstly, it has a handle to the thread that currently owns
  the monitor.  Only one thread at a time can own the monitor.  Refer to the Java JDK docs for more info
  on that.  The monitor also has a pointer to the object it is a monitor for.

  Additionally, the monitor contains two queues for VM threads.  One is a queue of threads that want to lock the
  monitor, (and therefore desire to have exclusive access to its associated object for threading purposes).  This is called
  the "lock queue".

  The other monitor queue is a queue for threads that are 'waiting' on it as a consequence of using the java Object.wait()
  method.  This is called the "wait queue".

  Threads waiting on an object never actually hold a lock on it, therefore a thread can only be be in one queue or the
  other - never both at the same time.  That is to say, the lock queue and the wait queue are mutually exclusive.

  The VM thread maintains a 'next_in_queue' member to point to the next thread in the same queue.  If it is the end of a queue
  (or not in one at all) this member will be \c NULL.

  When a thread 'wait' expires (if it indeed has a timeout), or the object has been the target of a Java \c notify()
  or \c notifyAll() the thread in question will attempt to lock the monitor and become its owner.  If
  it cannot do this, it simply goes into the monitor's lock queue and will be dealt with like all other
  threads in the lock queue - awaiting a chance to become the exclusive owner.

  The monitor also maintains the lock 'depth' as described in the JVMS.  It is incremented each time the
  current owner thread locks it, and decremented when is unlocks it.  A thread that issues a \c wait()
  on a monitor stores its current lock depth in the VM thread structure and is placed in the monitor's
  wait queue after relinquishing all lock(s) on it.  At some later stage when it again becomes the owner
  of the monitor the lock depth is restored (as per the JVMS).

  Monitors are cached.  When it is time to acquire a monitor, a cache (which is a simple linked list with its head at
  #bvm_gl_thread_monitor_list) is first inspected - if the cache is empty or no 'unused' monitors exist, a new monitor
  is allocated from the heap.  When that monitor is no longer used it will be emptied, marked as unused,
  and left in the list to be reused later.

  @section threads-threelists Three Thread Lists

  The VM maintains three lists of threads.

  The first list is simply a list of all threads.  As a thread is created is is added to this list.  This 'global'
  list is used by the GC to and scan and mark threads, or if they are terminated, remove them from the list.

  The other two lists are mutually exclusive and represent the 'runnable' threads and the 'timed callback'
  threads.  The VM thread structure has a field called #next_in_list.  This points to the next thread in
  the same list. It will be \c NULL if the thread in question is at the end of a list or not in one
  at all.

  The 'runnable' list contains only those threads that the VM can execute.  It does not have to make
  any decisions about threads in this list.  It gives each one execution time and cycles around them.

  Threads in the 'timed callback' list are waiting for a timeout so that they may execute a
  callback.  At this point, callbacks are only used for timed wait() invocation and for
  sleep() invocations.  When a thread goes into this list it has a 'time_to_wake' calculated
  for it.  When this time expires it is removed from the list and placed back into the runnable list.

  Likewise, a thread in the runnable list that becomes the subject of a sleep() or a timed wait()
  is removed from the runnable list and (with a calculated timeout) placed in the
  timed-callback list.

  Each time a thread switch occurs, the timed-callback list if traversed to see if any
  thread's time-to-wake has passed.  If so, the callback is executed.

  Sometimes, the runnable list may be exhausted while the timed-callback list is not.  In this case,
  the VM will keep spinning through the timed-callback list until a thread becomes runnable again.

  All threads in the runnable list will have the status #bvm_vmthread_t::BVM_THREAD_STATUS_RUNNABLE.  All threads in the
  timed-callback list will have the static #bvm_vmthread_t::BVM_THREAD_STATUS_BLOCKED.

  @section threads-status Thread Status

  There are four distinct VM thread statuses along with two status modifiers for blocked threads.

  @li #bvm_vmthread_t::BVM_THREAD_STATUS_NEW
  @li #bvm_vmthread_t::BVM_THREAD_STATUS_RUNNABLE
  @li #bvm_vmthread_t::BVM_THREAD_STATUS_BLOCKED
  @li #bvm_vmthread_t::BVM_THREAD_STATUS_TERMINATED

  All VM threads start life with a status of BVM_THREAD_STATUS_NEW.  Very quickly, they are moved to either
  BVM_THREAD_STATUS_RUNNABLE, or to BVM_THREAD_STATUS_BLOCKED.

  New threads become BVM_THREAD_STATUS_BLOCKED straight away if their 'run' method is synchronised and they cannot
  become the owner of their own monitor when they attempt to start.  Generally, new threads become
  BVM_THREAD_STATUS_RUNNABLE when started and are placed into the runnable list.

  Only BVM_THREAD_STATUS_BLOCKED threads may become BVM_THREAD_STATUS_RUNNABLE.

  When a thread is terminated it gets the BVM_THREAD_STATUS_TERMINATED status.  Nice and simple.  Only
  BVM_THREAD_STATUS_RUNNABLE threads may be terminated.

  The BVM_THREAD_STATUS_BLOCKED status needs a bit of explanation.  Any 'active' thread (one that is not
  BVM_THREAD_STATUS_NEW or BVM_THREAD_STATUS_TERMINATED) that is not BVM_THREAD_STATUS_RUNNABLE must be in
  a BVM_THREAD_STATUS_BLOCKED state.  A blocked thread will not appear in the runnable list, and *may* appear
  in the timed-callback list. Seems simple enough, but there actually, are three sorts of 'blocked'.

  Firstly, there is (I guess) 'vanilla' blocked which is when a thread is blocked waiting on a sync
  method or block.  No time-outs involved and therefore the thread is not in the timed-callback list. It
  will remain BVM_THREAD_STATUS_BLOCKED until it gets access to the monitor it seeks.

  The other blocked states are for threads that are waiting or sleeping - they are in the timed-callback list.
  They still have a status of BVM_THREAD_STATUS_BLOCKED but are or'd with the BVM_THREAD_STATUS_WAITING or
  BVM_THREAD_STATUS_TIMED_WAITING modifiers depending on the type of (erm) wait they are undertaking.

  According to the Java SDK Docs:

  BVM_THREAD_STATUS_WAITING is "Thread state for a waiting thread. A thread is in the waiting state due to
  calling one of the following methods"

  @li Object.wait with no timeout
  @li Thread.join with no timeout

  BVM_THREAD_STATUS_TIMED_WAITING is "Thread state for a waiting thread with a specified waiting time. A thread is
  in the timed waiting state due to calling one of the following methods with a specified
  positive waiting time:"

  @li Thread.sleep
  @li Object.wait with timeout
  @li Thread.join with timeout

  Additionally, an extra status is associated with remote debugging - #BVM_THREAD_STATUS_DBG_SUSPENDED.  This is
  set when a remote debugger requests a thread be suspended.

  @section threads-scheduling Scheduling

  The VM thread scheduler is very simple.  It works on a round-robin basis for those threads that
  are BVM_THREAD_STATUS_RUNNABLE.  Each thread is given a 'timeslice' which is calculated as
  'thread priority * thread_default_timeslice'.  Each thread will execute this number of bytecodes before
  a thread 'context' switch is requested.  The next available BVM_THREAD_STATUS_RUNNABLE thread is selected, the bytecode execution
  counter reset to the thread's calculated timeslice, and away it goes again.

  The #thread_default_timeslice global var is defaulted to the #BVM_THREAD_TIMESLICE compile time
  constant.

  At each thread switch, if the timed-callback list has any entries it is scanned to see if any threads in
  that list can have their callbacks executed (which *may* place them back into the runnable list).

  <i>NOTE : I guess this could be more efficient.  The VM could scan the list only when it knows their will be
  something to callback - each time a thread is added to the list a 'next visit time' could be
  calculated and the list would be ignored until then.  Keeping the list in 'time_to_wake' order could
  assist this.</i>

  Occasionally, the VM forces a thread switch by setting the bytecode execution counter to zero.  This means
  a thread switch will occur before the next bytecode is executed.

  @section threads-stacks Threads Stacks

  Each thread has it own stack.  Instead of taking the high road with stack sizes and creating each thread with
  a stack large enough to accommodate its most severe possible requirements - this VM does exactly the opposite - it
  creates each thread with the <i>smallest</i> possible stack and grows and shrinks it dynamically.

  The #bvm_stacksegment_t type represents a stack used by a thread.  Actually, the thread maintains a linked-list of them.  There
  is always at least one stack in the list for a non-terminated thread.  Instead of throwing a StackOverFlow error when
  the stack is exhausted, the VM creates a new stack and adds it to the end of the current stack.  Of course, if the
  current stack *already* has a 'next' one then that stack will be used if the new frame will fit in it - otherwise it
  will be discarded (and become GC'd) and a new stack created that *will* accommodate the new frame.

  The stack may grow and shrink as required.  Shrinking is performed during GC by truncating the list at the current stack
  in the list.  This is done by setting its 'next' pointer to \c NULL.  That means between GCs a thread will create a stack
  list as tall as it needs to and later during GC, if it is not used any more it will be cut back down to only
  what is being used.

  This approach gives the VM the ability to run many threads will little overhead.  A thread, its java object, and its stack
  can literally be as small as a couple of hundred bytes (in 32 bit).

  When a thread is terminated its stacks are immediately released back to the heap.  They will never be used again
  so they are freed straight away.

  Using dynamic stacks has implications for the what is stored per frame on the stack.  To support this, an
  extra element is in each stack frame - that element is a reference to the stack that the frame lives in.  This way, as
  a frame is popped, the stack it was in becomes the 'current' stack, just like the method in the popped frame becomes the
  'current' method.   There is a corresponding global register for the current stack called #bvm_gl_rx_stack.

  @section threads-daemons Daemon Threads

  The VM supports Daemon threads.  The VM will shut down when all non-daemon threads are terminated.
  Any Daemon threads left running at that stage are simply never given any more time and will cease
  bytecode execution.

  @section threads-uncaught Uncaught Exceptions

  Java 1.5 introduced the idea that uncaught exceptions in threads could be acted upon rather than
  just letting the thread die quietly.  This VM uses the Java SE defined means of catching and acting
  upon uncaught exception in threads, albeit with one difference - there are no threadgroups involved. When an
  exception is uncaught, the VM pushes the thread's 'dispatchUncaughtException' method onto the stack - and
  that is all.  The rest is bytecode.

*/

/** A pointer to the start of the global thread list.  This one-way linked list contains all threads and
 * is the root for GC thread scanning.
 */
bvm_vmthread_t *bvm_gl_threads = NULL;

/** A handle to the currently executing thread */
bvm_vmthread_t *bvm_gl_thread_current = NULL;

/** A handle to a list of threads awaiting a timed callback.  Threads that have been blocked
 * using \c Object.wait(x > 0) and \c Object.sleep(x) will have an entry here. */
static bvm_vmthread_t *thread_callback_list  = NULL;

/** A handle to the head of the list of runnable threads */
bvm_vmthread_t *bvm_gl_thread_runnable_list = NULL;

/** A handle to the head of a (cache) list of object monitors */
bvm_monitor_t *bvm_gl_thread_monitor_list = NULL;

/**
 * The default unit of timeslice given to a thread.  This is multiplied against the thread priority to
 * get the thread timeslice allocation to get an actual timeslice for a thread.
 */
static bvm_uint32_t thread_default_timeslice = BVM_THREAD_TIMESLICE;

/** Counter of all the active (daemon and non-daemon) threads.  Where active = started and not
 * yet terminated */
bvm_uint32_t bvm_gl_thread_active_count = 0;

/** Counter of the number of non-daemon threads that have started but not yet terminated */
bvm_uint32_t bvm_gl_thread_nondaemon_count = 0;

/** Counter that counts down to zero with each bytecode executed to determine when a thread switch takes
 * place.  Each thread switch will reset this value to the switched-in thread's timeslice setting.
 */
bvm_int32_t bvm_gl_thread_timeslice_counter;

/** As each thread is created it gets an id and it comes from here. */
static bvm_uint32_t thread_next_id = 0;

#if BVM_DEBUGGER_ENABLE
/** How many thread switches to do before checking if any data is ready for reading from the debugger */
static int bvmd_interaction_counter = BVM_DEBUGGER_INTERACTION_COUNT;
#endif

/**
 * Create a stack for a thread.  The new stack is allocated from the heap and will have its
 * \c top calculated and its \c next pointer set to \c NULL.
 *
 * @param height - the height in cells of the new stack (note: NOT in bytes).
 *
 * @return a pointer to a newly created #bvm_stacksegment_t.
 */
bvm_stacksegment_t *bvm_thread_create_stack(bvm_uint32_t height) {

	bvm_stacksegment_t *newstack = bvm_heap_alloc(sizeof(bvm_stacksegment_t) + ( height * sizeof(bvm_cell_t)), BVM_ALLOC_TYPE_DATA);

 	newstack->height = height;
	newstack->top    = newstack->body + height;
	newstack->next   = NULL;

	return newstack;
}

/**
 * Create a VM thread from a given Java thread object.  Establishes the VM thread to wrap
 * the Java thread.  The VM thread is created with a new stack.  The size of the stack is determined
 * from the global #bvm_gl_stack_height variable - this in turn is defaulted from #BVM_THREAD_STACK_HEIGHT.
 *
 * The new VM thread is created with a state of #bvm_vmthread_t::BVM_THREAD_STATUS_NEW and added at the head of the
 * global list of threads pointed to by #bvm_gl_threads.
 *
 * Note that this does NOT start a thread.  Threads do not start until explicitly started by
 * #bvm_thread_start().
 *
 * @param thread_obj - the Java thread object to create a VM thread for.
 *
 * @return a new vm thread
 *
 */
bvm_vmthread_t *bvm_thread_create_vmthread(bvm_thread_obj_t *thread_obj) {

	bvm_vmthread_t *vmthread;

	BVM_BEGIN_TRANSIENT_BLOCK {

		/* create memory for the new VM thread */
		vmthread = bvm_heap_calloc(sizeof(bvm_vmthread_t), BVM_ALLOC_TYPE_DATA);

		/* each new VM thread is created with the following state */
		vmthread->status = BVM_THREAD_STATUS_NEW;

		/* just in case the following stack list allocation GC's */
		BVM_MAKE_TRANSIENT_ROOT(vmthread);

		/* create the first stack segment for the new thread using the default height */
		vmthread->stack_list = bvm_thread_create_stack(bvm_gl_stack_height);

	} BVM_END_TRANSIENT_BLOCK

	/* default the current working stack to the start of the stack list */
	vmthread->rx_stack = vmthread->stack_list;

	/* set the thread stack pointer to the base of its stack and initialise the
	 * base of the current (first) frame. */
	vmthread->rx_sp = &(vmthread->rx_stack->body[0]);
	vmthread->rx_locals = vmthread->rx_sp;

	/* point the vmthread to the java thread */
	vmthread->thread_obj = thread_obj;

	/* ... and vice versa */
	thread_obj->vmthread = vmthread;

	/* set the timeslice for the new thread */
	bvm_thread_calc_timeslice(vmthread);

	/* add the new VM thread to the front of global VM threads list */
	vmthread->next = bvm_gl_threads;
	bvm_gl_threads = vmthread;

	return vmthread;
}

/**
 * Scan the timed callback list looking for anything that has timed out.  The list is traversed
 * front to back and all threads with expired timers have their callback function invoked.
 * It is the responsibility of the callback to remove the thread from the timed callback list.
 *
 * This function is not called if there is nothing in the timed callback list so it
 * assumes the list is not \c NULL.
 *
 */
static void resume_callback_timeouts() {

    // TODO: Not sure if I like the idea that callbacks remove it from the list.  Maybe check that out.

	bvm_vmthread_t *next, *vmthread;
	bvm_int64_t current_time;

	vmthread = thread_callback_list;
	current_time = bvm_pd_system_time();

	do {

		/* keep a handle to the next in the list.  If we do a callback that removes the thread from
		 * the list we'll be in trouble */
		next = vmthread->next_in_list;

		/* if the time has passed for the waiting thread, do the callback */
		if (BVM_INT64_compare_le(vmthread->time_to_awake,current_time)) {
			vmthread->callback(vmthread);
		}

		/* go to the next thread */
		vmthread = next;

		/* the callback will have removed the thread from the waiting list and also, if the thread was the only
		 * one in the list, the callback may have set the gl_waiting_threads to NULL or, if was the first
		 * on the list, moved it on to the next one. So we have to be careful about what gl_waiting_threads
		 * really represents each time we loop here. */

	} while ( vmthread != NULL);
}

/**
 * Handle a pending thread interrupt for the current executing thread.  If the thread had a pending
 * exception, that exception is thrown, otherwise #BVM_ERR_INTERRUPTED_EXCEPTION is thrown.
 *
 * Before the exception is thrown, the interrupted status of the thread is cleared.
 *
 */
void bvm_do_thread_interrupt() {
	bvm_throwable_obj_t *th = bvm_gl_thread_current->pending_exception;
	if (th == NULL) th = bvm_create_exception_c(BVM_ERR_INTERRUPTED_EXCEPTION, NULL);
	bvm_gl_thread_current->is_interrupted = BVM_FALSE;
	bvm_gl_thread_current->pending_exception = NULL;
	BVM_THROW(th);
}

/**
 * Switch from the current thread to the next runnable thread.
 *
 * The timeslice counter will be set to the timeslice for the selected thread.  If the current thread is
 * the only runnable thread it will remain current and, of course,  the thread timeslice
 * counter is reset.
 *
 * Each time this function is called the timed callback list is scanned (if there is anything in it).
 *
 * TODO:  A more efficient means of checking the callback list is required.  Maybe every 'n' times
 * this function is called might be a simple way.  A better way would be to only check it when sure there is
 * something to resume - like doing a smart thing when adding something to the list.
 */
void bvm_thread_switch() {

	bvm_vmthread_t *vmthread;

#if BVM_DEBUGGER_ENABLE
top:
#endif

	vmthread = NULL;

	/* If there are callback threads, have a look at them first to see if any can be resumed */
	if (thread_callback_list != NULL) {
		resume_callback_timeouts();

		/* If we have attempted to wake threads and found that afterwards there no runnable
		 * threads, we'll spin on the waiting ones until something happens */
		while ( (bvm_gl_thread_runnable_list == NULL) && (thread_callback_list != NULL) )
			resume_callback_timeouts();
	}

#if BVM_DEBUGGER_ENABLE
	/* After resuming any callback threads there may be, we'll interact with the debugger to see if there is
	 * anything to do.  This of course may have the side effect of suspending threads, and indeed the VM. */
	if (bvmd_is_session_open()) {
		if (bvmd_is_vm_suspended()) {
			bvmd_spin_on_debugger();
		} else {

			/* if any clazzes have been unloaded, send their events off */
			if (bvmd_gl_unloaded_clazzes != NULL) {

				bvmd_event_send_parked_clazz_unloads();

				/* if the VM has become suspended by this debugger interaction, start at the top again ... */
				if (bvmd_is_vm_suspended()) goto top;
			}

			/* check for a request from the debugger "every so often".  The \c BVM_DEBUGGER_INTERACTION_COUNT variable
			 * is used as a counter to determine how many thread switches occur between debugger checks */
			if (bvmd_interaction_counter-- == 0) {

				/* reset the "every so often" counter */
				bvmd_interaction_counter  = BVM_DEBUGGER_INTERACTION_COUNT;

				/* check for debugger interactions */
				bvmd_interact(0);

				/* if the VM has become suspended by this debugger interaction, start at the top again ... */
				if (bvmd_is_vm_suspended()) goto top;
			}
		}
	}
#endif

	/* what!  Have we managed to get to this point and have no threads left to run?  Nasty. */
	if ( (bvm_gl_thread_runnable_list == NULL) && (thread_callback_list == NULL) )
		BVM_VM_EXIT(BVM_FATAL_ERR_NO_RUNNABLE_OR_WAITING_THREADS, NULL);

	/* if the current thread has not been blocked (which means it is still in the runnable list)
	 * get the next one in the list. */
	if (bvm_gl_thread_current->status == BVM_THREAD_STATUS_RUNNABLE)
		vmthread = bvm_gl_thread_current->next_in_list;

	/* ... no joy? just go to head of the runnable list */
	if (vmthread == NULL) vmthread = bvm_gl_thread_runnable_list;

	/* If we are changing threads save the current thread state and restore the switched-to
	 * thread's state */
	if (vmthread != bvm_gl_thread_current) {
		bvm_thread_store_registers(bvm_gl_thread_current);
		bvm_thread_load_registers(vmthread);
		bvm_gl_thread_current = vmthread;
	}

	/* Reset the thread timeslice counter.  Even if we did not actually change threads (the current
	 * thread is the only running thread) we will still reset this.  The result being that the
	 * timeslice counter is reset every thread switch request. */
	bvm_gl_thread_timeslice_counter = vmthread->timeslice;

#if BVM_DEBUGGER_ENABLE
	/* if the switched-to thread has parked debug events, send them.  Being careful here
	 * because sending a debug event *may* cause the switched-to thread or the VM to become suspended!
	 * If this is the case, we'll just go back to the top of thread-switch and try for another
	 * thread.*/
	if ( (bvmd_is_session_open() && (bvm_gl_thread_current->dbg_parked_events != NULL) )) {
		bvmd_send_parked_events(bvm_gl_thread_current);
		if (bvm_gl_thread_current->status != BVM_THREAD_STATUS_RUNNABLE) goto top;
	}
#endif

	/* if the switched-to thread has a pending exception, deal with it.  The exception will be
	 * thrown from bvm_do_thread_interrupt() call */
	if (bvm_gl_thread_current->pending_exception != NULL) {
		bvm_do_thread_interrupt();
	}
}

/**
 * Remove a thread from a list.  If the thread is not in the list this function has no effect.
 *
 * @param list the list to remove from
 * @param vmthread the thread to remove
 */
static void remove_thread_from_list(bvm_vmthread_t **list, bvm_vmthread_t *vmthread) {

	if (*list == vmthread) {

		/* the thread is actually the head of the list */
		*list = vmthread->next_in_list;
		vmthread->next_in_list = NULL;
	}
	else {

		/* the thread is not the head of the list */

		bvm_vmthread_t *th = *list;

		while ( (th != NULL) ) {

			if (th->next_in_list == vmthread) {
				th->next_in_list = vmthread->next_in_list;
				vmthread->next_in_list = NULL;
				break;
			}

			th = th->next_in_list;
		}
	}
}

/**
 *  Put a thread into a given list of threads.  Used to place a thread into either
 *  the runnable list or the callback list.
 *
 * @param list the list to put the threa into
 * @param vmthread the thread
 */
static void put_thread_in_list(bvm_vmthread_t **list, bvm_vmthread_t *vmthread) {

	if (*list == NULL) {

		/* list is empty, so make it the head */
		*list = vmthread;
		vmthread->next_in_list = NULL;
	}
	else {

		/* list is not empty, but still insert it at the front */
		vmthread->next_in_list = (*list)->next_in_list;
		(*list)->next_in_list = vmthread;
	}
}


/**
 * The callback from the interp loop when a thread reaches the bottom of its execution stack.  Will cause
 * a thread switch to the next runnable thread.  The exited thread has it status set
 * to #bvm_vmthread_t::BVM_THREAD_STATUS_TERMINATED and its stack memory freed. Any other thread waiting on this thread's
 * monitor are notified.
 *
 * Params unused in termination callback.
 *
 * @param res1
 * @param res2
 * @param is_exception
 * @param data
 *
 * @return \c NULL.
 *
 * @throws #BVM_ERR_ILLEGAL_THREAD_STATE_EXCEPTION if the thread is not in the #bvm_vmthread_t::BVM_THREAD_STATUS_RUNNABLE state.
 */
bvm_obj_t *bvm_thread_terminated_callback(bvm_cell_t *res1, bvm_cell_t *res2, bvm_bool_t is_exception, void *data) {

    /* Params unused in termination callback. */
    UNUSED(res1);
    UNUSED(res2);
    UNUSED(is_exception);
    UNUSED(data);

	bvm_vmthread_t *vmthread;
	vmthread = bvm_gl_thread_current;

	/* only runnable threads may be terminated */
	if (vmthread->status != BVM_THREAD_STATUS_RUNNABLE)
		bvm_throw_exception(BVM_ERR_ILLEGAL_THREAD_STATE_EXCEPTION, NULL);

	/* reduce the counter for total number of running threads */
	bvm_gl_thread_active_count--;

	/* if it is a non-daemon thread, reduce the non-deamon thread count */
	if (!vmthread->thread_obj->is_daemon.int_value)
		bvm_gl_thread_nondaemon_count--;

	/* all terminated threads have a TERMINATED status */
	vmthread->status = BVM_THREAD_STATUS_TERMINATED;

	/* remove the thread from the runnable list*/
	remove_thread_from_list(&bvm_gl_thread_runnable_list, vmthread);

	/* pop the callback frame */
	bvm_frame_pop();

	/* free up stack thread memory straight away - it will never be used
	 * again. We'll traverse the thread's stack list from the front and free them all ...*/
	{
		bvm_stacksegment_t *current, *next;

		/* get a handle to the start of the list */
		current = vmthread->stack_list;

		/* set the head of the list to null */
		vmthread->stack_list = NULL;

		/* loop over each stack in the list and free it */
		while (current != NULL) {
			next = current->next;
			bvm_heap_free(current);
			current = next;
		}
	}

	/* reset timeslice counter to cause a thread switch on return to the top of the interp loop */
	bvm_gl_thread_timeslice_counter = 0;

	bvm_monitor_t *monitor = get_monitor_for_obj((bvm_obj_t *) vmthread->thread_obj);

	/* notify everything waiting on this thread's held monitor (if it has one) */
	if (monitor != NULL) {
		bvm_thread_notify(monitor, BVM_TRUE);
	}

#if BVM_DEBUGGER_ENABLE
	if (bvmd_is_session_open()) {

		bvmd_eventcontext_t *context = bvmd_eventdef_new_context(JDWP_EventKind_THREAD_DEATH, vmthread);
		/* note we do not use 'bvmd_do_event' with thread death. It just goes straight to the debugger at
		 * any time */
		bvmd_event_Thread(context);

		/* after this one has died, check that we are not now in a fully-suspended state (meaning, all
		 * remaining threads are suspended */
		bvmd_check_thread_suspensions();

	}
#endif

	/* no return value required for a thread termination callback */
	return NULL;
}

/**
 * Resume execution of a blocked thread. The given thread must be in a blocked state or
 * an IllegalThreadStateException will be thrown.
 *
 * The given thread will be placed into the runnable thread list and have its status set
 * to #bvm_vmthread_t::BVM_THREAD_STATUS_RUNNABLE.
 *
 * Note this function does not attempt to remove the thread from any list.  A blocked thread does
 * not necessarily have to be in the time-callback list - it could just be waiting on a monitor.
 *
 * @param vmthread the thread to resume.
 * @throws #BVM_ERR_ILLEGAL_THREAD_STATE_EXCEPTION if the thread is not in the #bvm_vmthread_t::BVM_THREAD_STATUS_BLOCKED state.
 */
static void thread_resume(bvm_vmthread_t *vmthread) {

	/* JVMS doesn't say to throw this, just seems reasonable for now.  At least we'll know that
	 * we're not attempting to put the thing into the runnable list more than once */
	if ( (vmthread->status & BVM_THREAD_STATUS_BLOCKED) == 0)
		bvm_throw_exception(BVM_ERR_ILLEGAL_THREAD_STATE_EXCEPTION, NULL);

#if BVM_DEBUGGER_ENABLE
	/* if the thread is blocked by debugger, do not resume */
	if (vmthread->status & BVM_THREAD_STATUS_DBG_SUSPENDED) {
		return;
	}
#endif

	/* put the thread into the front of the runnable list (just after the current thread).
	 * We don't switch to it, it'll get there in its own time.
	 * TODO.  Perhaps put at the *end* of the list - this could (might?) cause a thrashing
	 * situation where other threads are starved - would not matter if thread switch was a little
	 * more random in thread selection . */
	put_thread_in_list(&bvm_gl_thread_runnable_list, vmthread);

	/* set it now to have the status that all threads in the runnable list have */
	vmthread->status = BVM_THREAD_STATUS_RUNNABLE;

#if BVM_DEBUGGER_ENABLE
	/* if we are resuming a thread, they can no longer all be suspended */
	bvmd_gl_all_suspended = BVM_FALSE;
#endif
}


/**
 * Block a runnable thread.  The given thread must be a runnable thread or an
 * IllegalThreadStateException will be thrown.
 *
 * The given thread is removed from the runnable threads list and given a status
 * of #bvm_vmthread_t::BVM_THREAD_STATUS_BLOCKED.
 *
 * If the given thread is the current thread, the bytecode timeslice counter is set to zero
 * to cause a thread swap before the next bytecode is executed.
 *
 * @param vmthread
 * @throws #BVM_ERR_ILLEGAL_THREAD_STATE_EXCEPTION if the thread is not in the #bvm_vmthread_t::BVM_THREAD_STATUS_RUNNABLE state.
 */
static void thread_block(bvm_vmthread_t *vmthread) {

	/* a thread must be in a runnable state when we get to here */
	if (vmthread->status != BVM_THREAD_STATUS_RUNNABLE)
		bvm_throw_exception(BVM_ERR_ILLEGAL_THREAD_STATE_EXCEPTION, NULL);

	/* remove the thread from the runnable list */
	remove_thread_from_list(&bvm_gl_thread_runnable_list, vmthread);

	vmthread->status = BVM_THREAD_STATUS_BLOCKED;

	/* if we just blocked the current thread, expire the thread's timeslice counter - this
	 * will cause the interpreter loop to switch threads  */
	if (vmthread == bvm_gl_thread_current)
		bvm_gl_thread_timeslice_counter = 0;

#if BVM_DEBUGGER_ENABLE
	/* check and see if all threads are now suspended */
	bvmd_check_thread_suspensions();
#endif
}

/**
 * Given a new VM thread, set it up to run the correct thread \c run() method and attempt to get it started.
 *
 * Two frames are pushed onto the stack.  The first is a callback wedge that, when reached, signals that the thread
 * has finished execution, and the second is (optionally) the java \c run() method to execute.
 *
 * If the \c run() method is not synchronised, the thread status is set momentarily to #bvm_vmthread_t::BVM_THREAD_STATUS_BLOCKED,
 * and then usual blocked-thread resumption code kicks in to unblock it and add it to the runnable list
 * (all using #thread_resume).
 *
 * If the run method is synchronised, an attempt is made to gain the monitor for the thread object.  If it can be
 * acquired, the same process for an synchronised \ run() method happens and the thread ends up being runnable and in the
 * runnable list.  If the monitor cannot be acquired, the thread enters a blocked state and waits on the monitor
 * like any other blocked thread waiting on a monitor.
 *
 * @param vmthread - the thread to start
 * @param push_run_method - if BVM_FALSE, do thread startup but do not push the 'run' method as described above - just the
 * callback wedge.
 * @throws #BVM_ERR_ILLEGAL_THREAD_STATE_EXCEPTION if the thread is not in the #bvm_vmthread_t::BVM_THREAD_STATUS_NEW state.
 */
void bvm_thread_start(bvm_vmthread_t *vmthread, bvm_bool_t push_run_method) {

	bvm_thread_obj_t *thread_obj;
	bvm_method_t *runmethod = NULL;
	bvm_bool_t runnable;

	thread_obj = vmthread->thread_obj;

	/* thread must be NEW otherwise throw an exception */
	if (vmthread->status != BVM_THREAD_STATUS_NEW)
		bvm_throw_exception(BVM_ERR_ILLEGAL_THREAD_STATE_EXCEPTION, NULL);

	if (push_run_method) {

		/* get the Thread class 'run' method */
		runmethod = bvm_clazz_method_get( (bvm_instance_clazz_t *) thread_obj->clazz,
								 bvm_utfstring_pool_get_c("run", BVM_TRUE),
								 bvm_utfstring_pool_get_c("()V", BVM_TRUE),
								 (BVM_METHOD_SEARCH_CLAZZ | BVM_METHOD_SEARCH_SUPERS ) );

		/* no 'run' method?.  Nasty.  The JVMS doesn't actually say what should happen here - but I'm
		 * guessing a BVM_ERR_NO_SUCH_METHOD_ERROR is appropriate */
		if (runmethod == NULL)
			bvm_throw_exception(BVM_ERR_NO_SUCH_METHOD_ERROR, "run");
	}

	/* to get the thread all set up for a start we push the callback wedge and then the run method.  To do this, we
	 * momentarily make the new thread's registers global (and restore the old ones afterwards). */
	{
		/* store current thread registers and load new thread registers.  Note if the VM is
		 * just starting the current registers will be nonsense, so do not do it - this test only really
		 * matter once. */
		if (bvm_gl_vm_is_initialised) {
			bvm_thread_store_registers(bvm_gl_thread_current);
		}

		/* load the given thread's register to the global set (momentarily) */
		bvm_thread_load_registers(vmthread);

		/* push a frame onto the bottom of the stack that will let us know that the thread has finished.  We'll make
		 * it a special frame that has a VM callback. */
		bvm_frame_push(BVM_METHOD_CALLBACKWEDGE, bvm_gl_rx_sp, bvm_gl_rx_pc, bvm_gl_rx_pc, NULL);
		bvm_gl_rx_locals[0].ref_value = NULL;
		bvm_gl_rx_locals[1].callback = bvm_thread_terminated_callback;
		bvm_gl_rx_locals[2].ref_value = NULL;

		if (push_run_method) {
			/* next we'll push the run method onto the stack and push 'self' into the first local.  We make sure we
			 * capture the sync object if the run method is synchronised.  The addition of the return pc
			 * being BVM_THREAD_KILL_PC signals this callback is to terminate a thread */
			bvm_frame_push(runmethod, bvm_gl_rx_sp, bvm_gl_rx_pc, BVM_THREAD_KILL_PC, BVM_METHOD_IsSynchronized(runmethod) ? (bvm_obj_t *) thread_obj : NULL);
			bvm_gl_rx_locals[0].ref_value = (bvm_obj_t *) thread_obj;
		}

		/* store new thread's registers */
		bvm_thread_store_registers(vmthread);

		/* reload the current thread registers.  */
		if (bvm_gl_vm_is_initialised) {
			bvm_thread_load_registers(bvm_gl_thread_current);
		}
	}

	if (push_run_method) {

		/* If the run method of the thread is synchronised we'll try to get the thread object's monitor before we proceed.
		 * Before we do so, we'll set the status to runnable so the rest of the thread logic doesn't scream at the NEW status.
		 * If we can get the monitor we'll resume the thread and all is good, if we can not get it, we'll leave the new thread
		 * as blocked (as it will be if it fails to acquire a monitor in bvm_thread_monitor_acquire() ) */
		vmthread->status = BVM_THREAD_STATUS_RUNNABLE;
		runnable = BVM_METHOD_IsSynchronized(runmethod) ? bvm_thread_monitor_acquire( (bvm_obj_t *) thread_obj, vmthread) : BVM_TRUE;

		if (runnable) {
			/* momentarily set the thread to blocked.  The thread is not *really* blocked, this is just to allow usage of the
			 * thread_resume() function and maintain the 'only blocked threads may be resumed' invariant */
			vmthread->status = BVM_THREAD_STATUS_BLOCKED;

			/* ... and resume it from the blocked state to the runnable state */
			thread_resume(vmthread);
		} else {
			/* this blocked thread will never again execute code to attempt to reacquire the monitor. so when
			 * it reached the head of the lock queue and finally gets to be the owner of the monitor, it will
			 * have a lock count of 1. */
			vmthread->lock_depth = 1;
		}
	} else {
		/* momentarily set the thread to blocked.  The thread is not *really* blocked, this is just to allow usage of the
		 * thread_resume() function and maintain the 'only blocked threads may be resumed' invariant */
		vmthread->status = BVM_THREAD_STATUS_BLOCKED;

		/* ... and resume it from the blocked state to the runnable state */
		thread_resume(vmthread);
	}

	bvm_gl_thread_active_count++;

    /* keep a count of the non-daemon threads - VM exits when last non-daemon thread exits */
	if (!thread_obj->is_daemon.int_value)
		bvm_gl_thread_nondaemon_count++;

#if BVM_DEBUGGER_ENABLE
	if (bvmd_is_session_open()) {
		bvmd_eventcontext_t *context = bvmd_eventdef_new_context(JDWP_EventKind_THREAD_START, vmthread);
		bvmd_do_event(context);
	}
#endif
}


/**
 * Add a thread to the end of a queue.
 *
 * @param queue the thread queue to add a thread to
 * @param vmthread the thread to add
 */
static void add_thread_to_queue(bvm_vmthread_t **queue, bvm_vmthread_t *vmthread) {

	if (*queue == NULL) {
		/* make it the head of the list */
		*queue = vmthread;
	}
	else {

		/* list exists, add it to the end */

		bvm_vmthread_t *head = *queue;
		while (head->next_in_queue != NULL)
			head = head->next_in_queue;
		head->next_in_queue = vmthread;
	}

	vmthread->next_in_queue = NULL;
}

/**
 * Gets a monitor from created monitor list, or creates a new one at the end if
 * the cache is empty.
 *
 * @return a bvm_monitor_t*
 */
static bvm_monitor_t *get_unused_monitor() {

	bvm_monitor_t *monitor = bvm_gl_thread_monitor_list;

	if (bvm_gl_thread_monitor_list == NULL) {
		/* list is empty, create the first one - note the static type - we'll manage it ourselves and it'll
		 * be not GC'd */
		bvm_gl_thread_monitor_list = monitor = bvm_heap_calloc(sizeof(bvm_monitor_t), BVM_ALLOC_TYPE_STATIC);
	} else {

		/* list is not empty, loop through to find an unused one.  If all are used,
		 * create a new one at the end of the list.
		 */

		bvm_monitor_t *last = monitor;

		while (monitor != NULL) {
			if (!monitor->in_use) break;
			last = monitor;
			monitor = monitor->next;
		}

		/* none free, create one at the end */
		if (monitor == NULL) {
			last->next = monitor = bvm_heap_calloc(sizeof(bvm_monitor_t), BVM_ALLOC_TYPE_STATIC);
		}
	}

	return monitor;
}

/**
 * Scan the monitor list for a matching object.  If none found, returns \c NULL
 *
 * @param obj
 * @return the bvm_monitor_t for the given object, or \c NULL if not found.
 */
bvm_monitor_t *get_monitor_for_obj(bvm_obj_t *obj) {

	bvm_monitor_t *monitor = bvm_gl_thread_monitor_list;

    while (monitor != NULL) {
        if (monitor->in_use && monitor->owner_object == obj) {
            break;
        }
        monitor = monitor->next;
    }

    return monitor;
}

/**
 * Tests to determine if a monitor is unused by the object that it is associated with.  If
 * it is unused, object and monitor are disassociated and the monitor is placed in the monitor
 * cache.
 */
static void cache_monitor_if_unused(bvm_monitor_t *monitor) {

	if (monitor != NULL) {
		if ( (monitor->lock_queue == NULL) &&
			 (monitor->wait_queue == NULL) &&
			 (monitor->owner_thread == NULL)) {

		        // note that this is in a list
                bvm_monitor_t *next = monitor->next;

				/* clear the monitor - also has the effect of setting 'in_use' to BVM_FALSE */
				memset(monitor,0,sizeof(bvm_monitor_t));

				// restore list pointer
				monitor->next = next;
			}
	}
}

/**
 * Calculate and set the timeslice for a given thread.  A thread's timeslice is its priority multiplied
 * by the #thread_default_timeslice var - this gives the number of bytecodes to execute
 * before a thread switch.
 *
 * @param vmthread the thread to set the timeslice of.
 */
void bvm_thread_calc_timeslice(bvm_vmthread_t *vmthread) {
	vmthread->timeslice = vmthread->thread_obj->priority.int_value * thread_default_timeslice;
}


/**
 * Remove a thread from the wait queue of the object it is waiting on ...
 *
 * @param vmthread the thread to remove
 */
static void remove_from_wait_queue(bvm_vmthread_t *vmthread) {
	bvm_monitor_t *monitor = get_monitor_for_obj(vmthread->waiting_on_object);

	/* if this thread is the head of the list move the head, otherwise scan the queue */
	if (monitor->wait_queue == vmthread) {
		monitor->wait_queue = vmthread->next_in_queue;
	} else {
		bvm_vmthread_t *current;
		current = monitor->wait_queue;
		while ( (current->next_in_queue != NULL) && (current->next_in_queue != vmthread))  {
			current = current->next_in_queue;
		}

		if (current->next_in_queue == vmthread)
			current->next_in_queue = vmthread->next_in_queue;
	}

	vmthread->waiting_on_object = NULL;
}

/**
 * Given a monitor, promote one of the threads in its lock queue to become the new owner of
 * the monitor.  Unlike the JVMS which says it should be 'random', this just picks the first
 * one off the lock queue and promotes it.
 *
 * If the lock queue is empty, the owner of the monitor is set to \c NULL to indicate that
 * the monitor is not owned.
 *
 * @param monitor the monitor to act upon.
 */
static void promote_thread_in_lock_queue(bvm_monitor_t *monitor) {

	if (monitor->lock_queue == NULL) {
		monitor->owner_thread = NULL;
	} else {

		bvm_vmthread_t *new_owner = monitor->lock_queue;

		/* the first entry of the lock queue will now own the monitor */
		monitor->owner_thread = new_owner;

		/* move the start of the lock queue along to the next in the list (may be NULL) */
		monitor->lock_queue = new_owner->next_in_queue;

		/* restore the previous depth if there is one.*/
		monitor->lock_depth = (new_owner->lock_depth > 0) ? (new_owner->lock_depth) : 0;

		/* zero the lock depth of the new owner thread */
		new_owner->lock_depth = 0;

		/* get the thread back into the runnable list by resuming it */
		thread_resume(new_owner);
	}
}

/**
 * Add a thread to the timed callback list and calculate a time for it to be woken up to make its
 * callback.
 *
 * @param vmthread the thread to add to the callback list
 * @param wait_time the time to wait
 * @param callback a function pointer to a callback.
 */
static void establish_timed_callback(bvm_vmthread_t *vmthread, bvm_int64_t wait_time, void (*callback)(bvm_vmthread_t *)) {

	vmthread->callback = callback;

	/* less than zero?  Make it zero */
	if (BVM_INT64_zero_lt(wait_time)) BVM_INT64_setZero(wait_time);

	/* if the wait time is not zero we'll add the platform time to it to get the
	 * wake up time */
	if (!BVM_INT64_zero_eq(wait_time)) {
		bvm_int64_t ptime = bvm_pd_system_time();
		BVM_INT64_increase(wait_time, ptime);
	}

    vmthread->time_to_awake = wait_time;

	/* add to the waiting list at the end.  No reason, just seems reasonable to add it at the end */
	put_thread_in_list(&thread_callback_list, vmthread);
}

/**
 * Resume a thread from a timed wait.  The given thread must be blocked with #bvm_vmthread_t::BVM_THREAD_STATUS_TIMED_WAITING
 * modifier.
 *
 * The resumed thread will be in the runnable threads list after this and will be #bvm_vmthread_t::BVM_THREAD_STATUS_RUNNABLE.
 *
 * @param vmthread the thread to resume
 * @throws #BVM_ERR_ILLEGAL_THREAD_STATE_EXCEPTION if the thread is not in the #bvm_vmthread_t::BVM_THREAD_STATUS_TIMED_WAITING state.
 */
static void thread_sleep_timeout_callback(bvm_vmthread_t *vmthread) {

	if ( (vmthread->status & BVM_THREAD_STATUS_TIMED_WAITING) == 0 )
		bvm_throw_exception(BVM_ERR_ILLEGAL_THREAD_STATE_EXCEPTION, NULL);

	/* remove it from the waiting list */
	remove_thread_from_list(&thread_callback_list, vmthread);

	/* remove the waiting status */
	vmthread->status &= ~BVM_THREAD_STATUS_TIMED_WAITING;

	/* and resume it ... */
	thread_resume(vmthread);
}


/**
 * Cause the current thread to sleep a given number of milliseconds.
 *
 * After this function, the thread will be #bvm_vmthread_t::BVM_THREAD_STATUS_BLOCKED with a #bvm_vmthread_t::BVM_THREAD_STATUS_TIMED_WAITING modifier
 * and will be in the timed-callback list.
 *
 * @param sleep_time the time to sleep.
 */
void bvm_thread_sleep(bvm_int64_t sleep_time) {

	/* if the thread was interrupted before we get here, throw an exception and reset
	 * the interrupted flag */
	if (bvm_gl_thread_current->is_interrupted) {
		bvm_do_thread_interrupt();
	} else {

		/* block thread (which removes it from runnable list) */
		thread_block(bvm_gl_thread_current);

		/* give it a sleeping status */
		bvm_gl_thread_current->status |= BVM_THREAD_STATUS_TIMED_WAITING;

		/* and pop in the the timed-callback list */
		establish_timed_callback(bvm_gl_thread_current, sleep_time, thread_sleep_timeout_callback);
	}
}

/**
 * After a thread has been 'wait'ing and has come back because of a wait timeout or a notify
 * or notifyAll, we'll try to have it get the monitor immediately.  It the given thread can acquire
 * the monitor it is resumed.  If not, it is added to the lock queue of the monitor and waits with any other.
 *
 * @param monitor the monitor a thread is waiting on.
 * @param vmthread the thread waiting on the monitor
 */
static void acquire_monitor_or_wait_in_lock_queue(bvm_monitor_t *monitor, bvm_vmthread_t *vmthread) {

	/* if the monitor has no owner thread thread we'll grab it and resume,
	 * otherwise we'll just have the thread exit patiently in the acquire queue
	 * waiting for a bvm_thread_monitor_release */
	if (monitor->owner_thread == NULL) {

		monitor->owner_thread = vmthread;
		monitor->lock_depth = vmthread->lock_depth;

		vmthread->lock_depth = 0;

		/* and tell it to start again when it can */
		thread_resume(vmthread);
	} else {
		add_thread_to_queue(&monitor->lock_queue, vmthread);
	}
}


/**
 * A callback for threads that have been waiting in the timed-callback list and have had
 * their wait-time expire.  The thread will be removed from the timed-callback list and will
 * attempt to acquire the monitor it is waiting on.  It if can acquire it, it will become runnable,
 * it not, it will become blocked and join the lock queue on the monitor.
 *
 * @param vmthread
 * @throws #BVM_ERR_ILLEGAL_THREAD_STATE_EXCEPTION if the thread is not in the #bvm_vmthread_t::BVM_THREAD_STATUS_TIMED_WAITING state.
 */
static void thread_wait_timeout_callback(bvm_vmthread_t *vmthread) {

	bvm_monitor_t *monitor;

	/* we get a callback from a timed out waiting thread we remove the thread from the
	 * waiting queue of its monitor and resume it. */

	if ( (vmthread->status & BVM_THREAD_STATUS_TIMED_WAITING) == 0 )
		bvm_throw_exception(BVM_ERR_ILLEGAL_THREAD_STATE_EXCEPTION, NULL);

	/* grab the waiting object before the functions below clean it up */
	monitor = get_monitor_for_obj(vmthread->waiting_on_object);

	/* remove it from the monitor wait queue */
	remove_from_wait_queue(vmthread);

	/* remove it from the waiting list */
	remove_thread_from_list(&thread_callback_list, vmthread);

	/* remove the waiting status */
	vmthread->status &= ~BVM_THREAD_STATUS_TIMED_WAITING;

	/* try to get the monitor for the notified thread. If it cannot get it the thread will
	 * end up in the monitor's acquire queue */
	acquire_monitor_or_wait_in_lock_queue(monitor, vmthread);
}

/**
 * Causes the current thread to wait on another object.  If a wait time is specified, the thread
 * will be added to the timed-callbacks list and will try to acquire the monitor of the given object
 * when it wakes.
 *
 * If no wait_time is given, the thread is added to the given object's monitor wait queue.
 *
 * In all cases, the current thread will become blocked.
 *
 * @param obj the object to wait on.
 * @param wait_time the time in milliseconds to wait for it.
 */
void bvm_thread_wait(bvm_obj_t *obj, bvm_int64_t wait_time) {

	bvm_monitor_t *monitor;

	/* if the thread was interrupted before we get here, throw an exception and reset
	 * the interrupted flag */
	if (bvm_gl_thread_current->is_interrupted) {
		bvm_do_thread_interrupt();
	}

	monitor = get_monitor_for_obj(obj);

	/* remember the monitor and depth depth for use when this thread re-acquires the monitor */
	bvm_gl_thread_current->lock_depth = monitor->lock_depth;
	bvm_gl_thread_current->waiting_on_object = obj;

	/* and block the current thread */
	thread_block(bvm_gl_thread_current);

	/* If it has a wait time, establish it now as a waiting thread
	 * (will add to the wait list) */
	if (BVM_INT64_zero_gt(wait_time)) {
		establish_timed_callback(bvm_gl_thread_current, wait_time, thread_wait_timeout_callback);
		bvm_gl_thread_current->status |= BVM_THREAD_STATUS_TIMED_WAITING;
	} else {
		bvm_gl_thread_current->status |= BVM_THREAD_STATUS_WAITING;
	}

	/* add the waiting thread to the wait queue for the monitor */
	add_thread_to_queue(&monitor->wait_queue, bvm_gl_thread_current);

	/* promote and resume a waiting one if possible.  Has the effect of releasing all locks this
	 * thread has and establishing another as the owner thread. */
	promote_thread_in_lock_queue(monitor);
}

/**
 * Have the given thread attempt to acquire the given object's monitor. It the thread can acquire the
 * object's monitor it becomes the owner of that monitor.  If it cannot acquire it, the thread becomes blocked
 * and is added to the lock queue of the monitor and will have to re-attempt it at some other time.
 *
 * @param obj the object to lock upon
 * @param vmthread the thread that is attempt to get the object lock
 * @return #BVM_TRUE if the monitor was acquired, #BVM_FALSE otherwise.
* @throws #BVM_ERR_NULL_POINTER_EXCEPTION if obj is null.
  */
bvm_bool_t bvm_thread_monitor_acquire(bvm_obj_t *obj, bvm_vmthread_t *vmthread) {

	bvm_bool_t rv = BVM_TRUE;
	bvm_monitor_t *monitor;

	if (obj == NULL)
		bvm_throw_exception(BVM_ERR_NULL_POINTER_EXCEPTION, NULL);

	monitor = get_monitor_for_obj(obj);

	if (monitor == NULL) {
		/* there is no current monitor. Get a monitor and configure it */
		monitor = get_unused_monitor();
		monitor->owner_thread = vmthread;
		monitor->lock_depth++;
		monitor->in_use = BVM_TRUE;
		monitor->owner_object = obj;
	} else {

		/* there is already a monitor.  If the current thread owns it, we'll increase the lock depth
		 * and keep going otherwise we'll block the thread asking for it.  If it is not owned
		 * we'll make the current thread the owner. */

		if (monitor->owner_thread == NULL) {
			monitor->owner_thread = vmthread;

			monitor->lock_depth = (vmthread->lock_depth > 0) ? vmthread->lock_depth : 1;

			vmthread->lock_depth = 0;

		} else {

			if (monitor->owner_thread == vmthread) {
				monitor->lock_depth++;
			} else {

				/* add the thread to the lock queue for the monitor */
				add_thread_to_queue(&monitor->lock_queue, vmthread);

				/* and block it */
				thread_block(vmthread);

				/* return false to say the given thread did not get the lock */
				rv = BVM_FALSE;
			}
		}
	}

	return rv;
}

/**
 * Release a lock on an object.  If the lock depth on the object's monitor reaches zero, promote
 * another waiting thread (if any) to be the new owner of the object's monitor.
 *
 * @param obj the object to release a lock on.
* @throws #BVM_ERR_NULL_POINTER_EXCEPTION if obj is null.
* @throws #BVM_ERR_ILLEGAL_MONITOR_STATE_EXCEPTION if the monitor's owner thread is not the current thread.
 */
void bvm_thread_monitor_release(bvm_obj_t *obj) {

	bvm_monitor_t *monitor;

	/* no object?  whoops */
	if (obj == NULL)
		bvm_throw_exception(BVM_ERR_NULL_POINTER_EXCEPTION, NULL);

	monitor = get_monitor_for_obj(obj);

	/* monitor is not owned by the current thread?  Whoops */
	if (monitor->owner_thread != bvm_gl_thread_current)
		bvm_throw_exception(BVM_ERR_ILLEGAL_MONITOR_STATE_EXCEPTION, NULL);

	/* if by decreasing the depth we reach zero we either free the monitor if no one
	 * is waiting, or if a thread is waiting, we'll give it the monitor and resume it */
	if (--monitor->lock_depth == 0) {

		promote_thread_in_lock_queue(monitor);

		cache_monitor_if_unused(monitor);
	}
}


/**
 * Notifies one or all threads waiting on an object's monitor.  Performs the equivalent of the
 * java \c notify() and \c notifyAll() logic.
 *
 * @param obj the object that threads may be waiting on
 * @param do_all if \c BVM_TRUE, notify all waiting threads, if \c BVM_FALSE, notify just a single
 * waiting thread.
* @throws #BVM_ERR_ILLEGAL_THREAD_STATE_EXCEPTION if the thread is not in
 * the #bvm_vmthread_t::BVM_THREAD_STATUS_WAITING and #bvm_vmthread_t::BVM_THREAD_STATUS_TIMED_WAITING states.
  */
void bvm_thread_notify(bvm_monitor_t *monitor, bvm_bool_t do_all) {

	bvm_vmthread_t *vmthread;

	while (monitor->wait_queue != NULL) {

		/* take the first thread from the head of the queue */
		vmthread = monitor->wait_queue;
		monitor->wait_queue = vmthread->next_in_queue;

		/* if the thread is not actually in a waiting state something must
		 * have gone wrong */
		if ( ((vmthread->status & BVM_THREAD_STATUS_WAITING) == 0) &&
			 ((vmthread->status & BVM_THREAD_STATUS_TIMED_WAITING) == 0) )
			bvm_throw_exception(BVM_ERR_ILLEGAL_THREAD_STATE_EXCEPTION, NULL);

		/* detach it from the waiting list - if it is not in the list - no effect */
		remove_thread_from_list(&thread_callback_list, vmthread);

		/* remove waiting modifiers.  Leaves the status as BVM_THREAD_STATUS_BLOCKED.  May also be
		 * BVM_THREAD_STATUS_DBG_SUSPENDED as well. */
		vmthread->status &= ~BVM_THREAD_STATUS_WAITING;
		vmthread->status &= ~BVM_THREAD_STATUS_TIMED_WAITING;

		/* no longer waiting on any object */
		vmthread->waiting_on_object = NULL;

		/* try to acquire the monitor for the notified thread. If it cannot get it the thread will
		 * end up in the monitor's lock queue and will remain blocked */
		acquire_monitor_or_wait_in_lock_queue(monitor, vmthread);

		if (!do_all) break;
	}
}

/**
 * Interrupt a given thread.  If the thread is waiting it is given a 'pending' exception to throw when it
 * wakes and set to wake up immediately.
 *
 * @param vmthread
 */
void bvm_thread_interrupt(bvm_vmthread_t *vmthread) {

	/* only do for threads that are alive */
	if (bvm_thread_is_alive(vmthread)) {

		/* yes, always set the interrupted flag */
		vmthread->is_interrupted = BVM_TRUE;

		/* if the thread is sleeping or waiting give it an exception to throw when
		 * it next gets made the current thread. */
		if ( ( (vmthread->status & BVM_THREAD_STATUS_WAITING) != 0) ||
			 ( (vmthread->status & BVM_THREAD_STATUS_TIMED_WAITING) != 0) ) {

			/* create its interrupted exception */
			vmthread->pending_exception = bvm_create_exception_c(BVM_ERR_INTERRUPTED_EXCEPTION, NULL);

			/* if the thread is waiting or sleeping, set its wake-up time to zero to make
			 * it runnable again on the next thread switch */
			BVM_INT64_setZero(vmthread->time_to_awake);
		}

		/* if the current thread is being interrupting itself, we'll cause a thread switch
		 * so it exception gets thrown next time it becomes active. */
		if (bvm_gl_thread_current == vmthread) {
			bvm_gl_thread_timeslice_counter = 0;
		}
	}
}

/**
 * Create the system boot thread.  Starts the new thread and inserts it as the first element in the
 * runnable threads list.  The global thread registers are also defaulted from the new thread.
 */
static void thread_establish_bootstrap() {

	bvm_thread_obj_t *thread_obj;
	bvm_vmthread_t *vmthread;

	BVM_BEGIN_TRANSIENT_BLOCK {

		/* create a new Java thread object */
		thread_obj = (bvm_thread_obj_t *) bvm_object_alloc(BVM_THREAD_CLAZZ);
		BVM_MAKE_TRANSIENT_ROOT(thread_obj);

		thread_obj->priority.int_value = BVM_THREAD_PRIORITY_NORM;
		thread_obj->id.int_value = bvm_thread_get_next_id();
		thread_obj->name = bvm_string_create_from_cstring("main");

		vmthread = bvm_thread_create_vmthread(thread_obj);

	} BVM_END_TRANSIENT_BLOCK

	/* make it the current thread */
	bvm_gl_thread_current = vmthread;

	/* start it up - but do not push 'run' onto the stack - the startup
	thread just runs 'main' */
	bvm_thread_start(vmthread, BVM_FALSE);

	/* and switch to it */
	bvm_thread_switch();
}

/**
 * Push the java.lang.Thread#dispatchUncaughtException() method for the current thread onto
 * the stack.
 *
 * This method is called when an uncaught exception has been encountered for a thread - that is,
 * exception handling went all the way to the bottom of the stack and did not find a handler.
 *
 * This enables Java developers to pick up the mess.
 *
 * @param throwable the uncaught exception
 */
void bvm_thread_push_exceptionhandler(bvm_throwable_obj_t *throwable) {

	bvm_method_t *method;

	bvm_instance_clazz_t *clazz = (bvm_instance_clazz_t *) bvm_gl_thread_current->thread_obj->clazz;

	/* find the 'dispatchUncaughtException' method for the current thread.  Push it onto
	 * the stack with two arguments- 'this' and the throwable. */
	method = bvm_clazz_method_get(clazz, bvm_utfstring_pool_get_c("dispatchUncaughtException", BVM_TRUE),
							   bvm_utfstring_pool_get_c("(Ljava/lang/Throwable;)V", BVM_TRUE),
							   (BVM_METHOD_SEARCH_CLAZZ | BVM_METHOD_SEARCH_SUPERS) );

	/* push the run method onto the stack making sure to capture the sync object if required*/
	bvm_frame_push(
	        method,
	        bvm_gl_rx_sp,
	        bvm_gl_rx_pc,
	        bvm_gl_rx_pc,
	        BVM_METHOD_IsSynchronized(method) ? (bvm_obj_t *) bvm_gl_thread_current->thread_obj : NULL);

	/* populate the stack with the args to the method */
	bvm_gl_rx_locals[0].ref_value = (bvm_obj_t *) bvm_gl_thread_current->thread_obj;
	bvm_gl_rx_locals[1].ref_value = (bvm_obj_t *) throwable;
}

/**
 * Provide the next id for a thread.  Used for allocation if thread id's at the Java level.
 */
bvm_uint32_t bvm_thread_get_next_id() {
	return thread_next_id++;
}

/**
 * Performs initialisation for threading at VM startup
 */
void bvm_init_threading() {
	thread_establish_bootstrap();
}

/**
 * Tests whether a given thread is 'alive'.  A thread is alive if it is not \ NULL, is not NEW, and
 * is not TERMINATED.
 */
bvm_bool_t bvm_thread_is_alive(bvm_vmthread_t *vmthread) {
	return ( (vmthread != NULL) &&
			 ( (vmthread->status & BVM_THREAD_STATUS_NEW) == 0) &&
			 ( (vmthread->status & BVM_THREAD_STATUS_TERMINATED) == 0) );
}

/**
 * Copy the thread registers to the global registers
 *
 * @param vmthread the thread with registers to copy to global.
 */
void bvm_thread_load_registers(bvm_vmthread_t *vmthread) {

	bvm_gl_rx_method  = vmthread->rx_method;
	bvm_gl_rx_sp      = vmthread->rx_sp;
	bvm_gl_rx_pc  	  = vmthread->rx_pc;
	bvm_gl_rx_stack   = vmthread->rx_stack;
	bvm_gl_rx_locals  = vmthread->rx_locals;

	/* set the global class register... for quick access without having to dereference the method*/
	bvm_gl_rx_clazz   = (bvm_gl_rx_method == NULL) ? NULL : bvm_gl_rx_method->clazz;
}

/**
 * Copy the global registers to the thread registers
 *
 * @param vmthread the thread to copy the global registers to.
 */
void bvm_thread_store_registers(bvm_vmthread_t *vmthread) {

	/* TODO nice if this could be reduced to a single memory operation */
	vmthread->rx_method = bvm_gl_rx_method;
	vmthread->rx_sp     = bvm_gl_rx_sp;
	vmthread->rx_pc 	= bvm_gl_rx_pc;
	vmthread->rx_stack  = bvm_gl_rx_stack;
	vmthread->rx_locals = bvm_gl_rx_locals;
}

/**
 * Provides the depth of a given thread's stack.
 *
 * @param vmthread
 * @return the depth.
 */
int bvm_stack_get_depth(bvm_vmthread_t *vmthread) {

	int stack_depth;

	bvm_stack_visit(vmthread,0,-1,&stack_depth, NULL, NULL);

	return stack_depth;
}

/**
 * Visit a given thread's stack from the top to the bottom calling an optional callback handler for each frame.
 *
 * @param vmthread the given thread
 * @param startframe the frame number to start at - the callback will not be invoked until this depth is
 * reached.  Pass in zero to start at the very top.
 * @param count the number of frame to visit after \c startframe frames have been visited.  -1 means go all
 * the way.
 * @param depth an optional integer pointer that can be used by the calling function to count the
 * total number of frames traversed.  So, \c 'startframe=0' and 'length=-1' this will return the depth of
 * the stack.
 * @param callback an optional #bvm_stack_visit_callback_t function pointer to call for each visited frame
 * after \c startframe has been reached.  The callback will be passed a bvm_stack_frame_info_t handle describing
 * the frame.
 * @param data an optional handle to data to pass to the callback when it is invoked.
 */
void bvm_stack_visit(bvm_vmthread_t *vmthread, int startframe, int count, int *depth, bvm_stack_visit_callback_t callback, void *data) {

	bvm_cell_t *frame_locals;
	bvm_method_t *frame_method;
	bvm_uint8_t    *frame_pc;
	bvm_uint8_t    *frame_ppc;
	int visit_depth = 0;
	bvm_stack_frame_info_t stack_info;

	if (depth != NULL) *depth = 0;
	if (count == -1) count = 10000;	/* -1 means "to the bottom of the stack" - we'll set an stupid large number to compare against */

	bvm_thread_store_registers(bvm_gl_thread_current);

	frame_locals = vmthread->rx_locals;
	frame_method = vmthread->rx_method;
	frame_pc     = vmthread->rx_pc;
	frame_ppc    = NULL;

	/* in some strange circumstances, we can arrive here before anything has been pushed on the
	 * stack - this occurs when a debugger requested frame information before the VM
	 * has actually started or when loading the application's main class throws an exception.  */
	if (frame_method == BVM_METHOD_CALLBACKWEDGE) return;

	/* loop until we reach a frame that has the PC set to the 'kill switch' PC.  This PC
	 * indicates the very bottom frame of a thread stack.
	 */
	while (  !( (frame_method == BVM_METHOD_CALLBACKWEDGE) &&
		        (frame_pc == BVM_THREAD_KILL_PC) ) && (count > 0)) {

		/* if our current visit depth is between the requested ranges ... */
		if ( visit_depth >= startframe) {

			count--;

			/* let the calling function know the hit count */
			if (depth != NULL) (*depth)++;

			/* and if a callback has been given, call it */
			if (callback != NULL) {
				stack_info.vmthread = vmthread;
				stack_info.locals = frame_locals;
				stack_info.method = frame_method;
				stack_info.pc = frame_pc;
				stack_info.ppc = frame_ppc;
				stack_info.depth = visit_depth;
				if (!callback(&stack_info, data)) return;
			}
		}

		visit_depth++;

		/* get the previous method */
		frame_method = frame_locals[BVM_FRAME_METHOD_OFFSET].ptr_value;

		/* get the previous program counter */
		frame_pc = frame_locals[BVM_FRAME_PC_OFFSET].ptr_value;

		/* get the previous (previous!) program counter */
		frame_ppc = frame_locals[BVM_FRAME_PPC_OFFSET].ptr_value;

		/* move the frame locals pointer down to the previous frame locals */
		frame_locals = frame_locals[BVM_FRAME_LOCALS_OFFSET].ptr_value;
	}
}

#if BVM_DEBUGGER_ENABLE

/**
 * Check whether all threads are suspended.  Has the side effect of setting #bvmd_gl_all_suspended to
 * BVM_TRUE is all threads are suspended.
 *
 * @return \c BVM_TRUE if all thread are suspended, \c BVM_FALSE otherwise.
 */
bvm_bool_t bvmd_check_thread_suspensions() {

	bvm_bool_t allsuspended = BVM_TRUE;

	bvm_vmthread_t *vmthread = bvm_gl_threads;

	while (vmthread != NULL) {

		if ( (bvm_thread_is_alive(vmthread)) &&
			(vmthread->status == BVM_THREAD_STATUS_RUNNABLE) ) {
			allsuspended = BVM_FALSE;
			break;
		}
		vmthread = vmthread->next;
	}

	bvmd_gl_all_suspended = allsuspended;

	return allsuspended;
}

/**
 * Suspend a thread in a 'debugger' way.  This actually does a normal suspend on the thread if it is not
 * already suspended and then (regardless of whether it was already suspended or was not) increments
 * the thread debug suspension counter.  The thread will gain a status of bvm_vmthread_t::BVM_THREAD_STATUS_DBG_SUSPENDED
 * if it was not already.
 *
 * @param vmthread a given thread.
 */
void bvmd_thread_suspend(bvm_vmthread_t *vmthread) {

	if ((vmthread->status & BVM_THREAD_STATUS_DBG_SUSPENDED) == 0) {

		/* if the thread is runnable, block the thread - it will be removed from the runnable threads list. */
		if (vmthread->status == BVM_THREAD_STATUS_RUNNABLE) {
			thread_block(vmthread);
		} else {
			/* thread is already blocked */
		}

		/* set the debugger suspended modifier */
		vmthread->status |= BVM_THREAD_STATUS_DBG_SUSPENDED;
	}

	vmthread->dbg_suspend_count++;
}

/**
 * Suspend all threads in a 'debugger' way.
 */
void bvmd_thread_suspend_all() {

	bvm_vmthread_t *vmthread = bvm_gl_threads;

	while (vmthread != NULL) {
		if (bvm_thread_is_alive(vmthread))
			bvmd_thread_suspend(vmthread);
		vmthread = vmthread->next;
	}
}

/**
 * Resume a thread in a 'debugger' way.  Any thread acted upon by this function will have its thread
 * suspend count reduced.  If the count is reduced to zero the thread will be resumed for
 * real *if* the only thing making it suspended is the debugger.
 *
 * @param vmthread a given thread.
 */
void bvmd_thread_resume(bvm_vmthread_t *vmthread) {

	if (--vmthread->dbg_suspend_count <= 0) {

		vmthread->status &= ~BVM_THREAD_STATUS_DBG_SUSPENDED;
		vmthread->dbg_suspend_count = 0;

		/* if the thread is left as just blocked (not waiting / timed waiting) and it
		 * is not waiting on any object, it must have been 'running' when the debugger
		 * suspended it, or was waiting (etc) when the debugger suspended it and
		 * its timer has expired.  So event though the debugger may be resuming it, we'll
		 * really resume it unless it is in a state to be resumed.
		 */
		if ( (vmthread->status == BVM_THREAD_STATUS_BLOCKED) &&
			 (vmthread->waiting_on_object == NULL) ) {
			thread_resume(vmthread);
		}
	}
}

/**
 * Resumes all threads in a 'debugger' way.
 *
 * @param force if \c BVM_TRUE, the thread will be resumed regardless of how many times the debugger has
 * suspended it.
 * @param clear_parked if \c BVM_TRUE, the thread will have any parked debug events cleared and releaseed to
 * the heap (they are abandoned).
 */
void bvmd_thread_resume_all(bvm_bool_t force, bvm_bool_t clear_parked) {

	bvm_vmthread_t *vmthread = bvm_gl_threads;

	while (vmthread != NULL) {

		if (bvm_thread_is_alive(vmthread)) {
			if (force) vmthread->dbg_suspend_count = 0;
			bvmd_thread_resume(vmthread);
		}

		if (clear_parked) {

			bvmd_eventcontext_t *context = vmthread->dbg_parked_events;

			while (context != NULL) {
				bvmd_eventcontext_t *next = context->next;
				bvmd_eventdef_free_context(context);
				context = next;
			}

			vmthread->dbg_parked_events = NULL;
		}

		vmthread = vmthread->next;
	}
}

#endif

