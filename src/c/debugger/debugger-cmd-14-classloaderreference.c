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

  JDWP ClassLoaderReference commandset command handlers.

  @author Greg McCreath
  @since 0.0.10

*/

#if BVM_DEBUGGER_ENABLE

/**
 * JDWP Command handler for ClassLoaderReference/VisibleClasses.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_CLR_VisibleClasses(bvmd_instream_t* in, bvmd_outstream_t* out) {

	bvmd_streampos_t count_pos;
	bvm_class_instance_array_obj_t *clarray;
    bvm_int32_t length;
    bvm_int32_t clazzes_nr = 0;
	bvm_clazz_t *clazz;
	bvm_class_obj_t *class_obj;
	bvm_classloader_obj_t *obj = bvmd_in_readobject(in);

	if (obj == NULL) {
		out->error = JDWP_Error_INVALID_OBJECT;
		return;
	}

	if (!bvm_clazz_is_subclass_of(obj->clazz, (bvm_clazz_t *) BVM_CLASSLOADER_CLAZZ)) {
		out->error = JDWP_Error_INVALID_CLASS_LOADER;
		return;
	}

	/* write out a zero counter and remember the place in the stream where it was written */
	count_pos = bvmd_out_writeint32(out, 0);

	/* output all non-bootstrap-classloader classes including parentage   */
	do {
		clarray = obj->class_array;
		length = clarray->length.int_value;

		while (length--) {

			class_obj = clarray->data[length];

			if (class_obj != NULL) {
				clazzes_nr++;
				clazz = class_obj->refers_to_clazz;
				bvmd_out_writebyte(out, bvmd_get_jdwptag_from_clazz(clazz));
				bvmd_out_writeobject(out, clazz);
			}
		}
	} while( (obj = obj->parent) );

	/* output all bootstrap-classloader classes */
	{
		bvm_uint32_t i = bvm_gl_clazz_pool_bucketcount;

		/* go to each clazz in the clazz pool and check it */
		while (i--) {

			clazz = bvm_gl_clazz_pool[i];

			while (clazz != NULL) {

				if (clazz->classloader_obj == BVM_BOOTSTRAP_CLASSLOADER_OBJ) {

					clazzes_nr++;

					bvmd_out_writebyte(out, bvmd_get_jdwptag_from_clazz(clazz));
					bvmd_out_writeobject(out, clazz);
				}

				clazz = clazz->next;
			}
		}
	}

	/* rewrite the clazzes count */
    bvmd_out_rewriteint32(&count_pos, clazzes_nr);
}

#endif
