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

  JDWP VirtualMachine commandset command handlers.

  @author Greg McCreath
  @since 0.0.10

*/

#if BVM_DEBUGGER_ENABLE

/**
 * JDWP Command handler for VirtualMachine/Version.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_VM_Version(bvmd_instream_t* in, bvmd_outstream_t* out) {

	bvmd_out_writestring(out, BVM_VM_DESC);		/* description */
	bvmd_out_writeint32(out, 1);				    /* jdwpMajor (Java version) */
	bvmd_out_writeint32(out, 6);				    /* jdwpMinor (Java version) */
	bvmd_out_writestring(out, BVM_VM_VERSION);	/* vmVersion */
	bvmd_out_writestring(out, BVM_VM_NAME);		/* vmName */
}

/**
 * JDWP Command handler for VirtualMachine/ClassesBySignature.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_VM_ClassesBySignature(bvmd_instream_t* in, bvmd_outstream_t* out) {

	bvm_uint32_t clazzes_nr = 0;
	bvm_utfstring_t signature;
	bvmd_streampos_t count_pos;
	bvm_uint32_t i = bvm_gl_clazz_pool_bucketcount;

	/* read the signature to be matched */
	bvmd_in_readutfstring(in, &signature);

	/* write out a zero counter and remember the place in the stream where it was written */
	count_pos = bvmd_out_writeint32(out, 0);

	/* go to each clazz in the clazz pool and check for a signature match */
	while (i--) {

		bvm_clazz_t *clazz = bvm_gl_clazz_pool[i];

		while (clazz != NULL) {

			if (bvm_str_utfstringcmp(&signature, clazz->jni_signature) == 0) {

				clazzes_nr++;

				bvmd_out_writebyte( out, bvmd_clazz_reftype(clazz) );
				bvmd_out_writeobject( out, clazz);
				bvmd_out_writeint32( out, bvmd_clazz_status(clazz) );
			}

			clazz = clazz->next;
		}
	}

	/* rewrite the count of matching clazzes (if there is a count) */
	if (clazzes_nr != 0) bvmd_out_rewriteint32(&count_pos, clazzes_nr);

	/* clazz name is not used any more, free it */
	bvm_heap_free(signature.data);
}

/**
 * Command handler logic for both VirtualMachine/AllClasses and VirtualMachine/AllClassesWithGeneric.
 *
 * @param in the request input stream
 * @param out the response output stream
 * @param do_generic if BVM_TRUE, output the clazz generic signature.  Note:  Babe does not load the clazz
 * generic signature, so \c NULL will always be output instead.
 */
static void do_allclasses(bvmd_instream_t* in, bvmd_outstream_t* out, bvm_bool_t do_generic) {

	bvmd_streampos_t count_pos;
	bvm_uint32_t i = bvm_gl_clazz_pool_bucketcount;
	bvm_uint32_t clazzes_nr = 0;

	/* write out a zero counter and remember the place in the stream where it was written */
	count_pos = bvmd_out_writeint32(out, 0);

	/* for each clazz in each non-NULL bucket in the clazz pool */
	while (i--) {

		bvm_clazz_t *clazz = bvm_gl_clazz_pool[i];

		while (clazz != NULL) {

			clazzes_nr++;

			bvmd_out_writebyte(out, bvmd_clazz_reftype(clazz));
			bvmd_out_writeobject(out, clazz);
			bvmd_out_writeutfstring(out, clazz->jni_signature);

			if (do_generic) {

#if BVM_DEBUGGER_SIGNATURES_ENABLE

				if (BVM_CLAZZ_IsInstanceClazz(clazz)) {
					bvmd_out_writeutfstring(out, ((bvm_instance_clazz_t *) clazz)->generic_signature );
				} else {
					bvmd_out_writeutfstring(out, NULL);
				}
#else
				bvmd_out_writeutfstring(out, NULL);
#endif
			}

			bvmd_out_writeint32(out, bvmd_clazz_status(clazz));

			clazz = clazz->next;
		}

	}

	/* rewrite the count */
    bvmd_out_rewriteint32(&count_pos, clazzes_nr);
}

