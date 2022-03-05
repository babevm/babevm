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

  Storage for object or memory references the Debugger has requested not to be GC'd.

  References are held in a hashmap map (#bvmd_root_map) with #BVMD_ROOTMAP_SIZE buckets.  The
  hash is calculated on the reference address.  Each reference is stored in the map using a
  #bvmd_root_t instance.

  The map is considered a GC root.  References held in the map are considered 'reachable' by the GC.

  References will remain in the map until explicitly taken out or the debug session is closed - in which
  case the the entire map is freed and all held references will be subject to GC.

  @author Greg McCreath
  @since 0.0.10

*/

#if BVM_DEBUGGER_ENABLE

/**
 * An array of #bvmd_root_t pointers.  This is the map buckets.
 */
bvmd_root_t **bvmd_root_map = NULL;

/**
 * Create the #bvmd_root_map map with #BVMD_ROOTMAP_SIZE buckets.
 */
void bvmd_root_init() {
	size_t size = BVMD_ROOTMAP_SIZE * sizeof(bvmd_root_t*);
	bvmd_root_map = bvm_heap_calloc(size, BVM_ALLOC_TYPE_STATIC);
}

/**
 * Removes all entries from the map and frees the map.
 */
void bvmd_root_free() {

	int i = BVMD_ROOTMAP_SIZE;

	while (i--) {

		/* get the dbg_id at the hash bucket */
		bvmd_root_t *dbg_root = bvmd_root_map[i];

		/* for each element in the bucket list */
		while (dbg_root != NULL) {
			bvmd_root_t *next = dbg_root->next;
			bvm_heap_free(dbg_root);
			dbg_root = next;
		}
	}

	/* and free up the map */
	bvm_heap_free(bvmd_root_map);

	bvmd_root_map = NULL;
}

/**
 * Gets a entry from the map for a given memory address.
 *
 * @param addr a given address.
 * @return a handle to the #bvmd_root_t that holds the address reference, or \c NULL if it is
 * not in the map.
 */
static bvmd_root_t *dbg_root_get(void *addr) {

	bvmd_root_t *dbg_root = bvmd_root_map[calc_hash_for_ptr(addr, BVMD_ROOTMAP_SIZE)];

	while (dbg_root != NULL) {
		if (dbg_root->addr == addr) return dbg_root;
		dbg_root = dbg_root->next;
	}

	return NULL;
}


/**
 * Removes a given address from the map.  This function has no effect if the given address is not in
 * the map.
 *
 * @param addr a given address.
 */
void bvmd_root_remove(void *addr) {

	bvm_uint32_t hash = calc_hash_for_ptr(addr, BVMD_ROOTMAP_SIZE);

	bvmd_root_t *dbg_root, *prev_dbg_root;

	prev_dbg_root = NULL;
	dbg_root = bvmd_root_map[hash];

	while (dbg_root != NULL) {

		if (dbg_root->addr == addr) {

			/* if head of list */
			if (prev_dbg_root == NULL) {
				bvmd_root_map[hash] = dbg_root->next;
			} else {
				prev_dbg_root->next = dbg_root->next;
			}

			bvm_heap_free(dbg_root);
			return;
		}

		prev_dbg_root = dbg_root;
		dbg_root = dbg_root->next;
	}
}

/**
 * Puts an address into the map.  If the address is already in the map it is not placed in again.
 *
 * @param addr a given address to place in the map.
 */
void bvmd_root_put(void *addr) {

	bvm_uint32_t addrhash;
	bvmd_root_t *dbg_root;

	/* is the address already there? */
	if (dbg_root_get(addr) != NULL) return;

	/* nope .. get the memory for a bvmd_root_t and put it in the map */
	dbg_root = bvm_heap_calloc(sizeof(bvmd_root_t), BVM_ALLOC_TYPE_STATIC);
	dbg_root->addr = addr;

	/* calc the hash into the map buckets */
	addrhash = calc_hash_for_ptr(addr, BVMD_ROOTMAP_SIZE);

	/* and place it at the head of the list of bvmd_root_t in the bucket */
	dbg_root->next = bvmd_root_map[addrhash];
	bvmd_root_map[addrhash] = dbg_root;
}

#endif
