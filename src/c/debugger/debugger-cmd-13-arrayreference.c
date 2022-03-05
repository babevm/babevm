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

  JDWP ArrayReference commandset command handlers.

  @author Greg McCreath
  @since 0.0.10

*/

#if BVM_DEBUGGER_ENABLE

/**
 * JDWP Command handler for ArrayReference/Length.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_AR_Length(bvmd_instream_t* in, bvmd_outstream_t* out) {

	bvm_jarray_obj_t *array_obj = bvmd_in_readobject(in);

	if (array_obj == NULL) {
		out->error = JDWP_Error_INVALID_OBJECT;
		return;
	}

	if (!BVM_CLAZZ_IsArrayClazz(array_obj->clazz)) {
		out->error = JDWP_Error_INVALID_ARRAY;
		return;
	}

	bvmd_out_writeint32(out, array_obj->length.int_value);
}

/**
 * Read a value into a given index into an array from the input stream.
 *
 * @param in the request input stream.
 * @param tagtype the JDWP tag type of the array's elements.
 * @param array_obj the given array object.
 * @param index the index within the array to read in.
 */
static void dbg_read_array_value(bvmd_instream_t* in, bvm_uint8_t tagtype, bvm_jarray_obj_t *array_obj, int index) {

	switch (tagtype) {

		case (JDWP_Tag_BYTE):
		case (JDWP_Tag_BOOLEAN): {
			( (bvm_jbyte_array_obj_t *) array_obj)->data[index] = bvmd_in_readbyte(in);
			break;
		}

		case (JDWP_Tag_CHAR):
		case (JDWP_Tag_SHORT): {
			( (bvm_jshort_array_obj_t *) array_obj)->data[index] = bvmd_in_readint16(in);
			break;
		}

		case (JDWP_Tag_DOUBLE): {
#if BVM_FLOAT_ENABLE
			bvm_uint32_t *addr = (bvm_uint32_t *) &( (bvm_jdouble_array_obj_t *) array_obj)->data[index];
#if (!BVM_BIG_ENDIAN_ENABLE)
			bvmd_in_readint64(in, (bvm_uint32_t*) (addr+1), (bvm_uint32_t*) addr);
#else
			bvmd_in_readint64(in, (bvm_uint32_t*) addr, (bvm_uint32_t*) (addr+1) );
#endif
			break;
#else
			bvm_throw_exception(BVM_ERR_INTERNAL_ERROR, "double not supported");
#endif
		}
		case (JDWP_Tag_LONG): {

			bvm_uint32_t *addr = (bvm_uint32_t *) &( (bvm_jlong_array_obj_t *) array_obj)->data[index];
#if (!BVM_BIG_ENDIAN_ENABLE)
			bvmd_in_readint64(in, (bvm_uint32_t*) (addr+1), (bvm_uint32_t*) addr);
#else
			bvmd_in_readint64(in, (bvm_uint32_t*) addr, (bvm_uint32_t*) (addr+1) );
#endif
			break;
		}

		case (JDWP_Tag_FLOAT):
#if (!BVM_FLOAT_ENABLE)
			bvm_throw_exception(BVM_ERR_INTERNAL_ERROR, "float not supported");
#endif
		case (JDWP_Tag_INT): {
			( (bvm_jint_array_obj_t *) array_obj)->data[index] = bvmd_in_readint32(in);
			break;
		}

		case (JDWP_Tag_OBJECT):
		case (JDWP_Tag_VOID):
		case (JDWP_Tag_ARRAY):
		case (JDWP_Tag_CLASS_OBJECT):
		case (JDWP_Tag_THREAD_GROUP):
		case (JDWP_Tag_CLASS_LOADER):
		case (JDWP_Tag_STRING):
		case (JDWP_Tag_THREAD): {
			( (bvm_instance_array_obj_t *) array_obj)->data[index] =  bvmd_in_readobject(in);
			break;
		}
	}
}

/**
 * Write out a given index from an array to the output stream.
 *
 * @param out the response output stream.
 * @param tagtype the JDWP tag type of the array's elements.
 * @param array_obj the given array object.
 * @param index the index within the array to write out.
 */
