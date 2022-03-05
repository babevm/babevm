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

/**

 @file

 VM heap management (alloc/free)

 @author Greg McCreath
 @since 0.0.10

 @section ov Overview

 The allocator serves the VM by maintaining the heap.  The heap is a contiguous pool of memory
 taken from the host OS when the VM is initialised.  The size of the heap is static and does not grow or shrink during
 VM execution.

 The allocator provides the ability for the VM to request some dynamic memory from the heap and also to put it back.

 This allocator could be described as a "coalescing best-fit" allocator.  It uses a two-way list of free
 space within the heap to quickly find a memory chunk of a given size using #bvm_heap_alloc().  When memory is given back to
 the allocator using #bvm_heap_free() the memory is coalesced with neighbouring chunks free chunks - if any.

 Each allocated 'chunk' of memory has a 4 byte header associated with it. The minimum chunk size is
 those 4 bytes + 3 pointers sizes - so for 32 bit, it is 16 bytes, and for 64 is 32 - when 8 byte alignment is taken
 into account.  Requesting \c bvm_heap_alloc(0) will still allocate the minimum bytes. The extra 3 pointers are using for
 housekeeping when the chunk is free (we'll get to that) - but are not used when the chunk is in use.  So for (say 32 bit)
 requesting \c bvm_heap_alloc(12) will still consume just those 16 bytes. There is a min size - but it is not all overhead.

 Chunks can be thought of as either 'free' or 'in use'.  Free chunks will have a reference to them in the 'free list'.
 The free list is a two-way linked list of 'free' chunks sorted in size order.  Smallest free chunks at the
 front, largest at the end.  When allocating the list is scanned in smallest-to-largest fashion looking for a
 free chunk that will accommodate the requested size.

 When memory is requested using #bvm_heap_alloc or #bvm_heap_calloc a memory 'allocation type' is
 specified.  The allocator does nothing with the allocation type except place it into the chunk
 header.  The allocation type is used by the garbage collector to determine how to traverse the chunk memory looking for
 other memory references.

 Each requested chunk of memory is marked a #BVM_GC_COLOUR_WHITE as the start point for the garbage collector.  More
 documentation is in collector.c.

 Some important things to note:

 * The heap is static.  Once a chunk has been allocated it does not move when it is in use.  When it
   is freed it may remain where it is, or if it is coalesced into a larger free chunk it will be
   merged into that larger chunk.  But it will be remain in the same location.
 * The heap is central to the VM.  All dynamic memory usage is controlled by the heap.  There are only a few places where
   the VM asks the host OS for memory  - this is the main one - to establish the heap managed
   by this allocator.
 * Some sanity checking is undertaken when freeing memory.  A freed chunk cannot be re-freed for example.
   A chunk whose address is beyond the bounds of the heap is an obvious error.  A few more also.

 Although free chunks and in-use chunks both consume same bytes their internal structure is different
 depending on their free/in-use state, that is to say, when a chunk is free, its structure is used different to when in use.
 However, all chunks have a 4 byte (32 bit) header with the following structure :

 @li bits 01-24 : 24 bit - length (in bytes) of the chunk.  <i>Yes, largest chunk (therefore largest heap) is 16meg.</i>
 @li bits 25-28 : 4 bit - allocation 'type' of the memory, used by the garbage collector
 @li bits 29-30 C : 2 bit - used by garbage collector to specific a GC 'colour' for the chunk
 @li bit P   31 : 'prev-free' bit.  \c 1 if previous chunk in the heap is free (used to coalesce), \c 0 if not
 @li bit U   32 : 'in-use' bit. \c 1 if chunk is in-use, \c 0 if free.

 Like this:

 @verbatim
  chunk->+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         | Size of this chunk (24 bits)                  | type  |  C|P|U|
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 @endverbatim

 After that header - for an in-use chunk - is the user-data, or for a free chunk is free-list
 housekeeping.

 An in-use chunk has its U bit set to \c 1 and looks like this:

 @verbatim
  chunk->+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         | Size of this chunk (24 bits)                  | type  |  C|P|1|
   data->+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         |                                                               |
         +-                                                             -+
         :                whatever bytes have been requested             :
         :                                                               :
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 @endverbatim

Free chunks have their U bit as \c 0 .  Free chunks are placed into a free list
by setting prev/next pointers within the bodies of the chunks to form the
list.  The last \c sizeof(bvm_chunk_t*) bytes of a free chunk are a pointer to
the beginning of itself.  This 'back-pointer' is used when freeing chunks to see
 if the previous located chunk in the heap is also free - if so, it can be coalesced.

As below:

 @verbatim
  chunk->+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         | Size of this chunk (24 bits)                  | type  |  C|P|0|
   data->+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         | Pointer to previous chunk in free list                        |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         | Pointer to next chunk in free list                            |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         :                                                               :
         :                  unused  bytes             					 :
         :                                                               :
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         | Pointer to beginning of this chunk                            |
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 @endverbatim

You can see now why the minimum chunk size is header + 3 pointers. :-) yes, 'unused bytes' may be of zero length.

When a chunk is freed its U bit is unset, and then if the chunk's P bit is set it removes the previous
chunk from the free list and joins with it to form a larger chunk.  Also, if a chunk's P bit is set it can look at
the previous bytes (just before its own address) to get a pointer to the beginning of the previous chunk.

So a chunk whose previous chunk is free looks a bit like this:

 @verbatim
         :                                                               :
         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         | Pointer to beginning of previous chunk                        |
  chunk->+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         | Size of this chunk (24 bits)                  | type  |  C|1|U|
   data->+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         :                whatever bytes have been requested             :
         :                                                               :
 @endverbatim

When freeing a chunk, if the next chunk is also free (no special bit is required, just use the size of the current being-freed chunk to
calculate where the next one starts), remove it from the free list and join with it to create a new larger free chunk.

Finally, set the P bit of the next chunk (to mark that its previous chunk is free), insert the (now possibly coalesced and larger) free chunk
into appropriate place in the free list, and set its last bytes to point to the beginning of itself.

When a chunk is to be allocated, the free list is scanned looking for the first free chunk that is equal to or larger than the
requested size + overhead.  It is removed from the free list.  If the found free chunk is larger than required and the excess can be split off
into another valid chunk then it is split and the remainder put back into the free list. This simple algorithm means we only take what is needed
from a free chunk and give the rest back to the allocator.

Exhaustion:

If memory is requested and none can be granted for the requested size, a GC is performed and then
the allocation re-attempted.  A second failure will cause an out-of-memory situation (and an out of memory exception to be thrown).

Other Notes:

The free list uses known markers at its start and end.  The start points backwards to \c NULL and the end
points forwards to \c NULL.  At the very beginning, start points forward to the end and vice versa.  Free list traversal is thus
bounded by these start and finish markers.

This is a fairly vanilla implementation and as-of-yet, no major optimisation has been but in.  But, some thoughts might be:
 - 'free size buckets' to lessen the free chunks scanned
 - 'good enough' size when allocating - like if the first free chuck found is (say) within 30% of the size requested then do
 not split the remaining bytes off to go back into the list, just use the whole lot chunk.
 - keep some kind of measure on fragmentation and only coalesce if things go beyond a certain limit.
 - only coalesce for chunks beyond a certain size, on the basis that VMs operate on many small allocations, just make
 those allocations / de-allocations easier.

*/

