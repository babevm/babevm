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

  JDWP Object and address ID management.

  When an object or clazz reference is sent from the VM to the debugger in a reply (as an ID) this
  is where the ID/address cross reference is maintained.

  For speed of lookup either by address, or by ID, two arrays of entries are actually maintained, although
  they actually share the same entries.  An entry therefore is actually always participating in
  two linked lists - one for the ID array, and one for the address array.  Entries are always
  added and removed from both.

  The garbage collector removes addresses from here as it returns memory to the heap - hence the
  lookup by address.

  This addr/ID mapping is only actually valid when a debug session is active.  The session startup creates
  the map, and the session shutdown frees it up.

  @author Greg McCreath
  @since 0.0.10

*/

#if BVM_DEBUGGER_ENABLE

/**
 * Number of buckets in each array.
 */
#define DBG_IDMAP_SIZE 250

/**
 * Struct to hold ID/address pairs in the each array.
 */
typedef struct _dbgidstruct {
	bvm_int32_t id;
	void *addr;
	struct _dbgidstruct *next_id;
	struct _dbgidstruct *next_addr;
} dbg_id_t;

/**
 * An array of #dbg_id_t pointers for use as the ID map.
 */
dbg_id_t **dbg_id_map;

/**
 * An array of #dbg_id_t pointers for use as the address map.
 */
dbg_id_t **dbg_addr_map;

/**
 * Calculates a hash of the address to find the right bucket.
 */
bvm_uint32_t calc_hash_for_ptr(void* addr, int bucket_count) {
    return (bvm_uint32_t) (*(bvm_native_ulong_t *) addr) % (bucket_count-1);
}

static bvm_uint32_t calc_hash(bvm_uint32_t x) {
    return ( ( (bvm_uint32_t) x) % (DBG_IDMAP_SIZE-1));
}

/**
 * Retrieves the ID of a given address, or zero if the address is not registered.
 *
 * @param addr a given address
 *
 * @return the ID of the address, or zero if not registered.
 */
bvm_int32_t bvmd_id_get_by_address(void *addr) {

    bvm_uint32_t hash = calc_hash_for_ptr(addr, DBG_IDMAP_SIZE);

	dbg_id_t *dbg_id = dbg_addr_map[hash];

	while (dbg_id != NULL) {

		if (dbg_id->addr == addr) {
		    return dbg_id->id;
		}

		dbg_id = dbg_id->next_addr;
	}

	return 0;
}

/**
 * Retrieves the address of a given ID, or \c NULL if the ID is not registered.
 *
 * @param id a given ID
 *
 * @return the address of the ID, or \c NULL if not registered.
 */
void *bvmd_id_get_by_id(bvm_int32_t id) {

	bvm_uint32_t hash = calc_hash(id);

	dbg_id_t *dbg_id = dbg_id_map[hash];

	while (dbg_id != NULL) {

		if (dbg_id->id == id) return dbg_id->addr;

		dbg_id = dbg_id->next_id;
	}

	return NULL;
}

/**
 * Initialises and allocates memory for the ID map.
 */
void bvmd_id_init() {

	size_t size = DBG_IDMAP_SIZE * sizeof(dbg_id_t*);

	dbg_id_map = bvm_heap_calloc(size, BVM_ALLOC_TYPE_STATIC);
	dbg_addr_map = bvm_heap_calloc(size, BVM_ALLOC_TYPE_STATIC);
}

/**
 * Free the memory for the ID map arrays and cleans up.
 */
void bvmd_id_free() {

	bvm_uint32_t i = DBG_IDMAP_SIZE;

	while (i--) {

		/* get the dbg_id at the hash bucket */
		dbg_id_t *dbg_id = dbg_id_map[i];

		/* for each element in the bucket list */
		while (dbg_id != NULL) {
			dbg_id_t *next = dbg_id->next_id;
			bvm_heap_free(dbg_id);
			dbg_id = next;
		}
	}

	/* and free up the maps themselves */
	bvm_heap_free(dbg_id_map);
	bvm_heap_free(dbg_addr_map);

	dbg_id_map = NULL;
	dbg_addr_map = NULL;
}

