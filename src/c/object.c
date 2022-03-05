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

  Object creation and management routines.

  @author Greg McCreath
  @since 0.0.10

 */

#if 0
/**
 * Hash function.  Refer "http://www.burtleburtle.net/bob/hash/doobs.html".
 * Differs only in that no mask is supplied.
 */
bvm_uint32_t bvm_calchash(bvm_uint8_t *key, bvm_uint16_t len) {
	bvm_uint32_t hash, i;
	for (hash=0, i=len; i--;) {
		hash += key[i];
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}
	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);
	return hash;
}

bvm_uint32_t calchash2(bvm_uint8_t *key, bvm_uint16_t len) {

	bvm_uint32_t value, i;

	i = len;

	value = 0;
	while (i--)
		value += *key++;

	value = (value << 6) + len;

	return value;
}


bvm_uint32_t bvm_calchash(bvm_uint8_t *key, bvm_uint16_t len)
{
	bvm_uint32_t hash = 0;
    while (len--) {
        int c = *key++;
        hash = (((hash << 3) + hash)  << 2) + hash  + c;
    }
    return hash;
}

#endif

/**
 * Handle to a pre-built out-of-memory exception object.  Gets its value during VM initialisation. We do not create it
 * during memory allocation - difficult to create an "out of memory" object when there is no memory!
 */
bvm_throwable_obj_t *bvm_gl_out_of_memory_err_obj = NULL;

/**
 * Calculate a hash of a given range of bytes.
 *
 * @param key a pointer to an array of bytes to calculate a hash on.
 * @param len the length of the array of bytes used in the hash calculation.
 *
 * @return the hash of the bytes
 */
bvm_uint32_t bvm_calchash(bvm_uint8_t *key, bvm_uint16_t len) {

	bvm_uint32_t hash = 0;
	bvm_uint16_t i = 0;
	while (len--) {
	    hash = 31*hash + key[i++];
	}

	return hash;
}

/**
 * Create an object reference of the given clazz.  Memory is allocated for the new object and all its
 * instance fields.  All field values are zeroed.
 *
 * The object constructor is not called - hence the name of the function is simply 'alloc' rather
 * than 'construct'.
 *
 * @param clazz the clazz of the new object.
 *
 * @return a new object.
 */
bvm_obj_t *bvm_object_alloc(bvm_instance_clazz_t *clazz) {

	bvm_int32_t size;
	bvm_obj_t *obj;

	/* how many bytes storage required for object?  The size of an object is the size of
	 * a clazz pointer + the fields - each field is contained in a bvm_cell_t).
	 * The clazz pointer is also the size of a cell, so our memory requirement equals
	 * "nr of fields + 1 * size of a cell" */
	size = ( (clazz->instance_fields_count + 1 ) * sizeof(bvm_cell_t));

    /* get the memory (zeroed). We'll check if it is a string here - hopefully (given the number of strings
     * Java programs create, this will ease the GC a bit as the GC specifically looks for BVM_ALLOC_TYPE_STRING
     * allocations and does them faster - although we suffer the alloc type check overhead here - bugger. */
    obj = bvm_heap_calloc(size, clazz == BVM_STRING_CLAZZ ? BVM_ALLOC_TYPE_STRING : BVM_ALLOC_TYPE_OBJECT);

	/* set the class of the new object */
	obj->clazz = (bvm_clazz_t *) clazz;

	return obj;
}

/**
 * Create a primitive array object of the given size and component type.
 *
 * @param length the array length
 * @param type the primitive type of the array
 *
 * @return a new array object
 */