#include "../h/bvm.h"

/** A pointer to the first byte of the heap */
bvm_uint8_t *bvm_gl_heap_start;

/** A pointer to one byte past the last byte of the heap */
bvm_uint8_t *bvm_gl_heap_end;

/** The total memory (in bytes) in the free list */
bvm_uint32_t bvm_gl_heap_free;

/**
 * The total heap size in bytes.  Defaults to #BVM_HEAP_SIZE.
 */
bvm_uint32_t bvm_gl_heap_size = BVM_HEAP_SIZE;

/** Internal 'known' #bvm_chunk_t to mark the start of the free list.  Has a size of zero, points backwards
 * to \c NULL, and forwards to the free list.  If the list is empty, this chunk will point forward to the
 * #free_list_end marker chunk. */
static bvm_chunk_t free_list_start;

/** Internal 'known' #bvm_chunk_t to mark the end of the free list. Has a size equal to the total size of the heap.
 * Points forward to \c NULL and backwards to the last chunk in the free list.  In an empty list this will
 * point backwards to the #free_list_start marker chunk. */
static bvm_chunk_t free_list_end;

/**
 * For GC, To determine if a pointer to a chunk really points to a chunk.  A number of tests are
 * performed on the chunk - like checking its size and making sure it is within the heap
 * address range and so on.
 *
 * @param chunk - the chunk to check
 * @return bvm_bool_t - #BVM_TRUE is the chunk is a true chunk, or #BVM_FALSE otherwise
 *
 */
