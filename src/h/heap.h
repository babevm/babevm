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

#ifndef BVM_HEAP_H_
#define BVM_HEAP_H_

/**
  @file

  Constants/Macros/Functions/Types for heap management.

  @author Greg McCreath
  @since 0.0.10

*/

/** GC marking colour meaning "not marked" */
#define BVM_GC_COLOUR_WHITE  0
/** GC marking colour meaning "being marking" */
#define BVM_GC_COLOUR_GREY   1
/** GC marking colour meaning "marked" */
#define BVM_GC_COLOUR_BLACK  2

/**
 * A chunk of memory as managed by the allocator.   This structure here represents the structure of a 'free' chunk.  An
 * in-use chunk will not use the next/prev stuff.  It will be the header + user-data that will start at
 * the place where next_free_chunk is defined in this structure.  Note that a free chunk will have the last bytes of
 * itself populated as a pointer back to its beginning.  This is not represented in this structure, but it is put there
 * at runtime.  Hence the minimum size of a memory allocation is sizeof(bvm_chunk_t) + sizeof(pointer).
 */
typedef struct _bvmchunkstruct {

	/**
	 * header is 32 bits:
	 *
     * 24 bits size
	 * 4 bits alloc type
	 * 2 bits GC colour (white/grey/black)
	 * 1 bit prev chunk in-use/free status
	 * 1 bit this chunk in-use/free status
	 */
	bvm_uint32_t header;

	/** If this chunk is free, this points to the next chunk in the free list.  If this chunk is
	 * in-use, this marks the start of the user-data memory. See #bvm_inuse_chunk_t */
	struct _bvmchunkstruct *next_free_chunk;

	/** If this chunk is free, this points to the previous chunk in the free list.  If this chunk is
	 * in-use this field has no meaning. */
	struct _bvmchunkstruct *prev_free_chunk;

	/** note that when this chunk is free the last sizeof(bvm_chunk_t*) bytes will point back to
	 * beginning of this chunk.  Used for helping coalesce adjacent free chunks on the heap
	 * when free memory.  That is the reason why BVM_CHUNK_MIN_SIZE is larger than size of bvm_chunk_t.
	 */
} bvm_chunk_t;

/**
 * Used as a cast for the #bvm_chunk_t structure for convenient use as an in-use chunk
 */
typedef struct _bvminusechunkstruct {
	bvm_uint32_t header;
	bvm_uint8_t user_data[1];
} bvm_inuse_chunk_t;

/** Handle to the start of the heap */
extern bvm_uint8_t *bvm_gl_heap_start;

/** Handle to a byte past the end of the heap */
extern bvm_uint8_t  *bvm_gl_heap_end;

/** Total heap size */
extern bvm_uint32_t bvm_gl_heap_size;

/** Total bytes in the free list */
extern bvm_uint32_t bvm_gl_heap_free;

/** The minimum VM heap size */
#define BVM_HEAP_MIN_SIZE  (256 * BVM_KB)

/** The maximum heap size - limited by the 24 bit space in the header for size (16mb). */
#define BVM_HEAP_MAX_SIZE  0xFFFFFF

/** Chunk header mask for the in-use bit. */
#define BVM_CHUNK_INUSE_MASK  		0x1

/** Chunk header mask for the prev-free bit. */
#define BVM_CHUNK_PREV_FREE_MASK  	0x2

/** Chunk header mask to isolate the alloc type */
#define BVM_CHUNK_TYPE_MASK   		0xF0

/** Chunk header mask to isolate the GC colour */
#define CHUNK_COLOUR_MASK		0xC

/** To determine if a given chunk is in use */
#define BVM_CHUNK_IsInuse(c)       (((c)->header & BVM_CHUNK_INUSE_MASK) > 0)

/** To determine if the previous chunk of a given chunk is free */
#define BVM_CHUNK_IsPrevChunkFree(c)   (((c)->header & BVM_CHUNK_PREV_FREE_MASK) > 0)

