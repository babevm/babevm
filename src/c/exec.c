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
 * @file
 *
 * The Interpreter Loop
 *
 * @section exec-ov Overview
 *
 * The big loop.  You've finally found the meat of it all.  Yes, this is big.  The interpreter loop is a
 * classic large switch statement.
 *
 * The case statements are in order from 0 and are padded to 255 (in the hope that this helps the compiler).
 *
 * The VM runs only this one loop - #run_virtual_machine() is called at VM startup and exits when all non-daemon
 * threads are terminated.  It is not reentrant.  The VM spins on a loop within #run_virtual_machine().  Starting with
 * the bootstrap thread at VM startup, each thread gets execution time in this loop.  When a thread context
 * switch is made, the current thread's 'registers' are saved to the thread struct, and the new thread's registers
 * are restored to the 'global registers'.  See the doc in #frame.c for a better discussion on 'registers'.
 *
 * Every opcode is dealt with here within the #bvm_exec_run() loop.  Some documentation is provided
 * within them, but often they are small and mostly self-explanatory after having read the JVM specs.  Yes, you
 * will need to be *very* familiar with the JVM specifications to get all this.
 *
 * You will see some 'goto' statements and labels used in this main loop.  They are used carefully and
 * sparingly but with purpose.  "Rules are for the obedience of fools and the guidance of wise men". Nuff said.
 *
 * @section exec-exception Exception Handling
 *
 * The VM needs to cater for two types of exception: those thrown by the VM, and those thrown by the java bytecode.
 * The handling of each is almost identical.  The code in the \c athrow opcode handles both.
 *
 * The VM distinguishes whether an exception is native by the \c needs_native_try_reset field on an exception.  This is true
 * if the VM was the source of the exception and false if the bytecode was the source.
 *
 * The primary difference between native and bytecode exception handling is what to do *after* an exception has
 * been caught by a catch() java statement.  Native exceptions return to the 'new_try_block' label so that the VM
 * can reset all stuff ready to catch another native exception.  The bytecode exceptions return to
 * the 'exception_is_handled' label ready to do the next opcode.  That is why you'll notice that the entire
 * exec loop is surrounded by a BVM_TRY/BVM_CATCH block.  When a native exception is caught by this block, control
 * passes to the same code (in \c OPCODE_athrow) that handles normal bytecode exceptions - but after handling
 * the native exception, control passes to above the \c BVM_TRY to reenter it.
 *
 * The VM supports the Java 5 \c uncaughtExceptionHandler mechanism for threads.  Control will be passed to a
 * thread's default exception handler if an exception is not caught for that thread.
 *
 * Locating an exception's handler (try/catch/finally block) and actually moving to that location in the stack are
 * separated into two distinct 'exception handling' steps.  This is done for debugger support.
 *
 * For reporting of exceptions, the debugger wants to know all details of the exception, including where it is caught (if it
 * is indeed, caught) *before* the stack is altered.  So, actually gathering all info about the exception is the first step.
 * This may be delivered to a debugger, and the thread suspended, so the handling of the second step is quite distinct.  To
 * this end a new Babe-VM specific opcode \c 'OPCODE_pop_to_exception' has been introduced that performs the second
 * part of exception handling: that is altering the stack state to point to the exception handler location (or, if the
 * exception is uncaught, to pop everything off the stack).
 *
 * Nothing is pushed onto the stack for the OPCODE_pop_to_exception to work with.  All the info about an exception and its
 * handler locations is held in the current thread in the #bvm_find_exception_callback_data \c exception_location struct.
 *
 * @section exec-fastbytecode Fast Bytecode Substitution
 *
 * Some non-Java (Babe VM specific) opcodes have been used to help speed up some things.  These new opcode names end
 * with '_fast'.  Quite simply, they are some faster versions of standard opcodes.  For example, all the field
 * get/put opcodes have fast equivalents.  Why?  The first time a constant is used to reference a field a pointer
 * to that field is stored in the constant.  So the next time the same bytecode is executed we know there will
 * be a faster way to get at the field.  Rather than re-resolving the constant pool field, we can just get
 * it straight from the constant as a pointer.  Rather than test this every time (is the field resolved or not?)
 * we'll just point the opcode to another place so the next time (say) \c OPCODE_getfield is not executed, but
 * \c OPCODE_getfield_fast is instead. \c OPCODE_getfield_fast gets the pointer from the constant and uses it.  Much faster.
 * Additionally, some checks that the VM may perform first time around are unnecessary the second time.  Why
 * check again if the class is initialised?  We know it must be because we did it the first time.
 *
 * Note that the benefits of the fast opcodes are only realised if the code is executed more than once.  It
 * does not mean that every (say) opcode \c OPCODE_getfield will now use \c OPCODE_getfield_fast.  The opcode
 * substitution is only performed to a particular opcode in a particular method in a particular class.
 *
 * Fast bytecode substitution is disabled if the #BVM_DEBUGGER_ENABLE compile-time option is enabled.  During
 * debugging, the OPCODE_BREAKPOINT bytecode substitution takes place to facilitate debugger breakpoints.
 *
 * @section exec-registers Using Registers
 *
 * Some platforms may operate faster if the VM registers can be stored in CPU registers and used from
 * there.  This can only happen with local variables and not with global ones.  So, some compile time
 * options are implemented that will use (or not) the VM registers as local variables.  In order to use
 * the registers as local variables, whenever a function call is made from the interp loop that uses
 * and/or changes the global registers, the local ones will need to be copied to global before the function
 * call and restored from the globals afterwards.
 *
 * So, we get some upside (perhaps) from using CPU registers, and some downside with the save/restore of
 * the local registers to/from global registers.  To be honest, this mechanism was implemented thinking
 * it would make a huge difference.  Truth is, on MS VC with all optimisations, it actually slows
 * the VM down - I guess the swapping loses more than the registers gain.  But this may be different
 * on other platforms that are not as savvy - so the implementation is kept.  The compile time setting #BVM_USE_REGISTERS
 * is used to turn the usage of the local variable registers on/off.
 *
 * The number of functions calls made from the interp to other function loop is purposefully kept to a minimum.
 *
 * @section exec-stack Stack usage
 *
 * For something that does a lot of stack manipulation you may be surprised to see no 'push' or 'pop' functions
 * or macros.  All stack access and manipulation within each bytecode impl is relative to the current top
 * of the stack.  For example, if the JVMS spec for the byte code says the stack will have an object then an integer
 * and the result should be a (say) float, then the first stack element (the object) will be accessed at SP-2
 * (SP is 'Stack Pointer'), then the integer at SP-1, the result float will be placed in SP-2 and the current SP decremented
 * by 2 to move the current stack top for the next bytecode.
 *
 * Perhaps this might seem at first a strange way to go about manipulating a stack - but it is very efficient.  The basic premise
 * is that the state of the stack is known at the start and end of each executed bytecode - therefore why bother with
 * excessive pop / push when you know exactly where the bytecode input data is in the stack and exactly where its result
 * (if any) is should be at the end.  Simple.
 *
  @author Greg McCreath
  @since 0.0.10

 */

/*
 * When debugging the current opcode is not always at \c *bvm_gl_rx_pc.  These macros ensure the correct opcode
 * is being used.
 */
#if BVM_DEBUGGER_ENABLE
#	define CURRENT_OPCODE (opcode)
#else
#	define CURRENT_OPCODE (*bvm_gl_rx_pc)
#endif

/*
 * Macros to act as substitutions for 'switch', 'case', 'break', and whatever ends the switch block for both classic
 * 'big switch' opcode dispatching and GCC direct-threaded labels-as-values dispatching.
 */

#if BVM_DIRECT_THREADING_ENABLE
#	define OPCODE_DISPATCH(i) goto *opcode_labels[i];
#	define OPCODE_HANDLER(n) n ## _label
#	define OPCODE_NEXT goto top_of_interpreter_loop;
#	define OPCODE_DISPATCH_END {}
#else
#	define OPCODE_DISPATCH(i) switch(i) {
#	define OPCODE_HANDLER(n) case (n)
#	define OPCODE_NEXT break;
#	define OPCODE_DISPATCH_END }
#endif


/**
 * Stack visit callback for checking if a method catches a given exception.  If it does, the param
 * data (a bvm_exception_location_data_t) is populated with the catch location info.
 *
 * @param stackinfo a handle to stack frame info
 * @param data a bvm_exception_location_data_t
 *
 * @return BVM_FALSE is the exception is found, BVM_TRUE otherwise (as per the requirements of a stack
 * visitation.
 */
static bvm_bool_t exec_find_exception_callback(bvm_stack_frame_info_t *stackinfo, void *data) {

	int lc;
	int pcoffset;
	bvm_uint16_t nrexc;
	bvm_exception_location_data_t *location_data = data;
	bvm_method_t *method = stackinfo->method;
	bvm_uint8_t *throw_pc = (stackinfo->ppc != NULL) ? stackinfo->ppc : stackinfo->pc;

	/* note the last stack depth that was visited - this will be the same as the stack
	 * depth if the exception in uncaught */
	location_data->depth = stackinfo->depth;

	/* "if the method of the current frame is a synchronized method and the current thread
	 * is not the owner of the monitor acquired or reentered on invocation of the method,
	 * athrow throws an IllegalMonitorStateException instead of the
	 * object previously being thrown"
	 *
	 * So, if that is the case, we substitute the illegal monitor state exception here.
	 */
	if (BVM_METHOD_IsSynchronized(method)) {
		bvm_obj_t *sync_obj = (stackinfo->locals[BVM_FRAME_SYNCOBJ_OFFSET].ref_value);
		bvm_monitor_t *monitor = get_monitor_for_obj(sync_obj);

		if (monitor->owner_thread != stackinfo->vmthread) {
			location_data->throwable = bvm_create_exception_c(BVM_ERR_ILLEGAL_MONITOR_STATE_EXCEPTION, NULL);
		}
	}

	/* number of exceptions for the method's code.  If native, then none. */
	nrexc = BVM_METHOD_IsNative(method) ? 0 : method->exceptions_count;

	/* the pc in the method is the current pc address less the
	 * start address of the method's bytecode */
	pcoffset = (int) (throw_pc - method->code.bytecode);

	/* ok, look at each exception - there may be none */
	for (lc=0; lc < nrexc; lc++) {

		/* get a handle to the exception from the method's exceptions array */
		bvm_exception_t *exp = &(method->exceptions[lc]);

		/* if the current offset is within the offset range as specific by the exception */
		if ( (pcoffset >= exp->start_pc) && (pcoffset < exp->end_pc)) {

			/* within range, so let's check the clazz - default to NULL stops
			 * compiler complaining */
			bvm_clazz_t *ex_clazz = NULL;

			/* catch type contains the class name, or NULL for a 'finally' */
			bvm_utfstring_t *ct = exp->catch_type;

			/* if we are not at a 'finally' get the clazz of the exception at the catch. */
			if (ct != NULL) {
				ex_clazz = bvm_clazz_get(method->clazz->classloader_obj, ct);
			}

			/* Have we found one?  We have if we have reached a 'finally' or
			 * found a catch that has a class that is compatible with the
			 * thrown object. */
			if ((ct == NULL) || (bvm_clazz_is_assignable_from(location_data->throwable->clazz, ex_clazz))) {
				location_data->caught = BVM_TRUE;
				location_data->method = stackinfo->method;
				location_data->handler_pc = exp->handler_pc;

				/* found, return false to have stack visit stop */
				return BVM_FALSE;
			}
		}
	}

	/* not found, return true to have stack visit keep going */
	return BVM_TRUE;
}

/**
 * Finds the virtual implementation of a given method from a given clazz as a starting point for
 * the search.  Searches the class hierarchy looking for a method that matches the name and
 * JNI signature of the given methods and satisfies visibility and access restrictions.
 *
 * @param search_clazz the starting instance clazz for the search
 * @param resolved_method a method to search for a match.
 *
 * @return the bvm_method_t method if one if found, or NULL otherwise.
 */
static bvm_method_t *locate_virtual_method(bvm_instance_clazz_t *search_clazz, bvm_method_t *resolved_method) {

	bvm_utfstring_t *method_name = resolved_method->name;
	bvm_utfstring_t *method_sig  = resolved_method->jni_signature;

	bvm_method_t *virtual_method;

	/* JVMS invokevirtual opcode docs: "Let C be the class of ref_value. If C contains a
	 * declaration for an instance method with the same name and desc as the
	 * resolved method, and the resolved method is accessible from C, then this is
	 * the method to be invoked, and the lookup procedure terminates". */

	do {

		virtual_method = bvm_clazz_method_get(search_clazz, method_name, method_sig, (BVM_METHOD_SEARCH_CLAZZ | BVM_METHOD_SEARCH_SUPERS) );

		/* we have found a matching method - note that this method may actually not be in
		 * the search_clazz, it may have been found in one of its superclasses ....*/
		if (virtual_method != NULL) {

			/* if the resolved method is visible to the found invoke method (which
			 * implies it is an override) we have found our method.  The 'public' check here is
			 * redundant - it is also checked in bvm_clazz_is_member_accessible - but it is just to
			 * avoid the function call overhead as most methods are public ..*/
			if (BVM_ACCESS_IsPublic(BVM_METHOD_AccessFlags(resolved_method)) ||
					bvm_clazz_is_member_accessible(BVM_METHOD_AccessFlags(resolved_method), virtual_method->clazz, (bvm_instance_clazz_t *) resolved_method->clazz)) {
				return virtual_method;
			}

			/* ... otherwise, try the superclazz - note we are trying the superclazz of the clazz the
			 * found invoke method was located in, not the superclazz of the last clazz searched. */
			search_clazz = virtual_method->clazz->super_clazz;
		}

	} while (search_clazz != NULL && virtual_method != NULL);

	bvm_throw_exception(BVM_ERR_NO_SUCH_METHOD_ERROR, (char *) method_name->data);

	/* never reached */
	return NULL;
}

static void throw_array_index_bounds_exception() {
    bvm_throw_exception(BVM_ERR_ARRAY_INDEX_OUT_OF_BOUNDS_EXCEPTION, NULL);
}

static void throw_null_pointer_exception() {
    bvm_throw_exception(BVM_ERR_NULL_POINTER_EXCEPTION, NULL);
}

static void throw_unsupported_feature_exception_float() {
    bvm_throw_exception(BVM_ERR_INTERNAL_ERROR, "float not supported");
}

static void throw_unsupported_feature_exception() {
    bvm_throw_exception(BVM_ERR_INTERNAL_ERROR, "unsupported feature");
}

/**
 * The big execution loop.
 */