bvm_bool_t bvm_heap_is_chunk_valid(bvm_chunk_t *chunk) {

	bvm_uint32_t size;
	int type;

	if (chunk == NULL)
		return BVM_FALSE; /* cannot be NULL */

	if  ( (chunk > (bvm_chunk_t *) (bvm_gl_heap_end - BVM_CHUNK_MIN_SIZE)) ||  /* address cannot be greater than end of heap */
          (chunk < (bvm_chunk_t *) bvm_gl_heap_start) )					   /* address cannot be less than the start of heap */
		return BVM_FALSE;

	type = BVM_CHUNK_GetType(chunk);

	if ( (type < BVM_ALLOC_MIN_TYPE) ||				/* must be a valid alloc type */
	     (type > BVM_ALLOC_MAX_TYPE) )				/* must be a valid alloc type */
		return BVM_FALSE;

	size = BVM_CHUNK_GetSize(chunk);

	if ( (size < BVM_CHUNK_MIN_SIZE) ||				/* size must be greater than the minimum size */
	     (size > bvm_gl_heap_size)  ||				/* size cannot exceed the heap size */
	     (size != ( (size + BVM_CHUNK_ALIGN_MASK) & ~BVM_CHUNK_ALIGN_MASK) ) ) /* must be validly aligned using same algorithm as alloc */
		return BVM_FALSE;

	if (BVM_CHUNK_GetNextChunk(chunk) > (bvm_chunk_t *) bvm_gl_heap_end)
		return BVM_FALSE; /* chunk end cannot go past end of heap */

	if (BVM_CHUNK_GetColour(chunk) > BVM_GC_COLOUR_BLACK)
		return BVM_FALSE;

	return BVM_TRUE;
}

/**
 * Free a chunk and place it back into the free list.  In doing so, if the chunk's P bit is set the
 * previous chunk is removed from the free list and coalesced with.  Also, if the next chunk is free, it is also
 * removed from the free list and coalesced.
 *
 * This function is not intended for general developer use - just by the GC.  Developers should use #bvm_heap_free on
 * pointers returned from #bvm_heap_alloc or #bvm_heap_calloc.
 *
 * No correctness checking is performed to ensure the chunk is a correct chunk or is not presently in use.
 *
 * @param chunk - an in-use chunk to return to the free list.
 * @return the chunk that was free - this may be different to the requested chunk if the requested chunk
 * was coalesced.
 */
