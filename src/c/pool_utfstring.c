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

 VM cache for utf strings loaded from classes.

 @author Greg McCreath
 @since 0.0.10

 */

/**
 * An array of pointers to #bvm_utfstring_t - the utfstring pool.
 */
bvm_utfstring_t **bvm_gl_utfstring_pool = NULL;

/**
 * Default utf8 string pool size
 */
int bvm_gl_utfstring_pool_bucketcount = BVM_UTFSTRING_POOL_BUCKETCOUNT;

/**
* To get a utf string from the pool that matches the given char* (which must be null terminated).
* If add_if_missing is BVM_TRUE, the char* will be wrapped in a utfstring and added to the pool.
 *
 * @param data char * to string - must be null terminated
 * @param add_if_missing if #BVM_TRUE and the utf string is not in the pool it'll be added.
 * @return a handle to the pooled utf string - will be the same as \c data if added.
 */
bvm_utfstring_t *bvm_utfstring_pool_get_c(const char *data, bvm_bool_t add_if_missing) {
    bvm_utfstring_t tempstr;
    bvm_utfstring_t *pooled_str;

    tempstr.length = (bvm_uint16_t) strlen(data);
    tempstr.data = (bvm_uint8_t *) data;

    pooled_str = bvm_utfstring_pool_get(&tempstr, BVM_FALSE);

    /* if the string is not pooled and we are supposed to add it, create a new UTFString that uses
     * the char data and add it to the pool */
    if ((pooled_str == NULL) && add_if_missing) {

        size_t length = strlen(data);

        pooled_str = bvm_str_new_utfstring(length, BVM_ALLOC_TYPE_STATIC);

        /* copy data plus null terminator */
        memcpy(pooled_str->data, data, length + 1);

        pooled_str->next = NULL;

        bvm_utfstring_pool_add(pooled_str);
    }

    return pooled_str;
}

/**
 * Retrieve a uftstring from the pool.  Returns a pointer to the pooled string if it exists.  If it
 * does not exist if add_if_missing is #BVM_TRUE, the passing in utfstring will be added to the pool and
 * will be the function return value, otherwise (if add_if_missing is #BVM_FALSE) NULL will be
 * returned.
  *
  * @param str the bvm_utfstring_t to look for
  * @param add_if_missing
  * @return the found bvm_utfstring_t if in the pool (or NULL if not), or if not found and \c add_if_missing is #BVM_TRUE, then
  * the same pointer as \c str
  */
bvm_utfstring_t *bvm_utfstring_pool_get(bvm_utfstring_t *str, bvm_bool_t add_if_missing) {

    bvm_uint32_t hash;
    bvm_utfstring_t *pooled_str;

    hash = bvm_calchash(str->data, str->length) % bvm_gl_utfstring_pool_bucketcount;
    pooled_str = bvm_gl_utfstring_pool[hash];
    for (; (pooled_str != NULL) &&
           (bvm_str_utfstringcmp(str, pooled_str) != 0);
           pooled_str = pooled_str->next) {};

    if ((pooled_str == NULL) && add_if_missing) {
        bvm_utfstring_pool_add(str);
        return str;
    }

    return pooled_str;
}

/**
 * Add a utfstring to the pool.  Note this function does not check whether it exists in the
 * pool already, it simple adds it to the vmstring pool array as the first node of the
 * hash bucket
 *
 * @param str the bvm_utfstring_t to add.
 */
void bvm_utfstring_pool_add(bvm_utfstring_t *str) {
    bvm_uint32_t hash = bvm_calchash(str->data, str->length) % bvm_gl_utfstring_pool_bucketcount;
    str->next = bvm_gl_utfstring_pool[hash];
    bvm_gl_utfstring_pool[hash] = str;
}

