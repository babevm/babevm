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

#ifndef BVM_NATIVEMETHOD_POOL_H_
#define BVM_NATIVEMETHOD_POOL_H_

/**
  @file

  Constants/Macros/Functions/Types for the native method lookup.

  @author Greg McCreath
  @since 0.0.10

*/

/**
 * A native method descriptor - used in the native method pool
 */
typedef struct _bvmnativemethoddescstruct {

	/** Pointer to #bvm_utfstring_t class name in the utf string pool */
	bvm_utfstring_t *clazzname;

	/** Pointer to #bvm_utfstring_t method name in the utf string pool */
	bvm_utfstring_t *name;

	/** Pointer to #bvm_utfstring_t method description in the utf string pool */
	bvm_utfstring_t *desc;

	/** The native method function pointer */
	bvm_native_method_t method;

	/** pointer to the next native method descriptor in the same hash bucket as this one */
	struct _bvmnativemethoddescstruct *next;

} bvm_native_method_desc_t;


/* a pool of utfstrings.  An array of bvm_utfstring_t pointers */
extern bvm_native_method_desc_t **bvm_gl_native_method_pool;
extern int bvm_gl_native_method_pool_bucketcount;

bvm_native_method_desc_t *bvm_native_method_pool_get(bvm_utfstring_t *classname, bvm_utfstring_t *name, bvm_utfstring_t *desc);
void bvm_native_method_pool_register(char *clazzname, char *methodname, char *methoddesc, bvm_native_method_t method);

#endif /*BVM_NATIVEMETHOD_POOL_H_*/