bvm_chunk_t *bvm_heap_free_chunk(bvm_chunk_t *chunk) {

	bvm_chunk_t *iter;
	bvm_chunk_t *nextchunk;
	bvm_chunk_t *startchunkptr;
	/* the size of the chunk being added to the free list */
	bvm_uint32_t size = BVM_CHUNK_GetSize(chunk);

#if BVM_DEBUG_HEAP_ZERO_ON_FREE
	void *ptr = BVM_CHUNK_GetUserData(chunk);
	if (!bvm_heap_is_chunk_valid(chunk)) {
		BVM_VM_EXIT(1000, NULL);
	}
	memset(ptr, 0, size-BVM_CHUNK_OVERHEAD);
#endif

	/* we're increasing free space, so we'll note it in a totaller */
	bvm_gl_heap_free += size;

	/* flag it as being free  */
	chunk->header &= ~BVM_CHUNK_INUSE_MASK;

	/* Test if the previous adjacent chunk in the heap (not the free list) is free, if so we'll coalesce with it.
	 * We'll unlink that free chunk from the free list */
	if (BVM_CHUNK_IsPrevChunkFree(chunk) && (chunk != (bvm_chunk_t *) bvm_gl_heap_start) ) {

		/* the last bytes of a free chunk hold the address of the start of that same free chunk.  We'll
		 * get that address and point 'chunk' to it - this moves the start of our being-freed chunk to
		 * point to the beginning of the (free) previous chunk - in effect, the being-freed chunk now
		 * starts further back and encompasses both chunks */
		chunk = (bvm_chunk_t *) ( ((bvm_uint8_t *) chunk) - sizeof(bvm_chunk_t *));
		chunk = *(bvm_chunk_t **) chunk;

		/* unlink the unused previous chunk from the free list */
		chunk->prev_free_chunk->next_free_chunk = chunk->next_free_chunk;
		chunk->next_free_chunk->prev_free_chunk = chunk->prev_free_chunk;

		/* keep track of the total size of the new bigger (coalesced) chunk */
		size += BVM_CHUNK_GetSize(chunk);

		/* put the size into the header - this overwrites any previous contents (meaning ...  the header will only have
		 * the size in it after this operation */
		chunk->header = (size << BVM_CHUNK_SIZE_SHIFT);
	}

	/* 'chunk' will now point to the start of the chunk to free up. This could be
	 * the chunk as passed in as a parameter, or could be the previous chunk if we coalesced with it (above). */

	/* Now test the next chunk to see if it is also unused.  If it is, unlink it from the free
	 * list and increase the size of our chunk-to-free to encompass it. */

	nextchunk = BVM_CHUNK_GetNextChunk(chunk);

	if (nextchunk != (bvm_chunk_t *) bvm_gl_heap_end) {

		if (!BVM_CHUNK_IsInuse(nextchunk)) {

			/*debug_free_list_count--;*/

			/* unlink the unused next chunk from the free list */
			nextchunk->prev_free_chunk->next_free_chunk = nextchunk->next_free_chunk;
			nextchunk->next_free_chunk->prev_free_chunk = nextchunk->prev_free_chunk;

			/* keep track of the total size of our new bigger coalesced chunk */
			size += BVM_CHUNK_GetSize(nextchunk);

			/* put the size into the header - this overwrites any previous contents (meaning ... the header will only have
			 * the size in it after this operation */
			chunk->header = (size << BVM_CHUNK_SIZE_SHIFT);
		}

	}

	/* At this point we will have coalesced the prev and next chunks in the heap if they were
	 * free.  We have removed those chunks from the free list.  We are at a point where we
	 * can now try to insert the new (maybe larger) chunk into the free list */

	/* From start of the free list, loop to find a chunk whose *next* chunk is larger than or equal to
	 * the being-freed chunk size - we are looking for a place to insert it ...*/
	for (iter = &free_list_start; BVM_CHUNK_GetSize(iter->next_free_chunk)
			< size; iter = iter->next_free_chunk) {}

	/* Update the free list by updating next/prev chunk pointers and inserting the being-freed chunk. */
	chunk->prev_free_chunk = iter;
	chunk->next_free_chunk = iter->next_free_chunk;

	iter->next_free_chunk->prev_free_chunk = chunk;
	iter->next_free_chunk = chunk;

	/* Recalculate the next chunk (in case the next chunk was in fact coalesced with this one) */
	nextchunk = BVM_CHUNK_GetNextChunk(chunk);

	/* if the next chunk is not past the end of the heap (that is, we are not sitting on the last chunk)
	 * flag it as having its previous chunk being free */
	if (nextchunk != (bvm_chunk_t *) bvm_gl_heap_end) nextchunk->header |= BVM_CHUNK_PREV_FREE_MASK;

	/* and finally ... set last bytes of this being-freed chunk to point to the beginning of itself. Yes, this is
	 * a very difficult couple of lines to understand.  It casts the bytes previous to the next chunk as a
	 chunk pointer and sets its value - I hate it, but it seems to be the way it is done. */
	startchunkptr = (bvm_chunk_t *) ( ((bvm_uint8_t *) nextchunk) - sizeof(bvm_chunk_t *));
	*(bvm_chunk_t **) startchunkptr = chunk;

	/* return the handle to the chunk ... it may have moved to the prev chunk if coalesced */
	return chunk;
}

