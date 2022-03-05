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

#ifndef BVM_COLLECTOR_H_
#define BVM_COLLECTOR_H_

/**
  @file

  Constants/Macros/Functions for Garbage Collection.

  @author Greg McCreath
  @since 0.0.10

*/

/**
 * The heap usage types.  By specifying an alloc type when memory from the heap is allocated heap
 * we can tell the GC how to scan it for marking purposes.
 */
#define BVM_ALLOC_TYPE_OBJECT             0  /* for an non-class/non-array object reference */
#define BVM_ALLOC_TYPE_ARRAY_OF_PRIMITIVE 1  /* for an array of primitives - no references kept */
#define BVM_ALLOC_TYPE_ARRAY_OF_OBJECT    2  /* for an array of object references */
#define BVM_ALLOC_TYPE_STRING             3  /* for String references  */
#define BVM_ALLOC_TYPE_WEAK_REFERENCE     4  /* For Java WeakReference Objects */
// everything above here is an object allocation. See #BVM_ALLOC_MAX_OBJECT
#define BVM_ALLOC_TYPE_DATA               5  /* Just a block of raw data - not java objects */
#define BVM_ALLOC_TYPE_ARRAY_CLAZZ        6  /* for array clazz structure */
#define BVM_ALLOC_TYPE_PRIMITIVE_CLAZZ    7  /* for primitive clazz structure */
#define BVM_ALLOC_TYPE_INSTANCE_CLAZZ     8  /* for instance clazz structure */
#define BVM_ALLOC_TYPE_STATIC     		  9  /* GC will ignore */

/* min and max allocation types are used in pointer validity checking */
#define BVM_ALLOC_MIN_TYPE  BVM_ALLOC_TYPE_OBJECT
#define BVM_ALLOC_MAX_TYPE  BVM_ALLOC_TYPE_STATIC
#define BVM_ALLOC_MAX_OBJECT BVM_ALLOC_TYPE_WEAK_REFERENCE

extern bvm_cell_t *bvm_gl_gc_permanent_roots;

extern bvm_uint32_t bvm_gl_gc_permanent_roots_top;

extern bvm_cell_t *bvm_gl_gc_transient_roots;

extern bvm_uint32_t bvm_gl_gc_transient_roots_top;

extern bvm_uint32_t bvm_gl_gc_transient_roots_depth;

extern bvm_uint32_t bvm_gl_gc_permanent_roots_depth;

#define BVM_BEGIN_TRANSIENT_BLOCK  { bvm_uint32_t __transient_mark__ = bvm_gl_gc_transient_roots_top;

#define BVM_END_TRANSIENT_BLOCK bvm_gl_gc_transient_roots_top = __transient_mark__; }

void bvm_gc();

/**
 * Push a ptr onto the permanent root stack to stop it from becoming GC'd during a collection.  Note there
 * is no 'pop' equivalent - this is performed automatically by the BEGIN/BVM_END_TRANSIENT_BLOCK macros.
 *
 * @param ptr - a pointer to the object/data to be pushed.
 */
#define BVM_MAKE_PERMANENT_ROOT(ptr) 									\
	if (bvm_gl_gc_permanent_roots_top == bvm_gl_gc_permanent_roots_depth) 		\
		BVM_VM_EXIT(BVM_FATAL_ERR_PERMANENT_ROOTS_EXHAUSTED, NULL)				\
	bvm_gl_gc_permanent_roots[bvm_gl_gc_permanent_roots_top++].ptr_value = (ptr);

/**
 * Push a ptr onto the transient root stack to stop it from becoming GC'd during a collection.  Note there
 * is no 'pop' equivalent - this is performed automatically by the BEGIN/BVM_END_TRANSIENT_BLOCK macros.
 *
 * @param ptr - a pointer to the object/data to be pushed.
 */
#define BVM_MAKE_TRANSIENT_ROOT(ptr) 							        \
	if (bvm_gl_gc_transient_roots_top == bvm_gl_gc_transient_roots_depth) 	    \
	    BVM_VM_EXIT(BVM_FATAL_ERR_TRANSIENT_ROOTS_EXHAUSTED, NULL)				\
    bvm_gl_gc_transient_roots[bvm_gl_gc_transient_roots_top++].ptr_value = (ptr);


void bvm_debug_clear_heap();

#endif /*BVM_COLLECTOR_H_*/
