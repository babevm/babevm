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

 VM cache for loaded class files.

 @author Greg McCreath
 @since 0.0.10

*/

/**
 * An array of pointers to #bvm_clazz_t  - the class pool.
 */
bvm_clazz_t **bvm_gl_clazz_pool = NULL;

/**
 * Default clazz pool size
 */
int bvm_gl_clazz_pool_bucketcount = BVM_CLAZZ_POOL_BUCKETCOUNT;

/**
 * Given a ClassLoader object and a class name, search the clazz pool for an already loaded
 * clazz of that combination.  If not found, go to the parent ClassLoader and try again ..
 * and so on.
 *
 * @param loader a #bvm_classloader_obj_t class loader object
 * @param clazzname the #bvm_utfstring_t fully qualified name of the class to get
 *
 * @return the #bvm_clazz_t if found in the pool, or \c NULL if not found.
 */
bvm_clazz_t *bvm_clazz_pool_get(bvm_classloader_obj_t *loader, bvm_utfstring_t *clazzname) {

	bvm_uint32_t hash;
	bvm_clazz_t *pooled_clazz;
	bvm_classloader_obj_t *search_loader;

	hash = bvm_calchash(clazzname->data, clazzname->length) % bvm_gl_clazz_pool_bucketcount;
	pooled_clazz = bvm_gl_clazz_pool[hash];

	while (pooled_clazz != NULL) {
		search_loader = loader;

		while (BVM_TRUE) {

			/* if the class loader and class name match, cool, we've found it */
			if ( (bvm_str_utfstringcmp(clazzname, pooled_clazz->name) == 0) &&
			   (pooled_clazz->classloader_obj == search_loader)) return pooled_clazz;

			/* if not and we're on the boot loader, exit the loop and go to the next class in
			 * the hash bucket */
			if (search_loader == BVM_BOOTSTRAP_CLASSLOADER_OBJ)
				break;
			else
				/* otherwise, check out the parent class loader */
				search_loader = search_loader->parent;
		}

		pooled_clazz = pooled_clazz->next;
	}

	return pooled_clazz;
}


/**
 * Remove a class from the clazz pool.
 *
 * @param clazz the #bvm_clazz_t of the clazz to remove.
 */
void bvm_clazz_pool_remove(bvm_clazz_t *clazz) {

	bvm_uint32_t hash;
	bvm_clazz_t *pooled_clazz, *prev_clazz;

	hash = bvm_calchash(clazz->name->data, clazz->name->length) % bvm_gl_clazz_pool_bucketcount;

	prev_clazz = NULL;
	pooled_clazz = bvm_gl_clazz_pool[hash];

	while (pooled_clazz != NULL) {

		if (pooled_clazz == clazz) {

			/* if head of list */
			if (prev_clazz == NULL) {
				bvm_gl_clazz_pool[hash] = pooled_clazz->next;
			} else {
				prev_clazz->next = pooled_clazz->next;
			}

			break;
		}

		prev_clazz = pooled_clazz;
		pooled_clazz = pooled_clazz->next;
	}
}

/**
 * Given a ClassLoader object and a class name, scan the class pool for the class/loader
 * combination.
 *
 * Delegates searching to #bvm_clazz_pool_get()
 *
 * @param loader a #bvm_classloader_obj_t class loader object
 * @param clazzname a null-terminated, fully qualified clazz name to search for.
 */
bvm_clazz_t *clazz_pool_get_c(bvm_classloader_obj_t *loader, char *clazzname) {

	bvm_utfstring_t str;
	str.length = strlen(clazzname);
	str.data = (bvm_uint8_t *) clazzname;

	return bvm_clazz_pool_get(loader, &str);
}

/**
 * Add a clazz to the clazz pool.  No checking is performed to see if the clazz is already in
 * there.  It is just added.
 *
 * @param clazz and #bvm_clazz_t to add
 */
void bvm_clazz_pool_add(bvm_clazz_t *clazz) {
	bvm_uint32_t hash = bvm_calchash(clazz->name->data, clazz->name->length) % bvm_gl_clazz_pool_bucketcount;
	clazz->next = bvm_gl_clazz_pool[hash];
	bvm_gl_clazz_pool[hash] = clazz;
}

