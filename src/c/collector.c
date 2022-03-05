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

 The Garbage Collector.

 @section ov Overview

 The collector is implemented as a mark-and-sweep collector.  Tri-colour marking is used to keep
 the door open for a later implementation of incremental GC.

 Nothing controversial about the algorithm in use.  The first stage recursively marks each known root heap chunk
 and its children and then the heap is swept to free those chunks that were not marked.

 The allocator marks each chuck #BVM_GC_COLOUR_WHITE as it created.  During GC, as each memory structure is
 scanned it is marked #BVM_GC_COLOUR_GREY to note it is in-progress, then finally it is marked #BVM_GC_COLOUR_BLACK.

 After all scanning, the heap is traversed and anything still marked as #BVM_GC_COLOUR_WHITE that is not in the free list is freed.
 Everything else gets remarked as #BVM_GC_COLOUR_WHITE.

 This collector will also collect unused classes.

 The collector knows how to scan a chunk for children by using the "allocation type" that the chunk was created with.
 Alloc types define the structure and usage of each chunk.  Alloc types are used for no other purpose than to help
 the collector scan a chunk when marking it - to identify any other heap references it may contain.

 Alloc types are one of only a few touch points the collector has with the allocator.  Others are the
 colours used when marking chunks during collection, and the usage of the #bvm_heap_free() function.  Of course, the
 allocator will call the #bvm_gc() function when memory is exhausted.

 The collector and the allocator are designed to be quite separate.  The allocator supports 2 bits to specify
 GC colours and 4 bits for the allocation type.  The usage of these colours and allocation types is up to
 the implementation of the collector.

 @section roots Stack, Permanent and Transient Roots

 The set of known roots is established from three sources. The Thread Stacks, and the Permanent and
 Transient stacks.

 The thread stacks are marked by traversing and inspecting the locals and opstack area for each frame in the
 stack.  Each cell in that frame locals/opstack range is tested to see if it is a valid pointer into the heap, and
 if so it is considered a root and is recursively marked.  Stack data is reused so it could have junk in it from
 a previous usage - hence the 'test to see if it is a valid pointer'.

 The other two root sources, the transient and permanent root stack, are slightly different.  Each is also a stack, albeit
 far simpler than a thread stack.  Each of these stacks has an associated counter to mark the current top of the stack.

 @subsection proots Permanent Roots

 For permanent heap roots. Quite simply, when the VM consumes some heap for that it knows will never be freed it pushes it
 onto the permanent stack using the #BVM_MAKE_PERMANENT_ROOT macro.  For example, as bootstrap classes are loaded they are
 pushed onto the permanent stack.  Same for a few objects such as the pre-built "out of memory" exception object.  The
 permanent root stack never shrinks.   The purists of you out there would notice the lack of a 'pop' facility and
 probably argue it is not actually a stack -  but go fly a kite.

 The collector considers all entries in the permanent stack below the top as roots.  They will be marked
 and scanned and will not be freed.

 @subsection troots Transient Roots

 Keeping track of transient roots is more complicated.  Transient roots exist only during a 'transient' block of
 execution.  Transient roots are the answer to the rather complicated question of using heap memory and
 not having the GC clean it up before it is associated with a root or the VM has
 finished using it.  The danger occurs when the VM allocs some memory, then allocs
 some more before being finished with the first.  For example, when creating a String the memory for the
 string object is alloc'd, then moments later the memory for its char[] object is also alloc'd).

 Any alloc may potentially cause a GC.  In this case, without some means of keeping the string object memory from
 being GC'd, the char[] allocation may cause the string memory to be freed.  After all, the memory is not
 associated with any root as yet - the GC occurs in 'mid-stoke'. The VM needs some way of stopping the
 collector from freeing the string memory until such time as the VM has either associated it with a
 root, or if were a temporary alloc, is happy to have it freed.

 The transient stack is the solution.  The transient stack is like the permanent stack except that it can
 shrink.  The collector considers all entries in the transient stack below the top as roots.  They will be marked
 and scanned and will not be freed.

 There is also no 'pop' facility but this is handled by the 'transient block' mechanism.

 The #BVM_BEGIN_TRANSIENT_BLOCK macro marks the start of a block that may contain some transient roots.  The
 #BVM_END_TRANSIENT_BLOCK marks the end of the block.  When a transient block is entered using
 #BVM_BEGIN_TRANSIENT_BLOCK, the macro records the current top of the transient stack.  Inside the
 transient block any memory received from alloc() that is to be transiently protected from GC is placed on
 the stack using the #BVM_MAKE_TRANSIENT_ROOT macro - this macro effectively 'pushes' the root onto the transient
 stack - the top goes up.

 The collector considers anything below the top of the transient stack as being a root and is marked and scanned
 during GC.  As the transient block is exited (at #BVM_END_TRANSIENT_BLOCK) the transient stack top is restored to where
 it was on entry to the transient block.  In effect, this 'pops' all those transient roots that occurred in the transient
 block and therefore they will become eligible for collection - unless they can be reached by some other means (like
 being stored in a method local variable in a thread stack).

 Transient blocks may be nested (and normally are) as VM functions call each other.

 The BVM_TRY/BVM_CATCH mechanism also creates a transient stack mark for itself.  If an exception is caught or
 the BVM_TRY/BVM_CATCH exits normally, the top of the transient stack is restored in the same way as exiting
 a transient block defined by BEGIN/BVM_END_TRANSIENT_BLOCK does.  This has the effect of restoring the
 top of the stack regardless of the BEGIN/END_TRANSIENT_BLOCKS that occurred within the
 scope of the BVM_TRY/BVM_CATCH.

 @section cm Chunk Marking

 All heap chunks are subject to collection except those of #BVM_ALLOC_TYPE_STATIC.  Static chunks are ignored by
 the collector.

 Each alloc type has its own structure and the collector marker is effectively a switch on the alloc type - with
 subsequent processing for each type.  The types are designed to make life easy for the marker.  For example, rather
 than having all objects alloc'd as #BVM_ALLOC_TYPE_OBJECT which implies scanning for fields, there are others
 such as #BVM_ALLOC_TYPE_ARRAY_OF_PRIMITIVE which has no fields and can be marked very quickly.

 The collector will also collect unused clazz structures and all associated methods and fields and
 constants and so on.  If a classloader object becomes unreachable, this will cause the
 collector to free its managed Class objects and also clazz structures for that classloader.  Just like the
 JVMS says it should.  All elements of a loaded class are loaded as STATIC, except for the actual clazz structure itself.  In
 this way, there is no need to constantly be scanning clazz structures and all their stuff.  The things that make up a
 class are never eligible for collection (because they are STATIC).  But, when the Class object is, and then it associated
 \c bvm_clazz_t also is, and the collector takes special action and whips around to all the memory used by the clazz and frees it.

 It is important to note that interned String constants and pooled UTF strings for a clazz will NOT
 be freed when the clazz that they came from is freed.  Once a string is interned or a utf string pooled, it is there for
 the lifetime of the executing VM.

 @section bvm_gc-cm Weak References

 Weak references are supported in the same manner as CLDC 1.1.  CLDC 1.1 Weak references support is limited
 to the \c java.lang.ref.WeakReference class - there are no notification queues, nor are there different type of
 references - just 'weak'.

 The #BVM_ALLOC_TYPE_WEAK_REFERENCE denotes a weak reference object.  As a weak reference object is created the
 native method #java_lang_ref_WeakReference_makeweak changes the heap alloc type from #BVM_ALLOC_TYPE_OBJECT
 to #BVM_ALLOC_TYPE_WEAK_REFERENCE.

 Each GC cycle maintains a list of encountered WeakReference objects.  When the marking phase
 encounters a weak reference object, the marker adds the object to the head of the list of weak objects.

 After the marking phase is complete - and before the sweep, the list of weak reference objects is traversed.  If
 a 'referent' object that a weak reference object points to is going to be swept in this GC cycle, the referent
 object field of the weak reference object is set to \c NULL.  Simple.

 @section future Future notes:

 The current algorithm does not limit recursion depth so, in theory, it could crash the VM if it does not have enough 'C'
 stack space.  This is bad.  The collector really ought to be able to make it way through a collection without the possibility
 of crashing the VM.  Perhaps the same collection could be used to limit the recursion depth by stopping recursion
 at a given level.

  @author Greg McCreath
  @since 0.0.10

*/

/** The permanent roots stack */
bvm_cell_t *bvm_gl_gc_permanent_roots = NULL;

/** The current top of the permanent root stack */
bvm_uint32_t bvm_gl_gc_permanent_roots_top;

/** The transient roots stack */
bvm_cell_t *bvm_gl_gc_transient_roots = NULL;

/** The current top of the transient root stack */
bvm_uint32_t bvm_gl_gc_transient_roots_top;

/**
 * The default transient root stack depth (actual depth, not in BVM_KB). Defaults to #BVM_GC_TRANSIENT_ROOTS_DEPTH.
 */
bvm_uint32_t bvm_gl_gc_transient_roots_depth = BVM_GC_TRANSIENT_ROOTS_DEPTH;

/**
 * The permanent root stack depth (actual depth, not in BVM_KB).  Defaults to #BVM_GC_PERMANENT_ROOTS_DEPTH.
 */
bvm_uint32_t bvm_gl_gc_permanent_roots_depth = BVM_GC_PERMANENT_ROOTS_DEPTH;

/** Handle to head of weak references found during marking phase */
static bvm_weak_reference_obj_t *weak_refs = NULL;

/**
 * For a given memory chunk perform a mark on its contents.  The heap allocation type of the
 * chunk determines its structure and therefore how it is marked.  This function is recursive where
 * it needs to be.  Before recursing, the chunk colour will be set to #BVM_GC_COLOUR_GREY - we use the
 * grey colour to indicate that the chunk's marking is in progress.
 *
 * @param chunk the chunk to mark
 */
static void gc_mark_chunk(bvm_chunk_t *chunk) {


	int type = BVM_CHUNK_GetType(chunk);

	switch (type) {

		case BVM_ALLOC_TYPE_OBJECT: {

			/* For an object chunk we loop through every non-static reference field (object/array) defined for
			 * that object's class and all its superclasses.
			 *
			 * Note that clazz field definitions are sorted with all static fields first followed by
			 * all non-static ( .. or 'virtual') fields.  The virtual fields start at the
			 * 'virtual_field_offset' value stored with the clazz.
			 */

			bvm_obj_t *obj = BVM_CHUNK_GetUserData(chunk);
			bvm_instance_clazz_t *clazz = (bvm_instance_clazz_t *) obj->clazz;

			/* default the start and end of the loop to use virtual fields */
			int i;

			while (clazz != NULL) {

				/* search all the virtual fields of the object */
				for (i=clazz->virtual_field_offset; i < clazz->fields_count; i++) {
					bvm_chunk_t *field_chunk;
					bvm_obj_t *ptr;
					bvm_field_t *field = &clazz->fields[i];

					/* Deal only with array or object fields.  Arrays field descriptors will begin with '[' and
					 * reference field descriptors will begin with 'L' */
					if ( (field->jni_signature->data[0] == '[') || (field->jni_signature->data[0] == 'L')) {

						/* get a handle to the reference field value */
						ptr = obj->fields[field->value.offset].ref_value;

						/* if the field does not have a null value we'll check it out */
						if (ptr != NULL) {

							/* get the chunk that the value of the field is housed in */
							field_chunk = BVM_CHUNK_GetPointerChunk(ptr);

							/* if we've not yet marked this chunk, set about doing it */
							if (BVM_CHUNK_GetColour(field_chunk) == BVM_GC_COLOUR_WHITE)  {
								BVM_CHUNK_SetColour(field_chunk, BVM_GC_COLOUR_GREY);
								gc_mark_chunk(field_chunk);
							}
						}
					}
				}

				/* okay, done all this class's virtual fields, so go to it's superclass */
				clazz = clazz->super_clazz;
			}

			break;
		}
		case BVM_ALLOC_TYPE_ARRAY_OF_PRIMITIVE:
			/* Mark as black.  Nice and easy */
			break;
		case BVM_ALLOC_TYPE_ARRAY_OF_OBJECT: {

			/* If we have an array of objects we'll recurse on each of them individually. */
			bvm_instance_array_obj_t *array = (bvm_instance_array_obj_t *) BVM_CHUNK_GetUserData(chunk);
			bvm_uint32_t i;

			/* work through the array (backwards - slightly faster) */
			for (i=array->length.int_value; i--;) {

				/* get the object at array element 'i'. */
				bvm_obj_t *obj = (bvm_obj_t *) array->data[i];

				/* if the object has a value */
				if (obj != NULL) {

					/* get the object's chunk */
					bvm_chunk_t *obj_chunk = BVM_CHUNK_GetPointerChunk(obj);

					/* if we have not already marked the object's chunk, do so */
					if (BVM_CHUNK_GetColour(obj_chunk) == BVM_GC_COLOUR_WHITE) {
						BVM_CHUNK_SetColour(obj_chunk, BVM_GC_COLOUR_GREY);
						gc_mark_chunk(obj_chunk);
					}

					/* set the colour of the object to black - having arrived here means all
					 * its children have been marked / recursed. */
					BVM_CHUNK_SetColour(obj_chunk, BVM_GC_COLOUR_BLACK);
				}
			}

			break;
		}
		case BVM_ALLOC_TYPE_STRING: {

			/* Strings only have one child, so rather than recurse, we'll just mark the String and its
			 * child char array Black and make life simple and quick */

			/* set the colour of the String's char array to black.  Strings may share char arrays
			 * with StringBuffer objects and other String objects, but we'll not check if a char
			 * array has already been marked - we'll just mark it regardless.  */
			bvm_string_obj_t *string = (bvm_string_obj_t *) BVM_CHUNK_GetUserData(chunk);
			if (string->chars != NULL)
				BVM_CHUNK_SetColour(BVM_CHUNK_GetPointerChunk(string->chars), BVM_GC_COLOUR_BLACK);

			break;
		}
		case BVM_ALLOC_TYPE_WEAK_REFERENCE: {

			/* add the weak reference to the head of the list of weak references.  This list is scanned as the
			 * last thing before a sweep.  Any weak refs that have not had their referent objects marked have it
			 * set to NULL - we do not mark them here. The WeakReference object however, will be marked black. */

			bvm_weak_reference_obj_t *weak_reference = (bvm_weak_reference_obj_t *) BVM_CHUNK_GetUserData(chunk);

			weak_reference->next = weak_refs;
			weak_refs = weak_reference;

			break;
		}
		case BVM_ALLOC_TYPE_DATA:
			/* Simple.  Mark black */
			break;
		case BVM_ALLOC_TYPE_ARRAY_CLAZZ:
		case BVM_ALLOC_TYPE_PRIMITIVE_CLAZZ: {

			/* For an array class, mark its classloader.  This will have the side effect of marking
			 * all the other classes owned by the that classloader as well. */

			bvm_classloader_obj_t *classloader_obj = ((bvm_clazz_t *) BVM_CHUNK_GetUserData(chunk))->classloader_obj;

			/* Mark the class loader object */
			if (classloader_obj != NULL) {
				bvm_chunk_t* loader_chunk = BVM_CHUNK_GetPointerChunk(classloader_obj);

				if (BVM_CHUNK_GetColour(loader_chunk) == BVM_GC_COLOUR_WHITE) {
					BVM_CHUNK_SetColour(loader_chunk, BVM_GC_COLOUR_GREY);
					gc_mark_chunk(loader_chunk);
				}
			}

			break;
		}
		case BVM_ALLOC_TYPE_INSTANCE_CLAZZ: {

			/* mark all static fields for an instance clazz including those inherited. */

			bvm_chunk_t *clazz_chunk;
			bvm_instance_clazz_t *clazz = BVM_CHUNK_GetUserData(chunk);

			int i;

			while (clazz != NULL) {

				clazz_chunk = BVM_CHUNK_GetPointerChunk(clazz);

				/* Mark the class loader object */
				if (clazz->classloader_obj != NULL) {
					bvm_chunk_t* loader_chunk = BVM_CHUNK_GetPointerChunk(clazz->classloader_obj);

					if (BVM_CHUNK_GetColour(loader_chunk) == BVM_GC_COLOUR_WHITE) {
						BVM_CHUNK_SetColour(loader_chunk, BVM_GC_COLOUR_GREY);
						gc_mark_chunk(loader_chunk);
					}
				}

				/* search all the static fields of the clazz */
				for (i=clazz->virtual_field_offset; i--;) {
					bvm_chunk_t *field_chunk;
					bvm_obj_t *ptr;
					bvm_field_t *field = &clazz->fields[i];


					/* Deal only with reference fields.  Arrays field descriptors will begin
					 * with '[' and object descriptors will begin with 'L' */
					char c = field->jni_signature->data[0];
					if ( (c == '[') || (c == 'L')) {

						/* static values are held in the field struct */
						ptr = field->value.static_value.ref_value;

						/* if the field has a value */
						if (ptr != NULL) {

							/* get the chunk that the value of the field is housed in */
							field_chunk = BVM_CHUNK_GetPointerChunk(ptr);

							/* if we've not yet marked this chunk, set about doing it */
							if (BVM_CHUNK_GetColour(field_chunk) == BVM_GC_COLOUR_WHITE) {
								BVM_CHUNK_SetColour(field_chunk, BVM_GC_COLOUR_GREY);
								gc_mark_chunk(field_chunk);
							}
						}
					}

				}

				/* after having dealt with a clazz, set its colour to black - we do not ever
				 * need to traverse it again regardless of how we find it again during scanning.
				 * It may have the small side effect of occasionally colouring the current chunk
				 * black twice, but who cares. */
				BVM_CHUNK_SetColour(clazz_chunk, BVM_GC_COLOUR_BLACK);

				clazz = clazz->super_clazz;
			}

			break;
		}
		case BVM_ALLOC_TYPE_STATIC:
			/* Ignore.  Mark Black.  Will not be freed. */
			break;
	}

	/* set the chunk's colour to black to say we have fully scanned it */
	BVM_CHUNK_SetColour(chunk, BVM_GC_COLOUR_BLACK);
}

/**
 * Mark the String objects contained in the interned string pool as black.  Interned String objects are
 * not garbage collected.
 */
static void gc_mark_interned_strings() {

	int i;
	bvm_internstring_obj_t *string;
	bvm_chunk_t *chunk;

#if BVM_DEBUG_HEAP_GC_ON_ALLOC
	/* When doing a GC before each alloc (for debugging) it is possible we can GC before the
	 * intern string pool is alloc'd.  So we check. */
	if (bvm_gl_internstring_pool == NULL) return;
#endif

	/* loop through array for the hash pool (backwards - is slightly faster) */
	for (i = bvm_gl_internstring_pool_bucketcount; i--;) {

		/* get the string entry at hash bucket */
		string = bvm_gl_internstring_pool[i];

		/* for each interned string in the bucket */
		while (string != NULL)  {

			chunk = BVM_CHUNK_GetPointerChunk(string);

			/* if the string is white, colour it and its char array black */
			if (BVM_CHUNK_GetColour(chunk) == BVM_GC_COLOUR_WHITE) {
				BVM_CHUNK_SetColour(chunk, BVM_GC_COLOUR_BLACK);
				if (string->chars != NULL) {
					BVM_CHUNK_SetColour(BVM_CHUNK_GetPointerChunk(string->chars), BVM_GC_COLOUR_BLACK);
				}
			}

			string = string->next;
		}
	}
}

/**
 * Mark a range of cell roots.  Each cell is ASSUMED to be a valid heap pointer so no
 * pointer checking is required.  Why?  Because this is used to scan structures that are maintained by
 * the VM - and it is only putting heap references into these structures.
 *
 * @param cells - the address of the first cell to scan.
 * @param count - the number of cells to scan.
 */
static void gc_mark_cell_range(bvm_cell_t *cells, bvm_uint32_t count) {

	bvm_chunk_t *chunk;

	/* loop through cell range backwards - is slightly faster */
	for (;count--;) {

		bvm_obj_t *obj = cells[count].ref_value;

		if (obj != NULL) {

			/* get the chunk associated with the reference the cell points to */
			chunk = BVM_CHUNK_GetPointerChunk(obj);

			if (BVM_CHUNK_GetColour(chunk) == BVM_GC_COLOUR_WHITE) {
				BVM_CHUNK_SetColour(chunk, BVM_GC_COLOUR_GREY);
				gc_mark_chunk(chunk);
			}
		}
	}
}

/**
 * Mark the roots that have been pushed onto the transient roots stack.
 */
static void gc_mark_transient_roots() {

#if BVM_DEBUG_HEAP_GC_ON_ALLOC
	/* When doing a GC before each alloc (for debugging) it is possible we can GC before the
	 * transient roots stack is alloc'd.  So we check. */
	if (bvm_gl_gc_transient_roots == NULL) return;
#endif

	/* the range of cells to mark is defined as the start of the transient root stack up to
	 * its height */
	gc_mark_cell_range(bvm_gl_gc_transient_roots, bvm_gl_gc_transient_roots_top);
}

/**
 * Mark the roots that have been pushed onto the permanent roots stack.
 */
static void gc_mark_permanent_roots() {


#if BVM_DEBUG_HEAP_GC_ON_ALLOC
	/* When doing a GC before each alloc (for debugging) it is possible we can GC before the
	 * transient roots stack is alloc'd.  So we check. */
	if (bvm_gl_gc_permanent_roots == NULL) return;
#endif

	/* the range of cells to mark is defined as the start of the permanent root stack up to
	 * its height */
	gc_mark_cell_range(bvm_gl_gc_permanent_roots, bvm_gl_gc_permanent_roots_top);
}

#if BVM_DEBUGGER_ENABLE

/**
 * Pends a bvm_clazz_t struct for later sending of clazz unload events.
 *
 * @param clazz the given clazz
 */
static void gc_pend_clazz_unload(bvm_clazz_t *clazz) {

	/* remove the clazz from the dbg id cache - if there. */
	bvmd_id_remove_addr(clazz);

	/* if another GC occurs before this clazz's unload event has been sent and the clazz is
	 * again cleaned up - we do not want to get back here again ..., so changing it to STATIC means no
	 * more GC attempts on the given clazz - it will be cleaned up later manually */
	bvm_heap_set_alloc_type(clazz, BVM_ALLOC_TYPE_STATIC);

	/* if debugging, we cannot send the event now (it allocates memory) so we park it
	 * for later a event - and manual freeing. */
	bvmd_event_park_unloaded_clazz(clazz);
}

#endif

/**
 * Sweep the heap.  After the marking, any chunks in the heap that are not in use and are
 * coloured #BVM_GC_COLOUR_WHITE and are freed, everything else is marked #BVM_GC_COLOUR_WHITE.
 */
static void gc_sweep() {

	/* start at the heap start! */
	bvm_chunk_t *chunk = (bvm_chunk_t *) bvm_gl_heap_start;

#if BVM_DEBUGGER_ENABLE
	bvm_bool_t dbg = bvmd_is_session_open();
#endif

	/* scan until heap end */
	while ( chunk < (bvm_chunk_t *) bvm_gl_heap_end ) {

		/* if the chunk is marked as being used but is white (unreachable), free it. */
		if ( (BVM_CHUNK_IsInuse(chunk)) && (BVM_CHUNK_GetColour(chunk) == BVM_GC_COLOUR_WHITE)) {

			int type = BVM_CHUNK_GetType(chunk);

			switch (type) {
				case BVM_ALLOC_TYPE_OBJECT:
				case BVM_ALLOC_TYPE_ARRAY_OF_PRIMITIVE:
				case BVM_ALLOC_TYPE_ARRAY_OF_OBJECT:
				case BVM_ALLOC_TYPE_STRING:
				case BVM_ALLOC_TYPE_WEAK_REFERENCE:
				case BVM_ALLOC_TYPE_DATA:

#if BVM_DEBUGGER_ENABLE
					/* if we have a debugger session going, remove it from id cache (if it
					 * is there).  Yes, a bit brute force, but until a better way is implemented, this
					 * will have to do. */
					if (dbg) bvmd_id_remove_addr(BVM_CHUNK_GetUserData(chunk));
#endif

					chunk = bvm_heap_free_chunk(chunk);
					break;
				case BVM_ALLOC_TYPE_ARRAY_CLAZZ:
				case BVM_ALLOC_TYPE_PRIMITIVE_CLAZZ: {

					/* where we unload classes */
					bvm_clazz_t *clazz = (bvm_clazz_t *) BVM_CHUNK_GetUserData(chunk);

					bvm_clazz_pool_remove(clazz);
					// the class name for the array types is allocated from the heap
					// as a _copy_ of the name.
					bvm_heap_free(clazz->name);

#if BVM_DEBUGGER_ENABLE
					if (dbg) {
						gc_pend_clazz_unload(clazz);
						break;
					}
#endif
					chunk = bvm_heap_free_chunk(chunk);
					break;
				}
				case BVM_ALLOC_TYPE_INSTANCE_CLAZZ: {
					int i;
					bvm_instance_clazz_t *clazz = (bvm_instance_clazz_t *) BVM_CHUNK_GetUserData(chunk);

					if (clazz->state > BVM_CLAZZ_STATE_ERROR)
						bvm_clazz_pool_remove( (bvm_clazz_t *) clazz);

					if (clazz->constant_pool != NULL)
						bvm_heap_free(clazz->constant_pool);

					if (clazz->fields != NULL)
						bvm_heap_free(clazz->fields);

					if (clazz->interfaces != NULL)
						bvm_heap_free(clazz->interfaces);

					if (clazz->static_longs != NULL)
						bvm_heap_free(clazz->static_longs);

					for (i = clazz->methods_count; i--;) {

						bvm_method_t *method = &clazz->methods[i];

						if (!BVM_METHOD_IsNative(method) && (method->code.bytecode != NULL) )
							bvm_heap_free(method->code.bytecode);

						if (method->exceptions != NULL)
							bvm_heap_free(method->exceptions);

#if (BVM_LINE_NUMBERS_ENABLE || BVM_DEBUGGER_ENABLE)

						if (method->line_numbers != NULL)
							bvm_heap_free(method->line_numbers);

#endif

#if BVM_DEBUGGER_ENABLE
						if (method->local_variables != NULL)
							bvm_heap_free(method->local_variables);
#endif
					}

					if (clazz->methods != NULL)
						bvm_heap_free(clazz->methods);

#if BVM_DEBUGGER_ENABLE

#if BVM_DEBUGGER_JSR045_ENABLE
					if (clazz->source_debug_extension != NULL) {
						bvm_heap_free(clazz->source_debug_extension);
					}
#endif
					if (dbg) {
						gc_pend_clazz_unload( (bvm_clazz_t *) clazz);
						break;
					}
#endif
					chunk = bvm_heap_free_chunk(chunk);

					break;
				}
				case BVM_ALLOC_TYPE_STATIC:
					break;
				default :
					BVM_VM_EXIT(BVM_FATAL_ERR_INVALID_MEMORY_CHUNK, NULL);
			}
		} else {
			BVM_CHUNK_SetColour(chunk, BVM_GC_COLOUR_WHITE);
		}

		/* go to the next chunk */
		chunk = BVM_CHUNK_GetNextChunk(chunk);
	}
}

/**
 * Traverse a stack thread from top to bottom marking the range for each frame from the first
 * local var to the current top of the operand stack for the frame.  Each cell in a frame is
 * examined to determine whether it is an object pointer or not.
 *
 * When examining a cell value, all are first *assumed* to be object pointers.  If the pointer address is outside
 * the heap range then it is not a pointer at all - it is something else.  Having determined that the cell pointer
 * address is in the heap (and assuming it is an object), its #bvm_clazz_t pointer is also checked to make sure that is
 * in the heap.  If it is, and the clazz header contains the java magic number at the right place, we are almost
 * certain we have an object.
 *
 * Next, however, we check the object's chunk it is in use and that the memory allocation type is indeed
 * for an object.  After all that, we can be 100% sure it is a live java object.
 *
 * Note that stack frames are not zeroed before use.  This means that there may be cells in the frame locals and frame stack
 * that are residue from some previous usage of the same stack memory.  This is why make very certain (with the above checks)
 * that the cell is a real live java object.  Note that we do not care if it is 'residue' or not, were just concerned with
 * marking any reachable objects in the stack - if that happens to be residue in the current frame, who cares.  The only
 * downside is that an object that *should* be GC'd is not because the marker thinks it is still a live object (because it is
 * on the stack!).  This is a conservative approach, but better than zeroing every frame before use.
 *
 * A note about native methods:  Native methods do not use stack frames like non-native methods - yes, a stack frame is
 * pushed, but it is empty.  For that reason, this stack marker takes special action when marking a frame for a native
 * method.  Native methods arguments are not copied into a frame's 'local' area from the previous frame - like
 * non-native methods are, the argument to a non-native method exist only on the stack frame of the method calling
 * the native method - so the top/bottom cells of a native method cell scan are different to a non-native method.
 *
 * Scanning a thread stack finishes when the callback method for thread termination (which represents the
 * thread stack base) is reached.
 *
 * @param vmthread - the VM thread to scan and mark.
 */
static void gc_mark_thread_stack(bvm_vmthread_t *vmthread) {

	bvm_cell_t *frame_locals, *cell_ptr, *frame_top;
	bvm_stacksegment_t *temp_frame_stack;
	bvm_obj_t *obj_ptr;
	bvm_chunk_t *chunk;
	bvm_method_t *frame_method;

	/* get the method the frame is for */
	frame_method = vmthread->rx_method;

	/* start at the base of the uppermost frame on the VM stack */
	frame_locals = vmthread->rx_locals;

	/* The top of our first cell range is one cell past the top of the current method's stack.  This is NOT
	 * inclusive.  */
	frame_top = vmthread->rx_sp;

	/* truncate the stack list at the current stack segment so that all further segments in the stack
	 * list are severed and will be GC'd */
	vmthread->rx_stack->next = NULL;

	/* mark each of the remaining stacks in the stack list */
	temp_frame_stack = vmthread->stack_list;
	while (temp_frame_stack != NULL) {
		BVM_CHUNK_SetColour(BVM_CHUNK_GetPointerChunk(temp_frame_stack), BVM_GC_COLOUR_BLACK);
		temp_frame_stack = temp_frame_stack->next;
	}

	/* for each locals/operand-stack section of a frame (which we delimit with frame_locals/frame_top),
	 * attempt to mark each cell if it is a valid heap pointer */
	while (  !( (frame_method == BVM_METHOD_CALLBACKWEDGE) &&
		        (frame_locals[1].callback == bvm_thread_terminated_callback) )) {

		if (!BVM_METHOD_IsNative(frame_method)) {

			/* cell ptr is a moving ptr that we used to traverse the cell in the frame.  For a non-native
			 * method the range is from the 'locals' to the current stack pointer in the frame. */
			cell_ptr = frame_locals;

		} else {

			/* cell ptr is a moving ptr that we used to traverse the range
			 * defined by frame_locals/frame_top.  Note the special calculation here for a
			 * native method.  Native methods do not use the stack like a non-native method, and the
			 * method call arguments are not copied into the frame locals areas like a
			 * non-native method.  A native method's arguments are actually located at the stop of the stack
			 * in the *calling* frame - so when determining where to start our cell scan, we point it to
			 * the stack pointer in the previous frame.  The top of the cell scan is that position plus the number
			 * of arguments the method has + 'this' if the method is not static. */
			cell_ptr = frame_locals[BVM_FRAME_SP_OFFSET].ptr_value;

			frame_top = cell_ptr + frame_method->num_args + ( BVM_METHOD_IsStatic(frame_method) ? 0 : 1);
		}

		/* for each cell in the locals/stack part of the current frame ... */
		while (cell_ptr < frame_top) {

			/* assume everything is a java object reference pointer, and then attempt to
			 * prove it otherwise */
			obj_ptr = (bvm_obj_t *) cell_ptr->ptr_value;

			/* it will not be an object pointer if it lies outside the heap range.  bvm_gl_heap_start is
			 * inclusive, bvm_gl_heap_end is exclusive.  Note this expression also excludes NULLs. */
			if ( ( (bvm_uint8_t *) obj_ptr >= bvm_gl_heap_start) &&
				 ( (bvm_uint8_t *) obj_ptr < bvm_gl_heap_end) ) {

				/* get the bvm_clazz_t pointer of the candidate 'object' - if it is also within the heap then we can
				 * be more confident the 'object' is a real java object. */
				bvm_clazz_t *heap_ptr_clazz = obj_ptr->clazz;

				if ( ( (bvm_uint8_t *) heap_ptr_clazz >= bvm_gl_heap_start) &&
					 ( (bvm_uint8_t *) heap_ptr_clazz < bvm_gl_heap_end) ) {

					/* more confident now .... more tests though, each bvm_clazz_t stores the magic number '0xCAFEBABE'.  If the
					 * bvm_clazz_t pointer for the candidate java object is equal to that magic number, we are more confident
					 * it is a real object reference we are dealing with. */

					if (heap_ptr_clazz->magic_number == BVM_MAGIC_NUMBER) {

						/* okay, object has a class pointer in the heap and the magic number of the class
						 * at that pointer is correct.  We are almost sure it is a real object.  BUT, we make some final
						 * checks to  make sure it is in use, not been marked, and the chunk type is really for
						 * an object type. */

						/* get the chunk for the object pointer  */
						chunk = BVM_CHUNK_GetPointerChunk(obj_ptr);

						/* If it is in use, is an object type, and has not been coloured yet, colour it
						 * grey and proceed to mark it. */
						if ( (BVM_CHUNK_IsInuse(chunk)) &&
							 (BVM_CHUNK_GetType(chunk) <= BVM_ALLOC_MAX_OBJECT) &&
							 (BVM_CHUNK_GetColour(chunk) == BVM_GC_COLOUR_WHITE) &&
							 (bvm_heap_is_chunk_valid(chunk)) ) {

							BVM_CHUNK_SetColour(chunk, BVM_GC_COLOUR_GREY);
							gc_mark_chunk(chunk);
						}
					}
				}
			}

			/* continue on up the cell range defined by frame_locals/frame_top */
			cell_ptr++;
		}

		/* move down the VM stack to the previous frame.  Kind of like a pop_frame() but without any
		 * real popping */
		frame_method = frame_locals[BVM_FRAME_METHOD_OFFSET].ptr_value;
		frame_top    = frame_locals[BVM_FRAME_SP_OFFSET].ptr_value;
		frame_locals = frame_locals[BVM_FRAME_LOCALS_OFFSET].ptr_value;
	}
}

/**
 *  Scan the global VM threads list and mark all non-terminated threads.
 */
static void gc_mark_threads() {

	bvm_vmthread_t *vmthread;
	bvm_chunk_t *thread_chunk;

#if BVM_DEBUG_HEAP_GC_ON_ALLOC
	if (bvm_gl_threads == NULL) return;
#endif

	/* force the current gl registers back into the current thread registers */
	bvm_thread_store_registers(bvm_gl_thread_current);

	/* recalc the start of the threads list by finding the first un-terminated thread */
	while ( (bvm_gl_threads != NULL) && (bvm_gl_threads->status == BVM_THREAD_STATUS_TERMINATED))
		bvm_gl_threads = bvm_gl_threads->next;

	/* init our loop var to the (possibly newly recalculated - above) start of the threads list */
	vmthread = bvm_gl_threads;

	while (vmthread != NULL) {

		if (vmthread->status != BVM_THREAD_STATUS_TERMINATED) {

			/* colour the vm thread black */
			thread_chunk = BVM_CHUNK_GetPointerChunk(vmthread);
			BVM_CHUNK_SetColour(thread_chunk, BVM_GC_COLOUR_BLACK);

			/* mark the java thread object */
			gc_mark_chunk(BVM_CHUNK_GetPointerChunk(vmthread->thread_obj));

			/* only mark the thread stack if it exists.  Also, no need to scan the
			 * stack if the thread is NEW - it will have a stack (which may have multiple segments) - but it will
			 * not have anything on it to do. */
			if ( (vmthread->stack_list != NULL) ) {
				if (vmthread->status == BVM_THREAD_STATUS_NEW) {
					/* loop through the stack segments.  It is *possible* that there may be more than one
					 * for a NEW thread.  For example, the 'run' method may be very large and
					 * exceed the default stack size (after the callback wedge is placed into the stack) and therefore have a
					 * new larger one appended. */
					bvm_stacksegment_t *stack = vmthread->stack_list;
					while (stack != NULL) {
						BVM_CHUNK_SetColour(BVM_CHUNK_GetPointerChunk(stack), BVM_GC_COLOUR_BLACK);
						stack = stack->next;
					}
				}
				else {
					gc_mark_thread_stack(vmthread);
				}
			}

			/* If the thread has a pending exception, mark the exception */
			if (vmthread->pending_exception != NULL) {
				bvm_chunk_t *ex_chunk;
				ex_chunk = BVM_CHUNK_GetPointerChunk(vmthread->pending_exception);
				BVM_CHUNK_SetColour(ex_chunk, BVM_GC_COLOUR_GREY);
				gc_mark_chunk(ex_chunk);
			}

			if (vmthread->exception_location.throwable != NULL) {
				bvm_chunk_t *ex_chunk;
				ex_chunk = BVM_CHUNK_GetPointerChunk(vmthread->exception_location.throwable);
				BVM_CHUNK_SetColour(ex_chunk, BVM_GC_COLOUR_GREY);
				gc_mark_chunk(ex_chunk);
			}
		}

		/* if the next thread in the list is terminated, skip it.  Note that the actual real
		 * gl thread list is amended here to exclude the terminated thread.
		 * TODO - would be better to spin here until a non-terminated one is found? */
		if ( (vmthread->next != NULL) && (vmthread->next->status == BVM_THREAD_STATUS_TERMINATED) )
			vmthread->next = vmthread->next->next;

		vmthread = vmthread->next;
	}
}

#if BVM_DEBUGGER_ENABLE

/**
 * Mark objects in the JDWP debugger root map.  Objects in this map have put there as a consequence
 * of the JDWP command ObjectReference.DisableCollection.
 */
void gc_mark_debug_roots() {


	int i = BVMD_ROOTMAP_SIZE;
	bvm_chunk_t *chunk;

	while (i--) {

		bvmd_root_t *root = bvmd_root_map[i];

		while (root != NULL) {

			chunk = BVM_CHUNK_GetPointerChunk(root->addr);

			/* If it is in use, and has not been coloured yet, colour it and mark it. */
			if ( (BVM_CHUNK_IsInuse(chunk)) &&
				 (BVM_CHUNK_GetColour(chunk) == BVM_GC_COLOUR_WHITE) ) {

				BVM_CHUNK_SetColour(chunk, BVM_GC_COLOUR_GREY);
				gc_mark_chunk(chunk);
			}

			root = root->next;
		}
	}
}

#endif


/**
 * Scan the weak references list and set to \c NULL the referent object reference for any objects
 * that will be GC'd in the sweep.  The #weak_refs header will be \c NULL after this.
 */
static void gc_visit_weakrefs() {

	while (weak_refs != NULL) {

		if (weak_refs->referent != NULL) {
			bvm_chunk_t *chunk = BVM_CHUNK_GetPointerChunk(weak_refs->referent);
			if (BVM_CHUNK_GetColour(chunk) == BVM_GC_COLOUR_WHITE) {
				weak_refs->referent = NULL;
			}
		}

		weak_refs = weak_refs->next;
	}
}

/**
 * Perform a garbage collection cycle.  A detailed explanation is at the top of
 * this source file.
 */
void bvm_gc() {

	weak_refs = NULL;

	/* scan interned strings */
	/* TODO - could interned string be marked as STATIC when created and therefore avoid doing this
	 * each time ? */
	gc_mark_interned_strings();

	/* scan transient root stack */
	gc_mark_transient_roots();

	/* scan system root stack */
	gc_mark_permanent_roots();

	/* mark each thread */
	gc_mark_threads();

#if BVM_DEBUGGER_ENABLE
	/* mark the debug root, if any */
	if (bvmd_is_session_open()) gc_mark_debug_roots();
#endif

	/* before doing the sweep, set all found weak references referent object to NULL. */
	gc_visit_weakrefs();

	/* and finally sweep the heap */
	gc_sweep();
}

#if BVM_DEBUG_CLEAR_HEAP_ON_EXIT

static void clear_utfsstring_pool() {

	int i;
	bvm_utfstring_t *current, *next;

	for (i = bvm_gl_utfstring_pool_bucketcount; i--;) {

		current = bvm_gl_utfstring_pool[i];

		while (current != NULL)  {
			next = current->next;
			bvm_heap_free(current);
			current = next;
		}

		bvm_gl_utfstring_pool[i] = NULL;
	}
}

static void clear_method_pool() {

	int i;
	bvm_native_method_desc_t *current, *next;

	for (i = bvm_gl_native_method_pool_bucketcount; i--;) {

		current = bvm_gl_native_method_pool[i];

		while (current != NULL)  {
			next = current->next;
			bvm_heap_free(current);
			current = next;
		}

		bvm_gl_native_method_pool[i] = NULL;
	}
}

static void clear_threads() {

	bvm_vmthread_t *current, *next;

	current = bvm_gl_threads;

	while (current != NULL)  {
		next = current->next;
		if (current->stack_list != NULL) {

			bvm_stacksegment_t *c_stk, *n_stk;

			c_stk = current->stack_list;
			current->stack_list = NULL;

			while (c_stk != NULL) {
				n_stk = c_stk->next;
				bvm_heap_free(c_stk);
				c_stk = n_stk;
			}
		}
		bvm_heap_free(current);
		current = next;
	}

	bvm_gl_threads = NULL;
}

static void clear_intern_string_pool() {

	int i;

	/* intern strings are not STATIC, so they will be cleaned up in a GC */
	for (i = bvm_gl_internstring_pool_bucketcount; i--;) {
		bvm_gl_internstring_pool[i] = NULL;
	}
}

static void clear_thread_monitors() {

	bvm_monitor_t *monitor = bvm_gl_thread_monitor_list;

	while (monitor != NULL) {
		bvm_monitor_t *m = monitor;
		monitor = monitor->next;
		bvm_heap_free(m);
	}
}

void bvm_debug_clear_heap() {

	bvm_uint32_t leak_size;

	bvm_gc();

#if BVM_CONSOLE_ENABLE
	bvm_pd_console_out("\n");
	bvm_pd_console_out("At VM exit: heap=%d, free=%d\n", (int) bvm_gl_heap_size, (int) bvm_gl_heap_free);
#endif

	/* clear the intern strings */
	clear_intern_string_pool();

	/* clear the transient stack */
	bvm_gl_gc_transient_roots_top = 0;

	/* clear the permanent stack */
	bvm_gl_gc_permanent_roots_top = 0;

	/* reset the stack pointers - we could have arrived here after a BVM_VM_EXIT, in which case the
	 * stack pointers will be (erm) somewhere undefined. */
	bvm_gl_rx_sp = &(bvm_gl_rx_stack->body[0]);
	bvm_gl_rx_locals = bvm_gl_rx_sp;

	/* give back memory for threads */
	clear_threads();

	bvm_gc();

	/* clear UTF string pool after GC (names are used to clean up classes during GC) */
	clear_utfsstring_pool();

	/* clear the native method pool */
	clear_method_pool();

	/* thread monitors are allocated as static */
	clear_thread_monitors();

	bvm_heap_free(bvm_gl_clazz_pool);
	bvm_heap_free(bvm_gl_utfstring_pool);
	bvm_heap_free(bvm_gl_internstring_pool);
	bvm_heap_free(bvm_gl_native_method_pool);
	bvm_heap_free(bvm_gl_gc_transient_roots);
	bvm_heap_free(bvm_gl_gc_permanent_roots);

	leak_size = bvm_gl_heap_size - bvm_gl_heap_free;

#if BVM_CONSOLE_ENABLE
	bvm_pd_console_out("Cleared heap free=%d\n", (int) bvm_gl_heap_free);
	bvm_pd_console_out("Heap leak %d\n", leak_size) ;

	if (leak_size > 0) bvm_heap_debug_dump();
#endif

	if (leak_size != 0)
		bvm_pd_system_exit(BVM_FATAL_ERR_HEAP_LEAK_DETECTED_ON_VM_EXIT);
}

#endif