/**
 * JDWP Command handler for VirtualMachine/AllClasses.
 *
 * @param in the request input stream
 * @param out the response output stream
 *
 * @see #do_allclasses()
 */
void bvmd_ch_VM_AllClasses(bvmd_instream_t* in, bvmd_outstream_t* out) {
	do_allclasses(in, out, BVM_FALSE);
}

/**
 * JDWP Command handler for VirtualMachine/AllThreads.
 *
 * Outputs all active threads in the #bvm_gl_threads global list.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_VM_AllThreads(bvmd_instream_t* in, bvmd_outstream_t* out) {

	/* get the head of the global vmthreads list */
	bvm_vmthread_t *vm_thread = bvm_gl_threads;

	/* output number of threads */
	bvmd_out_writeint32(out, bvm_gl_thread_active_count);

	/* output each active thread */
	while (vm_thread != NULL) {

		/* if the thread is alive, output the java thread object */
		if (bvm_thread_is_alive(vm_thread))
			bvmd_out_writeobject(out, vm_thread->thread_obj);

		/* move onto the next in the global thread list */
		vm_thread = vm_thread->next;
	}
}


/**
 * JDWP Command handler for VirtualMachine/TopLevelThreadGroups.
 *
 * @param in the request input stream
 * @param out the response output stream
 *
 * @note Babe does not support thread groups, so here the standard response is to pretend some do exist.  The
 * #BVMD_THREADGROUP_SYSTEM_ID is output as the ID of the single top level thread group.
 *
 */
void bvmd_ch_VM_TopLevelThreadGroups(bvmd_instream_t* in, bvmd_outstream_t* out) {

	/* The top level thread group actually does not exist, but we tell the debugger there is one.
	 * Eclipse looks for named thread groups in it debugger setup.  The ID is hardcoded.
	 */
	bvmd_out_writeint32(out, 1);
	bvmd_out_writeint32(out, BVMD_THREADGROUP_SYSTEM_ID);
}

/**
 * JDWP Command handler for VirtualMachine/Dispose.
 *
 * Closes a debugger session - all cleanup for the session is done during debug session shutdown..
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_VM_Dispose(bvmd_instream_t* in, bvmd_outstream_t* out) {

	/* the debugger has disconnected, so shut everything down and free all resources used
	 * for the debug session */
	bvmd_session_close();
}

/**
 * JDWP Command handler for VirtualMachine/IDSizes.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_VM_IDSizes(bvmd_instream_t* in, bvmd_outstream_t* out) {

	bvm_uint32_t sz = bvm_ntohl(sizeof(size_t));
    bvm_int32_t sz2 = bvm_ntohl(sizeof(bvm_int32_t));

	bvm_uint32_t reply[5];

	reply[0] = sz;	/* fieldIDSize - field IDs are pointers to bvm_field_t */
	reply[1] = sz;	/* methodIDSize - method Ids are pointers to bvm_method_t */
	reply[2] = sz2; /* objectIDSize - object IDs are ints from the ID pool for the object pointer */
	reply[3] = sz2;	/* referenceTypeIDSize - ref ids are IDs from the ID pool for the clazz pointer */
	reply[4] = sz;	/* frameIDSize - frame IDs are pointers to the 'locals' in a frame */

	bvmd_out_writebytes(out, &reply[0], sizeof(reply));
}

/**
 * JDWP Command handler for VirtualMachine/Suspend.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_VM_Suspend(bvmd_instream_t* in, bvmd_outstream_t* out) {
	/* suspend all threads */
	bvmd_thread_suspend_all();
}

/**
 * JDWP Command handler for VirtualMachine/Resume.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_VM_Resume(bvmd_instream_t* in, bvmd_outstream_t* out) {
	/* un-suspend all threads, not force, no clearing parked thread events */
	bvmd_thread_resume_all(BVM_FALSE, BVM_FALSE);
}

