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

  JDWP ArrayType commandset command handlers.

  @author Greg McCreath
  @since 0.0.10

*/

#if BVM_DEBUGGER_ENABLE

/**
 * JDWP Command handler for ArrayType/NewInstance.
 *
 * Note that although this function does create a new array, the JDWP spec does not say that it should
 * be stopped from being GC'd, so ... it is only a matter of luck if this object is still about when the
 * debugger uses it.  Weird.
 *
 * @param in the request input stream
 * @param out the response output stream
 *
 * @see #bvmd_RT_GetSetValues()
 */
void bvmd_ch_AT_NewInstance(bvmd_instream_t* in, bvmd_outstream_t* out) {

	/* unsure of the utility of this command.  It allocates an array for the debugger, but it will be only
	 * pure luck if it is not GC'd by the time the debugger uses it for something.  Dunno.  Seems very inexact.
	 */

	bvm_array_clazz_t *clazz;
	bvm_obj_t *obj;
	bvm_int32_t length;

	if ( (clazz = (bvm_array_clazz_t *) bvmd_readcheck_reftype(in, out)) == NULL) return;

	/* the length of the new array */
	length = bvmd_in_readint32(in);

	/* check that we do have an array class, and the length is not negative */
	if ( (!BVM_CLAZZ_IsArrayClazz(clazz)) || (length < 0)) {
		out->error = JDWP_Error_INVALID_ARRAY;
		return;
	}

	BVM_BEGIN_TRANSIENT_BLOCK {

		/* use the component clazz on the array class to decide whether to create a instance
		 * or primitive array object.
		 */
		if (clazz->component_clazz != NULL) {
			obj = (bvm_obj_t *) bvm_object_alloc_array_reference( length, clazz->component_clazz);
		} else {
			obj = (bvm_obj_t *) bvm_object_alloc_array_primitive(length, clazz->component_jtype);
		}

		/* writing may allocate from the heap, so make the new array a transient root */
		BVM_MAKE_TRANSIENT_ROOT(obj);

		bvmd_out_writebyte(out, JDWP_TagType_ARRAY);
		bvmd_out_writeobject(out, obj);

	} BVM_END_TRANSIENT_BLOCK
}
#endif