static void dbg_write_array_value(bvmd_outstream_t* out, bvm_uint8_t tagtype, bvm_jarray_obj_t *array_obj, int index) {

	switch (tagtype) {

		case (JDWP_Tag_BYTE):
		case (JDWP_Tag_BOOLEAN): {
			bvmd_out_writebyte(out, ( (bvm_jbyte_array_obj_t *) array_obj)->data[index]);
			break;
		}

		case (JDWP_Tag_CHAR):
		case (JDWP_Tag_SHORT): {
			bvmd_out_writeint16(out, ( (bvm_jshort_array_obj_t *) array_obj)->data[index]);
			break;
		}

		case (JDWP_Tag_DOUBLE): {
#if BVM_FLOAT_ENABLE
			bvm_uint32_t *addr = (bvm_uint32_t *) &( (bvm_jdouble_array_obj_t *) array_obj)->data[index];
#if (!BVM_BIG_ENDIAN_ENABLE)
			bvmd_out_writeint64(out, *(addr+1), *addr);
#else
			bvmd_out_writeint64(out, *addr, *(addr+1));
#endif
			break;
#else
				bvm_throw_exception(BVM_ERR_INTERNAL_ERROR, "double not supported");
#endif
		}
		case (JDWP_Tag_LONG): {

			bvm_uint32_t *addr = (bvm_uint32_t *) &( (bvm_jlong_array_obj_t *) array_obj)->data[index];
#if (!BVM_BIG_ENDIAN_ENABLE)
			bvmd_out_writeint64(out, *(addr+1), *addr);
#else
			bvmd_out_writeint64(out, *addr, *(addr+1));
#endif
			break;
		}

		case (JDWP_Tag_FLOAT):
			/* No requirement to check for BVM_FLOAT_ENABLE.  Who cares here.  If we are supporting floats,
			 * then the bytes of the integer get sent. */
		case (JDWP_Tag_INT): {
			bvmd_out_writeint32(out, ( (bvm_jint_array_obj_t *) array_obj)->data[index]);
			break;
		}
		case (JDWP_Tag_OBJECT): {
			bvm_obj_t *obj = ( (bvm_instance_array_obj_t *) array_obj)->data[index];
			bvmd_out_writebyte(out, bvmd_get_jdwptag_from_object(obj));
			bvmd_out_writeobject(out, obj);
			break;
		}
		case (JDWP_Tag_VOID):
		case (JDWP_Tag_ARRAY):
		case (JDWP_Tag_CLASS_OBJECT):
		case (JDWP_Tag_THREAD_GROUP):
		case (JDWP_Tag_CLASS_LOADER):
		case (JDWP_Tag_STRING):
		case (JDWP_Tag_THREAD): {
			bvm_obj_t *obj = ( (bvm_instance_array_obj_t *) array_obj)->data[index];
			bvmd_out_writebyte(out, tagtype);
			bvmd_out_writeobject(out, obj);
			break;
		}
	}
}

/**
 * Internal command handler logic for ArrayReference/GetValues and ArrayReference/SetValues - they are
 * both basically the same.
 *
 * @param in the request input stream
 * @param out the response output stream
 * @param do_set if BVM_TRUE function will set arrays values, otherwise array values are read and then written
 * the output stream.
 */
static void do_array_GetSetValues(bvmd_instream_t* in, bvmd_outstream_t* out, bvm_bool_t do_set) {
	int firstindex;
	int length;
	bvm_uint8_t tagtype;
	int i;
	int limit;

	bvm_jarray_obj_t *array_obj = bvmd_in_readobject(in);
	bvm_array_clazz_t *clazz;

	if (array_obj == NULL) {
		out->error = JDWP_Error_INVALID_OBJECT;
		return;
	}

	if (!BVM_CLAZZ_IsArrayClazz(array_obj->clazz)) {
		out->error = JDWP_Error_INVALID_ARRAY;
		return;
	}

	firstindex = bvmd_in_readint32(in);
	length = bvmd_in_readint32(in);
	limit = firstindex + length;

	/* check some correctness of length and offset */
	if ( (firstindex < 0 || length < 0) || (firstindex >= array_obj->length.int_value) ||
		 (limit > array_obj->length.int_value) ){
		out->error = JDWP_Error_INVALID_LENGTH;
		return;
	}

	clazz = array_obj->clazz;

	/* if the array component type is primitive (we can tell because the array clazz's component
	 * clazz is NULL) get the primitive tag type from the JVMS type - otherwise, get the
	 * tagtype from the component clazz. */

	if (clazz->component_clazz == NULL) {
		tagtype = bvmd_get_jdwptag_from_jvmstype(clazz->component_jtype);
	} else {
		tagtype = bvmd_get_jdwptag_from_clazz(clazz->component_clazz);
	}

	if (!do_set) {
		/* write out array region */
		bvmd_out_writebyte(out, tagtype);
		bvmd_out_writeint32(out, length);
	}

	/* for each requested index .. */
	for (i=firstindex; i < limit; i++) {
		if (do_set) {
			dbg_read_array_value(in, tagtype, array_obj, i);
		} else {
			dbg_write_array_value(out, tagtype, array_obj, i);
		}
	}
}

/**
 * JDWP Command handler for ArrayReference/GetValues.
 *
 * @param in the request input stream
 * @param out the response output stream
 *
 * @see #do_array_GetSetValues()
 */
void bvmd_ch_AR_GetValues(bvmd_instream_t* in, bvmd_outstream_t* out) {
	do_array_GetSetValues(in, out, BVM_FALSE);
}

/**
 * JDWP Command handler for ArrayReference/SetValues.
 *
 * @param in the request input stream
 * @param out the response output stream
 *
 * @see #do_array_GetSetValues()
 */
void bvmd_ch_AR_SetValues(bvmd_instream_t* in, bvmd_outstream_t* out) {
	do_array_GetSetValues(in, out, BVM_TRUE);
}


#endif