/**
 * JDWP Command handler for VirtualMachine/Exit.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_VM_Exit(bvmd_instream_t* in, bvmd_outstream_t* out) {
	BVM_VM_EXIT(bvmd_in_readint32(in), NULL);
}

/**
 * JDWP Command handler for VirtualMachine/Capabilities.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_VM_Capabilities(bvmd_instream_t* in, bvmd_outstream_t* out) {

	bvm_uint8_t reply[] = {

		0, /* canWatchFieldModification	Can the VM watch field modification, and therefore can it send the Modification Watchpoint Event? */
		0, /* canWatchFieldAccess	Can the VM watch field access, and therefore can it send the Access Watchpoint Event?  */

#if BVM_DEBUGGER_BYTECODES_ENABLE
		1, /* canGetBytecodes	Can the VM get the bytecodes of a given method?   */
#else
		0, /* canGetBytecodes	Can the VM get the bytecodes of a given method?   */
#endif
		1, /* canGetSyntheticAttribute	Can the VM determine whether a field or method is synthetic? (that is, can the VM determine if the method or the field was invented by the compiler?) */
#if BVM_DEBUGGER_MONITORINFO_ENABLE
		1, /* canGetOwnedMonitorInfo	Can the VM get the owned monitors information for a thread?  */
		1, /* canGetCurrentContendedMonitor	Can the VM get the current contended monitor of a thread? */
		1  /* canGetMonitorInfo	Can the VM get the monitor information for a given object?   */
#else
		0, /* canGetOwnedMonitorInfo	Can the VM get the owned monitors information for a thread?  */
		0, /* canGetCurrentContendedMonitor	Can the VM get the current contended monitor of a thread? */
		0  /* canGetMonitorInfo	Can the VM get the monitor information for a given object?   */
#endif
	};

	bvmd_out_writebytes(out, &reply[0], sizeof(reply));
}

/**
 * JDWP Command handler for VirtualMachine/ClassPaths.
 *
 * @param in the request input stream
 * @param out the response output stream
 *
 * @note No classpath information is (as yet) returned from Babe.
 */
void bvmd_ch_VM_ClassPaths(bvmd_instream_t* in, bvmd_outstream_t* out) {
	bvmd_out_writeutfstring(out, NULL);
	bvmd_out_writeint32(out, 0);
	bvmd_out_writeint32(out, 0);
}

/**
 * JDWP Command handler for VirtualMachine/DisposeObjects.
 *
 * Each given object ID is removed from the dbg ID map.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_VM_DisposeObjects(bvmd_instream_t* in, bvmd_outstream_t* out) {
	bvm_int32_t count = bvmd_in_readint32(in);
	while (count--) {
		bvmd_id_remove_id(bvmd_in_readint32(in));
	}
}

/**
 * JDWP Command handler for VirtualMachine/CreateString.
 *
 * @param in the request input stream
 * @param out the response output stream
 *
 * @note Personally, I cannot understand the utility of this JDWP command at all.  The string will be
 * cleaned up at some stage - the JDWP spec does not even say to stop it
 * being GC'd.  It is only a matter of luck if this object is not GC'd the next time the
 * debugger attempts to uses it.  Go figure.
 *
 */
void bvmd_ch_VM_CreateString(bvmd_instream_t* in, bvmd_outstream_t* out) {

	bvm_utfstring_t utfstring;
	bvmd_in_readutfstring(in, &utfstring);

	BVM_BEGIN_TRANSIENT_BLOCK {
		bvm_string_obj_t *string_obj = bvm_string_create_from_utfstring(&utfstring, BVM_FALSE);

		BVM_MAKE_TRANSIENT_ROOT(string_obj);

		bvmd_out_writeobject(out, string_obj);
	} BVM_END_TRANSIENT_BLOCK

	bvm_heap_free(utfstring.data);
}

