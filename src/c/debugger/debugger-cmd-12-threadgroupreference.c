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

  JDWP ThreadGroupReference commandset command handlers.

  Babe does not support ThreadGroup objects.  However, for the purposes of JDWP, they are
  emulated by pretending they exist.  It gives the appearance to a debugger that there are
  real thread groups - indeed, some debuggers expect thread groups - so we can't just
  ignore them.

  Two thread groups are emulated - the 'system' thread group is object ID #BVMD_THREADGROUP_SYSTEM_ID,
  and the 'main' thread group is object ID #BVMD_THREADGROUP_MAIN_ID.

  The "system" thread group is the root thread group, and the "main" thread group is its only child.

  All VM threads are children of the "main" thread group.

  @author Greg McCreath
  @since 0.0.10

*/

#if BVM_DEBUGGER_ENABLE

char *DBG_THREADGROUP_SYSTEM_NAME = "system";
char *DBG_THREADGROUP_MAIN_NAME = "main";

/**
 * JDWP Command handler for ThreadGroupReference/Name.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_TGR_Name(bvmd_instream_t* in, bvmd_outstream_t* out) {

	int id = bvmd_in_readint32(in);

	if (id == BVMD_THREADGROUP_SYSTEM_ID) {
		bvmd_out_writestring(out, DBG_THREADGROUP_SYSTEM_NAME);
	} else if (id == BVMD_THREADGROUP_MAIN_ID) {
		bvmd_out_writestring(out, DBG_THREADGROUP_MAIN_NAME);
	} else {
		out->error = JDWP_Error_INVALID_THREAD_GROUP;
	}
}

/**
 * JDWP Command handler for ThreadGroupReference/Parent.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_TGR_Parent(bvmd_instream_t* in, bvmd_outstream_t* out) {

	int id = bvmd_in_readint32(in);

	if (id == BVMD_THREADGROUP_SYSTEM_ID) {
		bvmd_out_writeint32(out, 0);
	} else if (id == BVMD_THREADGROUP_MAIN_ID) {
		bvmd_out_writeint32(out, BVMD_THREADGROUP_SYSTEM_ID);
	} else {
		out->error = JDWP_Error_INVALID_THREAD_GROUP;
	}
}

/**
 * JDWP Command handler for ThreadGroupReference/Children.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_TGR_Children(bvmd_instream_t* in, bvmd_outstream_t* out) {

	int id = bvmd_in_readint32(in);

	if (id == BVMD_THREADGROUP_SYSTEM_ID) {
		/* no threads */
		bvmd_out_writeint32(out, 0);
		/* write out one child group */
		bvmd_out_writeint32(out, 1);
		/* and it is the "main" group */
		bvmd_out_writeint32(out, BVMD_THREADGROUP_MAIN_ID);
	} else if (id == BVMD_THREADGROUP_MAIN_ID) {
		/* write out 'all threads' as children */
		bvmd_ch_VM_AllThreads(in, out);
		/* write out zero child groups */
		bvmd_out_writeint32(out, 0);
	} else {
		out->error = JDWP_Error_INVALID_THREAD_GROUP;
	}
}

#endif