bvm_jarray_obj_t *bvm_object_alloc_array_primitive(bvm_uint32_t length, bvm_jtype_t type) {

	bvm_jarray_obj_t *array_obj;

	/* the actual size of the array memory to allocate is the size of the object array struct + the requested
	* length less one.  Why?  Well, the array object structures are all the 'array_obj_t' structures and they
	* include in their structure a 'data[1]' member.  This allows easy typed access in code.  If we left it at
	* that we would always be allocating one more array element than we need to.  For 1000 strings, that would
	* be 2k of unused chars.  So, we suffer a bit of code here to make sure we do not waste that memory on an
	* unused array element.
	*
	* TODO: We have to be careful here.  When using native 64bit longs, we may find that alignment of the 'data'
	*  member of the array struct is different to the others.  The 'data' member may become aligned on a 8 byte
	*  boundary. tackle that when we need to ... */
	int element_size = bvm_gl_type_array_info[type].element_size;
	int array_struct_size = 0; /*= (type == BVM_T_LONG) ? sizeof(bvm_jlong_array_obj_t) - sizeof(bvm_int64_t) : sizeof(bvm_jarray_obj_t);*/

	/* overflow protection for size calculation below.  If a very large length number is specified
	* for the array, the size calculation could trip up and overflow - nasty.  This trap limits
	* the length of the array to stop overflow, but the limit is still stupidly large for this
	* small VM. */
	if (length > 0x1000000) {
		BVM_THROW(bvm_gl_out_of_memory_err_obj)
	}

	switch(type) {
	 case(BVM_T_BOOLEAN):
		 array_struct_size = sizeof(bvm_jboolean_array_obj_t);
		 break;
	 case(BVM_T_CHAR):
		 array_struct_size = sizeof(bvm_jchar_array_obj_t);
		 break;
	 case(BVM_T_FLOAT):
#if BVM_FLOAT_ENABLE
		 array_struct_size = sizeof(bvm_jfloat_array_obj_t);
		 break;
#else
		bvm_throw_exception(BVM_ERR_INTERNAL_ERROR, "float not supported");
#endif
	 case(BVM_T_DOUBLE):
#if BVM_FLOAT_ENABLE
		 array_struct_size = sizeof(bvm_jdouble_array_obj_t);
		 break;
#else
		bvm_throw_exception(BVM_ERR_INTERNAL_ERROR, "double not supported");
#endif
	 case(BVM_T_BYTE):
		 array_struct_size = sizeof(bvm_jbyte_array_obj_t);
		 break;
	 case(BVM_T_SHORT):
		 array_struct_size = sizeof(bvm_jshort_array_obj_t);
		 break;
	 case(BVM_T_INT):
		 array_struct_size = sizeof(bvm_jint_array_obj_t);
		 break;
	 case(BVM_T_LONG):
		 array_struct_size = sizeof(bvm_jlong_array_obj_t);
		 break;
	 default:
		 array_struct_size = 0;
		 break;
	}

    /* alloc the memory for the array */
	array_obj = bvm_heap_calloc(array_struct_size + (element_size * (length-1) ), BVM_ALLOC_TYPE_ARRAY_OF_PRIMITIVE);

	array_obj->clazz  = bvm_gl_type_array_info[type].primitive_array_clazz;
	array_obj->length.int_value = length;

	return array_obj;
}

/**
 * Create an object array given a length and reference component type.
 *
 * @param length the length of the new array
 * @param component_clazz the component type #bvm_clazz_t
 *
 * @return a new array object
 */
bvm_instance_array_obj_t *bvm_object_alloc_array_reference(bvm_uint32_t length, bvm_clazz_t *component_clazz) {

	bvm_uint32_t size;
	bvm_instance_array_obj_t *array_obj;
	bvm_array_clazz_t *array_clazz;
	bvm_utfstring_t *component_clazz_name = component_clazz->name;
	char *array_clazz_name = NULL;

	/* overflow protection for size calculation below.  If a very large length number is specified
	* for the array, the size calculation could trip up and overflow - nasty.  This trap limits
	* the length of the array to stop overflow, but the limit is still stupidly large */
	if (length > 0x1000000) {
		BVM_THROW(bvm_gl_out_of_memory_err_obj)
	}

	/* size of instance + array length + the actual data */
	size = sizeof(bvm_jarray_obj_t) + (sizeof(void *) * length);

	/* a bit of string manipulation to create an array class name like '[Lxx/xx/xx;'
	 * so that we get the correct array calls.  Note that the class name
	 * specified for the array may actually *already* be an array type class
	 * and start with '[' (when an array of arrays is being created. If this
	 * is the case, we prefix only an '[' and not a '[L'. */
	array_clazz_name = bvm_heap_alloc(component_clazz_name->length + 4, BVM_ALLOC_TYPE_DATA);
	array_clazz_name[0] = '[';

	if (component_clazz_name->data[0] != '[') {
		array_clazz_name[1] = 'L';
		memcpy(&array_clazz_name[2],component_clazz_name->data,component_clazz_name->length);
		array_clazz_name[component_clazz_name->length+2] = ';';
		array_clazz_name[component_clazz_name->length+3] = 0;
	} else {
		memcpy(&array_clazz_name[1],component_clazz_name->data,component_clazz_name->length);
		array_clazz_name[component_clazz_name->length+1] = 0;
	}

	BVM_BEGIN_TRANSIENT_BLOCK {

		/* make the name memory transient so it does not get cleaned up in the next alloc */
		BVM_MAKE_TRANSIENT_ROOT(array_clazz_name);

		/* get the array class using our constructed array name */
		array_clazz = (bvm_array_clazz_t *) bvm_clazz_get_c(component_clazz->classloader_obj, array_clazz_name);

	} BVM_END_TRANSIENT_BLOCK

	/* free up the temp memory used for the class name */
	bvm_heap_free(array_clazz_name);

    /* get the zero-filled memory for the object array */
    array_obj = bvm_heap_calloc(size, BVM_ALLOC_TYPE_ARRAY_OF_OBJECT);

	array_obj->clazz = array_clazz;
	array_obj->length.int_value = length;

	return array_obj;
}

