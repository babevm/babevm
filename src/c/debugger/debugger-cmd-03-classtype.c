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

  JDWP ClassType commandset command handlers.

  @author Greg McCreath
  @since 0.0.10

*/

#if BVM_DEBUGGER_ENABLE

/**
 * JDWP Command handler for ClassType/Superclass.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_CT_Superclass(bvmd_instream_t* in, bvmd_outstream_t* out) {

	bvm_clazz_t *clazz;

	if ( (clazz = bvmd_readcheck_reftype(in, out)) == NULL) return;

	bvmd_out_writeobject(out, clazz->super_clazz);
}

/**
 * JDWP Command handler for ClassType/SetValues.
 *
 * @param in the request input stream
 * @param out the response output stream
 *
 * @see #bvmd_RT_GetSetValues()
 */
void bvmd_ch_CT_SetValues(bvmd_instream_t* in, bvmd_outstream_t* out) {
	/* get and set are largely the same - use the logic from the referencetype command handler. */
	bvmd_RT_GetSetValues(in, out, BVM_TRUE);
}

#endif