/**
 * Initialise the VM heap to \c size by requesting the memory from the OS using #bvm_pd_memory_alloc.  The size
 * will be rounded to a multiple of the #BVM_CHUNK_ALIGN_SIZE setting and must be between #BVM_HEAP_MIN_SIZE and
 * #BVM_HEAP_MAX_SIZE.
 *
 * The free list is established with the list markers #free_list_start and #free_list_end as delimiters. The
 * new heap space will become the first available chunk on the free list. Yes, the new heap memory is
 * added as a free chunk to the free list.  All subsequence allocations will subdivide this large free list chunk.
 *
 * @param size - the heap size to allocate from the OS for the VM.
 *
 * @throws #BVM_FATAL_ERR_HEAP_SIZE_LT_THAN_ALLOWABLE_MIN - if the requested heap size is less than the
 * allowable VM lower limit set by #BVM_HEAP_MIN_SIZE.
 *
 * @throws #BVM_FATAL_ERR_HEAP_SIZE_GT_THAN_ALLOWABLE_MAX - if the requested heap size is greater than the
 * allowable VM upper limit set by #BVM_HEAP_MAX_SIZE.
 *
 * @throws #BVM_FATAL_ERR_CANNOT_ALLOCATE_HEAP - if the requested heap size cannot be allocated from the OS.
 *
 */
void bvm_heap_init(size_t size) {

	bvm_chunk_t *heapchunk;

	/* align it */
	bvm_gl_heap_size = (size + BVM_CHUNK_ALIGN_MASK) & ~BVM_CHUNK_ALIGN_MASK;

	/* Less than specified min? Bang out. */
	if (bvm_gl_heap_size < BVM_HEAP_MIN_SIZE) BVM_VM_EXIT(BVM_FATAL_ERR_HEAP_SIZE_LT_THAN_ALLOWABLE_MIN, NULL);

	/* Greater than specified max? Bang out. */
	if (bvm_gl_heap_size > BVM_HEAP_MAX_SIZE) BVM_VM_EXIT(BVM_FATAL_ERR_HEAP_SIZE_GT_THAN_ALLOWABLE_MAX, NULL);

	/* get the heap memory from the platform OS */
	bvm_gl_heap_start = (bvm_uint8_t *) bvm_pd_memory_alloc(bvm_gl_heap_size);

	/* if we can't get any memory do not continue */
	if (bvm_gl_heap_start == NULL) BVM_VM_EXIT(BVM_FATAL_ERR_CANNOT_ALLOCATE_HEAP, NULL);

	/* set a pointer to the byte just past the end of the heap.  We'll use this to make sure that
	 * last chunk in the heap when allocated/freed does not set the previous used/free marker
	 * to something past the end of the heap. */
	bvm_gl_heap_end = bvm_gl_heap_start + bvm_gl_heap_size;

	/* init the free space to zero */
	bvm_gl_heap_free = 0;

	/* configure the heap as a large chunk and we'll place it as the first entry on the
	 * free list */
	heapchunk = (bvm_chunk_t *) bvm_gl_heap_start;
	heapchunk->header = (bvm_gl_heap_size << BVM_CHUNK_SIZE_SHIFT);

	/* end of free list points to nothing and has its size set to the size of the heap.  We
	 * can use both these markers when traversing the free list */
	free_list_end.header = (bvm_gl_heap_size << BVM_CHUNK_SIZE_SHIFT);
	free_list_end.next_free_chunk = NULL;
	free_list_end.prev_free_chunk = &free_list_start;

	free_list_start.header = 0;
	free_list_start.next_free_chunk = &free_list_end;
	free_list_start.prev_free_chunk = NULL;

	/* and now place it in the free list */
	bvm_heap_free_chunk(heapchunk);
}

/**
 * Find a free memory chunk that will accommodate the given size.  No idiocy checking is performed on the
 * size of the request.  The free list is scanned for the first accommodating chunk size.
 *
 * The found chunk is removed from the free list.  If the found chunk size is >=
 * to (requested size + #BVM_CHUNK_MIN_SIZE), the excess is trimmed off and inserted back
 * into the free list as a distinct new free chunk.
 *
 * @param size - the exact size of the memory to search for.
 * @return a bvm_chunk_t* or \c NULL if no accommodating chunk could be taken from
 *              (or split out of) the free list.
 */
