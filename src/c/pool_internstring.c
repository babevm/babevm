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

 VM cache for interned String object instances.

 @author Greg McCreath
 @since 0.0.10

*/


/**
 * An array of pointers to #bvm_internstring_obj_t - the intern string pool.
 */
bvm_internstring_obj_t **bvm_gl_internstring_pool = NULL;

/**
 * Default intern string pool size
 */
int bvm_gl_internstring_pool_bucketcount = BVM_INTERNSTRING_POOL_BUCKETCOUNT;

/**
 * Given a handle to a utf string, find if its value is already in cache.  If not in the cache and the
 * \c add_if_missing is \c #BVM_TRUE then add it.
 * @param str utf string to check value of
 * @param add_if_missing if #BVM_TRUE, string will be added to cache if not already there.
 * @return pointer to the found utf string, or if, then the value of the first argument 'utf'.
 */
bvm_internstring_obj_t *bvm_internstring_pool_get(bvm_utfstring_t *str, bvm_bool_t add_if_missing) {

	bvm_uint32_t hash;
	bvm_internstring_obj_t *pooled_str;

	hash = bvm_calchash(str->data, str->length) % bvm_gl_internstring_pool_bucketcount;
	pooled_str = bvm_gl_internstring_pool[hash];
	for (; (pooled_str != NULL) &&
	       (bvm_str_utfstringcmp(str, pooled_str->utfstring) != 0); pooled_str = pooled_str->next);

	if ( (pooled_str == NULL) && add_if_missing) {
		pooled_str = bvm_internstring_pool_add(str);
	}

	return pooled_str;
}

/**
 * Adds a utf string to the cache.  No checking is performed to see if it is already there.
 *
 * @param str the utf string to add
 * @return a pointer to the interned string.
 */
bvm_internstring_obj_t *bvm_internstring_pool_add(bvm_utfstring_t *str) {

	bvm_uint32_t hash;
	bvm_internstring_obj_t *internstr;

	internstr = (bvm_internstring_obj_t *) bvm_string_create_from_utfstring(str, BVM_TRUE);

	hash = bvm_calchash(str->data, str->length) % bvm_gl_internstring_pool_bucketcount;
	internstr->next = bvm_gl_internstring_pool[hash];
	bvm_gl_internstring_pool[hash] = internstr;

	return internstr;
}