void bvm_exec_run() {

	bvm_throwable_obj_t *throwable = NULL;

	/* when using fast bytecodes for method invocation things can jump around a bit .. the following local vars
	 * are used for invocation. */
	bvm_instance_clazz_t *invoke_clazz;
	bvm_method_t *invoke_method;
	bvm_obj_t *invoke_obj;
	int invoke_nr_args;
	int invoke_pc_offset;

#if BVM_DEBUGGER_ENABLE
	bvm_uint8_t opcode;
#endif

#if BVM_DIRECT_THREADING_ENABLE
	void *opcode_labels[] = {
			&&OPCODE_nop_label,
			&&OPCODE_aconst_null_label,
			&&OPCODE_iconst_m1_label,
			&&OPCODE_iconst_0_label,
			&&OPCODE_iconst_1_label,
			&&OPCODE_iconst_2_label,
			&&OPCODE_iconst_3_label,
			&&OPCODE_iconst_4_label,
			&&OPCODE_iconst_5_label,
			&&OPCODE_lconst_0_label,
			&&OPCODE_lconst_1_label,
			&&OPCODE_fconst_0_label,
			&&OPCODE_fconst_1_label,
			&&OPCODE_fconst_2_label,
			&&OPCODE_dconst_0_label,
			&&OPCODE_dconst_1_label,
			&&OPCODE_bipush_label,
			&&OPCODE_sipush_label,
			&&OPCODE_ldc_label,
			&&OPCODE_ldc_w_label,
			&&OPCODE_ldc2_w_label,
			&&OPCODE_iload_label,
			&&OPCODE_lload_label,
			&&OPCODE_fload_label,
			&&OPCODE_dload_label,
			&&OPCODE_aload_label,
			&&OPCODE_iload_0_label,
			&&OPCODE_iload_1_label,
			&&OPCODE_iload_2_label,
			&&OPCODE_iload_3_label,
			&&OPCODE_lload_0_label,
			&&OPCODE_lload_1_label,
			&&OPCODE_lload_2_label,
			&&OPCODE_lload_3_label,
			&&OPCODE_fload_0_label,
			&&OPCODE_fload_1_label,
			&&OPCODE_fload_2_label,
			&&OPCODE_fload_3_label,
			&&OPCODE_dload_0_label,
			&&OPCODE_dload_1_label,
			&&OPCODE_dload_2_label,
			&&OPCODE_dload_3_label,
			&&OPCODE_aload_0_label,
			&&OPCODE_aload_1_label,
			&&OPCODE_aload_2_label,
			&&OPCODE_aload_3_label,
			&&OPCODE_iaload_label,
			&&OPCODE_laload_label,
			&&OPCODE_faload_label,
			&&OPCODE_daload_label,
			&&OPCODE_aaload_label,
			&&OPCODE_baload_label,
			&&OPCODE_caload_label,
			&&OPCODE_saload_label,
			&&OPCODE_istore_label,
			&&OPCODE_lstore_label,
			&&OPCODE_fstore_label,
			&&OPCODE_dstore_label,
			&&OPCODE_astore_label,
			&&OPCODE_istore_0_label,
			&&OPCODE_istore_1_label,
			&&OPCODE_istore_2_label,
			&&OPCODE_istore_3_label,
			&&OPCODE_lstore_0_label,
			&&OPCODE_lstore_1_label,
			&&OPCODE_lstore_2_label,
			&&OPCODE_lstore_3_label,
			&&OPCODE_fstore_0_label,
			&&OPCODE_fstore_1_label,
			&&OPCODE_fstore_2_label,
			&&OPCODE_fstore_3_label,
			&&OPCODE_dstore_0_label,
			&&OPCODE_dstore_1_label,
			&&OPCODE_dstore_2_label,
			&&OPCODE_dstore_3_label,
			&&OPCODE_astore_0_label,
			&&OPCODE_astore_1_label,
			&&OPCODE_astore_2_label,
			&&OPCODE_astore_3_label,
			&&OPCODE_iastore_label,
			&&OPCODE_lastore_label,
			&&OPCODE_fastore_label,
			&&OPCODE_dastore_label,
			&&OPCODE_aastore_label,
			&&OPCODE_bastore_label,
			&&OPCODE_castore_label,
			&&OPCODE_sastore_label,
			&&OPCODE_pop_label,
			&&OPCODE_pop2_label,
			&&OPCODE_dup_label,
			&&OPCODE_dup_x1_label,
			&&OPCODE_dup_x2_label,
			&&OPCODE_dup2_label,
			&&OPCODE_dup2_x1_label,
			&&OPCODE_dup2_x2_label,
			&&OPCODE_swap_label,
			&&OPCODE_iadd_label,
			&&OPCODE_ladd_label,
			&&OPCODE_fadd_label,
			&&OPCODE_dadd_label,
			&&OPCODE_isub_label,
			&&OPCODE_lsub_label,
			&&OPCODE_fsub_label,
			&&OPCODE_dsub_label,
			&&OPCODE_imul_label,
			&&OPCODE_lmul_label,
			&&OPCODE_fmul_label,
			&&OPCODE_dmul_label,
			&&OPCODE_idiv_label,
			&&OPCODE_ldiv_label,
			&&OPCODE_fdiv_label,
			&&OPCODE_ddiv_label,
			&&OPCODE_irem_label,
			&&OPCODE_lrem_label,
			&&OPCODE_frem_label,
			&&OPCODE_drem_label,
			&&OPCODE_ineg_label,
			&&OPCODE_lneg_label,
			&&OPCODE_fneg_label,
			&&OPCODE_dneg_label,
			&&OPCODE_ishl_label,
			&&OPCODE_lshl_label,
			&&OPCODE_ishr_label,
			&&OPCODE_lshr_label,
			&&OPCODE_iushr_label,
			&&OPCODE_lushr_label,
			&&OPCODE_iand_label,
			&&OPCODE_land_label,
			&&OPCODE_ior_label,
			&&OPCODE_lor_label,
			&&OPCODE_ixor_label,
			&&OPCODE_lxor_label,
			&&OPCODE_iinc_label,
			&&OPCODE_i2l_label,
			&&OPCODE_i2f_label,
			&&OPCODE_i2d_label,
			&&OPCODE_l2i_label,
			&&OPCODE_l2f_label,
			&&OPCODE_l2d_label,
			&&OPCODE_f2i_label,
			&&OPCODE_f2l_label,
			&&OPCODE_f2d_label,
			&&OPCODE_d2i_label,
			&&OPCODE_d2l_label,
			&&OPCODE_d2f_label,
			&&OPCODE_i2b_label,
			&&OPCODE_i2c_label,
			&&OPCODE_i2s_label,
			&&OPCODE_lcmp_label,
			&&OPCODE_fcmpl_label,
			&&OPCODE_fcmpg_label,
			&&OPCODE_dcmpl_label,
			&&OPCODE_dcmpg_label,
			&&OPCODE_ifeq_label,
			&&OPCODE_ifne_label,
			&&OPCODE_iflt_label,
			&&OPCODE_ifge_label,
			&&OPCODE_ifgt_label,
			&&OPCODE_ifle_label,
			&&OPCODE_if_icmpeq_label,
			&&OPCODE_if_icmpne_label,
			&&OPCODE_if_icmplt_label,
			&&OPCODE_if_icmpge_label,
			&&OPCODE_if_icmpgt_label,
			&&OPCODE_if_icmple_label,
			&&OPCODE_if_acmpeq_label,
			&&OPCODE_if_acmpne_label,
			&&OPCODE_goto_label,
			&&OPCODE_jsr_label,
			&&OPCODE_ret_label,
			&&OPCODE_tableswitch_label,
			&&OPCODE_lookupswitch_label,
			&&OPCODE_ireturn_label,
			&&OPCODE_lreturn_label,
			&&OPCODE_freturn_label,
			&&OPCODE_dreturn_label,
			&&OPCODE_areturn_label,
			&&OPCODE_return_label,
			&&OPCODE_getstatic_label,
			&&OPCODE_putstatic_label,
			&&OPCODE_getfield_label,
			&&OPCODE_putfield_label,
			&&OPCODE_invokevirtual_label,
			&&OPCODE_invokespecial_label,
			&&OPCODE_invokestatic_label,
			&&OPCODE_invokeinterface_label,
			&&OPCODE_186_label,
			&&OPCODE_new_label,
			&&OPCODE_newarray_label,
			&&OPCODE_anewarray_label,
			&&OPCODE_arraylength_label,
			&&OPCODE_athrow_label,
			&&OPCODE_checkcast_label,
			&&OPCODE_instanceof_label,
			&&OPCODE_monitorenter_label,
			&&OPCODE_monitorexit_label,
			&&OPCODE_wide_label,
			&&OPCODE_multianewarray_label,
			&&OPCODE_ifnull_label,
			&&OPCODE_ifnonnull_label,
			&&OPCODE_goto_w_label,
			&&OPCODE_jsr_w_label,
			&&OPCODE_breakpoint_label,
			&&OPCODE_203_pop_for_exception_label,
			&&OPCODE_204_label,
			&&OPCODE_205_label,
			&&OPCODE_206_label,
			&&OPCODE_207_label,
			&&OPCODE_208_label,
			&&OPCODE_209_label,
			&&OPCODE_210_label,
			&&OPCODE_211_label,
			&&OPCODE_212_label,
			&&OPCODE_213_label,
			&&OPCODE_214_label,
			&&OPCODE_215_label,
			&&OPCODE_ldc_fast_1_label,
			&&OPCODE_ldc_fast_2_label,
			&&OPCODE_ldc_w_fast_1_label,
			&&OPCODE_ldc_w_fast_2_label,
			&&OPCODE_getstatic_fast_label,
			&&OPCODE_getstatic_fast_long_label,
			&&OPCODE_putstatic_fast_label,
			&&OPCODE_putstatic_fast_long_label,
			&&OPCODE_getfield_fast_label,
			&&OPCODE_getfield_fast_long_label,
			&&OPCODE_putfield_fast_label,
			&&OPCODE_putfield_fast_long_label,
			&&OPCODE_new_fast_label,
			&&OPCODE_229_invokestatic_fast_label,
			&&OPCODE_230_invokespecial_fast_label,
			&&OPCODE_231_invokeinterface_fast_label,
			&&OPCODE_232_invokevirtual_fast_label,
			&&OPCODE_233_label,
			&&OPCODE_234_label,
			&&OPCODE_235_label,
			&&OPCODE_236_label,
			&&OPCODE_237_label,
			&&OPCODE_238_label,
			&&OPCODE_239_label,
			&&OPCODE_240_label,
			&&OPCODE_241_label,
			&&OPCODE_242_label,
			&&OPCODE_243_label,
			&&OPCODE_244_label,
			&&OPCODE_245_label,
			&&OPCODE_246_label,
			&&OPCODE_247_label,
			&&OPCODE_248_label,
			&&OPCODE_249_label,
			&&OPCODE_250_label,
			&&OPCODE_251_label,
			&&OPCODE_252_label,
			&&OPCODE_253_label,
			&&OPCODE_impdep1_label,
			&&OPCODE_impdep2_label
	};

#endif

	/* we'll come back to this label after a native exception has been thrown and a handler PC located
	 * for it. Coming back to this label means entering a new BVM_TRY block ready to catch
	 * another native exception should one occur. */
	new_try_block:

	if (throwable != NULL) throwable->needs_native_try_reset.int_value = BVM_FALSE;

	BVM_TRY {

		/* we'll come back to this label after a non-native exception has been dealt with - no need to
		 * enter a fresh try/catch block */
		exception_is_handled:

		throwable = NULL;

		while (BVM_TRUE) {

			top_of_interpreter_loop:

			/* the thread switch counter check.  If the counter is zero, a thread switch takes place */
			if (bvm_gl_thread_timeslice_counter-- == 0) {
				bvm_thread_switch();
			}

#if BVM_DEBUGGER_ENABLE

			/* exec loop debugger support. */

			/* only be concerned with debugger stuff in the exec loop if we are actually attached to a debugger */
			if (bvmd_is_session_open()) {

				/* if the current thread was just at a breakpoint, reset the next opcode to be the
				 * one the debug breakpoint opcode has displaced. */
				if (bvm_gl_thread_current->dbg_is_at_breakpoint) {
					opcode = bvm_gl_thread_current->dbg_breakpoint_opcode;
					bvm_gl_thread_current->dbg_is_at_breakpoint = BVM_FALSE;
				} else {

					/* in we go!  Some notes on stepping:  There are three JDWP step 'depths'
					 * - INTO, OVER, and OUT.  Also, JDWP defines two step 'sizes', MIN, which means by
					 * bytecode, and LINE, which means what it says.  The logic for each step
					 * depth if the same regardless of what the size is.  Interestingly, the tests
					 * for each depth type are cumulative.  In the sense that if we are hunting for an INTO,
					 * and we do not have one, then we should test for an OVER, because the line we're on may
					 * have no method calls (or have already stepped into them all).  Failing an OVER, we
					 * should test for an OUT as we (for example) may be on the last instruction for a method (or
					 * the 'return' itself), so stepping OUT of a method actually satisfies an INTO in those
					 * circumstances - it actually works out to be surprisingly simple.
					 *
					 * When the step only event is registered, we store the current executing location for
					 * the thread - before each bytecode is to be executed we compare against this original location
					 * to determine if we have satisfied an INTO/OVER/OUT.
					 */

					if (bvm_gl_thread_current->dbg_is_stepping) {

						/* boolean to hold whether we should do a step event or not - set in the following code */
						bvm_bool_t do_step_event = BVM_FALSE;

						/* calculate the change in the stack depth from the last stepped-to bytecode  */
						int stack_depth_delta = bvm_stack_get_depth(bvm_gl_thread_current) - bvm_gl_thread_current->dbg_step.stack_depth;

						switch (bvm_gl_thread_current->dbg_step.depth) {

							/* going in? */
							case (JDWP_StepDepth_INTO) : {
								/* for an INTO, the frame depth must increase from the last time  */
								do_step_event = (stack_depth_delta > 0);
								if (do_step_event) break;
							}

							/* going over ? */
							case (JDWP_StepDepth_OVER) : {
								/* for an OVER, the 'position' must change within the same method */
                                bvm_native_ulong_t position = bvmd_step_get_position(bvm_gl_thread_current->dbg_step.size, bvm_gl_rx_method, bvm_gl_rx_pc);
								do_step_event = ( ( (bvm_gl_thread_current->dbg_step.method == bvm_gl_rx_method) &&
													(stack_depth_delta == 0) &&
										            (position != bvm_gl_thread_current->dbg_step.position)) );
								if (do_step_event) break;
							}

							/* going out ? */
							case (JDWP_StepDepth_OUT) : {
								/* for an OUT, the frame depth must decrease */
								do_step_event = (stack_depth_delta < 0);
								break;
							}

						}

						/* okay ... if satisfied, a step event is required to be sent to the debugger - perhaps a breakpoint
						 * event also at the same time .. which does make things a little more complex. */
						if (do_step_event) {

							/* are we at a breakpoint ? */
							bvm_bool_t at_breakpoint = *bvm_gl_rx_pc == OPCODE_breakpoint;

							/* create a new event context */
							bvmd_eventcontext_t *context = bvmd_eventdef_new_context(JDWP_EventKind_SINGLE_STEP, bvm_gl_thread_current);

							/* populate the context location */
							context->location.clazz = (bvm_clazz_t *) bvm_gl_rx_clazz;
							context->location.method = bvm_gl_rx_method;
							if (!BVM_METHOD_IsNative(bvm_gl_rx_method))
								context->location.pc_index = bvm_gl_rx_pc - bvm_gl_rx_method->code.bytecode;

							/* and context step info */
							context->step.vmthread = bvm_gl_thread_current;
							context->step.size = bvm_gl_thread_current->dbg_step.size;
							context->step.depth = bvm_gl_thread_current->dbg_step.depth;

							/* do the single-step event for the context noting if we are at a breakpoint or not.  The
							 * return value informs us if we actually sent an event.  */
							bvm_bool_t stepped = bvmd_event_SingleStep(context, at_breakpoint);

							if (at_breakpoint) {

								/* if we have single stepped and we are actually at a breakpoint opcode we
								 * really just want to skip the breakpoint - no point in now breaking on the line
								 * we have just stepped to.  The following logic sets the thread at a breakpoint
								 * and finds the breakpoint's displaced opcode for use as the next opcode to
								 * execute. */

								bvm_gl_thread_current->dbg_is_at_breakpoint = BVM_TRUE;
								if (!bvmd_breakpoint_opcode_for_location(&(context->location), &(bvm_gl_thread_current->dbg_breakpoint_opcode))) {
									/* .. and if we didn't find one, something has gone wrong */
									BVM_VM_EXIT(BVM_FATAL_ERR_NO_BREAKPOINT_FOUND, NULL);
								}
							}

							/* get rid of the event context, no longer used */
							bvmd_eventdef_free_context(context);

							/* .. and then just go to the top of the interp loop, otherwise, just exec the
							 * next bytecode  */
							if (stepped) goto top_of_interpreter_loop;
						}
					}
					opcode = *bvm_gl_rx_pc;
				}
			} else {
				opcode = *bvm_gl_rx_pc;
			}

			OPCODE_DISPATCH(opcode)
#else
			OPCODE_DISPATCH(*bvm_gl_rx_pc)
#endif
				OPCODE_HANDLER(OPCODE_nop) : /* 0 */
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_aconst_null): /* 1 */
					bvm_gl_rx_sp[0].ref_value = NULL;
					bvm_gl_rx_sp++;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_iconst_m1) : /* 2  */
					bvm_gl_rx_sp[0].int_value = -1;
					bvm_gl_rx_sp++;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_iconst_0): /* 3 */
					bvm_gl_rx_sp[0].int_value = 0;
					bvm_gl_rx_sp++;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_iconst_1) : /* 4 */
					bvm_gl_rx_sp[0].int_value = 1;
					bvm_gl_rx_sp++;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_iconst_2) : /* 5 */
					bvm_gl_rx_sp[0].int_value = 2;
					bvm_gl_rx_sp++;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_iconst_3) : /* 6 */
					bvm_gl_rx_sp[0].int_value = 3;
					bvm_gl_rx_sp++;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_iconst_4) : /* 7 */
					bvm_gl_rx_sp[0].int_value = 4;
					bvm_gl_rx_sp++;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_iconst_5) : /* 8 */
					bvm_gl_rx_sp[0].int_value = 5;
					bvm_gl_rx_sp++;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_lconst_0) : /* 9 */
					bvm_gl_rx_sp[0].int_value = 0;
					bvm_gl_rx_sp[1].int_value = 0;
					bvm_gl_rx_sp += 2;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_lconst_1) : /* 10 */
                    bvm_gl_rx_sp[0].int_value = 0;
                    bvm_gl_rx_sp[1].int_value = 1;
					bvm_gl_rx_sp += 2;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_fconst_0) : /* 11 */
#if BVM_FLOAT_ENABLE
		            bvm_gl_rx_sp[0].float_value = 0.0f;
					bvm_gl_rx_sp++;
		            bvm_gl_rx_pc++;
		            OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				OPCODE_HANDLER(OPCODE_fconst_1) : /* 12 */
#if BVM_FLOAT_ENABLE
		            bvm_gl_rx_sp[0].float_value = 1.0f;
					bvm_gl_rx_sp++;
		            bvm_gl_rx_pc++;
		            OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				OPCODE_HANDLER(OPCODE_fconst_2) : /* 13 */
#if BVM_FLOAT_ENABLE
		            bvm_gl_rx_sp[0].float_value = 2.0f;
					bvm_gl_rx_sp++;
		            bvm_gl_rx_pc++;
		            OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				OPCODE_HANDLER(OPCODE_dconst_0) : /* 14 */
#if BVM_FLOAT_ENABLE
                    BVM_DOUBLE_to_cells(bvm_gl_rx_sp, (bvm_double_t) 0.0);
					bvm_gl_rx_sp += 2;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				OPCODE_HANDLER(OPCODE_dconst_1) : /* 15 */
#if BVM_FLOAT_ENABLE
                    BVM_DOUBLE_to_cells(bvm_gl_rx_sp, (bvm_double_t) 1.0);
					bvm_gl_rx_sp += 2;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				OPCODE_HANDLER(OPCODE_bipush) : /* 16 */
					bvm_gl_rx_sp[0].int_value = ((char *)bvm_gl_rx_pc)[1];
					bvm_gl_rx_sp++;
					bvm_gl_rx_pc += 2;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_sipush) : /* 17 */
					bvm_gl_rx_sp[0].int_value = BVM_VM2INT16(bvm_gl_rx_pc+1);
					bvm_gl_rx_sp++;
					bvm_gl_rx_pc += 3;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_ldc) : {/* 18 */

					bvm_uint16_t index = (bvm_uint16_t)bvm_gl_rx_pc[1];

					if (BVM_CONSTANT_Tag(bvm_gl_rx_clazz, index) != BVM_CONSTANT_Class) {
						bvm_gl_rx_sp[0] = bvm_gl_rx_clazz->constant_pool[index].data.value;
#if (!BVM_DEBUGGER_ENABLE)
						/* substitute a go-faster opcode */
						*bvm_gl_rx_pc = OPCODE_ldc_fast_1;
#endif
					} else {
						bvm_gl_rx_sp[0].ref_value = (bvm_obj_t *) bvm_clazz_resolve_cp_clazz(bvm_gl_rx_clazz, index)->class_obj;
#if (!BVM_DEBUGGER_ENABLE)
						/* substitute a go-faster opcode */
						*bvm_gl_rx_pc = OPCODE_ldc_fast_2;
#endif
					}
					bvm_gl_rx_sp++;
					bvm_gl_rx_pc += 2;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_ldc_w) : {/* 19 */

					bvm_uint16_t index = BVM_VM2UINT16(bvm_gl_rx_pc+1);

					if (BVM_CONSTANT_Tag(bvm_gl_rx_clazz, index) != BVM_CONSTANT_Class) {
						bvm_gl_rx_sp[0] = bvm_gl_rx_clazz->constant_pool[index].data.value;
#if (!BVM_DEBUGGER_ENABLE)
						/* substitute a go-faster opcode */
						*bvm_gl_rx_pc = OPCODE_ldc_w_fast_1;
#endif
					} else {
						bvm_gl_rx_sp[0].ref_value = (bvm_obj_t *) bvm_clazz_resolve_cp_clazz(bvm_gl_rx_clazz, index)->class_obj;
#if (!BVM_DEBUGGER_ENABLE)
						/* substitute a go-faster opcode */
						*bvm_gl_rx_pc = OPCODE_ldc_w_fast_2;
#endif
					}
					bvm_gl_rx_sp++;
					bvm_gl_rx_pc += 3;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_ldc2_w) : {/* 20 */
					bvm_uint16_t index = BVM_VM2UINT16(bvm_gl_rx_pc+1);

					bvm_uint32_t high = (bvm_uint32_t) bvm_gl_rx_clazz->constant_pool[index++].data.value.int_value;
					bvm_uint32_t low = (bvm_uint32_t) bvm_gl_rx_clazz->constant_pool[index].data.value.int_value;

                    bvm_gl_rx_sp->int_value = high;
                    (bvm_gl_rx_sp+1)->int_value = low;

					bvm_gl_rx_sp += 2;
					bvm_gl_rx_pc += 3;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_iload): /* 21 */
					bvm_gl_rx_sp[0] = bvm_gl_rx_locals[bvm_gl_rx_pc[1]];
					bvm_gl_rx_sp++;
					bvm_gl_rx_pc += 2;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_lload): /* 22 */
					bvm_gl_rx_sp[0] = bvm_gl_rx_locals[bvm_gl_rx_pc[1]];
					bvm_gl_rx_sp[1] = bvm_gl_rx_locals[bvm_gl_rx_pc[1]+1];
					bvm_gl_rx_sp += 2;
					bvm_gl_rx_pc += 2;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_fload): /* 23 */
					bvm_gl_rx_sp[0] = bvm_gl_rx_locals[bvm_gl_rx_pc[1]];
					bvm_gl_rx_sp++;
					bvm_gl_rx_pc += 2;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_dload): /* 24 */