/**
 * Unregister a given address from the address map.
 *
 * @param addr a given address
 *
 * @return the dbg_id_t entry for the address it is was registered, or \c NULL if
 * it was not.
 */
static dbg_id_t *remove_from_addrmap(void *addr) {

    bvm_uint32_t hash = calc_hash_for_ptr(addr, DBG_IDMAP_SIZE);

	dbg_id_t *dbg_id, *prev_dbg_id;

	prev_dbg_id = NULL;
	dbg_id = dbg_addr_map[hash];

	while (dbg_id != NULL) {

		if (dbg_id->addr == addr) {

			/* if head of list */
			if (prev_dbg_id == NULL) {
				dbg_addr_map[hash] = dbg_id->next_addr;
			} else {
				prev_dbg_id->next_addr = dbg_id->next_addr;
			}

			return dbg_id;

		}

		prev_dbg_id = dbg_id;
		dbg_id = dbg_id->next_addr;
	}

	return NULL;
}

/**
 * Unregister a given ID from the id map.
 *
 * @param id a given ID
 *
 * @return the dbg_id_t entry for the id it is registered, or \c NULL if
 * it was not.
 */
static dbg_id_t *remove_from_idmap(bvm_int32_t id) {

	bvm_uint32_t hash = calc_hash(id);

	dbg_id_t *dbg_id, *prev_dbg_id;

	prev_dbg_id = NULL;
	dbg_id = dbg_id_map[hash];

	while (dbg_id != NULL) {

		if (dbg_id->id == id) {

			/* if head of list */
			if (prev_dbg_id == NULL) {
				dbg_id_map[hash] = dbg_id->next_id;
			} else {
				prev_dbg_id->next_id = dbg_id->next_id;
			}

			return dbg_id;

		}

		prev_dbg_id = dbg_id;
		dbg_id = dbg_id->next_id;
	}

	return NULL;
}

/**
 * Removes a given address (and its ID) from the id map.
 *
 * @param addr a given address
 */
void bvmd_id_remove_addr(void *addr) {
	dbg_id_t *dbg_id = remove_from_addrmap(addr);
	if (dbg_id != NULL) {
		remove_from_idmap(dbg_id->id);
		bvm_heap_free(dbg_id);
	}
}

/**
 * Removes a given ID (and its address) from the ID map.
 *
 * @param id a given ID
 */
void bvmd_id_remove_id(bvm_int32_t id) {
	dbg_id_t *dbg_id = remove_from_idmap(id);
	if (dbg_id != NULL) {
		remove_from_addrmap(dbg_id->addr);
		bvm_heap_free(dbg_id);
	}
}

/**
 * Adds a given #dbg_id_t to both the ID and addr maps.
 *
 * @param dbg_id a given dbg_id_t
 *
 */
static void add_to_maps(dbg_id_t *dbg_id) {

    bvm_uint32_t idhash = calc_hash(dbg_id->id);
    bvm_uint32_t addrhash = calc_hash_for_ptr(dbg_id->addr, DBG_IDMAP_SIZE);

	dbg_id->next_id = dbg_id_map[idhash];
	dbg_id_map[idhash] = dbg_id;

	dbg_id->next_addr = dbg_addr_map[addrhash];
	dbg_addr_map[addrhash] = dbg_id;
}

/**
 * Puts a given address into the ID map and returns its associated ID.  If the address is already
 * registered, its existing ID is returned.
 *
 * @param addr a given address
 *
 * @return the ID of the given address.
 */
bvm_int32_t bvmd_id_put(void *addr) {

	dbg_id_t *dbg_id;
	bvm_int32_t id;

	/* is the address there? */
	id = bvmd_id_get_by_address(addr);

	/* if so, return it */
	if (id != 0) return id;

	/* create a new dbg_id*/
	dbg_id = bvm_heap_calloc(sizeof(dbg_id_t), BVM_ALLOC_TYPE_STATIC);
	dbg_id->id = bvmd_nextid();
	dbg_id->addr = addr;

	/* put the new dbg_id into both the id and addr maps. */
	add_to_maps(dbg_id);

	return dbg_id->id;
}

#endif