static bvm_chunk_t *heap_get_chunk(size_t size) {

	bvm_chunk_t *chunk, *prev_chunk, *next_chunk;
	bvm_chunk_t *rv = NULL;

	/* traverse free list for the first chunk that is equal to or larger than than
	 * the requested size */
	prev_chunk = &free_list_start;
	chunk = free_list_start.next_free_chunk;

	while ( ( BVM_CHUNK_GetSize(chunk) < size ) && (chunk->next_free_chunk != NULL )) {
		prev_chunk = chunk;
		chunk = chunk->next_free_chunk;
	}

	/* if we have not reached the end of the list ... it means we found one */
	if (chunk != &free_list_end) {

		/* set the return value as the chunk we have found */
		rv = chunk;

		/* Remove the found chunk from the free list */
		chunk->next_free_chunk->prev_free_chunk = prev_chunk;
		prev_chunk->next_free_chunk = chunk->next_free_chunk;

		/* we've removed a chunk from the free list, so we'll note the memory usage in our
		 * free-memory totaller */
		bvm_gl_heap_free -= BVM_CHUNK_GetSize(chunk);

		/* if the chunk is larger than the requested size + BVM_CHUNK_MIN_SIZE, we'll split it in two and
		 * add the remainder back to the free list*/
		if (BVM_CHUNK_GetSize(chunk) >= (size + BVM_CHUNK_MIN_SIZE)) {

			bvm_chunk_t *newchunk;

			/* get a pointer to the beginning of the excess bytes and treat it as a 'remainder' chunk */
			newchunk = (bvm_chunk_t *) (BVM_CHUNK_AsBytePtr(chunk) + size);

			/* set the size of the 'remainder' chunk */
			newchunk->header = ( (BVM_CHUNK_GetSize(chunk) - size) << BVM_CHUNK_SIZE_SHIFT);

			/* truncate the size of the requested chunk */
			chunk->header = (size << BVM_CHUNK_SIZE_SHIFT);

			/* put the new 'remainder' chunk into the free list */
			bvm_heap_free_chunk(newchunk);
		}

		/* set the in-use flag of our found chunk */
		chunk->header |= BVM_CHUNK_INUSE_MASK;

		/* give it a GC colour of white */
		BVM_CHUNK_SetColour(chunk, BVM_GC_COLOUR_WHITE);

		/* if the next chunk is not past the end of the heap (that is, we are not presently on the last heap chunk)
		 * clear its previous chunk free flag. */
		next_chunk = BVM_CHUNK_GetNextChunk(chunk);
		if (next_chunk != (bvm_chunk_t *) bvm_gl_heap_end) {
			next_chunk->header &= ~BVM_CHUNK_PREV_FREE_MASK;
		}
	}

	return rv;
}

/**
 * Request the allocator to provide memory of the given size.
 *
 * The size requested is massaged to be a correct multiple of #BVM_CHUNK_ALIGN_SIZE.
 * Requesting 0 bytes will successfully return a void*.  However, there is overhead and
 * minimum allocation size is defined by #BVM_CHUNK_MIN_SIZE.  Continually requesting zero
 * bytes will exhaust memory.
 *
 * Unlike other allocators may do, this one will not return \c NULL if no memory can be allocated.
 * If memory cannot be allocated a GC will be invoked and then allocation will be
 * re-attempted.  A second allocation failure will throw an \c OutOfMemoryException if the VM is initialised.
 * If the VM has not yet been initialised the VM will exit with #BVM_FATAL_ERR_OUT_OF_MEMORY.
 *
 * @param size - the size in bytes to get
 * @param alloc_type - the bvm_gc allocation type of the memory
 * @return void* - a pointer to the allocated memory.
 * @throw OutOfMemoryException if memory cannot be allocated.
 */