#if BVM_FLOAT_ENABLE
					bvm_gl_rx_sp[0] = bvm_gl_rx_locals[bvm_gl_rx_pc[1]];
					bvm_gl_rx_sp[1] = bvm_gl_rx_locals[bvm_gl_rx_pc[1]+1];
					bvm_gl_rx_sp += 2;
					bvm_gl_rx_pc += 2;
					OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				OPCODE_HANDLER(OPCODE_aload): /* 25 */
					bvm_gl_rx_sp[0] = bvm_gl_rx_locals[bvm_gl_rx_pc[1]];
					bvm_gl_rx_sp++;
					bvm_gl_rx_pc += 2;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_iload_0): /* 26 */
					bvm_gl_rx_sp[0] = bvm_gl_rx_locals[0];
					bvm_gl_rx_sp++;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_iload_1): /* 27 */
					bvm_gl_rx_sp[0] = bvm_gl_rx_locals[1];
					bvm_gl_rx_sp++;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_iload_2): /* 28 */
					bvm_gl_rx_sp[0] = bvm_gl_rx_locals[2];
					bvm_gl_rx_sp++;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_iload_3): /* 29 */
					bvm_gl_rx_sp[0] = bvm_gl_rx_locals[3];
					bvm_gl_rx_sp++;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_lload_0): /* 30 */
					/* get two values from the stack */
					bvm_gl_rx_sp[0] = bvm_gl_rx_locals[0];
					bvm_gl_rx_sp[1] = bvm_gl_rx_locals[1];
					bvm_gl_rx_sp += 2;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_lload_1): /* 31 */
					/* get two values from the stack */
					bvm_gl_rx_sp[0] = bvm_gl_rx_locals[1];
					bvm_gl_rx_sp[1] = bvm_gl_rx_locals[2];
					bvm_gl_rx_sp += 2;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_lload_2): /* 32 */
					/* get two values from the stack */
					bvm_gl_rx_sp[0] = bvm_gl_rx_locals[2];
					bvm_gl_rx_sp[1] = bvm_gl_rx_locals[3];
					bvm_gl_rx_sp += 2;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_lload_3): /* 33 */
					/* get two values from the stack */
					bvm_gl_rx_sp[0] = bvm_gl_rx_locals[3];
					bvm_gl_rx_sp[1] = bvm_gl_rx_locals[4];
					bvm_gl_rx_sp += 2;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_fload_0): /* 34 */
				OPCODE_HANDLER(OPCODE_fload_1): /* 35 */
				OPCODE_HANDLER(OPCODE_fload_2): /* 36 */
				OPCODE_HANDLER(OPCODE_fload_3): /* 37 */
#if BVM_FLOAT_ENABLE
					/* a shorthand (if slightly confusing way) of getting the float local at 0..3.  The 0..3
					 * is calculated as the current opcode - OPCODE_fload_0.  Note we have not done individual opcode
					 * handlers for each of these float opcodes - on the basis that floats are probably not often
					 * used on a small platform and therefore we save some code space here.*/
					bvm_gl_rx_sp[0] = bvm_gl_rx_locals[CURRENT_OPCODE - OPCODE_fload_0];
					bvm_gl_rx_sp++;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				OPCODE_HANDLER(OPCODE_dload_0): /* 38 */
				OPCODE_HANDLER(OPCODE_dload_1): /* 39 */
				OPCODE_HANDLER(OPCODE_dload_2): /* 40 */
				OPCODE_HANDLER(OPCODE_dload_3): /* 41 */ {
#if BVM_FLOAT_ENABLE
					/* a shorthand (if slightly confusing way) of getting the double local at 0..3.  The 0..3
					 * is calculated as the current opcode - OPCODE_dload_0 Note we have not done individual opcode
					 * handlers for each of these float opcodes - on the basis that double are probably not often
					 * used on a small platform and therefore we save some code space here.*/
                    bvm_double_t value = BVM_DOUBLE_from_cells(&(bvm_gl_rx_locals[CURRENT_OPCODE - OPCODE_dload_0]));
                    BVM_DOUBLE_to_cells(bvm_gl_rx_sp, value);
					bvm_gl_rx_sp += 2;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				}
				OPCODE_HANDLER(OPCODE_aload_0): /* 42 */
					bvm_gl_rx_sp[0] = bvm_gl_rx_locals[0];
					bvm_gl_rx_sp++;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_aload_1): /* 43 */
					bvm_gl_rx_sp[0] = bvm_gl_rx_locals[1];
					bvm_gl_rx_sp++;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_aload_2): /* 44 */
					bvm_gl_rx_sp[0] = bvm_gl_rx_locals[2];
					bvm_gl_rx_sp++;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_aload_3): /* 45 */
					bvm_gl_rx_sp[0] = bvm_gl_rx_locals[3];
					bvm_gl_rx_sp++;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_iaload): {/* 46 */

					bvm_int32_t index = (bvm_int32_t) bvm_gl_rx_sp[-1].int_value;
					bvm_jarray_obj_t *array_obj = (bvm_jarray_obj_t *) bvm_gl_rx_sp[-2].ref_value;

					/* what, no array ? */
					if (array_obj == NULL) {
					    throw_null_pointer_exception();
					}

					/* asking for a silly index? */
					if ( (index < 0) || (index >= array_obj->length.int_value)) {
					    throw_array_index_bounds_exception();
					}

					bvm_gl_rx_sp[-2].int_value = ((bvm_jint_array_obj_t *) array_obj)->data[index];
					bvm_gl_rx_sp--;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_laload): {/* 47 */

					bvm_int32_t index = bvm_gl_rx_sp[-1].int_value;
					bvm_jarray_obj_t *array_obj = (bvm_jarray_obj_t *) bvm_gl_rx_sp[-2].ref_value;

					/* what, no array ? */
					if (array_obj == NULL) {
					    throw_null_pointer_exception();
					}

					/* asking for a silly index? */
					if ( (index < 0) || (index >= array_obj->length.int_value)) {
					    throw_array_index_bounds_exception();
					}

					bvm_int64_t val = ((bvm_jlong_array_obj_t *) array_obj)->data[index];
                    BVM_INT64_to_cells(bvm_gl_rx_sp-2, val);

					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_faload): {/* 48 */

#if BVM_FLOAT_ENABLE
					bvm_int32_t index = bvm_gl_rx_sp[-1].int_value;
					bvm_jarray_obj_t *array_obj = (bvm_jarray_obj_t *) bvm_gl_rx_sp[-2].ref_value;

					/* what, no array ? */
					if (array_obj == NULL) {
					    throw_null_pointer_exception();
					}

					/* asking for a silly index? */
					if ( (index < 0) || (index >= array_obj->length.int_value)) {
					    throw_array_index_bounds_exception();
					}

					bvm_gl_rx_sp[-2].float_value = (bvm_float_t) ((bvm_jfloat_array_obj_t *) array_obj)->data[index];
					bvm_gl_rx_sp--;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				}
				OPCODE_HANDLER(OPCODE_daload): {/* 49 */

#if BVM_FLOAT_ENABLE
					bvm_int32_t index = bvm_gl_rx_sp[-1].int_value;
					bvm_jarray_obj_t *array_obj = (bvm_jarray_obj_t *) bvm_gl_rx_sp[-2].ref_value;

					/* what, no array ? */
					if (array_obj == NULL) {
					    throw_null_pointer_exception();
					}

					/* asking for a silly index? */
					if ( (index < 0) || (index >= array_obj->length.int_value)) {
					    throw_array_index_bounds_exception();
					}

					bvm_double_t value = ((bvm_jdouble_array_obj_t *) array_obj)->data[index];
					BVM_DOUBLE_to_cells(bvm_gl_rx_sp-2, value);

					bvm_gl_rx_pc++;
					OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				}
				OPCODE_HANDLER(OPCODE_aaload): {/* 50 */

					bvm_int32_t index = bvm_gl_rx_sp[-1].int_value;
					bvm_jarray_obj_t *array_obj = (bvm_jarray_obj_t *) bvm_gl_rx_sp[-2].ref_value;

					/* what, no array ? */
					if (array_obj == NULL) {
					    throw_null_pointer_exception();
					}

					/* asking for a silly index? */
					if ( (index < 0) || (index >= array_obj->length.int_value)) {
					    throw_array_index_bounds_exception();
					}

					bvm_gl_rx_sp[-2].ref_value = ((bvm_instance_array_obj_t *) array_obj)->data[index];
					bvm_gl_rx_sp--;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_baload): {/* 51 */

					bvm_int32_t index = bvm_gl_rx_sp[-1].int_value;
					bvm_jarray_obj_t *array_obj = (bvm_jarray_obj_t *) bvm_gl_rx_sp[-2].ref_value;

					/* what, no array ? */
					if (array_obj == NULL) {
					    throw_null_pointer_exception();
					}

					/* asking for a silly index? */
					if ( (index < 0) || (index >= array_obj->length.int_value)) {
					    throw_array_index_bounds_exception();
					}

					bvm_gl_rx_sp[-2].int_value = (bvm_int32_t) ((bvm_jbyte_array_obj_t *) array_obj)->data[index];
					bvm_gl_rx_sp--;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_caload): {/* 52 */

					bvm_int32_t index = bvm_gl_rx_sp[-1].int_value;
					bvm_jarray_obj_t *array_obj = (bvm_jarray_obj_t *) bvm_gl_rx_sp[-2].ref_value;

					/* what, no array ? */
					if (array_obj == NULL) {
					    throw_null_pointer_exception();
					}

					/* asking for a silly index? */
					if ( (index < 0) || (index >= array_obj->length.int_value)) {
					    throw_array_index_bounds_exception();
					}

					bvm_gl_rx_sp[-2].int_value = (bvm_int32_t) ((bvm_jchar_array_obj_t *) array_obj)->data[index];
					bvm_gl_rx_sp--;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_saload): {/* 53 */

					bvm_int32_t index = bvm_gl_rx_sp[-1].int_value;
					bvm_jarray_obj_t *array_obj = (bvm_jarray_obj_t *) bvm_gl_rx_sp[-2].ref_value;

					/* what, no array ? */
					if (array_obj == NULL) {
					    throw_null_pointer_exception();
					}

					/* asking for a silly index? */
					if ( (index < 0) || (index >= array_obj->length.int_value)) {
					    throw_array_index_bounds_exception();
					}

					bvm_gl_rx_sp[-2].int_value = (bvm_int32_t) ((bvm_jshort_array_obj_t *) array_obj)->data[index];
					bvm_gl_rx_sp--;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_istore): /* 54 */
					bvm_gl_rx_locals[bvm_gl_rx_pc[1]] = bvm_gl_rx_sp[-1];
					bvm_gl_rx_sp--;
					bvm_gl_rx_pc+=2;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_lstore): /* 55 */
					bvm_gl_rx_locals[bvm_gl_rx_pc[1]]   = bvm_gl_rx_sp[-2];
					bvm_gl_rx_locals[bvm_gl_rx_pc[1]+1] = bvm_gl_rx_sp[-1];
					bvm_gl_rx_sp-=2;
					bvm_gl_rx_pc+=2;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_fstore): /* 56 */
					bvm_gl_rx_locals[bvm_gl_rx_pc[1]] = bvm_gl_rx_sp[-1];
					bvm_gl_rx_sp--;
					bvm_gl_rx_pc+=2;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_dstore): /* 57 */
#if BVM_FLOAT_ENABLE
					bvm_gl_rx_locals[bvm_gl_rx_pc[1]]   = bvm_gl_rx_sp[-2];
					bvm_gl_rx_locals[bvm_gl_rx_pc[1]+1] = bvm_gl_rx_sp[-1];
					bvm_gl_rx_sp-=2;
					bvm_gl_rx_pc+=2;
					OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				OPCODE_HANDLER(OPCODE_astore): /* 58 */
					bvm_gl_rx_locals[bvm_gl_rx_pc[1]] = bvm_gl_rx_sp[-1];
					bvm_gl_rx_sp--;
					bvm_gl_rx_pc += 2;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_istore_0): /* 59 */
					bvm_gl_rx_locals[0] = bvm_gl_rx_sp[-1];
					bvm_gl_rx_sp--;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_istore_1): /* 60 */
					bvm_gl_rx_locals[1] = bvm_gl_rx_sp[-1];
					bvm_gl_rx_sp--;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_istore_2): /* 61 */
					bvm_gl_rx_locals[2] = bvm_gl_rx_sp[-1];
					bvm_gl_rx_sp--;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_istore_3): /* 62 */
					bvm_gl_rx_locals[3] = bvm_gl_rx_sp[-1];
					bvm_gl_rx_sp--;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_lstore_0): /* 63 */
					bvm_gl_rx_locals[0] = bvm_gl_rx_sp[-2];
					bvm_gl_rx_locals[1] = bvm_gl_rx_sp[-1];
					bvm_gl_rx_sp-=2;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_lstore_1): /* 64 */
					bvm_gl_rx_locals[1] = bvm_gl_rx_sp[-2];
					bvm_gl_rx_locals[2] = bvm_gl_rx_sp[-1];
					bvm_gl_rx_sp-=2;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_lstore_2): /* 65 */
					bvm_gl_rx_locals[2] = bvm_gl_rx_sp[-2];
					bvm_gl_rx_locals[3] = bvm_gl_rx_sp[-1];
					bvm_gl_rx_sp-=2;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_lstore_3): /* 66 */
					bvm_gl_rx_locals[3] = bvm_gl_rx_sp[-2];
					bvm_gl_rx_locals[4] = bvm_gl_rx_sp[-1];
					bvm_gl_rx_sp-=2;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_fstore_0): /* 67 */
				OPCODE_HANDLER(OPCODE_fstore_1): /* 68 */
				OPCODE_HANDLER(OPCODE_fstore_2): /* 69 */
				OPCODE_HANDLER(OPCODE_fstore_3): /* 70 */
#if BVM_FLOAT_ENABLE
					bvm_gl_rx_locals[CURRENT_OPCODE-OPCODE_fstore_0] = bvm_gl_rx_sp[-1];
					bvm_gl_rx_sp--;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				OPCODE_HANDLER(OPCODE_dstore_0): /* 71 */
				OPCODE_HANDLER(OPCODE_dstore_1): /* 72 */
				OPCODE_HANDLER(OPCODE_dstore_2): /* 73 */
				OPCODE_HANDLER(OPCODE_dstore_3): /* 74 */ {
#if BVM_FLOAT_ENABLE
				    // TODO: likely could be done just by copying cell int values
                    bvm_double_t value = BVM_DOUBLE_from_cells(&(bvm_gl_rx_sp[-2]));
                    BVM_DOUBLE_to_cells(&(bvm_gl_rx_locals[CURRENT_OPCODE-OPCODE_dstore_0]), value);
					bvm_gl_rx_sp-=2;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				}
				OPCODE_HANDLER(OPCODE_astore_0): /* 75 */
					bvm_gl_rx_locals[0] = bvm_gl_rx_sp[-1];
					bvm_gl_rx_sp--;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_astore_1): /* 76 */
					bvm_gl_rx_locals[1] = bvm_gl_rx_sp[-1];
					bvm_gl_rx_sp--;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_astore_2): /* 77 */
					bvm_gl_rx_locals[2] = bvm_gl_rx_sp[-1];
					bvm_gl_rx_sp--;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_astore_3): /* 78 */
					bvm_gl_rx_locals[3] = bvm_gl_rx_sp[-1];
					bvm_gl_rx_sp--;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_iastore): {/* 79 */

					/* the array instance */
					bvm_jarray_obj_t *array_obj = (bvm_jarray_obj_t *) bvm_gl_rx_sp[-3].ref_value;

					/* the index in the array to store a reference */
					bvm_int32_t index = bvm_gl_rx_sp[-2].int_value;

					/* no array ? Nasty. */
					if (array_obj == NULL) {
					    throw_null_pointer_exception();
					}

					/* ensure index is in valid range */
					if ( (index < 0) || (index >= array_obj->length.int_value)) {
					    throw_array_index_bounds_exception();
					}

					((bvm_jint_array_obj_t *) array_obj)->data[index] = bvm_gl_rx_sp[-1].int_value;
					bvm_gl_rx_sp -= 3;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_lastore): {/* 80 */

					/* the array instance */
					bvm_jarray_obj_t *array_obj = (bvm_jarray_obj_t *) bvm_gl_rx_sp[-4].ref_value;

					/* the index in the array to store a reference */
					bvm_int32_t index = bvm_gl_rx_sp[-3].int_value;

					/* no array? Nasty. */
					if (array_obj == NULL) {
					    throw_null_pointer_exception();
					}

					/* ensure index is in valid range */
					if ( (index < 0) || (index >= array_obj->length.int_value)) {
					    throw_array_index_bounds_exception();
					}

					bvm_int64_t val = BVM_INT64_from_cells(bvm_gl_rx_sp-2);
                    ((bvm_jlong_array_obj_t *) array_obj)->data[index] = val;

					bvm_gl_rx_sp-=4;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_fastore): {/* 81 */
#if BVM_FLOAT_ENABLE
					/* the array instance */
					bvm_jarray_obj_t *array_obj = (bvm_jarray_obj_t *) bvm_gl_rx_sp[-3].ref_value;

					/* the index in the array to store a reference */
					bvm_int32_t index = bvm_gl_rx_sp[-2].int_value;

					/* no array ? Nasty. */
					if (array_obj == NULL) {
					    throw_null_pointer_exception();
					}

					/* ensure index is in valid range */
					if ( (index < 0) || (index >= array_obj->length.int_value)) {
					    throw_array_index_bounds_exception();
					}

					((bvm_jfloat_array_obj_t *) array_obj)->data[index] = bvm_gl_rx_sp[-1].float_value;
					bvm_gl_rx_sp -= 3;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				}
				OPCODE_HANDLER(OPCODE_dastore): {/* 82 */
#if BVM_FLOAT_ENABLE
					/* the array instance */
					bvm_jarray_obj_t *array_obj = (bvm_jarray_obj_t *) bvm_gl_rx_sp[-4].ref_value;

					/* the index in the array to store a reference */
					bvm_int32_t index = bvm_gl_rx_sp[-3].int_value;

					/* no array? Nasty. */
					if (array_obj == NULL) {
					    throw_null_pointer_exception();
					}

					/* ensure index is in valid range */
					if ( (index < 0) || (index >= array_obj->length.int_value)) {
					    throw_array_index_bounds_exception();
					}

                    bvm_double_t value = BVM_DOUBLE_from_cells(bvm_gl_rx_sp-2);
                   ((bvm_jdouble_array_obj_t *) array_obj)->data[index] = value;

					bvm_gl_rx_sp-=4;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				}
				OPCODE_HANDLER(OPCODE_aastore): {/* 83 */

					/* the array instance */
					bvm_jarray_obj_t *array_obj = (bvm_jarray_obj_t *) bvm_gl_rx_sp[-3].ref_value;

					/* the index in the array to store a reference */
					bvm_int32_t index = bvm_gl_rx_sp[-2].int_value;

					/* no array ? Nasty. */
					if (array_obj == NULL) {
					    throw_null_pointer_exception();
					}

					/* ensure index is in valid range */
					if ( (index < 0) || (index >= array_obj->length.int_value)) {
					    throw_array_index_bounds_exception();
					}

					/* before storing a reference type into an array we must check for assignment compatibility */
					if ( (bvm_gl_rx_sp[-1].ref_value != NULL) &&
						 (!bvm_clazz_is_assignable_from(bvm_gl_rx_sp[-1].ref_value->clazz, array_obj->clazz->component_clazz)) ) {
						bvm_throw_exception(BVM_ERR_ARRAYSTORE_EXCEPTION, NULL);
					}

					((bvm_instance_array_obj_t *) array_obj)->data[index] = bvm_gl_rx_sp[-1].ref_value;

					bvm_gl_rx_sp -= 3;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_bastore): {/* 84 */

					/* the array instance */
					bvm_jarray_obj_t *array_obj = (bvm_jarray_obj_t *) bvm_gl_rx_sp[-3].ref_value;

					/* the index in the array to store a reference */
					bvm_int32_t index = bvm_gl_rx_sp[-2].int_value;

					/* no array ? Nasty. */
					if (array_obj == NULL) {
					    throw_null_pointer_exception();
					}

					/* ensure index is in valid range */
					if ( (index < 0) || (index >= array_obj->length.int_value)) {
					    throw_array_index_bounds_exception();
					}

					((bvm_jbyte_array_obj_t *) array_obj)->data[index] = (bvm_int8_t) bvm_gl_rx_sp[-1].int_value;
					bvm_gl_rx_sp -= 3;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_castore): {/* 85 */

					/* the array instance */
					bvm_jarray_obj_t *array_obj = (bvm_jarray_obj_t *) bvm_gl_rx_sp[-3].ref_value;

					/* the index in the array to store a reference */
					bvm_int32_t index = bvm_gl_rx_sp[-2].int_value;

					/* no array ? Nasty. */
					if (array_obj == NULL) {
					    throw_null_pointer_exception();
					}

					/* ensure index is in valid range */
					if ( (index < 0) || (index >= array_obj->length.int_value)) {
					    throw_array_index_bounds_exception();
					}

					((bvm_jchar_array_obj_t *) array_obj)->data[index] = (bvm_uint16_t) bvm_gl_rx_sp[-1].int_value;
					bvm_gl_rx_sp -= 3;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_sastore): {/* 86 */

					/* the array instance */
					bvm_jarray_obj_t *array_obj = (bvm_jarray_obj_t *) bvm_gl_rx_sp[-3].ref_value;

					/* the index in the array to store a reference */
					bvm_int32_t index = bvm_gl_rx_sp[-2].int_value;

					/* no array ? Nasty. */
					if (array_obj == NULL) {
					    throw_null_pointer_exception();
					}

					/* ensure index is in valid range */
					if ( (index < 0) || (index >= array_obj->length.int_value)) {
					    throw_array_index_bounds_exception();
					}

					((bvm_jshort_array_obj_t *) array_obj)->data[index] = (bvm_int16_t) bvm_gl_rx_sp[-1].int_value;
					bvm_gl_rx_sp -= 3;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_pop): /* 87 */
					bvm_gl_rx_sp--;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_pop2): /* 88 */
					bvm_gl_rx_sp -= 2;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_dup): /* 89 */
					bvm_gl_rx_sp[0] = bvm_gl_rx_sp[-1];
					bvm_gl_rx_sp++;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_dup_x1): /* 90 */
					bvm_gl_rx_sp[0]  = bvm_gl_rx_sp[-1];
					bvm_gl_rx_sp[-1] = bvm_gl_rx_sp[-2];
					bvm_gl_rx_sp[-2] = bvm_gl_rx_sp[0];
					bvm_gl_rx_sp++;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_dup_x2): /* 91 */
					bvm_gl_rx_sp[0]  = bvm_gl_rx_sp[-1];
					bvm_gl_rx_sp[-1] = bvm_gl_rx_sp[-2];
					bvm_gl_rx_sp[-2] = bvm_gl_rx_sp[-3];
					bvm_gl_rx_sp[-3] = bvm_gl_rx_sp[0];
					bvm_gl_rx_sp++;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_dup2): /* 92 */
					bvm_gl_rx_sp[1] = bvm_gl_rx_sp[-1];
					bvm_gl_rx_sp[0] = bvm_gl_rx_sp[-2];
					bvm_gl_rx_sp += 2;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_dup2_x1): /* 93 */
					bvm_gl_rx_sp[1]  = bvm_gl_rx_sp[-1];
					bvm_gl_rx_sp[0]  = bvm_gl_rx_sp[-2];
					bvm_gl_rx_sp[-1] = bvm_gl_rx_sp[-3];
					bvm_gl_rx_sp[-2] = bvm_gl_rx_sp[1];
					bvm_gl_rx_sp[-3] = bvm_gl_rx_sp[0];
					bvm_gl_rx_sp += 2;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_dup2_x2): /* 94 */
					bvm_gl_rx_sp[1]  = bvm_gl_rx_sp[-1];
					bvm_gl_rx_sp[0]  = bvm_gl_rx_sp[-2];
					bvm_gl_rx_sp[-1] = bvm_gl_rx_sp[-3];
					bvm_gl_rx_sp[-2] = bvm_gl_rx_sp[-4];
					bvm_gl_rx_sp[-3] = bvm_gl_rx_sp[1];
					bvm_gl_rx_sp[-4] = bvm_gl_rx_sp[0];
					bvm_gl_rx_sp += 2;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_swap): { /* 95 */
					bvm_cell_t v  = bvm_gl_rx_sp[-2];
					bvm_gl_rx_sp[-2] = bvm_gl_rx_sp[-1];
					bvm_gl_rx_sp[-1] = v;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_iadd): {/* 96 */
                    bvm_int32_t left = (bvm_int32_t) bvm_gl_rx_sp[-2].int_value;
                    bvm_int32_t right = (bvm_int32_t) bvm_gl_rx_sp[-1].int_value;
                    bvm_gl_rx_sp[-2].int_value = (bvm_int32_t) left + right;
                    bvm_gl_rx_sp--;
                    bvm_gl_rx_pc++;
                    OPCODE_NEXT;
                }
				OPCODE_HANDLER(OPCODE_ladd): {/* 97 */
			        bvm_int64_t value1 = BVM_INT64_from_cells(bvm_gl_rx_sp - 4);
			        bvm_int64_t value2 = BVM_INT64_from_cells(bvm_gl_rx_sp - 2);
			        BVM_INT64_increase(value1, value2);
                    BVM_INT64_to_cells(bvm_gl_rx_sp - 4, value1);
			        bvm_gl_rx_sp-=2;
			        bvm_gl_rx_pc++;
			        OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_fadd) : /* 98 */