/**
 * Recursive creation of a multi-dimensional array from the stack.  Support code
 * for #OPCODE_multianewarray.
 *
 * Developer notes:  This one can be a little tough to get your head around.  Firstly, unlike
 * #bvm_object_alloc_array_reference and #bvm_object_alloc_array_primitive, the clazz passed in here is the
 * actual clazz of the multi-array to create, not the clazz of the component of the to-be-created array.
 *
 * Secondly, the \c dimensions argument may seem superflous given that we can find the number of dimension
 * from the class name (like, just look at the '[' char in the front) - but this is not the case.  In Java, the
 * array class (say) int[][][] = new int[2][3][] has only two dimensions according to the bytecode. So we
 * must respect the number of dimensions given in bytecode, not the name.
 *
 * This function will create an object of the given array type and populate its elements with
 * instances of its component arrays.  This will go down as far as the number of dimensions permit, or until
 * the component clazz is no longer an array (then we know we have reached the innermost array).
 *
 * @param clazz the #bvm_array_clazz_t of the array to create
 * @param dimensions the number of dimensions of the array
 * @param lengths an array of integer lengths - one for each array dimension
 *
 * @return a instance object array populated with sub-arrays.
 */
bvm_instance_array_obj_t *bvm_object_alloc_array_multi(bvm_array_clazz_t *clazz, int dimensions, bvm_native_long_t lengths[]) {

	bvm_instance_array_obj_t *array_obj;
	bvm_int32_t lc;
	bvm_int32_t length;
	bvm_jtype_t component_type;

	/* the length of this array */
	length = lengths[0];

	/* negatives not allowed */
	if (length < 0) bvm_throw_exception(BVM_ERR_NEGATIVE_ARRAY_SIZE_EXCEPTION, NULL);

	/* what component type is it? */
	component_type = clazz->component_jtype;

	/* create a new array of the component type. Differentiate between primitives and objects */
	array_obj = (component_type > BVM_T_ARRAY) ? (bvm_instance_array_obj_t *) bvm_object_alloc_array_primitive(length, component_type) : bvm_object_alloc_array_reference(length, clazz->component_clazz);

	/* if the component type is an array and we're not at the last dimension, fill the current array with
	 * instances of sub-arrays */
	if ( (component_type == BVM_T_ARRAY) && (dimensions > 1) ) {

		BVM_BEGIN_TRANSIENT_BLOCK {

			int subdims       = dimensions - 1;	            /* dimensions for subarray are one less than this array */
			bvm_native_long_t *sublengths = &lengths[1];	/* lengths array for subarrays starts at lengths element 1 of this array */

			BVM_MAKE_TRANSIENT_ROOT(array_obj);

			/* create the subarrays to populate this dimension of the array.  Excludes zero lengths to
			 * cater for JVMS "If any count value is zero, no subsequent dimensions are allocated" */

			for (lc = length; lc--;) {
				/* create the subarray and put it into the current-dimension array */
				array_obj->data[lc] =
					(bvm_obj_t *) bvm_object_alloc_array_multi( (bvm_array_clazz_t *) clazz->component_clazz, subdims, sublengths);
			}

		} BVM_END_TRANSIENT_BLOCK
	}

	return array_obj;
}