void *bvm_heap_alloc(size_t size, int alloc_type) {

	bvm_chunk_t *chunk = NULL;
	size_t real_size;

	/* if BVM_DEBUG_HEAP_GC_ON_ALLOC is defined we'll do a GC before, erm, each
	 * allocation.  This will help weed out the circumstances where a temporary
	 * root has been GC'd when it should not have been. */
#if BVM_DEBUG_HEAP_GC_ON_ALLOC
	bvm_gc();
#endif

	/* add the size of the chunk overhead onto the requested size and make
	 * sure it is aligned correctly - note we're adding the align mask (which is always one less than
	 * the BVM_CHUNK_MIN_SIZE) - this rounds up.  For example, if 7 bytes are requested and we're 4 byte
	 * aligning, then ((7+3) & ~3) is 8. But, requesting 8 bytes will give 8. */
	real_size = (size + BVM_CHUNK_OVERHEAD + BVM_CHUNK_ALIGN_MASK) & ~BVM_CHUNK_ALIGN_MASK;

	/* cannot allocate a chunk smaller than the BVM_CHUNK_MIN_SIZE - so round up */
	if (real_size < BVM_CHUNK_MIN_SIZE)
		real_size = BVM_CHUNK_MIN_SIZE;

	/* find a chunk that will fit the requested size.*/
	chunk = heap_get_chunk(real_size);

	/* whoops.  Can't find any at all. */
	if (chunk == NULL) {

		/* do a GC  */
		bvm_gc();

		/* .. and try again to get the memory */
		chunk = heap_get_chunk(real_size);
	}

	/* still can't get it?  Memory must be exhausted (or fragmented in a bad way).
	 * Throw a pre-cooked "out of memory" exception.  Note that it is
	 * pre-cooked so we do not have to perform an alloc when memory is
	 * exhausted.*/
	if (chunk == NULL) {

		/* bvm_gl_out_of_memory_err_obj will be NULL here if we are allocating memory *before* the VM has had
		 * the chance to initialise the standard bootstrap objects.  So, if we run into memory
		 * trouble before the VM is initialised, we exit the VM in a horrid way */
		if (bvm_gl_vm_is_initialised) {
			BVM_THROW(bvm_gl_out_of_memory_err_obj)
		}
		else
			BVM_VM_EXIT(BVM_FATAL_ERR_OUT_OF_MEMORY, NULL);

	} else {
		/* set the allocation type of the chunk.  */
		BVM_CHUNK_SetAllocType(chunk, alloc_type);
	}

	/* Return a pointer to the user data part of the chunk, not the start of the chunk.*/
	return ((bvm_inuse_chunk_t *) chunk)->user_data;
}

/**
 * Provide memory to the VM that is initialised to 0.
 *
 * @param size - the size in bytes of the memory to allocate
 * @param alloc_type - the bvm_gc allocation type of the memory
 */
void *bvm_heap_calloc(size_t size, int alloc_type) {
	void *ptr = bvm_heap_alloc(size, alloc_type);
	memset(ptr, 0, size);
	return ptr;
}

/**
 * Make an exact copy of memory allocated by the allocator.  New memory is allocated and the
 * contents of the \c ptr argument are copied into it.  The \c ptr argument must be a pointer to
 * heap memory that was gained using #bvm_heap_alloc or #bvm_heap_calloc.
 *
 * @param ptr pointer to the memory to clone.
 *
 * @return cloned memory allocated from the heap.
 */
void *bvm_heap_clone(void *ptr) {

	void *new_ptr;
	size_t size;

	/* chunk will begin at the start of the ptr less the chunk header size */
	bvm_chunk_t *chunk = BVM_CHUNK_GetPointerChunk(ptr);

#if BVM_DEBUG_HEAP_CHECK_CHUNKS
	/* Do some correctness checking just to make sure (as much as we can) that the thing being
	 * cloned is a valid address as supplied by bvm_heap_alloc.  We'll consider issues here
	 * as VM fatal. */
	if (!bvm_heap_is_chunk_valid(chunk) || (!BVM_CHUNK_IsInuse(chunk)))
		BVM_VM_EXIT(BVM_FATAL_ERR_INVALID_MEMORY_CHUNK, NULL);
#endif

	size = BVM_CHUNK_GetSize(chunk);

	new_ptr = bvm_heap_alloc(size, BVM_CHUNK_GetType(chunk));
	memcpy(new_ptr, ptr, size);
	return new_ptr;
}

