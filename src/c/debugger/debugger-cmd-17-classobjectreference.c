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

  JDWP ClassObjectReference commandset command handlers.

  @author Greg McCreath
  @since 0.0.10

*/

#if BVM_DEBUGGER_ENABLE

/**
 * JDWP Command handler for ClassObjectReference/ReflectedType.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_COR_ReflectedType(bvmd_instream_t* in, bvmd_outstream_t* out) {

	bvm_class_obj_t *obj = bvmd_in_readobject(in);

	if ( (obj == NULL) || (obj->clazz != (bvm_clazz_t *) BVM_CLASS_CLAZZ))  {
		out->error = JDWP_Error_INVALID_OBJECT;
		return;
	}

	bvmd_out_writebyte(out, bvmd_clazz_reftype(obj->refers_to_clazz));
	bvmd_out_writeobject(out, obj->refers_to_clazz);
}

#endif