#if BVM_FLOAT_ENABLE
					bvm_gl_rx_sp[-2].float_value += bvm_gl_rx_sp[-1].float_value;
					bvm_gl_rx_sp--;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				OPCODE_HANDLER(OPCODE_dadd) : { /* 99 */
#if BVM_FLOAT_ENABLE
			        bvm_double_t value1 = BVM_DOUBLE_from_cells(bvm_gl_rx_sp - 4);
			        bvm_double_t value2 = BVM_DOUBLE_from_cells(bvm_gl_rx_sp - 2);
                    BVM_DOUBLE_to_cells(bvm_gl_rx_sp - 4, value1 + value2);
			        bvm_gl_rx_sp-=2;
			        bvm_gl_rx_pc++;
			        OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				}
				OPCODE_HANDLER(OPCODE_isub) : {/* 100 */
                    bvm_int32_t left = (bvm_int32_t) bvm_gl_rx_sp[-2].int_value;
                    bvm_int32_t right = (bvm_int32_t) bvm_gl_rx_sp[-1].int_value;
                    bvm_gl_rx_sp[-2].int_value = (bvm_int32_t) left - right;
                    bvm_gl_rx_sp--;
                    bvm_gl_rx_pc++;
                    OPCODE_NEXT;
                }
				OPCODE_HANDLER(OPCODE_lsub): { /* 101 */
			        bvm_int64_t value1 = BVM_INT64_from_cells(bvm_gl_rx_sp - 4);
			        bvm_int64_t value2 = BVM_INT64_from_cells(bvm_gl_rx_sp - 2);
			        BVM_INT64_decrease(value1, value2);
                    BVM_INT64_to_cells(bvm_gl_rx_sp - 4, value1);
			        bvm_gl_rx_sp-=2;
			        bvm_gl_rx_pc++;
			        OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_fsub): /* 102 */
#if BVM_FLOAT_ENABLE
					bvm_gl_rx_sp[-2].float_value -= bvm_gl_rx_sp[-1].float_value;
					bvm_gl_rx_sp--;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				OPCODE_HANDLER(OPCODE_dsub): {/* 103 */
#if BVM_FLOAT_ENABLE
			        bvm_double_t value1 = BVM_DOUBLE_from_cells(bvm_gl_rx_sp - 4);
			        bvm_double_t value2 = BVM_DOUBLE_from_cells(bvm_gl_rx_sp - 2);
                    BVM_DOUBLE_to_cells(bvm_gl_rx_sp - 4, value1-value2);
			        bvm_gl_rx_sp-=2;
			        bvm_gl_rx_pc++;
			        OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				}
				OPCODE_HANDLER(OPCODE_imul):
                { /* 104 */
                    bvm_int32_t left = (bvm_int32_t) bvm_gl_rx_sp[-2].int_value;
                    bvm_int32_t right = (bvm_int32_t) bvm_gl_rx_sp[-1].int_value;
                    bvm_gl_rx_sp[-2].int_value = (bvm_int32_t) left * right;
                    bvm_gl_rx_sp--;
                    bvm_gl_rx_pc++;
                    OPCODE_NEXT;
                }
				OPCODE_HANDLER(OPCODE_lmul): {/* 105 */
			        bvm_int64_t value1 = BVM_INT64_from_cells(bvm_gl_rx_sp - 4);
			        bvm_int64_t value2 = BVM_INT64_from_cells(bvm_gl_rx_sp - 2);
			        BVM_INT64_to_cells(bvm_gl_rx_sp - 4, BVM_INT64_mul(value1, value2));
			        bvm_gl_rx_sp-=2;
			        bvm_gl_rx_pc++;
			        OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_fmul): /* 106 */
#if BVM_FLOAT_ENABLE
					bvm_gl_rx_sp[-2].float_value *= bvm_gl_rx_sp[-1].float_value;
					bvm_gl_rx_sp--;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				OPCODE_HANDLER(OPCODE_dmul): {/* 107 */
#if BVM_FLOAT_ENABLE
			        bvm_double_t value1 = BVM_DOUBLE_from_cells(bvm_gl_rx_sp - 4);
			        bvm_double_t value2 = BVM_DOUBLE_from_cells(bvm_gl_rx_sp - 2);
                    BVM_DOUBLE_to_cells(bvm_gl_rx_sp - 4, value1 * value2);
			        bvm_gl_rx_sp-=2;
			        bvm_gl_rx_pc++;
			        OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				}
				OPCODE_HANDLER(OPCODE_idiv): {/* 108 */
                    bvm_int32_t left = (bvm_int32_t) bvm_gl_rx_sp[-2].int_value;
                    bvm_int32_t right = (bvm_int32_t) bvm_gl_rx_sp[-1].int_value;
                    if (right == 0) {
                        bvm_throw_exception(BVM_ERR_ARITHMETIC_EXCEPTION, "/ by zero");
                    } else if (left == BVM_MIN_INT && right == -1) {
                        /*
                         * if the dividend is the negative integer of largest possible magnitude for the int
                         * type, and the divisor is -1, then overflow occurs, and the result is equal to the
                         * dividend. Despite the overflow, no exception is thrown in this case.
                         */
                        /* np op. leave the current value on the stack ... */
                    } else {
                        bvm_gl_rx_sp[-2].int_value = (bvm_int32_t) left/right;
                    }
                    bvm_gl_rx_sp--;
                    bvm_gl_rx_pc++;
                    OPCODE_NEXT;
                }
				OPCODE_HANDLER(OPCODE_ldiv): {/* 109 */

			        bvm_int64_t value1 = BVM_INT64_from_cells(bvm_gl_rx_sp - 4);
			        bvm_int64_t value2 = BVM_INT64_from_cells(bvm_gl_rx_sp - 2);

			        if (BVM_INT64_zero_eq(value2)) {
			        	bvm_throw_exception(BVM_ERR_ARITHMETIC_EXCEPTION,"/ by zero");
			        }

			        // JVMS: "if the dividend is the negative integer of largest possible magnitude for the long type and the
			        // divisor is -1, then overflow occurs and the result is equal to the dividend; despite the overflow,
			        // no exception is thrown in this case."
			        if (BVM_INT64_compare_eq(BVM_MIN_LONG, value1) && (BVM_INT64_compare_eq(BVM_INT64_MINUS_ONE, value2))) {
                        /* np op. leave the current value on the stack ... */
			        } else {
                        BVM_INT64_to_cells(bvm_gl_rx_sp - 4, BVM_INT64_div(value1, value2));
			        }

		            bvm_gl_rx_sp-=2;
		            bvm_gl_rx_pc++;
		            OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_fdiv): /* 110 */
#if BVM_FLOAT_ENABLE
					bvm_gl_rx_sp[-2].float_value /= bvm_gl_rx_sp[-1].float_value;
					bvm_gl_rx_sp--;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				OPCODE_HANDLER(OPCODE_ddiv): { /* 111 */
#if BVM_FLOAT_ENABLE
			        bvm_double_t value1 = BVM_DOUBLE_from_cells(bvm_gl_rx_sp - 4);
			        bvm_double_t value2 = BVM_DOUBLE_from_cells(bvm_gl_rx_sp - 2);
                    BVM_DOUBLE_to_cells(bvm_gl_rx_sp - 4, value1 / value2);

		            bvm_gl_rx_sp-=2;
		            bvm_gl_rx_pc++;
		            OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				}
				OPCODE_HANDLER(OPCODE_irem): /* 112 */ {

					bvm_int32_t left = bvm_gl_rx_sp[-2].int_value;
					bvm_int32_t right = bvm_gl_rx_sp[-1].int_value;

					bvm_gl_rx_sp--;

					if (right == 0) {
						bvm_throw_exception(BVM_ERR_ARITHMETIC_EXCEPTION, "zero divisor");
					}

			        if (left == BVM_MIN_INT && right == -1)
						bvm_gl_rx_sp[-1].int_value = 0;
			        else
			        	bvm_gl_rx_sp[-1].int_value = (bvm_int32_t) left % right;

					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_lrem): {/* 113 */

			        bvm_int64_t value1 = BVM_INT64_from_cells(bvm_gl_rx_sp - 4);
			        bvm_int64_t value2 = BVM_INT64_from_cells(bvm_gl_rx_sp - 2);

			        if (BVM_INT64_zero_eq(value2)) {
			        	bvm_throw_exception(BVM_ERR_ARITHMETIC_EXCEPTION, "zero divisor");
			        }

                    // JVMS: "in the special case in which the dividend is the negative long of largest
                    // possible magnitude for its type and the divisor is -1 (the remainder is 0)"
                    if (BVM_INT64_compare_eq(BVM_MIN_LONG, value1) && (BVM_INT64_compare_eq(BVM_INT64_MINUS_ONE, value2))) {
                        /* np op. leave the current value on the stack ... */
                        BVM_INT64_to_cells(bvm_gl_rx_sp - 4, BVM_INT64_ZERO);
                    } else {
                        BVM_INT64_to_cells(bvm_gl_rx_sp - 4, BVM_INT64_rem(value1, value2));
                    }

		            bvm_gl_rx_sp-=2;
			        bvm_gl_rx_pc++;
			        OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_frem): { /* 114 */
#if BVM_FLOAT_ENABLE
					bvm_float_t value1 = bvm_gl_rx_sp[-2].float_value;
					bvm_float_t value2 = bvm_gl_rx_sp[-1].float_value;

		            bvm_gl_rx_sp[-2].float_value = fmodf(value1, value2);

					bvm_gl_rx_sp--;
		            bvm_gl_rx_pc++;
		            OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				}
				OPCODE_HANDLER(OPCODE_drem): { /* 115 */
#if BVM_FLOAT_ENABLE
			        bvm_double_t value1 = BVM_DOUBLE_from_cells(bvm_gl_rx_sp - 4);
			        bvm_double_t value2 = BVM_DOUBLE_from_cells(bvm_gl_rx_sp - 2);
			        bvm_double_t drem;

#ifdef BVM_CPU_X86
                    bvm_int64_t l2 = *(bvm_int64_t *)&value2;
					bvm_int64_t l1 = *(bvm_int64_t *)&value1;

					if (((l2 == BVM_DOUBLE_INFINITY_POS) || (l2 == BVM_DOUBLE_INFINITY_NEG)) &&
						((l1 & BVM_MAX_LONG) < BVM_DOUBLE_INFINITY_POS)) {
						drem = value1;
					} else {
						drem = fmod(value1, value2);
						/* Retrieve the sign bit to find +/- 0.0 */

						if ((l1 & BVM_MIN_LONG) == BVM_MIN_LONG) {
							if ((*(bvm_int64_t *)&drem & BVM_MIN_LONG) != BVM_MIN_LONG) {
								drem *= -1;
							}
						}
					}
#else
					drem = fmod(value1, value2);
#endif

                    BVM_DOUBLE_to_cells(bvm_gl_rx_sp - 4, drem);

		            bvm_gl_rx_sp-=2;
			        bvm_gl_rx_pc++;
			        OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				}
				OPCODE_HANDLER(OPCODE_ineg): /* 116 */
					bvm_gl_rx_sp[-1].int_value = -((bvm_int32_t)bvm_gl_rx_sp[-1].int_value);
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_lneg): {/* 117 */
			        bvm_int64_t value = BVM_INT64_from_cells(bvm_gl_rx_sp - 2);
			        BVM_INT64_negate(value);
                    BVM_INT64_to_cells(bvm_gl_rx_sp - 2, value);
			        bvm_gl_rx_pc++;
			        OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_fneg): /* 118 */
#if BVM_FLOAT_ENABLE
					bvm_gl_rx_sp[-1].float_value = -bvm_gl_rx_sp[-1].float_value;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				OPCODE_HANDLER(OPCODE_dneg): { /* 119 */
#if BVM_FLOAT_ENABLE
			        bvm_double_t value = BVM_DOUBLE_from_cells(bvm_gl_rx_sp - 2);
                    BVM_DOUBLE_to_cells(bvm_gl_rx_sp - 2, -value);
			        bvm_gl_rx_pc++;
			        OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				}
				OPCODE_HANDLER(OPCODE_ishl): { /* 120 */
				    bvm_int32_t left = (bvm_int32_t) bvm_gl_rx_sp[-2].int_value;
			        bvm_int32_t right = ((bvm_int32_t ) bvm_gl_rx_sp[-1].int_value) & 0x0000001F;
					bvm_gl_rx_sp[-2].int_value = (bvm_int32_t) (left << right);
					bvm_gl_rx_sp--;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_lshl): { /* 121 */

					/* JVMS Note: "A long result is calculated by shifting value1 left by s bit positions, where
					 * s is the low 6 bits of value2" - we mask off 'value2' to get 6 the bits */
			        bvm_int32_t s = ((bvm_int32_t ) bvm_gl_rx_sp[-1].int_value) & 0x0000003F;
			        bvm_int64_t value1 = BVM_INT64_from_cells(bvm_gl_rx_sp - 3);

                    BVM_INT64_to_cells(bvm_gl_rx_sp - 3, BVM_INT64_shl(value1, s));

			        bvm_gl_rx_sp--;
			        bvm_gl_rx_pc++;
			        OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_ishr): { /* 122 */
				    bvm_int32_t left = (bvm_int32_t) bvm_gl_rx_sp[-2].int_value;
			        bvm_int32_t s = ((bvm_int32_t) bvm_gl_rx_sp[-1].int_value) & 0x0000001F;
					bvm_gl_rx_sp[-2].int_value = (bvm_int32_t)( left >> s);
					bvm_gl_rx_sp--;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_lshr): {/* 123 */

			        /* JVMS Note: "A long result is calculated by shifting value1 right by s bit positions, with
			         * sign extension, where s is the value of the low 6 bits of value2" - we mask off 'value2' to get 6
			         * the bits */
			        bvm_int32_t s = ((bvm_int32_t) bvm_gl_rx_sp[-1].int_value) & 0x0000003F;
			        bvm_int64_t value1 = BVM_INT64_from_cells(bvm_gl_rx_sp - 3);

                    BVM_INT64_to_cells(bvm_gl_rx_sp - 3, BVM_INT64_shr(value1, s));

			        bvm_gl_rx_sp--;
			        bvm_gl_rx_pc++;
			        OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_iushr): { /* 124 */

					bvm_int32_t left = (bvm_int32_t ) bvm_gl_rx_sp[-2].int_value;
					bvm_int32_t right = ((bvm_int32_t) bvm_gl_rx_sp[-1].int_value) & 0x0000001F;

					bvm_gl_rx_sp[-2].int_value = (bvm_uint32_t) left >> right;

					bvm_gl_rx_sp--;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_lushr): {/* 125 */

			        bvm_int64_t value1 = BVM_INT64_from_cells(bvm_gl_rx_sp - 3);
			        bvm_int32_t s = ((bvm_int32_t) bvm_gl_rx_sp[-1].int_value) & 0x0000003F;

                    BVM_INT64_to_cells(bvm_gl_rx_sp - 3, BVM_INT64_ushr(value1, s));

			        bvm_gl_rx_sp--;
			        bvm_gl_rx_pc++;
			        OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_iand): {/* 126 */
				    bvm_int32_t left = (bvm_int32_t) bvm_gl_rx_sp[-2].int_value;
				    bvm_int32_t right = (bvm_int32_t) bvm_gl_rx_sp[-1].int_value;
					bvm_gl_rx_sp[-2].int_value = (bvm_int32_t) left &right;
					bvm_gl_rx_sp--;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_land): /* 127 */
				    // TODO 64 might not be correct?
					bvm_gl_rx_sp[-3].int_value &= bvm_gl_rx_sp[-1].int_value;
					bvm_gl_rx_sp[-4].int_value &= bvm_gl_rx_sp[-2].int_value;
					bvm_gl_rx_sp-=2;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_ior): { /* 128 */
                    bvm_int32_t left = (bvm_int32_t) bvm_gl_rx_sp[-2].int_value;
                    bvm_int32_t right = (bvm_int32_t) bvm_gl_rx_sp[-1].int_value;
                    bvm_gl_rx_sp[-2].int_value = (bvm_int32_t) left | right;
                    bvm_gl_rx_sp--;
                    bvm_gl_rx_pc++;
                    OPCODE_NEXT;
                }
				OPCODE_HANDLER(OPCODE_lor): /* 129 */
					bvm_gl_rx_sp[-3].int_value |= bvm_gl_rx_sp[-1].int_value;
					bvm_gl_rx_sp[-4].int_value |= bvm_gl_rx_sp[-2].int_value;
					bvm_gl_rx_sp-=2;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_ixor): { /* 130 */
                    bvm_int32_t left = (bvm_int32_t) bvm_gl_rx_sp[-2].int_value;
                    bvm_int32_t right = (bvm_int32_t) bvm_gl_rx_sp[-1].int_value;
                    bvm_gl_rx_sp[-2].int_value = (bvm_int32_t) left ^ right;
                    bvm_gl_rx_sp--;
                    bvm_gl_rx_pc++;
                    OPCODE_NEXT;
                }
				OPCODE_HANDLER(OPCODE_lxor): /* 131 */
					bvm_gl_rx_sp[-3].int_value ^= bvm_gl_rx_sp[-1].int_value;
					bvm_gl_rx_sp[-4].int_value ^= bvm_gl_rx_sp[-2].int_value;
					bvm_gl_rx_sp-=2;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_iinc): {/* 132 */
                    bvm_int32_t left = (bvm_int32_t) bvm_gl_rx_locals[bvm_gl_rx_pc[1]].int_value;
                    bvm_int8_t right = (bvm_int8_t) bvm_gl_rx_pc[2];
                    bvm_gl_rx_locals[bvm_gl_rx_pc[1]].int_value = (bvm_int32_t) left + right;
                    bvm_gl_rx_pc += 3;
                    OPCODE_NEXT;
                }
				OPCODE_HANDLER(OPCODE_i2l): {/* 133 */
			        bvm_int32_t value = bvm_gl_rx_sp[-1].int_value;

                    #if (!BVM_NATIVE_INT64_ENABLE)
                        bvm_int32_t upper = (value < 0) ? 0xFFFFFFFF : 0;
                        #if (BVM_BIG_ENDIAN_ENABLE)
                        bvm_int64_t lvalue = {upper, value}
                        #else
                        bvm_int64_t lvalue = {value, upper};
                        #endif
                        BVM_INT64_to_cells(bvm_gl_rx_sp-1, lvalue);
                    #else
                        BVM_INT64_to_cells(bvm_gl_rx_sp-1, (bvm_int64_t) value);
                    #endif

					bvm_gl_rx_sp++;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_i2f): /* 134 */