/**
 * Change the alloc type of a given pointer.  The pointer must have been a heap pointer returned
 * from #bvm_heap_alloc or #bvm_heap_calloc.
 *
 * @param ptr the pointer to change the alloc type of.
 * @param alloc_type the new alloc type.
 */
void bvm_heap_set_alloc_type(void *ptr, int alloc_type) {

	bvm_chunk_t *chunk = BVM_CHUNK_GetPointerChunk(ptr);

    BVM_CHUNK_SetAllocType(chunk, alloc_type);
}

/**
 * To release memory allocated by #bvm_heap_alloc or #bvm_heap_calloc back to the heap.
 *
 * @param ptr the pointer to heap memory to free.
 */
void bvm_heap_free(void *ptr) {

	/* chunk will begin at the start of the ptr less the chunk header size */
	bvm_chunk_t *chunk = BVM_CHUNK_GetPointerChunk(ptr);

#if BVM_DEBUG_HEAP_CHECK_CHUNKS
	/* Do some correctness checking just to make sure (as much as we can) that the thing being
	 * freed is a valid address as supplied by bvm_heap_alloc.  We'll consider issues here
	 * as VM fatal. */
	if (!bvm_heap_is_chunk_valid(chunk) || (!BVM_CHUNK_IsInuse(chunk)))
		BVM_VM_EXIT(BVM_FATAL_ERR_INVALID_MEMORY_CHUNK, NULL);
#endif

	bvm_heap_free_chunk(chunk);
}

/**
 * Pass the entire VM heap back to the operating system.
 *
 * Called upon exit from the VM.
 */
void bvm_heap_release() {
	bvm_pd_memory_free(bvm_gl_heap_start);
}

/**
 * Debug function to dump a table of the heap contents to the console.
 */
void bvm_heap_debug_dump() {

	bvm_chunk_t *chunk = (bvm_chunk_t *) bvm_gl_heap_start;

    bvm_pd_console_out("heap free: %u\n", bvm_gl_heap_free);

	while (chunk != (bvm_chunk_t *) bvm_gl_heap_end) {

		bvm_uint32_t size = BVM_CHUNK_GetSize(chunk);
		int colour = BVM_CHUNK_GetColour(chunk);
		int inuse = BVM_CHUNK_IsInuse(chunk);
		int prevfree = BVM_CHUNK_IsPrevChunkFree(chunk);
        int type = BVM_CHUNK_GetType(chunk);

        unsigned long memoffset = (bvm_uint8_t *) chunk - bvm_gl_heap_start;

		bvm_pd_console_out("addr: %p \t offset: %u \t size: %u \t type: %u \t inuse :%u \t  prev: %u  \t  colour : %u \n", (void *) chunk, memoffset, (int) size, type, inuse, prevfree, colour);

		chunk = BVM_CHUNK_GetNextChunk(chunk);
	}
	bvm_pd_console_out("**************\n");
}

void printFreeChunk(bvm_chunk_t *chunk) {

    bvm_chunk_t *next_free;
    bvm_chunk_t *prev_free;

    bvm_uint32_t size = BVM_CHUNK_GetSize(chunk);
    next_free = chunk->next_free_chunk;
    prev_free = chunk->prev_free_chunk;

    long memoffset = (bvm_uint8_t *) chunk - bvm_gl_heap_start;

    bvm_pd_console_out("addr: %p \t offset: %u \t size: %u \t prev :%p \t  next: %p \n", (void *) chunk, memoffset, (int) size, (void*) prev_free, (void*) next_free);
}

/**
 * Debug function to dump the free list to the console
 */
void bvm_heap_debug_dump_free_list() {

    bvm_chunk_t *chunk = free_list_start.next_free_chunk;

    bvm_pd_console_out("free list\n");

    printFreeChunk(&free_list_start);

    int i = 0;

    while (chunk != &free_list_end && chunk != NULL) {
        printFreeChunk(chunk);
        chunk = chunk->next_free_chunk;
        i++;
    }

    printFreeChunk(&free_list_end);

	bvm_pd_console_out("**************\n");
}