/**
 * JDWP Command handler for VirtualMachine/HoldEvents.
 *
 * Causes the VM to stop and spin waiting for a 'ReleaseEvents' from the debugger.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_VM_HoldEvents(bvmd_instream_t* in, bvmd_outstream_t* out) {
	bvmd_gl_holdevents = BVM_TRUE;
	bvmd_spin_on_debugger();
}

/**
 * JDWP Command handler for VirtualMachine/ReleaseEvents.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_VM_ReleaseEvents(bvmd_instream_t* in, bvmd_outstream_t* out) {
	bvmd_gl_holdevents = BVM_FALSE;
}

/**
 * JDWP Command handler for VirtualMachine/CapabilitiesNew.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_VM_CapabilitiesNew(bvmd_instream_t* in, bvmd_outstream_t* out) {

	bvm_uint8_t reply[] = {
		0, /* canRedefineClasses	Can the VM redefine classes?  */
		0, /* canAddMethod	Can the VM add methods when redefining classes?  */
		0, /* canUnrestrictedlyRedefineClasses	Can the VM redefine classesin arbitrary ways? */
		0, /* canPopFrames	Can the VM pop stack frames?  */
		0, /* canUseInstanceFilters	Can the VM filter events by specific object? */
#if BVM_DEBUGGER_JSR045_ENABLE
		1, /* canGetSourceDebugExtension	Can the VM get the source debug extension?  */
#else
		0, /* canGetSourceDebugExtension	Can the VM get the source debug extension?  */
#endif
		0, /* canRequestVMDeathEvent	Can the VM request VM death events?  */
#if BVM_DEBUGGER_JSR045_ENABLE
		1, /* canSetDefaultStratum	Can the VM set a default stratum?  */
#else
		0, /* canSetDefaultStratum	Can the VM set a default stratum?  */
#endif
		0, /* canGetInstanceInfo	Can the VM return instances, counts of instances of classes and referring objects? */
		0, /* canRequestMonitorEvents	Can the VM request monitor events?  */
		0, /* canGetMonitorFrameInfo	Can the VM get monitors with frame depth info? */
		0, /* canUseSourceNameFilters	Can the VM filter class prepare events by source name? */
		0, /* canGetConstantPool	Can the VM return the constant pool information?  */
		0, /* canForceEarlyReturn	Can the VM force early return from a method?  */
		0, /* reserved22	Reserved for future capability  */
		0, /* reserved23	Reserved for future capability  */
		0, /* reserved24	Reserved for future capability  */
		0, /* reserved25	Reserved for future capability  */
		0, /* reserved26	Reserved for future capability  */
		0, /* reserved27	Reserved for future capability  */
		0, /* reserved28	Reserved for future capability  */
		0, /* reserved29	Reserved for future capability  */
		0, /* reserved30	Reserved for future capability  */
		0, /* reserved31	Reserved for future capability  */
		0  /* reserved32	Reserved for future capability  */
	};

	/* the older 'Capabilities' command reply is the first part of
	 * the 'CapabilitiesNew' command reply. */
	bvmd_ch_VM_Capabilities(in, out);

	bvmd_out_writebytes(out, &reply[0], sizeof(reply));
}


/**
 * JDWP Command handler for VirtualMachine/SetDefaultStratum.
 *
 * Requires compile-time variable #BVM_DEBUGGER_JSR045_ENABLE to be true.
 *
 * @param in the request input stream
 * @param out the response output stream
 *
 * @note This is support for JSR045 "Debugging Other Languages". However, the JSR does not actually say what is to be done
 * with the "default stratum".  Nor does JDWP.  Funnily, there is no "getDefaultStratum" - so the debugger is never
 * interested in getting it, only setting it, then we don't know what to do with it.  The only possible thing I can imagine
 * it might be useful for is providing stack traces that respect the line numbers and various non-java source artifacts
 * that created the java class.  But ... that is not specified anywhere.
 *
 * So ... this function does nothing ....
 *
 */
void bvmd_ch_VM_SetDefaultStratum(bvmd_instream_t* in, bvmd_outstream_t* out) {
	/* No operation */
}


/**
 * JDWP Command handler for VirtualMachine/AllClassesWithGeneric.
 *
 * @param in the request input stream
 * @param out the response output stream
 *
 * @see #do_allclasses()
 *
 */
void bvmd_ch_VM_AllClassesWithGeneric(bvmd_instream_t* in, bvmd_outstream_t* out) {
	do_allclasses(in, out, BVM_TRUE);
}

#endif