#if BVM_FLOAT_ENABLE
					bvm_gl_rx_sp[-1].float_value = (bvm_float_t) ((bvm_int32_t) bvm_gl_rx_sp[-1].int_value);
		            bvm_gl_rx_pc++;
		            OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				OPCODE_HANDLER(OPCODE_i2d): {/* 135 */
#if BVM_FLOAT_ENABLE
                    BVM_DOUBLE_to_cells(bvm_gl_rx_sp-1, (bvm_double_t) ((bvm_int32_t) bvm_gl_rx_sp[-1].int_value));
					bvm_gl_rx_sp++;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				}
				OPCODE_HANDLER(OPCODE_l2i): {/* 136 */

#if (!BVM_NATIVE_INT64_ENABLE)
                    bvm_gl_rx_sp[-2].int_value = (BVM_INT64_from_cells(bvm_gl_rx_sp - 2)).low;
#else
                    bvm_gl_rx_sp[-2].int_value = (bvm_int32_t)(BVM_INT64_from_cells(bvm_gl_rx_sp-2));
#endif

#if (!BVM_NATIVE_INT64_ENABLE)
#if (BVM_BIG_ENDIAN_ENABLE)
            /* do nothing - the lower long half is already on the stack */
#else
            bvm_gl_rx_sp[-2] = bvm_gl_rx_sp[-1];
#endif
#else
            bvm_gl_rx_sp[-2].int_value = (bvm_int32_t)(BVM_INT64_from_cells(bvm_gl_rx_sp-2));
#endif
//					#elif (!BVM_NATIVE_INT64_ENABLE)
//						/* do nothing - the lower long half is already on the stack */
//					#else
//						bvm_gl_rx_sp[-2].int_value = (bvm_int32_t)(BVM_INT64_from_cells(bvm_gl_rx_sp-2));
//					#endif

            /* if little endian it is already in the right place on the stack. */
//					#if BVM_BIG_ENDIAN_ENABLE
//					   bvm_gl_rx_sp[-2] = bvm_gl_rx_sp[-1];
//					#elif (!BVM_NATIVE_INT64_ENABLE)
//						/* do nothing - the lower long half is already ion the stack */
//					#else
//						bvm_gl_rx_sp[-2].int_value = (bvm_int32_t)(BVM_INT64_from_cells(bvm_gl_rx_sp-2));
//					#endif

                    bvm_gl_rx_sp--;
                    bvm_gl_rx_pc++;
                    OPCODE_NEXT;
                }
				OPCODE_HANDLER(OPCODE_l2f): {/* 137 */
#if BVM_FLOAT_ENABLE
					bvm_float_t fl;
			        bvm_int64_t value = BVM_INT64_from_cells(bvm_gl_rx_sp-2);

#if (!BVM_NATIVE_INT64_ENABLE)
			        fl = (bvm_float_t) ((bvm_int32_t) value->low);
#else
			        fl = (bvm_float_t) value;
#endif
			        bvm_gl_rx_sp[-2].float_value = fl;
			        bvm_gl_rx_sp--;
			        bvm_gl_rx_pc++;
			        OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				}
				OPCODE_HANDLER(OPCODE_l2d): {/* 138 */
#if BVM_FLOAT_ENABLE
				    // NOTE: I'm not so sure this actually does anything ...
				    BVM_DOUBLE_to_cells(bvm_gl_rx_sp-2, (bvm_double_t) BVM_INT64_from_cells(bvm_gl_rx_sp-2) );
			        bvm_gl_rx_pc++;
			        OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				}
				OPCODE_HANDLER(OPCODE_f2i): /* 139 */ {
#if BVM_FLOAT_ENABLE
					bvm_float_t f = bvm_gl_rx_sp[-1].float_value;
					bvm_int32_t i = (bvm_int32_t) f;

					if (f != f) i = 0; /* NaN */
					else if (f > 2147483647.0) i = BVM_MAX_INT;
					else if (f < -2147483648.0) i = BVM_MIN_INT;

		        	bvm_gl_rx_sp[-1].int_value = (bvm_int32_t) i;
			        bvm_gl_rx_pc++;
			        OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				}
				OPCODE_HANDLER(OPCODE_f2l): { /* 140 */
#if BVM_FLOAT_ENABLE
					bvm_float_t f = bvm_gl_rx_sp[-1].float_value;
					bvm_int64_t l = (bvm_int64_t) f;

					if (f != f) l = 0;  /* NaN */
					else if ( f > 9223372036854775807.0 ) l = BVM_MAX_LONG;
					else if ( f < -9223372036854775808.0 ) l = BVM_MIN_LONG;

		        	BVM_INT64_to_cells(bvm_gl_rx_sp-1, l);

		        	bvm_gl_rx_sp++;
			        bvm_gl_rx_pc++;
			        OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				}
				OPCODE_HANDLER(OPCODE_f2d): {/* 141 */
#if BVM_FLOAT_ENABLE
                    BVM_DOUBLE_to_cells(bvm_gl_rx_sp-1, (bvm_double_t) bvm_gl_rx_sp[-1].float_value);
		        	bvm_gl_rx_sp++;
			        bvm_gl_rx_pc++;
			        OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				}
				OPCODE_HANDLER(OPCODE_d2i): {/* 142 */
#if BVM_FLOAT_ENABLE
					bvm_double_t d = BVM_DOUBLE_from_cells(bvm_gl_rx_sp-2);
					bvm_int32_t i = (bvm_int32_t) d;

					if (d != d) i = 0;  /* NaN */
					else if (d > 2147483647.0) i = BVM_MAX_INT;
					else if (d < -2147483648.0) i = BVM_MIN_INT;

					bvm_gl_rx_sp[-2].int_value = (bvm_int32_t) i;

					bvm_gl_rx_sp-=1;
			        bvm_gl_rx_pc++;
			        OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				}
				OPCODE_HANDLER(OPCODE_d2l): {/* 143 */
#if BVM_FLOAT_ENABLE
					bvm_double_t d = BVM_DOUBLE_from_cells(bvm_gl_rx_sp-2);
					bvm_int64_t l = (bvm_int64_t) d;

					if (d != d) l = 0;  /* NaN */
					else if ( d > 9223372036854775807.0 ) l = BVM_MAX_LONG;
					else if ( d < -9223372036854775808.0 ) l = BVM_MIN_LONG;

                    BVM_INT64_to_cells(bvm_gl_rx_sp-2, l);
			        bvm_gl_rx_pc++;
			        OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				}
				OPCODE_HANDLER(OPCODE_d2f): /* 144 */ {
#if BVM_FLOAT_ENABLE
					bvm_gl_rx_sp[-2].float_value = (bvm_float_t) BVM_DOUBLE_from_cells(bvm_gl_rx_sp-2);
		        	bvm_gl_rx_sp-=1;
			        bvm_gl_rx_pc++;
			        OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				}
				OPCODE_HANDLER(OPCODE_i2b): /* 145 */
					bvm_gl_rx_sp[-1].int_value = (bvm_int32_t)((bvm_int8_t)(((bvm_int32_t)bvm_gl_rx_sp[-1].int_value) & 0xFF));
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_i2c): /* 146 */
					bvm_gl_rx_sp[-1].int_value = (bvm_int32_t)((bvm_uint16_t)(((bvm_int32_t)bvm_gl_rx_sp[-1].int_value) & 0xFFFF));
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_i2s): /* 147 */
					bvm_gl_rx_sp[-1].int_value = (bvm_int32_t)((bvm_int16_t)(((bvm_int32_t)bvm_gl_rx_sp[-1].int_value) & 0xFFFF));
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_lcmp): {/* 148 */
			        bvm_int32_t result;

			        bvm_int64_t value1 = BVM_INT64_from_cells(bvm_gl_rx_sp-4);
			        bvm_int64_t value2 = BVM_INT64_from_cells(bvm_gl_rx_sp-2);

                    result = BVM_INT64_compare_eq(value1, value2) ? 0 : BVM_INT64_compare_lt(value1, value2) ? -1 : 1;
			        bvm_gl_rx_sp[-4].int_value = (bvm_int32_t) result;

			        bvm_gl_rx_sp -= 3;
			        bvm_gl_rx_pc++;
			        OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_fcmpl): /* 149 */
				OPCODE_HANDLER(OPCODE_fcmpg): { /* 150 */
#if BVM_FLOAT_ENABLE
					bvm_float_t value1 = bvm_gl_rx_sp[-2].float_value;
					bvm_int32_t i1 = bvm_gl_rx_sp[-2].int_value;

					bvm_float_t value2 = bvm_gl_rx_sp[-1].float_value;
					bvm_int32_t i2 = bvm_gl_rx_sp[-1].int_value;

					bvm_gl_rx_sp--;

					if ( bvm_fp_is_fnan(i1) || bvm_fp_is_fnan(i2) )  {
						/* OPCODE_fcmpl and OPCODE_fcmpg differ only in the return code for a NaN */
						bvm_gl_rx_sp[-1].int_value = (CURRENT_OPCODE == OPCODE_fcmpl) ? -1 : 1;
					} else {
						bvm_gl_rx_sp[-1].int_value = (value1 >  value2) ?  1 : (value1 == value2) ?  0 : -1;
					}

			        bvm_gl_rx_pc++;
			        OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				}
				OPCODE_HANDLER(OPCODE_dcmpl): /* 151 */
				OPCODE_HANDLER(OPCODE_dcmpg): { /* 152 */
#if BVM_FLOAT_ENABLE
			        bvm_double_t value1 = BVM_DOUBLE_from_cells(bvm_gl_rx_sp-4);
			        bvm_int64_t l1 = BVM_INT64_from_cells(bvm_gl_rx_sp-4);

			        bvm_double_t value2 = BVM_DOUBLE_from_cells(bvm_gl_rx_sp-2);
			        bvm_int64_t l2 = BVM_INT64_from_cells(bvm_gl_rx_sp-2);

					bvm_gl_rx_sp-=3;

					if( bvm_fp_is_dnan(l1) || bvm_fp_is_dnan(l2) )  {
						/* OPCODE_dcmpl and OPCODE_dcmpg differ only in the return code for a NaN */
				        bvm_gl_rx_sp[-1].int_value = (CURRENT_OPCODE == OPCODE_dcmpl) ? -1 : 1;
					} else {
						bvm_gl_rx_sp[-1].int_value = (value1 >  value2) ?  1 : (value1 == value2) ?  0 : -1;
					}

			        bvm_gl_rx_pc++;
			        OPCODE_NEXT;
#else
					throw_unsupported_feature_exception_float();
#endif
				}
				OPCODE_HANDLER(OPCODE_ifeq): /* 153 */
					if ((bvm_int32_t)bvm_gl_rx_sp[-1].int_value == 0)
						bvm_gl_rx_pc += BVM_VM2INT16(bvm_gl_rx_pc+1);
					else
						bvm_gl_rx_pc += 3;
					bvm_gl_rx_sp--;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_ifne): /* 154 */
					if ((bvm_int32_t)bvm_gl_rx_sp[-1].int_value != 0)
						bvm_gl_rx_pc += BVM_VM2INT16(bvm_gl_rx_pc+1);
					else
						bvm_gl_rx_pc += 3;
					bvm_gl_rx_sp--;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_iflt): /* 155 */
					if ((bvm_int32_t)bvm_gl_rx_sp[-1].int_value < 0)
						bvm_gl_rx_pc += BVM_VM2INT16(bvm_gl_rx_pc+1);
					else
						bvm_gl_rx_pc += 3;
					bvm_gl_rx_sp--;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_ifge): /* 156 */
					if ((bvm_int32_t)bvm_gl_rx_sp[-1].int_value >= 0)
						bvm_gl_rx_pc += BVM_VM2INT16(bvm_gl_rx_pc+1);
					else
						bvm_gl_rx_pc += 3;
					bvm_gl_rx_sp--;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_ifgt): /* 157 */
					if ((bvm_int32_t)bvm_gl_rx_sp[-1].int_value > 0)
						bvm_gl_rx_pc += BVM_VM2INT16(bvm_gl_rx_pc+1);
					else
						bvm_gl_rx_pc += 3;
					bvm_gl_rx_sp--;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_ifle): /* 158 */
					if ((bvm_int32_t)bvm_gl_rx_sp[-1].int_value <= 0)
						bvm_gl_rx_pc += BVM_VM2INT16(bvm_gl_rx_pc+1);
					else
						bvm_gl_rx_pc += 3;
					bvm_gl_rx_sp--;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_if_icmpeq): /* 159 */
					if ((bvm_int32_t)bvm_gl_rx_sp[-2].int_value == (bvm_int32_t)bvm_gl_rx_sp[-1].int_value)
						bvm_gl_rx_pc += BVM_VM2INT16(bvm_gl_rx_pc+1);
					else
						bvm_gl_rx_pc += 3;
					bvm_gl_rx_sp -= 2;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_if_icmpne): /* 160 */
					if ((bvm_int32_t)bvm_gl_rx_sp[-2].int_value != (bvm_int32_t)bvm_gl_rx_sp[-1].int_value)
						bvm_gl_rx_pc += BVM_VM2INT16(bvm_gl_rx_pc+1);
					else
						bvm_gl_rx_pc += 3;
					bvm_gl_rx_sp -= 2;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_if_icmplt): /* 161 */
					if ((bvm_int32_t)bvm_gl_rx_sp[-2].int_value < (bvm_int32_t)bvm_gl_rx_sp[-1].int_value)
						bvm_gl_rx_pc += BVM_VM2INT16(bvm_gl_rx_pc+1);
					else
						bvm_gl_rx_pc += 3;
					bvm_gl_rx_sp -= 2;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_if_icmpge): /* 162 */
					if ((bvm_int32_t)bvm_gl_rx_sp[-2].int_value >= (bvm_int32_t)bvm_gl_rx_sp[-1].int_value)
						bvm_gl_rx_pc += BVM_VM2INT16(bvm_gl_rx_pc+1);
					else
						bvm_gl_rx_pc += 3;
					bvm_gl_rx_sp -= 2;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_if_icmpgt): /* 163 */
					if ((bvm_int32_t)bvm_gl_rx_sp[-2].int_value > (bvm_int32_t)bvm_gl_rx_sp[-1].int_value)
						bvm_gl_rx_pc += BVM_VM2INT16(bvm_gl_rx_pc+1);
					else
						bvm_gl_rx_pc += 3;
					bvm_gl_rx_sp -= 2;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_if_icmple): /* 164 */
					if ((bvm_int32_t)bvm_gl_rx_sp[-2].int_value <= (bvm_int32_t)bvm_gl_rx_sp[-1].int_value)
						bvm_gl_rx_pc += BVM_VM2INT16(bvm_gl_rx_pc+1);
					else
						bvm_gl_rx_pc += 3;
					bvm_gl_rx_sp -= 2;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_if_acmpeq): /* 165 */
					if (bvm_gl_rx_sp[-2].ref_value == bvm_gl_rx_sp[-1].ref_value)
						bvm_gl_rx_pc += BVM_VM2INT16(bvm_gl_rx_pc+1);
					else
						bvm_gl_rx_pc += 3;
					bvm_gl_rx_sp -= 2;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_if_acmpne): /* 166 */
					if (bvm_gl_rx_sp[-2].ref_value != bvm_gl_rx_sp[-1].ref_value)
						bvm_gl_rx_pc += BVM_VM2INT16(bvm_gl_rx_pc+1);
					else
						bvm_gl_rx_pc += 3;
					bvm_gl_rx_sp -= 2;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_goto): /* 167 */
					bvm_gl_rx_pc += BVM_VM2INT16(bvm_gl_rx_pc+1);
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_jsr): /* 168 */
					bvm_gl_rx_sp[0].ptr_value = bvm_gl_rx_pc + 3;
					bvm_gl_rx_sp++;
					bvm_gl_rx_pc += BVM_VM2INT16(bvm_gl_rx_pc+1);
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_ret): /* 169 */
					bvm_gl_rx_pc = bvm_gl_rx_sp[bvm_gl_rx_pc[1]].ptr_value;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_tableswitch): {/* 170  */

                    bvm_int32_t index = bvm_gl_rx_sp[-1].int_value;
                    bvm_uint8_t *rxpc = bvm_gl_rx_pc + 1;

                    // the tableswitch/lookupswitch operands are 4 byte aligned.
                    rxpc += (4 - ((rxpc - bvm_gl_rx_method->code.bytecode) % 4)) % 4;

                    bvm_int32_t defaultRxpc = BVM_VM2INT32(rxpc);
                    rxpc += 4;

                    bvm_int32_t low = BVM_VM2INT32(rxpc);
                    rxpc += 4;

                    bvm_int32_t high = BVM_VM2INT32(rxpc);
                    rxpc += 4;

					if (index < low || index > high)
						bvm_gl_rx_pc += defaultRxpc;
					else
						bvm_gl_rx_pc += BVM_VM2INT32(&rxpc[(index - low) * 4]);

					bvm_gl_rx_sp--;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_lookupswitch): {/* 171 */

					bvm_int32_t i, low, mid, high;

                    bvm_int32_t key = bvm_gl_rx_sp[-1].int_value;
                    bvm_uint8_t *rxpc = bvm_gl_rx_pc + 1;

                    // the tableswitch/lookupswitch operands are 4 byte aligned.
                    rxpc += (4 - ((rxpc - bvm_gl_rx_method->code.bytecode) % 4)) % 4;

                    bvm_int32_t defaultRxpc = BVM_VM2INT32(rxpc);
                    rxpc += 4;

                    bvm_int32_t pairsCnt = BVM_VM2INT32(rxpc);
                    rxpc += 4;

                    /* JVMS: "The match-offset pairs are sorted to support lookup routines that are
                     * quicker than linear search".  The following is simple binary search */
					if (pairsCnt > 0) {
						low = 0;
						high = pairsCnt;
						while (BVM_TRUE) {
							mid = (high + low) / 2;
							// next candidate key
							i = BVM_VM2INT32(rxpc + (mid * 8));
							if (key == i) {
							    // found.  To offset given after candidate key
								bvm_gl_rx_pc += BVM_VM2INT32(rxpc + (mid * 8)+ 4);
								break;
							}
							if (mid == low) {
							    // not found.  Go to default offset.
								bvm_gl_rx_pc += defaultRxpc;
								break;
							}
							if (key < i)
								high = mid;
							else
								low = mid;
						}
					} else {
						bvm_gl_rx_pc += defaultRxpc;
					}

					bvm_gl_rx_sp--;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_ireturn): /* 172 */
				OPCODE_HANDLER(OPCODE_lreturn): /* 173 */
				OPCODE_HANDLER(OPCODE_freturn): /* 174 */
				OPCODE_HANDLER(OPCODE_dreturn): /* 175 */
				OPCODE_HANDLER(OPCODE_areturn): /* 176 */
				OPCODE_HANDLER(OPCODE_return):  /* 177 */
				doreturn: {

					bvm_cell_t res1;
					bvm_cell_t res2;

					/* number of cells returned by the current method 0, 1, or 2. */
					bvm_uint8_t nr_return_cells = bvm_gl_rx_method->returns_value;

					/* release monitor if the current method is synchronised */
					if (BVM_METHOD_IsSynchronized(bvm_gl_rx_method)) {
						bvm_thread_monitor_release(BVM_STACK_ExecMethodSyncObject());
					}

					/* If we are expecting a return value, store it for a moment - we'll
					 * need the value after a frame pop. Rather than expend cycles by making a
					 * decision about whether we do or not, or how many cells are actually
					 * returned, we'll just store two cells from the top of the stack (to make
					 * sure we cater for a 'long' return. We may or may not require them later. */
					res1 = bvm_gl_rx_sp[-1];
					res2 = bvm_gl_rx_sp[-2];

					/* pop a return frame off of the stack */
					bvm_frame_pop();

					/* if the method after the pop is actually a callback wedge (and has a
					 * callback) - call it.  After the callback, if the number of non-daemon threads has been reduced
					 * to zero (hedging bets here in case the callback is a thread termination callback)
					 * then stop the interp loop.
					 */
					if (bvm_gl_rx_method == BVM_METHOD_CALLBACKWEDGE) {

						if (bvm_gl_rx_locals[1].callback != NULL) {
							bvm_gl_rx_locals[1].callback(&res1, &res2, BVM_FALSE, bvm_gl_rx_locals[2].ref_value);
							if (bvm_gl_thread_nondaemon_count == 0 ) return;

							/* callbacks do not return any value, so go to the top of the interp loop. */
							goto top_of_interpreter_loop;
						}
					}

					/* if there is a return value, push it on the stack. A long/double returns pushes 2,
					 * normal return push 1*/
					if (nr_return_cells == 0) {
						/* noop */
					} else if (nr_return_cells == 1) {
						bvm_gl_rx_sp[0] = res1;
						bvm_gl_rx_sp++;
					} else if (nr_return_cells == 2) {
						bvm_gl_rx_sp[0] = res2;
						bvm_gl_rx_sp[1] = res1;
						bvm_gl_rx_sp += 2;
					}
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_getstatic): {/* 178 */

					bvm_field_t *field;
					bvm_uint16_t index = BVM_VM2UINT16(bvm_gl_rx_pc+1);

					/* resolve static field */
					field = bvm_clazz_resolve_cp_field(bvm_gl_rx_clazz, index, BVM_TRUE);

					/* if class it not initialised, then do not move the pc/sp.  The
					 * initialise class will push a frame onto the stack, so we'll eventually get
					 * back here sometime after the class is initialised. */
					if (!BVM_CLAZZ_IsInitialised(field->clazz)) {
                        if (bvm_clazz_initialise(field->clazz)) goto top_of_interpreter_loop;
					}

					/* change opcode so that it runs faster next time. */
					if (BVM_FIELD_IsLong(field)) {
#if (!BVM_DEBUGGER_ENABLE)
						*bvm_gl_rx_pc = OPCODE_getstatic_fast_long;
#endif
						/* push the field static 64 bit value onto the stack - works for longs
						 * and doubles - we're copying the bits ... not the int/double 'value' .... */
						bvm_uint64_t val = *(bvm_uint64_t*)(field->value.static_value.ptr_value);
						BVM_INT64_to_cells(bvm_gl_rx_sp, val);
						bvm_gl_rx_sp++;
					} else {
#if (!BVM_DEBUGGER_ENABLE)
						*bvm_gl_rx_pc = OPCODE_getstatic_fast;
#endif
						/* push the field static value onto the stack */
						bvm_gl_rx_sp[0] = field->value.static_value;
					}

					bvm_gl_rx_sp++;
					bvm_gl_rx_pc += 3;

					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_putstatic): {/* 179 */

					bvm_field_t *field;
					bvm_uint16_t index = BVM_VM2UINT16(bvm_gl_rx_pc+1);

					/* resolve static field */
					field = bvm_clazz_resolve_cp_field(bvm_gl_rx_clazz, index, BVM_TRUE);

#if BVM_BINARY_COMPAT_CHECKING_ENABLE
					/* "if the field is final, it must be declared in the current
					 * class. Otherwise, an IllegalAccessError is thrown" */
					if (BVM_FIELD_IsFinal(field) && (field->clazz != bvm_gl_rx_clazz)) {
						bvm_throw_exception(BVM_ERR_ILLEGAL_ACCESS_ERROR, NULL);
					}
#endif

					/* if class it not initialised, then do not move the pc/sp.  The
					 * initialise_class will push a frame onto the stack, so we'll eventually get
					 * back here sometime after the class is initialised. */
					if (!BVM_CLAZZ_IsInitialised(field->clazz)) {
						if (bvm_clazz_initialise(field->clazz)) goto top_of_interpreter_loop;
					}

					/* if we got here we're all okay - the class is initialised and the field is
					 * resolved.  Next time, if the value is not a long, make life easier and do it a bit
					 * faster with a fast opcode */

					/* change opcode so that it runs faster next time. */
					if (BVM_FIELD_IsLong(field)) {
#if (!BVM_DEBUGGER_ENABLE)
						*bvm_gl_rx_pc = OPCODE_putstatic_fast_long;
#endif
						/* copy the 64 value from the stack into the field - works for longs and doubles
						 * as we're effectively copying the bits .. not the value */
						bvm_int64_t val = BVM_INT64_from_cells(bvm_gl_rx_sp-2);
                        (*(bvm_int64_t*)(field->value.static_value.ptr_value)) = val;
						bvm_gl_rx_sp--;
					} else {
#if (!BVM_DEBUGGER_ENABLE)
						*bvm_gl_rx_pc = OPCODE_putstatic_fast;
#endif
						/* push the field static value into the field */
						field->value.static_value = bvm_gl_rx_sp[-1];
					}

					bvm_gl_rx_sp--;
					bvm_gl_rx_pc += 3;

					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_getfield): {/* 180 */

					bvm_field_t *field;
					bvm_obj_t *obj = bvm_gl_rx_sp[-1].ref_value;
					bvm_uint16_t index = BVM_VM2UINT16(bvm_gl_rx_pc+1);

					/* No object?  Bang out. */
					if (obj == NULL) throw_null_pointer_exception();

					/* resolve non-static field */
					field = bvm_clazz_resolve_cp_field(bvm_gl_rx_clazz, index, BVM_FALSE);

					/* change opcode so that it runs faster next time. */
					if (BVM_FIELD_IsLong(field)) {
#if (!BVM_DEBUGGER_ENABLE)
						*bvm_gl_rx_pc = OPCODE_getfield_fast_long;
#endif
						/* get the two halves of the value */
						bvm_gl_rx_sp[-1] = obj->fields[field->value.offset];
						bvm_gl_rx_sp[0]  = obj->fields[field->value.offset+1];
						bvm_gl_rx_sp++;
					} else {
#if (!BVM_DEBUGGER_ENABLE)
						*bvm_gl_rx_pc = OPCODE_getfield_fast;
#endif
						/* get the value of the object at the offset given by the field. */
						bvm_gl_rx_sp[-1] = obj->fields[field->value.offset];
					}

					bvm_gl_rx_pc += 3;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_putfield): { /* 181 */

					bvm_field_t *field;
					int stack_pos;
					bvm_obj_t *obj;
					bvm_int16_t index = BVM_VM2UINT16(bvm_gl_rx_pc+1);

					/* resolve non-static field */
					field = bvm_clazz_resolve_cp_field(bvm_gl_rx_clazz, index, BVM_FALSE);

					/* if the field is a long field we need to dig further into the stack to
					 * get our value */
					stack_pos = BVM_FIELD_IsLong(field) ? -3 : -2;

					obj = bvm_gl_rx_sp[stack_pos].ref_value;

					/* No object?  Bang out. */
					if (obj == NULL) throw_null_pointer_exception();

#if BVM_BINARY_COMPAT_CHECKING_ENABLE
					/* "if the field is final, it must be declared in the current
					 * class. Otherwise, an IllegalAccessError is thrown" */
					if (BVM_FIELD_IsFinal(field) && (field->clazz != bvm_gl_rx_clazz)) {
						bvm_throw_exception(BVM_ERR_ILLEGAL_ACCESS_ERROR, NULL);
					}
#endif
					/* change opcode so that it runs faster next time. */
					if (BVM_FIELD_IsLong(field)) {
#if (!BVM_DEBUGGER_ENABLE)
						*bvm_gl_rx_pc = OPCODE_putfield_fast_long;
#endif
						/* set the value of the object at the offset given by the field plus the
						 * extra one for a long. */
						obj->fields[field->value.offset]   = bvm_gl_rx_sp[-2];
						obj->fields[field->value.offset+1] = bvm_gl_rx_sp[-1];
						bvm_gl_rx_sp--;
					} else {
#if (!BVM_DEBUGGER_ENABLE)
						*bvm_gl_rx_pc = OPCODE_putfield_fast;
#endif
						/* set the value of the object at the offset given by the field. */
						obj->fields[field->value.offset] = bvm_gl_rx_sp[-1];
					}

					bvm_gl_rx_sp -= 2;
					bvm_gl_rx_pc += 3;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_invokevirtual): { /* 182 */

					bvm_method_t *resolved_method;

					bvm_uint16_t method_index;

					bvm_instance_clazz_t *search_clazz;

					method_index = BVM_VM2INT16(bvm_gl_rx_pc+1);

					if (BVM_CONSTANT_IsOptimised(bvm_gl_rx_clazz,method_index))
						resolved_method = bvm_gl_rx_clazz->constant_pool[method_index].resolved_ptr;
					else
						resolved_method = bvm_clazz_resolve_cp_method(bvm_gl_rx_clazz, method_index);

					invoke_method = resolved_method;

					invoke_clazz = invoke_method->clazz;

					invoke_nr_args = invoke_method->num_args+1;

					/* the object ref_value we are making a call upon */
					invoke_obj = bvm_gl_rx_sp[-invoke_nr_args].ref_value;

					/* if no object throw a NullPointerException */
					if (invoke_obj == NULL) throw_null_pointer_exception();

					/* if it is the same class, let's assume we have found it, otherwise, begin the hunt ... */
					if ( invoke_clazz != (bvm_instance_clazz_t *)  invoke_obj->clazz) {

						/* the class of the object reference we are making the method call upon */
						search_clazz = (bvm_instance_clazz_t *) invoke_obj->clazz;

						/* if ref_value object clazz is not an instance clazz, defer all method
						 * invocations to the Object class. *Except* if it is the 'clone' method.
						 * This is the only method supported by arrays. We handle it
						 * specially by doing a shallow memory copy of the object. */
						if (!BVM_CLAZZ_IsInstanceClazz(search_clazz)) {
							if (BVM_CLAZZ_IsArrayClazz(search_clazz) &&
									strncmp("clone", (char *) invoke_method->name->data, 5) == 0) {
								bvm_gl_rx_sp -= invoke_nr_args;
								bvm_gl_rx_sp[0].ptr_value = bvm_heap_clone(invoke_obj);
								bvm_gl_rx_sp++;
								bvm_gl_rx_pc += 3;
								goto top_of_interpreter_loop;
							} else
								search_clazz = BVM_OBJECT_CLAZZ;
						}

						invoke_method = locate_virtual_method(search_clazz, resolved_method);
						invoke_clazz = invoke_method->clazz;
					}

#if BVM_BINARY_COMPAT_CHECKING_ENABLE
					/* well, if we *did* find one and it is static throw an exception. There has
					 * obviously been a change in compatibility */
					if (BVM_METHOD_IsStatic(invoke_method)) {
						bvm_throw_exception(BVM_ERR_INCOMPATIBLE_CLASS_CHANGE_ERROR, NULL);
					}
#endif

#if (!BVM_DEBUGGER_ENABLE)
					/* substitute a go-faster opcode - if the resolved method is final or private, or the class
					 * of the resolved method is final we really do not need to go through method lookup stuff again -
					 * the resolved method does not so no need to be dynamically looked up.
					 *
					 * Note that we do not have a specific 'fast' opcode for invokevirtual. There is not really a lot
					 * that we can do.  Using the invokespecial_fast opcode optimisation here is making
					 * use of the fact that with final/private methods or final classes, the resolved
					 * method will be the same as the dynamically found one always - so we just do not look it up
					 * after the first time. */
	                if ( (resolved_method->access_flags & (BVM_METHOD_ACCESS_PRIVATE | BVM_METHOD_ACCESS_FINAL))
	                    || (resolved_method->clazz->access_flags & BVM_CLASS_ACCESS_FINAL) ) {
	                    *bvm_gl_rx_pc = OPCODE_230_invokespecial_fast;
	                }
#endif
					invoke_pc_offset = 3;

					goto do_method_invocation;
				}
				OPCODE_HANDLER(OPCODE_invokespecial): {/* 183 */

					bvm_uint16_t method_index = BVM_VM2INT16(bvm_gl_rx_pc+1);

					if (BVM_CONSTANT_IsOptimised(bvm_gl_rx_clazz,method_index))
						invoke_method = bvm_gl_rx_clazz->constant_pool[method_index].resolved_ptr;
					else
						invoke_method = bvm_clazz_resolve_cp_method(bvm_gl_rx_clazz, method_index);

					invoke_clazz = invoke_method->clazz;

					invoke_nr_args = invoke_method->num_args+1;

					/* the object ref_value we are making a call upon */
					invoke_obj = bvm_gl_rx_sp[-invoke_nr_args].ref_value;

					/* if no object throw a NullPointerException */
					if (invoke_obj == NULL) throw_null_pointer_exception();

#if BVM_BINARY_COMPAT_CHECKING_ENABLE

					/* "if the resolved method is an instance initialisation method, and the class in which
					 * it is declared is not the class symbolically referenced by the instruction,
					 * a NoSuchMethodError is thrown" */
					if ((invoke_method->name->data[0] == '<') &&
						(invoke_method->name->data[1] == 'i')) {
						bvm_utfstring_t *initclassname = bvm_clazz_cp_ref_clazzname(bvm_gl_rx_clazz, method_index);
						if (invoke_clazz->name != initclassname) {
							bvm_throw_exception(BVM_ERR_NO_SUCH_METHOD_ERROR, NULL);
						}
					}

					/* "if the resolved method is a class (static) method, the invokespecial
					 * instruction throws an IncompatibleClassChangeError." */
					if (BVM_METHOD_IsStatic(invoke_method)) {
						bvm_throw_exception(BVM_ERR_INCOMPATIBLE_CLASS_CHANGE_ERROR, NULL);
					}
#endif

#if (!BVM_DEBUGGER_ENABLE)
					/* substitute a go-faster opcode */
					*bvm_gl_rx_pc = OPCODE_230_invokespecial_fast;
#endif

					/* skip Object <init> method.  It does nothing so we'll save a small amount
					 * of time (but we'll save it a lot of times). */
					if ( (invoke_clazz == BVM_OBJECT_CLAZZ) &&
						 (invoke_method->name->data[0] == '<')) {
						bvm_gl_rx_sp -= invoke_nr_args;
						bvm_gl_rx_pc += 3;
						goto top_of_interpreter_loop;
					}

					invoke_pc_offset = 3;

					goto do_method_invocation;

				}
				OPCODE_HANDLER(OPCODE_invokestatic): { /* 184 */

					bvm_uint16_t method_index = BVM_VM2INT16(bvm_gl_rx_pc+1);

					if (BVM_CONSTANT_IsOptimised(bvm_gl_rx_clazz,method_index))
						invoke_method = bvm_gl_rx_clazz->constant_pool[method_index].resolved_ptr;
					else
						invoke_method = bvm_clazz_resolve_cp_method(bvm_gl_rx_clazz, method_index);

					invoke_clazz = invoke_method->clazz;

					invoke_nr_args = invoke_method->num_args;

					/* the class object of the clazz (we'll use it for synchronisation if the method is synced */
					invoke_obj = (bvm_obj_t *) invoke_clazz->class_obj;

#if BVM_BINARY_COMPAT_CHECKING_ENABLE
					/* "if the resolved method is an instance method, the invokestatic
					 * instruction throws an IncompatibleClassChangeError" */
					if (!BVM_METHOD_IsStatic(invoke_method)) {
						bvm_throw_exception(BVM_ERR_INCOMPATIBLE_CLASS_CHANGE_ERROR, NULL);
					}
#endif
					/* can't call method if class was loaded in error */
					if (invoke_clazz->state == BVM_CLAZZ_STATE_ERROR) {
						bvm_throw_exception(BVM_ERR_NO_CLASS_DEF_FOUND_ERROR, "Class in ERROR state");
					}

					/* 2.17.4 Initialization.  If the class is not initialised, request it be done. Do not
					 * move the sp/pc on as the initialise_clazz may have pushed a frame on
					 * the stack.  We'll get back here after that has run.  */
					if (!BVM_CLAZZ_IsInitialised(invoke_clazz)) {
						/*bvm_clazz_initialise(invoke_clazz);*/
						/*if (!BVM_CLAZZ_IsInitialised(invoke_clazz)) */
						if (bvm_clazz_initialise(invoke_clazz)) goto top_of_interpreter_loop;
					}

					/* check this current class can access the method -  note that the isPublic() is redundant -
					 * on the basis that most methods are public this is purely to avoid the overhead of the
					 * function call to bvm_clazz_is_member_accessible() if possible */
					if (bvm_gl_rx_clazz != invoke_clazz && !BVM_ACCESS_IsPublic(BVM_METHOD_AccessFlags(invoke_method))) {
						if (!bvm_clazz_is_member_accessible(BVM_METHOD_AccessFlags(invoke_method), bvm_gl_rx_clazz, invoke_clazz)) {
							bvm_throw_exception(BVM_ERR_ILLEGAL_ACCESS_ERROR, (char *) invoke_method->name->data);
						}
					}

#if (!BVM_DEBUGGER_ENABLE)
					/* substitute a go-faster opcode */
					*bvm_gl_rx_pc = OPCODE_229_invokestatic_fast;
#endif

					invoke_pc_offset = 3;

					goto do_method_invocation;
				}
				OPCODE_HANDLER(OPCODE_invokeinterface): { /* 185 */

					bvm_method_t *resolved_method;

					bvm_uint16_t method_index;

					bvm_instance_clazz_t *search_clazz;

					method_index = BVM_VM2INT16(bvm_gl_rx_pc+1);

					if (BVM_CONSTANT_IsOptimised(bvm_gl_rx_clazz,method_index))
						invoke_method = bvm_gl_rx_clazz->constant_pool[method_index].resolved_ptr;
					else
						invoke_method = bvm_clazz_resolve_cp_method(bvm_gl_rx_clazz, method_index);

					invoke_clazz = invoke_method->clazz;

					invoke_nr_args = invoke_method->num_args+1;

					/* the object ref_value we are making a call upon */
					invoke_obj = bvm_gl_rx_sp[-invoke_nr_args].ref_value;

					/* if no object throw a NullPointerException */
					if (invoke_obj == NULL) throw_null_pointer_exception();

#if BVM_BINARY_COMPAT_CHECKING_ENABLE
					/* ... but first, let's check for binary compatibility of interfaces ... */

					/* "if the class of objectref does not implement the resolved interface,
					 * invokeinterface throws an IncompatibleClassChangeError" */
					if (!bvm_clazz_implements_interface((bvm_instance_clazz_t *) invoke_obj->clazz, invoke_clazz)) {
						bvm_throw_exception(BVM_ERR_INCOMPATIBLE_CLASS_CHANGE_ERROR, NULL);
					}

					/* "if the selected method is not public, invokeinterface throws
					 * an IllegalAccessError." */
					if (!BVM_METHOD_IsPublic(invoke_method)) {
						bvm_throw_exception(BVM_ERR_ILLEGAL_ACCESS_ERROR, NULL);
					}
#endif
					/* if it is the same class, let's assume we have found it, otherwise, begin the hunt ... */
					if ( invoke_clazz != (bvm_instance_clazz_t *)  invoke_obj->clazz) {

						resolved_method = invoke_method;

						/* the class of the object reference we are making the method call upon */
						search_clazz = (bvm_instance_clazz_t *) invoke_obj->clazz;

						invoke_method = locate_virtual_method(search_clazz, resolved_method);
						invoke_clazz = invoke_method->clazz;

					}

#if BVM_BINARY_COMPAT_CHECKING_ENABLE
					/* well, if we *did* find one and it is static then throw an exception. There has
					 * obviously been a change in compatibility */
					if (BVM_METHOD_IsStatic(invoke_method)) {
						bvm_throw_exception(BVM_ERR_INCOMPATIBLE_CLASS_CHANGE_ERROR, NULL);
					}
#endif
					invoke_pc_offset = 5;

					goto do_method_invocation;
				}
				OPCODE_HANDLER(OPCODE_186): { /* 186 */
					/* unused */
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_new): { /* 187 */

					bvm_instance_clazz_t *cl;
					bvm_uint16_t index = BVM_VM2UINT16(bvm_gl_rx_pc+1);

					/* get the class name from the constant pool */
					cl = (bvm_instance_clazz_t *) bvm_clazz_resolve_cp_clazz(bvm_gl_rx_clazz, index);

#if BVM_BINARY_COMPAT_CHECKING_ENABLE

					/* throw InstantiationError if interface or abstract class */
					if (BVM_CLAZZ_IsInterface(cl) || BVM_CLAZZ_IsAbstract(cl)) {
						bvm_throw_exception(BVM_ERR_INSTANTIATION_ERROR, NULL);
					}
#endif
					/* can't instantiate if class was loaded in error */
					if (cl->state == BVM_CLAZZ_STATE_ERROR) {
						bvm_throw_exception(BVM_ERR_NO_CLASS_DEF_FOUND_ERROR, "Class in ERROR state");
					}

					/* 2.17.4 Initialization.  If we need to init the class request we go to top
					 * of the interp loop.  The initialise_clazz() may push a frame into the stack.  We'll
					 * arrive back here after initialisation. */
					if (!BVM_CLAZZ_IsInitialised(cl)) {
                        if (bvm_clazz_initialise(cl)) goto top_of_interpreter_loop;
					}

#if (!BVM_DEBUGGER_ENABLE)
					/* make it faster next time */
					*bvm_gl_rx_pc = OPCODE_new_fast;
#endif
					/* create the object and put it on the stack */
					bvm_gl_rx_sp[0].ref_value = bvm_object_alloc(cl);

					bvm_gl_rx_sp++;
					bvm_gl_rx_pc += 3;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_newarray): { /* 188 */

					/* the requested length of the array */
					bvm_int32_t length = (bvm_int32_t) bvm_gl_rx_sp[-1].int_value;

					/* if requested length is less than zero, throw exception */
					if (length < 0) {
						bvm_throw_exception(BVM_ERR_NEGATIVE_ARRAY_SIZE_EXCEPTION, NULL);
					}

					/* create new primitive array instance and push new onto stack */
					bvm_gl_rx_sp[-1].ref_value = (bvm_obj_t *) bvm_object_alloc_array_primitive(length, bvm_gl_rx_pc[1]);

					bvm_gl_rx_pc += 2;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_anewarray): { /* 189 */

					bvm_clazz_t *component_clazz;
					bvm_utfstring_t *clazzname;

					/* the requested length of the array */
					bvm_int32_t length = bvm_gl_rx_sp[-1].int_value;

					/* if requested length is less than zero, throw exception */
					if (length < 0) {
						bvm_throw_exception(BVM_ERR_NEGATIVE_ARRAY_SIZE_EXCEPTION, NULL);
					}

					/* the class name of the component of the new array */
					clazzname = bvm_clazz_cp_utfstring_from_index(bvm_gl_rx_clazz, BVM_VM2UINT16(bvm_gl_rx_pc+1));

					/* the clazz of the array component */
					component_clazz = bvm_clazz_get(bvm_gl_rx_clazz->classloader_obj, clazzname);

					/* if the array component clazz is not accessible throw an IllegalAccessError */
					if (!bvm_clazz_is_class_accessible((bvm_clazz_t *) bvm_gl_rx_clazz, (bvm_clazz_t *) component_clazz)) {
						bvm_throw_exception(BVM_ERR_ILLEGAL_ACCESS_ERROR, (char *) component_clazz->name->data);
					}

					bvm_gl_rx_sp[-1].ref_value = (bvm_obj_t *) bvm_object_alloc_array_reference(length, component_clazz);

					bvm_gl_rx_pc += 3;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_arraylength): { /* 190 */

					/* get the array object */
					bvm_jarray_obj_t *array_obj = (bvm_jarray_obj_t *) bvm_gl_rx_sp[-1].ref_value;

					/* No object? */
					if (array_obj == NULL) throw_null_pointer_exception();

					/* push length */
					bvm_gl_rx_sp[-1].int_value = array_obj->length.int_value;
					bvm_gl_rx_pc++;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_athrow): /* 191 */