/** Determine the size of a given chunk */
#define BVM_CHUNK_GetSize(c)  ( (c)->header >> BVM_CHUNK_SIZE_SHIFT)

/** Determine the alloc type of a given chunk. */
#define BVM_CHUNK_GetType(c)  ( ((c)->header & BVM_CHUNK_TYPE_MASK) >> BVM_CHUNK_TYPE_SHIFT)

/** Cast a given chunk pointer as a byte pointer */
#define BVM_CHUNK_AsBytePtr(c) ((bvm_uint8_t *) (c))

/** Get the the next chunk in the heap relative to a given chunk. */
#define BVM_CHUNK_GetNextChunk(c)   (bvm_chunk_t *) (BVM_CHUNK_AsBytePtr(c) + BVM_CHUNK_GetSize(c))

/** Get the GC colour of a given chunk */
#define BVM_CHUNK_GetColour(c)  (((c)->header & CHUNK_COLOUR_MASK) >> BVM_CHUNK_COLOUR_SHIFT)

/** Set the GC colour of a given chunk */
#define BVM_CHUNK_SetColour(c, k)  \
	{(c)->header = ((c)->header & ~CHUNK_COLOUR_MASK) | ((k) << BVM_CHUNK_COLOUR_SHIFT);}

/** Set the GC alloc type of a given chunk */
#define BVM_CHUNK_SetAllocType(c, t)  \
	{(c)->header = ((c)->header & ~BVM_CHUNK_TYPE_MASK) | ((t) << BVM_CHUNK_TYPE_SHIFT);}

/** Gets the pointer to the #bvm_chunk_t associated with a user data pointer.  No correctness checking is
 * performed - this will return a pointer to the position where the chunk will start if it is a
 * valid chunk. */
#define BVM_CHUNK_GetPointerChunk(p) ( (bvm_chunk_t *) ( (( bvm_uint8_t *) (p)) - BVM_CHUNK_OVERHEAD) )

/** Gets a (void *) pointer to the user data portion of a given chunk */
#define BVM_CHUNK_GetUserData(c)     (void *) (((bvm_inuse_chunk_t *) (c))->user_data)

/** The size of the header of a chunk */
#define BVM_CHUNK_OVERHEAD      sizeof(bvm_uint32_t)

/** The minimum allowable size for a chunk - space for header, next/prev pointers and last bytes used as
 * prev heap chunk pointer when not in use */
#define BVM_CHUNK_MIN_SIZE     sizeof(bvm_chunk_t)+sizeof(bvm_chunk_t*)

/** Memory alignment size */
#define BVM_CHUNK_ALIGN_SIZE 	sizeof(size_t)

/** MUST be one less than #BVM_CHUNK_ALIGN_SIZE */
#define BVM_CHUNK_ALIGN_MASK 	(BVM_CHUNK_ALIGN_SIZE-1)

/** How many bits we have to shift the header to access the chunk size. */
#define BVM_CHUNK_SIZE_SHIFT 	8

/** How many bits we have to shift the header to determine the GC alloc type. */
#define BVM_CHUNK_TYPE_SHIFT 	4

/** How many bits required to shift the header for the GC colour */
#define BVM_CHUNK_COLOUR_SHIFT  2

void bvm_heap_init(size_t size);
void bvm_heap_release();
void *bvm_heap_alloc(size_t size, int alloc_type);
void *bvm_heap_calloc(size_t size, int alloc_type);
void bvm_heap_free(void *ptr);
bvm_chunk_t *bvm_heap_free_chunk(bvm_chunk_t *chunk);
void *bvm_heap_clone(void *ptr);
void bvm_heap_set_alloc_type(void *ptr, int alloc_type);

void bvm_heap_debug_dump_free_list();
void bvm_heap_debug_dump();
bvm_bool_t bvm_heap_is_chunk_valid(bvm_chunk_t *chunk);

#endif /*BVM_HEAP_H_*/
