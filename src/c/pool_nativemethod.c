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

 VM cache for native methods.

 @author Greg McCreath
 @since 0.0.10

 Functions for the management of native methods.  Native methods are managed as a pool - similar to
 the way other pools are managed.  Native methods are resolved at class loading time - as a class
 is loaded, if a method is flagged as native the native method pool will be interrogated and
 the class method structure will directly reference the pooled method.

 The #bvm_native_method_desc_t struct is used to describe a native method.

 */

/**
 * An array of pointers to #bvm_native_method_desc_t - the native method pool.
 */
bvm_native_method_desc_t **bvm_gl_native_method_pool = NULL;

/**
 * Default native method pool size.
 */
int bvm_gl_native_method_pool_bucketcount = BVM_NATIVEMETHOD_POOL_BUCKETCOUNT;

/**
 * Retrieve a native method from the native method pool.
 *
 * @param clazzname - a bvm_utfstring_t* contain the full internalised name of the class (yes, it must
 * have '/'s and not '.'s as package separators.
 * @param name - a bvm_utfstring_t* of the name of the method
 * @param desc - a bvm_utfstring_t* of the method signature of the method
 * @return a function pointer to a #bvm_native_method_t function, or NULL if one
 * cannot be found.
 */
bvm_native_method_desc_t *bvm_native_method_pool_get(bvm_utfstring_t *clazzname, bvm_utfstring_t *name, bvm_utfstring_t *desc) {

	bvm_uint32_t hash;
	bvm_native_method_desc_t *pooled_method;

	/* calc a hash on the method name.  Nice and simple - no need to hash all three params. */
	hash = bvm_calchash(name->data, name->length) % bvm_gl_native_method_pool_bucketcount;

	/* get the entry for the hash in the pool */
	pooled_method = bvm_gl_native_method_pool[hash];

	/* while we have and entry, scan it (and its list) looking for a match on
	 * classname/name/desc */
	while ( (pooled_method != NULL) && ( (bvm_str_utfstringcmp(clazzname, pooled_method->clazzname) != 0) || (bvm_str_utfstringcmp(name, pooled_method->name) != 0) || (bvm_str_utfstringcmp(desc, pooled_method->desc) != 0)))
		pooled_method = pooled_method->next;

	return pooled_method;
}

/**
 * Add a native method to the native method pool.  Note that NO checking is performed to see if it
 * is already there.  If the described method is already in the pool, it will be added again.  Don't do it.
 *
 * @param method_desc a native method descriptor of the method to add to the pool.
 *
 */
static void native_method_pool_add(bvm_native_method_desc_t *method_desc) {

	/* calc hash on the method name */
	bvm_uint32_t hash = bvm_calchash(method_desc->name->data, method_desc->name->length) % bvm_gl_native_method_pool_bucketcount;

	/* new entries are added at the front of the hash list */

	/* set the 'next' pointer to the head of the bucket list */
	method_desc->next = bvm_gl_native_method_pool[hash];

	/* set the front of the pool bucket to the new entry */
	bvm_gl_native_method_pool[hash] = method_desc;
}

/**
 */

/**
 * To register a native method for later retrieval by the class loading mechanism.  No checking is performed
 * as whether the method is already registered.
 *
 * @param clazzname - a bvm_utfstring_t* contain the full internalised name of the class (yes, it must
 * have '/'s and not '.'s as package separators.
 * @param methodname char * to name of method
 * @param methoddesc char * to method description (signature) of method (like '(Ljava/lang/String;)V')
 * @param method function pointer to native method
 */
void bvm_native_method_pool_register(char *clazzname, char *methodname, char *methoddesc, bvm_native_method_t method) {

	bvm_native_method_desc_t *native_method_desc = bvm_heap_alloc(sizeof(bvm_native_method_desc_t), BVM_ALLOC_TYPE_STATIC);

	native_method_desc->clazzname = bvm_utfstring_pool_get_c(clazzname, BVM_TRUE);
	native_method_desc->name	  = bvm_utfstring_pool_get_c(methodname, BVM_TRUE);
	native_method_desc->desc      = bvm_utfstring_pool_get_c(methoddesc, BVM_TRUE);
	native_method_desc->method    = method;

	native_method_pool_add(native_method_desc);
}