opcode_athrow:

				{

					/* if the throwable var is not yet set (the throwable will be non-NULL here if a native exception is thrown),
					 * get it from the stack: it is a normal java bytecode 'athrow' . If still NULL, substitute a null pointer
					 * exception error.  'null' can actually be thrown as an exception (null can be cast to
					 * exception).  .. but it is stupid to do so, so we substitute a real exception here if it is null.
					 * This is just like Java SE which substitutes a null pointer exception ...*/
					if (throwable == NULL) {
						throwable = (bvm_throwable_obj_t *) bvm_gl_rx_sp[-1].ref_value;
						if (throwable == NULL) throw_null_pointer_exception();
					}

#if BVM_DEBUGGER_ENABLE
					/* if the current thread is suspended at this moment, it is most likely that a
					 * class_prepare for the exception class caused the thread to become suspended.  There is
					 * no point in doing the exception handing now, if the debugger is in the processes of requesting an
					 * exception breakpoint (which is why an exception class_prepare will cause a thread suspension) it
					 * will not be read/registered by the time exception processing has completed if we proceed.
					 *
					 * To that end, if the current thread is suspended, we pend the exception with
					 * the thread and go back to the main loop.  The next time the thread comes alive,
					 * the exception will be re-thrown and we will arrive back here again ....
					 */

					if (bvm_gl_thread_current->status != BVM_THREAD_STATUS_RUNNABLE) {
						bvm_gl_thread_current->pending_exception = throwable;
						if (throwable->needs_native_try_reset.int_value) goto new_try_block;
						goto exception_is_handled;
					}
#endif
					/* exception handling part 1 - get the catch location */
					memset(&(bvm_gl_thread_current->exception_location), 0, sizeof(bvm_exception_location_data_t));
					bvm_gl_thread_current->exception_location.throwable = throwable;
					/* visit the stack looking for an exception handler. If one is found, put the info into
					 * bvm_gl_thread_current->exception_location.  */
					bvm_stack_visit(bvm_gl_thread_current,0,-1,NULL,exec_find_exception_callback, &(bvm_gl_thread_current->exception_location) );

#if BVM_DEBUGGER_ENABLE

					if (bvmd_is_session_open() && BVMD_IS_EVENTKIND_REGISTERED(BVMD_EventKind_EXCEPTION)) {

						bvmd_eventcontext_t *context = bvmd_eventdef_new_context(JDWP_EventKind_EXCEPTION, bvm_gl_thread_current);
						context->location.clazz = (bvm_clazz_t *) bvm_gl_rx_clazz;
						context->location.method = bvm_gl_rx_method;
						if (!BVM_METHOD_IsNative(bvm_gl_rx_method))
							context->location.pc_index = bvm_gl_rx_pc - bvm_gl_rx_method->code.bytecode;
						context->exception.throwable = throwable;
						context->exception.caught = bvm_gl_thread_current->exception_location.caught;

						/* populate the catch location if it will be caught */
						if (context->exception.caught) {
							context->catch_location.method = bvm_gl_thread_current->exception_location.method;
							context->catch_location.clazz = (bvm_clazz_t *) bvm_gl_thread_current->exception_location.method->clazz;
							context->catch_location.pc_index = bvm_gl_thread_current->exception_location.handler_pc;
						}

						/* go and see if any exception breakpoints are satisfied */
						bvmd_event_Exception(context);
						bvmd_eventdef_free_context(context);

						/* if we have come back from exception breakpoint event and the current thread is no longer runnable,
						 * set the thread as 'breakpointed', set the breakpoint opcode to be the 2nd part of exception handling
						 * and hop to the top of the interp loop.
						 */
						if (bvm_gl_thread_current->status != BVM_THREAD_STATUS_RUNNABLE) {
							bvm_gl_thread_current->dbg_is_at_breakpoint = BVM_TRUE;
							bvm_gl_thread_current->dbg_breakpoint_opcode = OPCODE_203_pop_for_exception;
							if (throwable->needs_native_try_reset.int_value) goto new_try_block;
							goto exception_is_handled;
						}
					}
#endif
					/* exception handling part 2 - unwind the stack to get to the exception handler frame. */
					goto opcode_pop_for_exception;
				}
				OPCODE_HANDLER(OPCODE_checkcast): /* 192 */
				OPCODE_HANDLER(OPCODE_instanceof): {/* 193 */

					bvm_bool_t iscompatible = BVM_FALSE;
					bvm_uint16_t targetindex;
					bvm_utfstring_t *targetname;
					bvm_clazz_t *target_cl;

					bvm_obj_t *obj = bvm_gl_rx_sp[-1].ref_value;

					/* if the reference is null and we are doing an instanceof, we can push failure
					 * and then continue */
				    if (obj == NULL) {
						 if (CURRENT_OPCODE == OPCODE_instanceof) bvm_gl_rx_sp[-1].int_value = BVM_FALSE;
						 bvm_gl_rx_pc += 3;
						 goto top_of_interpreter_loop;
					}

					targetindex = BVM_VM2UINT16(bvm_gl_rx_pc+1);
					targetname  = bvm_clazz_cp_utfstring_from_index(bvm_gl_rx_clazz, targetindex);

					target_cl   = bvm_clazz_get(bvm_gl_rx_clazz->classloader_obj, targetname);

					/* a simple check before the incurring expense of a function call to is_assignable_from().
					 * If the two clazzes are the same or the target is the Object class, forget it.
					 * Note: is_assignable_from also performs these same two checks. */
					if ((obj->clazz == target_cl) || (target_cl == (bvm_clazz_t *) BVM_OBJECT_CLAZZ))
						iscompatible = BVM_TRUE;
					else
						iscompatible = bvm_clazz_is_assignable_from(obj->clazz, target_cl);

					/* checkcast throws an exception if the two are not compatible, instanceof
					 * pushes a false on the stack */
					if (CURRENT_OPCODE == OPCODE_checkcast) {
						if (!iscompatible) {
							bvm_throw_exception(BVM_ERR_CLASS_CAST_EXCEPTION, NULL);
						}
					} else
						bvm_gl_rx_sp[-1].int_value = iscompatible;

					bvm_gl_rx_pc += 3;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_monitorenter): { /* 194 */

					/* attempt to acquire the monitor for the object.  If it cannot be
					 * acquired the current thread must have been suspended so we'll just
					 * loop back to the top where a thread switch will take place. */
					if (!bvm_thread_monitor_acquire(bvm_gl_rx_sp[-1].ref_value, bvm_gl_thread_current)) {
						goto top_of_interpreter_loop;
					}

					bvm_gl_rx_pc++;
					bvm_gl_rx_sp--;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_monitorexit): {/* 195 */

					bvm_thread_monitor_release(bvm_gl_rx_sp[-1].ref_value);

					bvm_gl_rx_pc++;
					bvm_gl_rx_sp--;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_wide): {/* 196 */

					bvm_uint16_t index = BVM_VM2UINT16(bvm_gl_rx_pc+2);
					switch (bvm_gl_rx_pc[1]) {
						case OPCODE_iload:
						case OPCODE_fload:
						case OPCODE_aload:
							bvm_gl_rx_sp[0] = bvm_gl_rx_locals[index];
							bvm_gl_rx_sp++;
							bvm_gl_rx_pc += 4;
							break;
						case OPCODE_lload:
						case OPCODE_dload:
							bvm_gl_rx_sp[0] = bvm_gl_rx_locals[index];
							bvm_gl_rx_sp[1] = bvm_gl_rx_locals[index+1];
							bvm_gl_rx_sp += 2;
							bvm_gl_rx_pc += 4;
							break;
						case OPCODE_astore:
						case OPCODE_istore:
						case OPCODE_fstore:
							bvm_gl_rx_locals[index] = bvm_gl_rx_sp[-1];
							bvm_gl_rx_sp--;
							bvm_gl_rx_pc += 4;
							break;
						case OPCODE_lstore:
						case OPCODE_dstore:
							bvm_gl_rx_locals[index]   = bvm_gl_rx_sp[-2];
							bvm_gl_rx_locals[index+1] = bvm_gl_rx_sp[-1];
							bvm_gl_rx_sp-=2;
							bvm_gl_rx_pc+=4;
							break;
						case OPCODE_iinc: {
						    bvm_int32_t left = (bvm_int32_t) bvm_gl_rx_locals[index].int_value;
                            bvm_int16_t right = BVM_VM2INT16(bvm_gl_rx_pc + 4);
                            bvm_gl_rx_locals[index].int_value = (bvm_int32_t) left + right;
                            bvm_gl_rx_pc += 6;
                            break;
                        }
						case OPCODE_ret:
							bvm_gl_rx_pc = bvm_gl_rx_locals[index].ptr_value;
							bvm_gl_rx_pc+=4;
							break;
						default :
							throw_unsupported_feature_exception();
					}
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_multianewarray): { /* 197 */

					bvm_array_clazz_t *cl;
					bvm_array_clazz_t *access_check_clazz;

					/* index into constant pool for clazz name */
					bvm_uint16_t index = BVM_VM2UINT16(bvm_gl_rx_pc+1);

					/* get the class name - this will be an array class name like '[[x' */
					bvm_utfstring_t *clazzname = bvm_clazz_cp_utfstring_from_index(bvm_gl_rx_clazz, index);

					/* nr dimensions of the multi array */
					int dimensions = bvm_gl_rx_pc[3];

					/* get the array class - unlike the 'anewarray' opcode which specifies the component
					 * clazz, this is the actual array class */
					cl = (bvm_array_clazz_t *) bvm_clazz_get(bvm_gl_rx_clazz->classloader_obj, clazzname);

					/* JVMS: "if the current class does not have permission to access the element
					 * type of the resolved array class, multianewarray throws an IllegalAccessError."
					 * To do this we'll find the innermost array and, if it is an object array we'll check
					 * if the current clazz has access to it.*/
					access_check_clazz = (bvm_array_clazz_t *) cl->component_clazz;
					while (access_check_clazz->component_jtype == BVM_T_ARRAY)
						access_check_clazz = (bvm_array_clazz_t *) access_check_clazz->component_clazz;

					if (access_check_clazz->component_jtype == BVM_T_OBJECT) {
						if (!bvm_clazz_is_class_accessible( (bvm_clazz_t *) bvm_gl_rx_clazz, access_check_clazz->component_clazz)) {
							bvm_throw_exception(BVM_ERR_ILLEGAL_ACCESS_ERROR, (char *) access_check_clazz->component_clazz->name->data);
						}
					}

					/* sort of a pop, lower the sp to point to the start of the list of array sizes.  We'll pass
					 * the list to the create_multi_array function as an array of bvm_int32_t */
					bvm_gl_rx_sp -= dimensions;

					/* create the new multi array - note the int array passed here are actually cells - the size of the pointer
					 * here matches the size if the int in an bvm_cell_t */
					bvm_gl_rx_sp[0].ref_value = (bvm_obj_t *) bvm_object_alloc_array_multi(cl , dimensions, (bvm_native_long_t *) bvm_gl_rx_sp);

					bvm_gl_rx_sp++;
					bvm_gl_rx_pc += 4;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_ifnull): /* 198 */
					if (bvm_gl_rx_sp[-1].ref_value == NULL)
						bvm_gl_rx_pc += BVM_VM2INT16(bvm_gl_rx_pc+1);
					else
						bvm_gl_rx_pc += 3;
					bvm_gl_rx_sp--;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_ifnonnull): /* 199 */
					if (bvm_gl_rx_sp[-1].ref_value != NULL)
						bvm_gl_rx_pc += BVM_VM2INT16(bvm_gl_rx_pc+1);
					else
						bvm_gl_rx_pc += 3;
					bvm_gl_rx_sp--;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_goto_w): /* 200 */
					bvm_gl_rx_pc += BVM_VM2INT32(bvm_gl_rx_pc+1);
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_jsr_w): /* 201 */
					bvm_gl_rx_sp[0].ptr_value = bvm_gl_rx_pc + 5;
					bvm_gl_rx_pc += BVM_VM2INT32(bvm_gl_rx_pc+1);
					bvm_gl_rx_sp++;
					OPCODE_NEXT;
				OPCODE_HANDLER(OPCODE_breakpoint): {/* 202 */

#if BVM_DEBUGGER_ENABLE

					if (bvmd_is_session_open()) {

						bvmd_location_t location;
						location.clazz = (bvm_clazz_t *) bvm_gl_rx_clazz;
						location.method = bvm_gl_rx_method;
						location.pc_index = bvm_gl_rx_pc - bvm_gl_rx_method->code.bytecode;

						/* Send the breakpoint event.  Breakpoint events are never parked.  As a convenience, the
						 * bvmd_event_Breakpoint function gives us the original opcode the breakpoint displaced. */
						bvmd_event_Breakpoint(&location);

						bvm_gl_thread_current->dbg_is_at_breakpoint = BVM_TRUE;
						if (!bvmd_breakpoint_opcode_for_location(&location, &(bvm_gl_thread_current->dbg_breakpoint_opcode))) {
							/* .. and if we didn't find one, something has gone horribly wrong */
							BVM_VM_EXIT(BVM_FATAL_ERR_NO_BREAKPOINT_FOUND, NULL);
						}

					} else {
						/* if we come across a breakpoint and we are *not* attached to a debugger, then
						 * something has gone wrong somewhere.
						 */
						BVM_VM_EXIT(BVM_FATAL_ERR_INVALID_BREAKPOINT_ENCOUNTERED, NULL);
					}

					/* note: we do NOT move the PC on, or adjust the SP */
					OPCODE_NEXT;
#else
					throw_unsupported_feature_exception();
#endif
				}
				OPCODE_HANDLER(OPCODE_203_pop_for_exception): {

					bvm_exception_location_data_t *exception_location;
opcode_pop_for_exception:

					exception_location = &(bvm_gl_thread_current->exception_location);
					throwable = exception_location->throwable;

					/* pop frames of the stack to get down to where the exception is.  Depth is
					 * zero-based meaning 0 == current frame == no pops required. */
					while (bvm_gl_thread_current->exception_location.depth--) {

						if (BVM_METHOD_IsSynchronized(bvm_gl_rx_method)) {
							bvm_thread_monitor_release(BVM_STACK_ExecMethodSyncObject());
						}

						bvm_frame_pop();
					}

					if (!exception_location->caught) {

#if BVM_EXIT_ON_UNCAUGHT_EXCEPTION

#if BVM_CONSOLE_ENABLE

						/* if the error is actually the "out of memory error" we really can't produce a
						 * stacktrace - it uses heap.  So, if that if the case, we'll just output something
						 * to the console. */
						if (throwable == bvm_gl_out_of_memory_err_obj) {
							bvm_pd_console_out("Out Of Memory Error.\n");
							BVM_VM_EXIT(BVM_FATAL_ERR_OUT_OF_MEMORY, NULL);
						} else {
							bvm_stacktrace_print_to_console(throwable);
							BVM_VM_EXIT(BVM_FATAL_ERR_UNCAUGHT_EXCEPTION, NULL);
						}
#endif

#else
						bvm_thread_push_exceptionhandler(throwable);

						exception_location->throwable = NULL;

						if (throwable->needs_native_try_reset.int_value) goto new_try_block;
						goto exception_is_handled;
#endif
					}

					/* The spec says to discard all values on the operand stack stack for the method
					 * that catches it.  So get to the base of the operand stack for the current
					 * method.  That is the locals pointer (base of frame) + the number of local vars. */
					bvm_gl_rx_sp = bvm_gl_rx_locals + bvm_gl_rx_method->max_locals;

					/* push the exception object onto the stack */
					bvm_gl_rx_sp[0].ref_value = (bvm_obj_t *) throwable;
					bvm_gl_rx_sp++;

					/* get the pc of the exception handler */
					bvm_gl_rx_pc = bvm_gl_rx_method->code.bytecode + exception_location->handler_pc;

					exception_location->throwable = NULL;

					if (throwable->needs_native_try_reset.int_value) goto new_try_block;
					goto exception_is_handled;
				}
				OPCODE_HANDLER(OPCODE_204):
				OPCODE_HANDLER(OPCODE_205):
				OPCODE_HANDLER(OPCODE_206):
				OPCODE_HANDLER(OPCODE_207):
				OPCODE_HANDLER(OPCODE_208):
				OPCODE_HANDLER(OPCODE_209):
				OPCODE_HANDLER(OPCODE_210):
				OPCODE_HANDLER(OPCODE_211):
				OPCODE_HANDLER(OPCODE_212):
				OPCODE_HANDLER(OPCODE_213):
				OPCODE_HANDLER(OPCODE_214):
				OPCODE_HANDLER(OPCODE_215):
					throw_unsupported_feature_exception();

#if (!BVM_DEBUGGER_ENABLE)

				/*
				 * A series of faster version of the normal opcodes for fields.  The first
				 * time a field opcode is performed in a method it is replaced with a faster
				 * one.  The faster ones no longer perform static checks or attempt to resolve
				 * the field or whatever.  There are also specific faster opcodes for longs
				 * and otherwise.  yes, a bit more code space - but worth the speed.  All thse
				 * opcodes basically just perform pointer operations and copying.  No
				 * function calls etc.
				 *
				 * Fast opcodes are only used when debugger support is *not* compiled in
				 */

				OPCODE_HANDLER(OPCODE_ldc_fast_1): { /* 216 */
					bvm_uint16_t index = (bvm_uint16_t)bvm_gl_rx_pc[1];
					bvm_gl_rx_sp[0] = bvm_gl_rx_clazz->constant_pool[index].data.value;
					bvm_gl_rx_sp++;
					bvm_gl_rx_pc += 2;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_ldc_fast_2): { /* 217 */
					bvm_uint16_t index = (bvm_uint16_t)bvm_gl_rx_pc[1];
					bvm_clazzconstant_t *constant = &bvm_gl_rx_clazz->constant_pool[index];
					bvm_gl_rx_sp[0].ref_value = (bvm_obj_t *) ((bvm_clazz_t *) constant->resolved_ptr)->class_obj;
					bvm_gl_rx_sp++;
					bvm_gl_rx_pc += 2;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_ldc_w_fast_1): { /* 218 */
					bvm_uint16_t index = BVM_VM2UINT16(bvm_gl_rx_pc+1);
					bvm_gl_rx_sp[0] = bvm_gl_rx_clazz->constant_pool[index].data.value;
					bvm_gl_rx_sp++;
					bvm_gl_rx_pc += 3;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_ldc_w_fast_2): { /* 219 */
					bvm_uint16_t index = BVM_VM2UINT16(bvm_gl_rx_pc+1);
					bvm_clazzconstant_t *constant = &bvm_gl_rx_clazz->constant_pool[index];
					bvm_gl_rx_sp[0].ref_value = (bvm_obj_t *) ((bvm_clazz_t *) constant->resolved_ptr)->class_obj;
					bvm_gl_rx_sp++;
					bvm_gl_rx_pc += 3;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_getstatic_fast):  {/* 220 */

					bvm_field_t *field;
					bvm_uint16_t index = BVM_VM2UINT16(bvm_gl_rx_pc+1);

					/* get the optimised field */
					field = bvm_gl_rx_clazz->constant_pool[index].resolved_ptr;

					bvm_gl_rx_sp[0] = field->value.static_value;

					bvm_gl_rx_sp++;
					bvm_gl_rx_pc += 3;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_getstatic_fast_long):  {/* 221 */

					bvm_field_t *field;
					bvm_uint16_t index = BVM_VM2UINT16(bvm_gl_rx_pc+1);

					/* get the optimised field */
					field = bvm_gl_rx_clazz->constant_pool[index].resolved_ptr;

                    /* push the field static 64 bit value onto the stack - works for longs
                     * and doubles - we're copying the bits ... not the int/double 'value' .... */
					bvm_int64_t val = *(bvm_int64_t*)(field->value.static_value.ptr_value);
                    BVM_INT64_to_cells(bvm_gl_rx_sp, val);

					bvm_gl_rx_sp += 2;
					bvm_gl_rx_pc += 3;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_putstatic_fast):  { /* 222 */

					bvm_field_t *field;
					bvm_uint16_t index = BVM_VM2UINT16(bvm_gl_rx_pc+1);

					/* get the optimised field */
					field = bvm_gl_rx_clazz->constant_pool[index].resolved_ptr;

					field->value.static_value = bvm_gl_rx_sp[-1];

					bvm_gl_rx_sp--;
					bvm_gl_rx_pc += 3;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_putstatic_fast_long):  { /* 223 */

					bvm_field_t *field;
					bvm_uint16_t index = BVM_VM2UINT16(bvm_gl_rx_pc+1);

					/* get the optimised field */
					field = bvm_gl_rx_clazz->constant_pool[index].resolved_ptr;

					/* copy the 64 value from the stack into the field - works for longs and doubles
					 * as we're effectively copying the bits .. not the value */
					bvm_int64_t val = BVM_INT64_from_cells(bvm_gl_rx_sp-2);
                    (*(bvm_int64_t*)(field->value.static_value.ptr_value)) = val;

					bvm_gl_rx_sp -= 2;
					bvm_gl_rx_pc += 3;

					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_getfield_fast):{   /* 224 */

					bvm_field_t *field;
					bvm_obj_t *obj = bvm_gl_rx_sp[-1].ref_value;
					bvm_uint16_t index = BVM_VM2UINT16(bvm_gl_rx_pc+1);

					/* No object?  Bang out. */
					if (obj == NULL) throw_null_pointer_exception();

					/* get the optimised field */
					field = bvm_gl_rx_clazz->constant_pool[index].resolved_ptr;

					/* get the value of the object at the offset given by the field. */
					bvm_gl_rx_sp[-1] = obj->fields[field->value.offset];

					bvm_gl_rx_pc += 3;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_getfield_fast_long): { /* 225 */

					bvm_field_t *field;
					bvm_obj_t *obj = bvm_gl_rx_sp[-1].ref_value;
					bvm_uint16_t index = BVM_VM2UINT16(bvm_gl_rx_pc+1);

					/* No object?  Bang out. */
					if (obj == NULL) throw_null_pointer_exception();

					/* get the optimised field */
					field = bvm_gl_rx_clazz->constant_pool[index].resolved_ptr;

					bvm_gl_rx_sp[-1] = obj->fields[field->value.offset];
					bvm_gl_rx_sp[0]  = obj->fields[field->value.offset+1];

					bvm_gl_rx_sp++;
					bvm_gl_rx_pc += 3;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_putfield_fast):  { /* 226 */

					bvm_field_t *field;
					bvm_obj_t *obj = bvm_gl_rx_sp[-2].ref_value;
					bvm_uint16_t index = BVM_VM2UINT16(bvm_gl_rx_pc+1);

					/* No object?  Bang out. */
					if (obj == NULL) throw_null_pointer_exception();

					/* get optimised field */
					field = bvm_gl_rx_clazz->constant_pool[index].resolved_ptr;

					/* set the value of the object at the offset given by the field. */
					obj->fields[field->value.offset] = bvm_gl_rx_sp[-1];

					bvm_gl_rx_sp -= 2;
					bvm_gl_rx_pc += 3;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_putfield_fast_long):  { /* 227 */
					bvm_field_t *field;
					bvm_obj_t *obj  = bvm_gl_rx_sp[-3].ref_value;
					bvm_int16_t index = BVM_VM2UINT16(bvm_gl_rx_pc+1);

					/* get optimised field */
					field = bvm_gl_rx_clazz->constant_pool[index].resolved_ptr;

					/* No object?  Bang out. */
					if (obj == NULL) throw_null_pointer_exception();

					obj->fields[field->value.offset]   = bvm_gl_rx_sp[-2];
					obj->fields[field->value.offset+1] = bvm_gl_rx_sp[-1];

					bvm_gl_rx_sp -= 3;
					bvm_gl_rx_pc += 3;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_new_fast):  {/* 228 */

					bvm_instance_clazz_t *cl;
					bvm_uint16_t index = BVM_VM2UINT16(bvm_gl_rx_pc+1);

					/* get the class direct from the constant pool */
					cl = (bvm_instance_clazz_t *) bvm_gl_rx_clazz->constant_pool[index].resolved_ptr;

					/* create the object and push on the stack */
					bvm_gl_rx_sp[0].ref_value = bvm_object_alloc(cl);

					bvm_gl_rx_sp++;
					bvm_gl_rx_pc += 3;
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_229_invokestatic_fast): {

					bvm_uint16_t method_index = BVM_VM2INT16(bvm_gl_rx_pc+1);

					invoke_method = bvm_gl_rx_clazz->constant_pool[method_index].resolved_ptr;
					invoke_clazz = invoke_method->clazz;

					invoke_nr_args = invoke_method->num_args;

					invoke_obj = (bvm_obj_t *) invoke_clazz->class_obj;

					invoke_pc_offset = 3;

					goto do_method_invocation;
				}
				OPCODE_HANDLER(OPCODE_230_invokespecial_fast): {
					bvm_uint16_t method_index = BVM_VM2INT16(bvm_gl_rx_pc+1);

					invoke_method = bvm_gl_rx_clazz->constant_pool[method_index].resolved_ptr;
					invoke_clazz = invoke_method->clazz;

					invoke_nr_args = invoke_method->num_args+1;

					/* the object ref_value we are making a call upon */
					invoke_obj = bvm_gl_rx_sp[-invoke_nr_args].ref_value;

					/* if no object throw a NullPointerException */
					if (invoke_obj == NULL) throw_null_pointer_exception();

					/* skip Object <init> method.  It does nothing so we'll save a small amount
					 * of time (but we'll save it a lot of times). */
					if ( (invoke_clazz == BVM_OBJECT_CLAZZ) &&
						 (invoke_method->name->data[0] == '<')) {
						bvm_gl_rx_sp -= invoke_nr_args;
						bvm_gl_rx_pc += 3;
						goto top_of_interpreter_loop;
					}

					invoke_pc_offset = 3;

					goto do_method_invocation;
				}
				OPCODE_HANDLER(OPCODE_231_invokeinterface_fast): {
					OPCODE_NEXT;
				}
				OPCODE_HANDLER(OPCODE_232_invokevirtual_fast): {
					OPCODE_NEXT;
				}
#else

				/* If debugging support is compiled in, we do not use the fast opcodes */
				OPCODE_HANDLER(OPCODE_ldc_fast_1):  /* 216 */
				OPCODE_HANDLER(OPCODE_ldc_fast_2):  /* 217 */
				OPCODE_HANDLER(OPCODE_ldc_w_fast_1): /* 218 */
				OPCODE_HANDLER(OPCODE_ldc_w_fast_2): /* 219 */
				OPCODE_HANDLER(OPCODE_getstatic_fast): /* 220 */
				OPCODE_HANDLER(OPCODE_getstatic_fast_long): /* 221 */
				OPCODE_HANDLER(OPCODE_putstatic_fast): /* 222 */
				OPCODE_HANDLER(OPCODE_putstatic_fast_long): /* 223 */
				OPCODE_HANDLER(OPCODE_getfield_fast): /* 224 */
				OPCODE_HANDLER(OPCODE_getfield_fast_long):  /* 225 */
				OPCODE_HANDLER(OPCODE_putfield_fast): /* 226 */
				OPCODE_HANDLER(OPCODE_putfield_fast_long):  /* 227 */
				OPCODE_HANDLER(OPCODE_new_fast):/* 228 */
				OPCODE_HANDLER(OPCODE_229_invokestatic_fast):
				OPCODE_HANDLER(OPCODE_230_invokespecial_fast):
				OPCODE_HANDLER(OPCODE_231_invokeinterface_fast):
				OPCODE_HANDLER(OPCODE_232_invokevirtual_fast):
					throw_unsupported_feature_exception();
#endif
				OPCODE_HANDLER(OPCODE_233):
				OPCODE_HANDLER(OPCODE_234):
				OPCODE_HANDLER(OPCODE_235):
				OPCODE_HANDLER(OPCODE_236):
				OPCODE_HANDLER(OPCODE_237):
				OPCODE_HANDLER(OPCODE_238):
				OPCODE_HANDLER(OPCODE_239):
				OPCODE_HANDLER(OPCODE_240):
				OPCODE_HANDLER(OPCODE_241):
				OPCODE_HANDLER(OPCODE_242):
				OPCODE_HANDLER(OPCODE_243):
				OPCODE_HANDLER(OPCODE_244):
				OPCODE_HANDLER(OPCODE_245):
				OPCODE_HANDLER(OPCODE_246):
				OPCODE_HANDLER(OPCODE_247):
				OPCODE_HANDLER(OPCODE_248):
				OPCODE_HANDLER(OPCODE_249):
				OPCODE_HANDLER(OPCODE_250):
				OPCODE_HANDLER(OPCODE_251):
				OPCODE_HANDLER(OPCODE_252):
				OPCODE_HANDLER(OPCODE_253):
				OPCODE_HANDLER(OPCODE_impdep1): /* 254 */
				OPCODE_HANDLER(OPCODE_impdep2): /* 255 */
					throw_unsupported_feature_exception();
				/*default :
					throw_unsupported_feature_exception();*/

				/* the shared logic for all method invocation opcodes */
do_method_invocation:
			{

				bvm_cell_t *arguments_pos;

				/* if the method is synchronised, we must try to acquire the monitor for the sync object.
				 * For non-static methods, the sync object will be the object the method is
				 * being called upon.  For static methods it is the Class object of the resolved clazz.
				 * If we cannot acquire the monitor here, we'll just go to the top of the interpreter
				 * loop.  A by-product of attempting to acquire a monitor and failing is that the
				 * current thread will be suspended.  We go to the top of the loop for a thread
				 * switch, and at some other time when this executing thread wakes up we'll have
				 * another attempt to acquire the monitor and really execute the method */
				if (BVM_METHOD_IsSynchronized(invoke_method)) {
					if (!bvm_thread_monitor_acquire(invoke_obj, bvm_gl_thread_current))
						goto top_of_interpreter_loop;
				}

#if BVM_BINARY_COMPAT_CHECKING_ENABLE

				/* If the method is abstract throw an exception */
				if (BVM_METHOD_IsAbstract(invoke_method)) {
					bvm_throw_exception(BVM_ERR_ABSTRACT_METHOD_ERROR, NULL);
				}
#endif

				/* Remember the position on the stack in the current frame where the method call arguments start.
				 * These will have been pushed onto the stack as part of normal bytecode execution.  After a frame
				 * push, we'll need to use this position to populate the arguments into the locals/args for the
				 * new frame. */
				arguments_pos = bvm_gl_rx_sp - invoke_nr_args;

				/* Push a stack frame and also set the return registers info at the base of the frame */
				bvm_frame_push(invoke_method, arguments_pos, bvm_gl_rx_pc, bvm_gl_rx_pc + invoke_pc_offset, invoke_obj);

                /* different processing for native/bytecode methods.  A native method is
                 * called and its result processed immediately.  A non-native method
                 * sets the pc to the method bytecode start and goes to the top of the
                 * interpreter loop */
				if (!BVM_METHOD_IsNative(invoke_method)) {

					/* now, using our remembered position on the stack of where the method locals started, lets
					 * push them as local variables into the new frame at the new frame's 'locals' start. */
					memcpy(bvm_gl_rx_locals, arguments_pos, invoke_nr_args * sizeof(bvm_cell_t));

					goto top_of_interpreter_loop;

				} else {

					/* Oops!  We have a native method but no linked code!! */
					if (invoke_method->code.nativemethod == NULL) {
						bvm_throw_exception(BVM_ERR_UNSATISFIED_LINK_ERROR, NULL);
					}

					/* Call the native method wrapping it a transient block - with it wrapped in a transient
					 * block native methods do not have to declare their own transient blocks */
					BVM_BEGIN_TRANSIENT_BLOCK {
						/* Using 'stack_pos' gives the native method all the locals but avoids the
						 * copy (as above with the non-native method). Native methods do not use a
						 * stack area like a non-native method - so this is a handy optimization. Just as long
						 * as the native method does not modify the stack I guess! */
						invoke_method->code.nativemethod(arguments_pos);
					} BVM_END_TRANSIENT_BLOCK;

					/* if we have come back from a native method and there is an exception
					 * pending then throw it. */
					if (bvm_gl_thread_current->pending_exception != NULL) {
						bvm_do_thread_interrupt();
					}

					/* go straight away to the return value processing */
					goto doreturn;
				}

				goto top_of_interpreter_loop;
			}

			OPCODE_DISPATCH_END
		}

	} BVM_CATCH(ex) {
		/* catch an exception and defer handling to the athrow opcode */
		throwable = ex;
		goto opcode_athrow;
	} BVM_END_CATCH
}
