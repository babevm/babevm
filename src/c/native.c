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

#include <stdlib.h>
#include <math.h>
#include "../h/bvm.h"

/**

  @file

  Java core classes native methods.

  @author Greg McCreath
  @since 0.0.10

 */

/***************************************************************************************************
 * java.lang.Object
 **************************************************************************************************/

/*
 * int hashCode()
 */
void java_lang_Object_hashCode(void *args) {
    // treat memory location as its hashcode.  Will not give exact address on 64 bit as int is 32 bit.
	NI_ReturnInt(NI_GetParameterAsInt(0));
}

/*
 * Class<?> getClass()
 */
void java_lang_Object_getClass(void *args) {
	jclass cl = NI_GetObjectClass(NI_GetParameterAsObject(0));
	NI_ReturnObject(cl);
}

/*
 * void wait(long millis) throws InterruptedException
 */
void java_lang_Object_wait(void *args) {

	bvm_int64_t wait_time;
	bvm_monitor_t *monitor;

	/* param 0 is the object with the monitor to wait on*/
	bvm_obj_t *obj = NI_GetParameterAsObject(0);

	/* param 1 is the *long* length of time to wait (it takes two cells)*/
    wait_time = NI_GetParameterAsLong(1);

	/* timeout must be >= 0. */
	if (!BVM_INT64_zero_ge(wait_time))
		bvm_throw_exception(BVM_ERR_ILLEGAL_ARGUMENT_EXCEPTION, "timeout negative");

	monitor = get_monitor_for_obj(obj);

	/* monitor is not owned by the current thread?  Whoops */
	if ( (monitor == NULL) || (monitor->owner_thread != bvm_gl_thread_current) )
		bvm_throw_exception(BVM_ERR_ILLEGAL_MONITOR_STATE_EXCEPTION, NULL);

	bvm_thread_wait(obj, wait_time);

	NI_ReturnVoid();
}

/*
 * void notify()
 */
void java_lang_Object_notify(void *args) {

	bvm_obj_t *obj = NI_GetParameterAsObject(0);

	bvm_monitor_t *monitor = get_monitor_for_obj(obj);

	/* monitor is not owned by the current thread?  Whoops */
	if ( (monitor == NULL) || (monitor->owner_thread != bvm_gl_thread_current) )
		bvm_throw_exception(BVM_ERR_ILLEGAL_MONITOR_STATE_EXCEPTION, NULL);

	bvm_thread_notify(monitor, BVM_FALSE);

	NI_ReturnVoid();
}

/*
 * void notifyAll()
 */
void java_lang_Object_notifyAll(void *args) {

	bvm_obj_t *obj = NI_GetParameterAsObject(0);

	bvm_monitor_t *monitor = get_monitor_for_obj(obj);

	/* monitor is not owned by the current thread?  Whoops */
	if ( (monitor == NULL) || (monitor->owner_thread != bvm_gl_thread_current) )
		bvm_throw_exception(BVM_ERR_ILLEGAL_MONITOR_STATE_EXCEPTION, NULL);

	bvm_thread_notify(monitor, BVM_TRUE);

	NI_ReturnVoid();
}

/*
 * Object clone() throws CloneNotSupportedException
 */
void java_lang_Object_clone(void *args) {

	void *newobj;

	bvm_obj_t *obj = NI_GetParameterAsObject(0);

	/* it better implement cloneable or CloneNotSupportedException */
	if (!bvm_clazz_implements_interface((bvm_instance_clazz_t *) obj->clazz,
			(bvm_instance_clazz_t *) bvm_clazz_get_c(BVM_BOOTSTRAP_CLASSLOADER_OBJ, "java/lang/Cloneable")))
		bvm_throw_exception(BVM_ERR_CLONE_NOT_SUPPORTED_EXCEPTION, NULL);

	newobj = bvm_heap_clone(obj);

	NI_ReturnObject(newobj);
}

/***************************************************************************************************
 * java.lang.Class
 **************************************************************************************************/

/*
 * T newInstance() throws IllegalAccessException, InstantiationException
 */
void java_lang_Class_newInstance(void *args) {

	bvm_obj_t *obj;
	bvm_method_t *method;

	/* the stack element below the 'locals' of the current frame is a pointer to the
	 * class that called the current method */
	bvm_instance_clazz_t *calling_clazz = BVM_STACK_GetCallingClass();

	/* The first field of java.lang.Class is used to hold a ptr to the clazz of the 'real' object
	 * the Class object represents the class of.  This clazz is used to create a new object */
	bvm_instance_clazz_t *clazz = (bvm_instance_clazz_t *)
	                             ((bvm_class_obj_t *) NI_GetParameterAsObject(0))->refers_to_clazz;

	/* Check for illegal access. Note the IllegalAccessException (not IllegalAccessError).
	 * The IllegalAccessException is thrown by reflective construction. */
	if (!bvm_clazz_is_class_accessible((bvm_clazz_t *) calling_clazz, (bvm_clazz_t *) clazz))
		bvm_throw_exception(BVM_ERR_ILLEGAL_ACCESS_EXCEPTION, (char *) clazz->name->data);

	/* Cannot instantiate if class it is an interface or is abstract or is an array or is primitive. */
	if (BVM_CLAZZ_IsAbstract(clazz) || BVM_CLAZZ_IsInterface(clazz) || !BVM_CLAZZ_IsInstanceClazz(clazz))
		bvm_throw_exception(BVM_ERR_INSTANTIATION_EXCEPTION, NULL);

	/* Get the default (nullary) constructor */
	method = bvm_clazz_method_get(clazz, bvm_utfstring_pool_get_c("<init>", BVM_TRUE),
			                    bvm_utfstring_pool_get_c("()V", BVM_TRUE),
			                    BVM_METHOD_SEARCH_CLAZZ);

	/* No default constructor?  Bang out */
	if (method == NULL)
		bvm_throw_exception(BVM_ERR_INSTANTIATION_EXCEPTION, "No default constructor");

	/* constructor method cannot be abstract */
	if (BVM_METHOD_IsAbstract(method))
		bvm_throw_exception(BVM_ERR_ABSTRACT_METHOD_ERROR, (char *) method->name->data);

	/* constructor method cannot be protected and must be accessible */
	if ( (BVM_METHOD_IsProtected(method)) ||
		 (!bvm_clazz_is_member_accessible(BVM_METHOD_AccessFlags(method),calling_clazz,clazz)) )
		bvm_throw_exception(BVM_ERR_ILLEGAL_ACCESS_EXCEPTION, (char *) method->name->data);

	bvm_frame_pop();

	/* last, just to give 'doreturn' in the exec loop something to do as we return from this
	 * native method (it *always* pops a frame), we'll stuff a dummy 'noop' frame on the stack.
	 * The effect is that the exec loop will pop this noop frame and find
	 * either the <init> frame or the <clinit> frame.  The noop method 'BVM_METHOD_NOOP_RET' fools
	 * the stack into thinking it needs to pop a value after the <init> - newInstance has
	 * already pushed a value onto the stack*/
	bvm_frame_push(BVM_METHOD_NOOP_RET, bvm_gl_rx_sp, bvm_gl_rx_pc, bvm_gl_rx_pc, NULL);
	/* create the new object and push it on the stack */
	obj = bvm_object_alloc(clazz);
	NI_ReturnObject(obj);

	/* next, push a frame for the new object's <init> method on the stack - an init method
	 * cannot be synchronised so we push NULL as the sync object*/
	bvm_frame_push(method, bvm_gl_rx_sp, bvm_gl_rx_pc, bvm_gl_rx_pc, NULL);
	bvm_gl_rx_locals[0].ref_value = obj;

	/* next, if the class needs initialising, request it to do so.  That may push yet
	 * another frame onto the stack for <clinit> Note that we are only interested in
	 * initialising instance clazzes.  Primitive and array clazzes are initialized when
	 * they are created.
	 */
	if ( (!BVM_CLAZZ_IsInitialised(clazz)) && (BVM_CLAZZ_IsInstanceClazz(clazz)) )
		bvm_clazz_initialise( (bvm_instance_clazz_t *) clazz);

	bvm_frame_push(BVM_METHOD_NOOP, bvm_gl_rx_sp, bvm_gl_rx_pc, bvm_gl_rx_pc, NULL);
}

static bvm_class_obj_t *class_forname(bvm_string_obj_t *string_obj, bvm_classloader_obj_t *classloader_obj) {

	bvm_clazz_t *clazz;

	bvm_utfstring_t *utfstring;

    // TODO.  Likely should throw a class not found exception if the class name has any forward or backward
    //  slashes in it.  That would affect where it loads from.

	/* copy the char array to the utfstring - space allocated from the heap*/
	utfstring = bvm_str_unicode_to_utfstring(string_obj->chars->data, string_obj->offset.int_value, string_obj->length.int_value);
	BVM_MAKE_TRANSIENT_ROOT(utfstring);

	/* replace '.'s in class name with '/'s - turn a class name into an
	 * internalised class name. */
	bvm_str_replace_char( (char *) utfstring->data, utfstring->length, '.', '/');

	/* get the clazz reflectively */
	clazz = bvm_clazz_get_by_reflection(classloader_obj, utfstring);

	/* free up the temp memory we have used.  If get_clazz_by_reflection throws an exception
	 * and we don't get free it now, the temp string 'utfstring' will be freed in the next GC */
	bvm_heap_free(utfstring);

	/* now that we have loaded the clazz, return the Class object that represents it .. */
	return clazz->class_obj;
}

/*
 * static Class<?> forName(String name) throws ClassNotFoundException
 */
void java_lang_Class_forName(void *args) {

	/* first argument is the name of the class to load */
	bvm_string_obj_t *string_obj = NI_GetParameterAsObject(0);

	NI_ReturnObject(class_forname(string_obj, BVM_STACK_GetCallingClass()->classloader_obj));
}

/*
 * static Class<?> forName(String name, boolean initialize, ClassLoader classloader) throws ClassNotFoundException
 */
void java_lang_Class_forName2(void *args) {

	/* first argument is the name of the class to load */
	bvm_string_obj_t *string_obj = NI_GetParameterAsObject(0);

	/* second argument is ignored */

	/* third argument is the class loader object */
	bvm_classloader_obj_t *classloader_obj = NI_GetParameterAsObject(2);

	NI_ReturnObject(class_forname(string_obj, classloader_obj));
}

/*
 * Class<? super T> getSuperclass()
 */
void java_lang_Class_getSuperclass(void *args) {
	jobject class_obj = NI_GetParameterAsObject(0);
	NI_ReturnObject(NI_GetSuperclass(class_obj) );
}

/*
 * boolean isInterface()
 */
void java_lang_Class_isInterface(void *args) {

	/* object on stack is a Class object */
	bvm_class_obj_t *instance = NI_GetParameterAsObject(0);

	/* get the clazz it represents */
	bvm_clazz_t *clazz = instance->refers_to_clazz;

	NI_ReturnInt( BVM_CLAZZ_IsInterface(clazz) );
}

/*
 * Thread getInitThread()
 */
void java_lang_Class_getInitThread(void *args) {
    UNUSED(args);
	/* return the current thread object */
	NI_ReturnObject(bvm_gl_thread_current->thread_obj);
}

/*
 * boolean desiredAssertionStatus()
 */
void java_lang_Class_desiredAssertionStatus(void *args) {
    UNUSED(args);
	/* returns the current VM setting of assertions enabled */
	NI_ReturnBoolean(bvm_gl_assertions_enabled);
}


/*
 * ClassLoader getClassLoader()
 */
void java_lang_Class_getClassLoader0(void *args) {

	/* object on stack is a Class object */
	bvm_class_obj_t *instance = NI_GetParameterAsObject(0);

	/* returns the current VM setting of assertions enabled */
	NI_ReturnObject(instance->refers_to_clazz->classloader_obj);
}

/*
 * void __doclinit()
 */
void java_lang_Class_doclinit(void *args) {

	bvm_method_t *method;

	/* object on stack is a Class object */
	bvm_class_obj_t *class_obj = NI_GetParameterAsObject(0);

	/* get the clazz it represents */
	bvm_clazz_t *clazz = class_obj->refers_to_clazz;

	/* Get the static <clinit> method */
	method = bvm_clazz_method_get( (bvm_instance_clazz_t *) clazz, bvm_utfstring_pool_get_c("<clinit>", BVM_TRUE),
			                   bvm_utfstring_pool_get_c("()V", BVM_TRUE),
			                   BVM_METHOD_SEARCH_CLAZZ);

	if (method != NULL) {
		/* remove the __doclinit method from the stack */
		bvm_frame_pop();

		/* next, push a frame for the <clinit> method on the stack */
		bvm_frame_push(method, bvm_gl_rx_sp, bvm_gl_rx_pc, bvm_gl_rx_pc, NULL);

		/* next push a dummy frame on the stack so the interp loop is happy
		 * to pop something on return from __doinit.  This leaves <clinit> on top.*/
		bvm_frame_push(BVM_METHOD_NOOP, bvm_gl_rx_sp, bvm_gl_rx_pc, bvm_gl_rx_pc, NULL);
	}

	NI_ReturnVoid();
}

/*
 * int getState()
 */
void java_lang_Class_getState(void *args) {

	/* object on stack is a Class object */
	bvm_class_obj_t *class_obj = NI_GetParameterAsObject(0);

	/* get the clazz it represents */
	bvm_clazz_t *clazz = class_obj->refers_to_clazz;

	NI_ReturnInt( clazz->state );
}

/*
 * void setState(int newState)
 */
void java_lang_Class_setState(void *args) {

	/* object on stack is a Class object */
	bvm_class_obj_t *class_obj = NI_GetParameterAsObject(0);

	/* get the clazz it represents */
	bvm_clazz_t *clazz = class_obj->refers_to_clazz;

	/* second arg is the new clazz state */
	clazz->state = NI_GetParameterAsInt(1);

	NI_ReturnVoid();
}

/*
 * String getName()
 */
void java_lang_Class_getName(void *args) {

    // TODO: maybe intern string as object on first use then return that next time.

	bvm_string_obj_t *string_obj;

	/* object on the stack is a Class object */
	bvm_class_obj_t *class_obj = NI_GetParameterAsObject(0);

	/* get the clazz it represents */
	bvm_clazz_t *clazz = (bvm_clazz_t *) class_obj->refers_to_clazz;

	/* copy the clazz name to a temp bvm_utfstring_t so we can create a class name from
	 * the qualified class name (that is, replace all '/' with '.') */
	bvm_utfstring_t s;
	s.length = clazz->name->length;
	s.data = bvm_heap_alloc(s.length+1, BVM_ALLOC_TYPE_DATA); /* extra '1' is for null terminator */
	BVM_MAKE_TRANSIENT_ROOT(s.data);

	/* copy the data and the null terminator into the temp utfstring */
	memcpy(s.data, clazz->name->data, s.length+1);

	/* replace '/' with '.' */
	bvm_str_replace_char( (char *) s.data, s.length,'/','.');

	/* create a String object for the name */
	string_obj = bvm_string_create_from_utfstring(&s, BVM_FALSE);

	/* free the temp string data.  If create_String_utfstring throws and exception this temp
	 * string will be freed in the next GC */
	bvm_heap_free(s.data);

	NI_ReturnObject(string_obj);
}

/***************************************************************************************************
 * java.lang.ClassLoader
 **************************************************************************************************/

/*
 * static ClassLoader getSystemClassLoader()
 */
void java_lang_ClassLoader_getSystemClassLoader(void *args) {
    UNUSED(args);
	NI_ReturnObject(BVM_SYSTEM_CLASSLOADER_OBJ);
}

/***************************************************************************************************
 * java.lang.Runtime
 **************************************************************************************************/

/*
 * long freeMemory()
 */
void java_lang_Runtime_freeMemory(void *args) {
    UNUSED(args);
	bvm_int64_t l64;
	BVM_INT64_int32_to_int64(l64, bvm_gl_heap_free);
	NI_ReturnLong(l64);
}

/*
 * long totalMemory()
 */
void java_lang_Runtime_totalMemory(void *args) {
    UNUSED(args);
	bvm_int64_t l64;
	BVM_INT64_int32_to_int64(l64, bvm_gl_heap_size);
	NI_ReturnLong(l64);
}

/*
 * static void bvm_gc()
 */
void java_lang_Runtime_gc(void *args) {
    UNUSED(args);
	bvm_gc();
	NI_ReturnVoid();
}

/*
 * static void exit(int code)
 */
void java_lang_Runtime_exit0(void *args) {

#if BVM_RUNTIME_EXIT_ENABLE
	/* exit code is param 1, this is not a static function */
	BVM_VM_EXIT(NI_GetParameterAsInt(1), NULL);
#else
	bvm_throw_exception(BVM_ERR_UNSUPPORTED_OPERATION_EXCEPTION, "System.exit");
#endif

	NI_ReturnVoid();
}

/***************************************************************************************************
 * java.lang.System
 **************************************************************************************************/

/**
  Raw byte copy of all contents of an array object to another array object.  No checking at all.  None.
  This code assumes the types of both arrays are the same, and thus, the byte length of each
  element for both arrays is the same.
*/
static void bvm_raw_copy_array_contents(void *src_array_obj,
                                        void *dest_array_obj,
                                        int srcPos,
                                        int destPos,
                                        int length) {
    bvm_uint8_t typesize;
    bvm_uint8_t *srcPtr;
    bvm_uint8_t *destPtr;

    typesize = bvm_gl_type_array_info[((bvm_jarray_obj_t *)src_array_obj)->clazz->component_jtype].element_size;

    srcPtr = ((bvm_uint8_t *) ((bvm_jbyte_array_obj_t *)src_array_obj)->data) + (typesize * srcPos);
    destPtr = ((bvm_uint8_t *) ((bvm_jbyte_array_obj_t *)dest_array_obj)->data) + (typesize * destPos);

    memmove(destPtr, srcPtr, typesize * length);
}

/*
 * static void arraycopy(Object src, int srcPos, Object dest, int destPos, int length)
 */
void java_lang_System_arraycopy(void *args) {

	/* Gather info from the method args */
	bvm_jbyte_array_obj_t *src_array_obj  = NI_GetParameterAsObject(0);
	jint srcPos 		   = NI_GetParameterAsInt(1);
	bvm_jbyte_array_obj_t *dest_array_obj = NI_GetParameterAsObject(2);
	jint destPos 		   = NI_GetParameterAsInt(3);
	jint length            = NI_GetParameterAsInt(4);

	/*
	 * JVMS - "If dest is null, then a NullPointerException is thrown.
	 * If src is null, then a NullPointerException is thrown and the
	 * destination array is not modified."
	 */
	if ( (src_array_obj == NULL) || (dest_array_obj == NULL)) bvm_throw_exception(BVM_ERR_NULL_POINTER_EXCEPTION, NULL);

	/*
	 * JVMS - "Otherwise, if any of the following is true, an ArrayStoreException is thrown
	 * and the destination is not modified:
	 *
	 * * The src argument refers to an object that is not an array.
	 * * The dest argument refers to an object that is not an array.
	 * * The src argument and dest argument refer to arrays whose component types
	 *   are different primitive types.
	 * * The src argument refers to an array with a primitive component type and
	 *   the dest argument refers to an array with a reference component type.
	 * * The src argument refers to an array with a reference component type and
	 *   the dest argument refers to an array with a primitive component type."
	 */

	if ( !BVM_CLAZZ_IsArrayClazz(src_array_obj->clazz) ||
		 !BVM_CLAZZ_IsArrayClazz(dest_array_obj->clazz)) bvm_throw_exception(BVM_ERR_ARRAYSTORE_EXCEPTION, NULL);

	if  ( ( ( src_array_obj->clazz->component_jtype > BVM_T_ARRAY) || (dest_array_obj->clazz->component_jtype > BVM_T_ARRAY) ) &&
		    ( src_array_obj->clazz->component_jtype != dest_array_obj->clazz->component_jtype))
			bvm_throw_exception(BVM_ERR_ARRAYSTORE_EXCEPTION, NULL);

	/*
	 * JVMS - "Otherwise, if any of the following is true, an IndexOutOfBoundsException
	 * is thrown and the destination is not modified:
	 *
	 * * The srcPos argument is negative.
	 * * The destPos argument is negative.
	 * * The length argument is negative.
	 * * srcPos+length is greater than src.length, the length of the source array.
	 * * destPos+length is greater than dest.length, the length of the destination array."
	 */

	if ( (srcPos < 0)  ||
		 (destPos < 0) ||
		 (length < 0)  ||
		 ( (srcPos + length) > src_array_obj->length.int_value) ||
		 ( (destPos + length) > dest_array_obj->length.int_value) ) {
		bvm_throw_exception(BVM_ERR_ARRAY_INDEX_OUT_OF_BOUNDS_EXCEPTION, NULL);
	}

	/* Element by element compatibility checking will not have to be done if :
	 *
	 * 1) The elements are primitive
	 * 2) The src component type is assignment compatible with the dest component type.
	 *
	 * if ok ... we can do a bulk memory copy.  Note the use of memmove to deal with
	 * overlapping memory if we are copying to the same object.
	 */
	if ( (src_array_obj->clazz->component_jtype > BVM_T_ARRAY) ||
		(bvm_clazz_is_assignable_from((bvm_clazz_t *) src_array_obj->clazz->component_clazz,
		                     (bvm_clazz_t *) dest_array_obj->clazz->component_clazz))) {
        bvm_raw_copy_array_contents(src_array_obj, dest_array_obj, srcPos, destPos, length);
    }
	else {
		/* else array is reference type so loop through each element to be copied and
		 * check that each is compatible before copying it.  Yes, SLOW, but absolutely necessary. */
		int lc;

		for (lc = length; lc--;) {

			/* get the object from the src array to be copied into the dest array */
			bvm_obj_t *element_obj = ((bvm_instance_array_obj_t *)src_array_obj)->data[srcPos + lc];

			/* if the src and dest are not assignment compatible, throw an exception */
			if (element_obj != NULL) {
				if (!bvm_clazz_is_assignable_from((bvm_clazz_t *)element_obj->clazz,
										 (bvm_clazz_t *)dest_array_obj->clazz->component_clazz))
					bvm_throw_exception(BVM_ERR_ARRAYSTORE_EXCEPTION, NULL);
			}

			/* if all okay, copy src to dest */
			((bvm_instance_array_obj_t *)dest_array_obj)->data[destPos + lc] = ((bvm_instance_array_obj_t *)src_array_obj)->data[srcPos + lc];
		}
	}

	NI_ReturnVoid();
}

/*
 * static long currentTimeMillis()
 */
void java_lang_System_currentTimeMillis(void *args) {
    UNUSED(args);
	bvm_int64_t l64 = bvm_pd_system_time();
	NI_ReturnLong(l64);
}

/***************************************************************************************************
 * java.lang.String
 **************************************************************************************************/

/*
 * String intern()
 */
void java_lang_String_intern(void *args) {

	bvm_internstring_obj_t *internstring_obj;

	bvm_string_obj_t *string_obj = NI_GetParameterAsObject(0);

	/* create a new utfstring based on the String's contents.  The utfstring is
	 * allocated from the heap */
	bvm_utfstring_t *utfstr = bvm_str_unicode_to_utfstring(string_obj->chars->data, string_obj->offset.int_value, string_obj->length.int_value);
	BVM_MAKE_TRANSIENT_ROOT(utfstr);

	/* get the string from the pool.  If it is not there add it. */
	internstring_obj = bvm_internstring_pool_get(utfstr, BVM_TRUE);

	/* If the utfstring we get back it is not the same as the utfstring we passed in, then
	 * it must have already  been in the pool.  So lets clean up. The GC would get it
	 * later anyways, but better sooner I reckon */
	if (internstring_obj->utfstring != utfstr) bvm_heap_free(utfstr);

	NI_ReturnObject(internstring_obj);
}

/*
* public int hashCode();
 */
void java_lang_String_hashCode(void *args) {

	jint i, h, len;

	bvm_string_obj_t *string_obj = NI_GetParameterAsObject(0);
	bvm_jchar_array_obj_t *char_array_obj = string_obj->chars;

	h = 0;
	len = string_obj->length.int_value + string_obj->offset.int_value;

	for (i = string_obj->offset.int_value; i < len; i++) {
		h = 31 * h + char_array_obj->data[i];
	}

	NI_ReturnInt(h);
}

/*
 * public boolean equals(Object obj);
 */
void java_lang_String_equals(void *args) {

	/* this */
	bvm_string_obj_t *this_obj = NI_GetParameterAsObject(0);

	/* assume argument is a string .. saves a bit of typecasting later */
	bvm_string_obj_t *other_string_obj = NI_GetParameterAsObject(1);

	/* if it is the same object, all good */
	if (this_obj == other_string_obj) {
		NI_ReturnBoolean(BVM_TRUE);
		return;
	}

	/* if it is not actually a string, forget it */
	if (!bvm_clazz_is_instanceof( (bvm_obj_t *) other_string_obj, (bvm_clazz_t *) BVM_STRING_CLAZZ)) {
		NI_ReturnBoolean(BVM_FALSE);
		return;
	}

	/* a quick length check, they cannot be the equal if the have different lengths! */
	if (this_obj->length.int_value != other_string_obj->length.int_value) {
		NI_ReturnBoolean(BVM_FALSE);
		return;
	}

	/* .. and finally check equality of all the chars in their respective char arrays */
	{
		int i;

		jint toffset = this_obj->offset.int_value;
		jint ooffset = other_string_obj->offset.int_value;

		bvm_jchar_array_obj_t *tchar_array_obj = this_obj->chars;
		bvm_jchar_array_obj_t *ochar_array_obj = other_string_obj->chars;

		jint l = this_obj->length.int_value;

		for (i = 0; i < l; i++) {
			if (tchar_array_obj->data[i + toffset] != ochar_array_obj->data[i + ooffset]) {
				NI_ReturnBoolean(BVM_FALSE);
				return;
			}
		}

	}

	NI_ReturnBoolean(BVM_TRUE);
}

/*
 * public boolean startsWith(String prefix, int offset)
 */
void java_lang_String_startsWith(void *args) {

	bvm_jchar_array_obj_t *tchar_array_obj, *pchar_array_obj;
	jint tindex, pindex, pl;

	/* this */
	bvm_string_obj_t *this_obj = NI_GetParameterAsObject(0);

	/* this string prefix */
	bvm_string_obj_t *prefix_obj = NI_GetParameterAsObject(1);

	/* the offset to start at */
	jint offset = NI_GetParameterAsInt(2);

	if (prefix_obj == NULL)
		bvm_throw_exception(BVM_ERR_NULL_POINTER_EXCEPTION, NULL);

	tchar_array_obj = this_obj->chars;
	pchar_array_obj = prefix_obj->chars;

	tindex = offset + this_obj->offset.int_value;
	pindex = prefix_obj->offset.int_value;

	pl = prefix_obj->length.int_value;

	if ((offset < 0) || (offset > this_obj->length.int_value - pl)) {
		NI_ReturnBoolean(BVM_FALSE);
		return;
	}

	while (--pl >= 0) {
		if (tchar_array_obj->data[tindex++] != pchar_array_obj->data[pindex++]) {
			NI_ReturnBoolean(BVM_FALSE);
			return;
		}
	}

	NI_ReturnBoolean(BVM_TRUE);
}

/*
 * 	public int compareTo(String str)
 */
void java_lang_String_compareTo(void *args) {
    UNUSED(args);
}


/*
 * 	public int indexOf(int ch, int fromIndex)
 */
void java_lang_String_indexOf(void *args) {

	jint i;

	/* this */
	bvm_string_obj_t *this_obj = NI_GetParameterAsObject(0);

	/* this char  */
	jint ch = NI_GetParameterAsInt(1);

	/* the offset to start at */
	jint fromIndex = NI_GetParameterAsInt(2);

	bvm_jchar_array_obj_t *char_aray_obj = this_obj->chars;

	jint len = this_obj->length.int_value;
	jint offset = this_obj->offset.int_value;

	jint limit = offset + len;

	if (fromIndex < 0) {
		fromIndex = 0;
	} else if (fromIndex >= len) {
		NI_ReturnInt(-1);
		return;
	}

	for (i = fromIndex + offset; i < limit; i++) {
		if (char_aray_obj->data[i] == ch) {
			NI_ReturnInt(i - offset);
			return;
		}
	}
	NI_ReturnInt(-1);
}

/*
 * 	public int lastIndexOf(int ch, int fromIndex)
 */
void java_lang_String_lastIndexOf(void *args) {

	jint i;

	/* this */
	bvm_string_obj_t *this_obj = NI_GetParameterAsObject(0);

	/* this char  */
	jint ch = NI_GetParameterAsInt(1);

	/* the offset to start at */
	jint fromIndex = NI_GetParameterAsInt(2);

	bvm_jchar_array_obj_t *char_aray_obj = this_obj->chars;

	jint offset = this_obj->offset.int_value;
	jint length = this_obj->length.int_value;

	for (i = offset + ((fromIndex >= length) ? length - 1 : fromIndex); i >= offset; i--) {
		if (char_aray_obj->data[i + offset] == ch) {
			NI_ReturnInt(i - offset);
			return;
		}
	}

	NI_ReturnInt(-1);
}

/*
 * 	public char charAt(int i)
 */
void java_lang_String_charAt(void *args) {

	/* this */
	bvm_string_obj_t *this_obj = NI_GetParameterAsObject(0);

	/* index */
	jint i = NI_GetParameterAsInt(1);

	if ((i < this_obj->offset.int_value) || (i - this_obj->offset.int_value >= this_obj->length.int_value))
		bvm_throw_exception(BVM_ERR_ARRAY_INDEX_OUT_OF_BOUNDS_EXCEPTION, NULL);

	NI_ReturnInt(this_obj->chars->data[i]);
}

/***************************************************************************************************
 * java.lang.StringBuffer
 **************************************************************************************************/

/**
 * Unshare a StringBuilder object.  StringBuilder objects can share their char array with String
 * objects that are derived from it using StringBuilder.toString().  When this is the case the StringBuilder
 * is 'shared'.  Subsequent modifications to the StringBuilder object will 'unshare' it.  Unsharing is simply
 * making a new copy of its internal char array to work on, and setting the StringBuilder to unshared.  This function
 * does that.
 *
 * @param stringbuffer_obj a StringBuffer object
 */
static void unshare_buffer(bvm_stringbuffer_obj_t *stringbuffer_obj) {

	stringbuffer_obj->chars = (bvm_jchar_array_obj_t *) bvm_heap_clone(stringbuffer_obj->chars);
	stringbuffer_obj->is_shared.int_value = BVM_FALSE;
}

/**
 * Expand a StringBuffer object's buffer to a new requested size.  This function has no effect is the request new size
 * is not larger than the current size.  If a new size if effected, the buffer will be unshared as a result.
 *
 * Note the subtlety in the difference between set_buffer_length and this function.    This function set the size of the char
 * data buffer that the StrugBuffer object deals with.  It is not related to what the StringBuffer think is the length of the String
 * it building.
 *
 * The set_buffer_length function deals with the String length inside the buffer's char array.
 *
 * @param stringbuffer_obj a StringBuffer object
 * @param newlength the new length.
 */
static void expand_buffer(bvm_stringbuffer_obj_t *stringbuffer_obj, jint newlength) {

	if (newlength > stringbuffer_obj->chars->length.int_value) {

		/* we need to create a new array for the buffer and copy the old contents to it */

		/* create a new array for the buffer */
		bvm_jchar_array_obj_t *char_array_obj = (bvm_jchar_array_obj_t *) bvm_object_alloc_array_primitive(newlength, BVM_T_CHAR);

		/* copy the old contents into it */
		memcpy(char_array_obj->data, stringbuffer_obj->chars->data, stringbuffer_obj->length.int_value * sizeof(bvm_uint16_t));
		stringbuffer_obj->chars = char_array_obj;

		/* mark as unshared now .. */
		stringbuffer_obj->is_shared.int_value = BVM_FALSE;
	}
}

/**
 *  Set the length of the char sequence in a StringBuffer object.  Refer the runtime classes docs for
 *  StringBuffer.setLength().
 *
 * @param stringbuffer_obj a StringBuffer object
 * @param newlength the new length
 */
static void set_buffer_length(bvm_stringbuffer_obj_t *stringbuffer_obj, jint newlength) {

	if (newlength > stringbuffer_obj->chars->length.int_value) {
		/* expand the buffer, new chars will be null */
		expand_buffer(stringbuffer_obj, newlength);
	} else {

		/* if the buffer is shared, make a copy before going on */
		if (stringbuffer_obj->is_shared.int_value) unshare_buffer(stringbuffer_obj);

		/* no growth of buffer required.  When shrinking we'll just set the length, when
		 * growing we now know that the new length is not larger than the buffer size so
		 * we'll set the new chars in range to null and then set the length */

		if (newlength > stringbuffer_obj->length.int_value) {
			memset(&stringbuffer_obj->chars->data[stringbuffer_obj->length.int_value], 0, (newlength - stringbuffer_obj->length.int_value) * sizeof(bvm_uint16_t));
		}
	}

	stringbuffer_obj->length.int_value = newlength;
}

/**
 * Given a StringBuffer object, ensure its capacity for the given min_capacity.  If the current capacity is not
 * enough the string buffer will be expanded.  The size of the expanded capacity is as-per Java SDK and
 * if the larger of the specified min_capacity or the (current capacity*2 + 2).
 *
 * @param stringbuffer_obj
 * @param min_capacity
 */
static void ensure_buffer_capacity(bvm_stringbuffer_obj_t *stringbuffer_obj, jint min_capacity) {

	jint curr_capacity = stringbuffer_obj->chars->length.int_value;

	if ( min_capacity > curr_capacity) {

		jint larger_capacity = (curr_capacity * 2) + 2;  /* as per the Java JDK */

		/* expand the buffer to the larger of the specified min capacity or a new calced larger one */
		expand_buffer(stringbuffer_obj, larger_capacity > min_capacity ? larger_capacity : min_capacity);
	}
}

/*
 * void ensureCapacity(int minimumCapacity)
 */
void java_lang_StringBuffer_ensureCapacity(void *args) {

	/* arg zero is the string buffer */
	bvm_stringbuffer_obj_t *stringbuffer_obj = NI_GetParameterAsObject(0);

	/* arg 1 is the minimum required capacity */
	jint min_capacity =  NI_GetParameterAsInt(1);

	if (min_capacity > 0)
		ensure_buffer_capacity(stringbuffer_obj, min_capacity);

	NI_ReturnVoid();
}

static void stringbuffer_insert(bvm_stringbuffer_obj_t *stringbuffer_obj,
								jint position,
								jchar *src_chars,
								jint src_pos,
								jint length) {

	if ( (position < 0) || (position > stringbuffer_obj->length.int_value))
		bvm_throw_exception(BVM_ERR_STRING_INDEX_OUT_OF_BOUNDS_EXCEPTION, NULL);

	/* make sure the buffer can hold the new contents */
	ensure_buffer_capacity(stringbuffer_obj, stringbuffer_obj->length.int_value + length);

	/* if the buffer is shared, unshare it before modifying it */
	if (stringbuffer_obj->is_shared.int_value) unshare_buffer(stringbuffer_obj);

	/* if the requested insert position is before current end of the buffer shift stuff
	 * up to make room for it, otherwise, just stick it on the end */
	if (position < stringbuffer_obj->length.int_value) {

		/* how many chars do we need to shuffle up? */
		jint shuffle_cnt = stringbuffer_obj->chars->length.int_value-(position+length);

		/* shuffle the data along.  Using memmove in case of memory overlap */
		memmove(&stringbuffer_obj->chars->data[position+length], &stringbuffer_obj->chars->data[position], shuffle_cnt * sizeof(jchar));
	}

	/* do the insert / append */
	memcpy(&stringbuffer_obj->chars->data[position], src_chars+src_pos, length * sizeof(jchar));

	/* set the new length of the buffer to include the size of the insert */
	stringbuffer_obj->length.int_value += length;
}

/*
 * StringBuffer insert0(int position, char chars[], int offset, int length)
 */
void java_lang_StringBuffer_insert0(void *args) {

	/* arg zero is the string buffer */
	bvm_stringbuffer_obj_t *stringbuffer_obj = NI_GetParameterAsObject(0);

	/* arg 1 is the position in the buffer to affect the insert */
	jint position =  NI_GetParameterAsInt(1);

	/* arg 2 is the src jchar array */
	bvm_jchar_array_obj_t *src_array_obj = NI_GetParameterAsObject(2);

	/* arg 3 is the copy pos of the src array */
	jint src_pos =  NI_GetParameterAsInt(3);

	/* arg 4 is the length of the copy*/
	jint length =  NI_GetParameterAsInt(4);

	stringbuffer_insert(stringbuffer_obj, position, src_array_obj->data, src_pos, length);

	NI_ReturnObject(stringbuffer_obj);
}

/*
 * void setLength(int newlength)
 */
void java_lang_StringBuffer_setLength(void *args) {

	/* arg zero is the string buffer */
	bvm_stringbuffer_obj_t *stringbuffer_obj = NI_GetParameterAsObject(0);

	/* arg 1 is the new length */
	jint length =  NI_GetParameterAsInt(1);

	if (length < 0)
		bvm_throw_exception(BVM_ERR_INDEX_OUT_OF_BOUNDS_EXCEPTION, NULL);

	if (length != stringbuffer_obj->length.int_value) {
		set_buffer_length(stringbuffer_obj, length);
	}

	NI_ReturnVoid();
}

/*
 * void append(String s)
 */
void java_lang_StringBuffer_append(void *args) {

	/* arg zero is the string buffer */
	bvm_stringbuffer_obj_t *stringbuffer_obj = NI_GetParameterAsObject(0);

	/* arg 1 is the string we are appending */
	bvm_string_obj_t *string_obj = NI_GetParameterAsObject(1);

	if (string_obj != NULL) {

		/* 'insert' at the end */
		stringbuffer_insert(stringbuffer_obj, stringbuffer_obj->length.int_value, string_obj->chars->data, string_obj->offset.int_value, string_obj->length.int_value);

	} else {

	    jchar data[4] = {(jchar) 'n', (jchar) 'u', (jchar) 'l', (jchar) 'l'};

		/* 'insert' "null" at the end - '4' is the length of 'data'. */
		stringbuffer_insert(stringbuffer_obj, stringbuffer_obj->length.int_value, data, 0, 4);
	}

	NI_ReturnObject(stringbuffer_obj);
}


/***************************************************************************************************
 * java.lang.Integer
 **************************************************************************************************/

/*
 * static String toString(int i)
 */
void java_lang_Integer_toString(void *args) {

	bvm_utfstring_t str;
	bvm_string_obj_t *string_obj;

	char buf[15];
	char *bufc = "-2147483648";

	jint value = NI_GetParameterAsInt(0);

	if (value == BVM_MIN_INT) {
		str = bvm_str_wrap_utfstring(bufc);
	} else {
		bvm_str_itoa(value, buf, 10);
		str = bvm_str_wrap_utfstring(buf);
	}

	string_obj = bvm_string_create_from_utfstring(&str, BVM_FALSE);

	NI_ReturnObject(string_obj);
}

/***************************************************************************************************
 * java.lang.Float
 **************************************************************************************************/


#if BVM_FLOAT_ENABLE

/**
 * Converts a given double to a String.  Not an exact replication of the Java toString for float
 * and double, but a reasonable approximation.  All exceptional cases (like pos/neg infinity and
 * pos/neg zero) are handled, but this function produces more decimal places than
 * Java.
 *
 * @param d a given double
 */
static bvm_string_obj_t *double_tostring(jdouble d) {

	bvm_utfstring_t temp_utfstring;

	char buf[64];

	int i;

	jlong intv = * (jlong *) &d;

	if (d == 0.0) {
		if ( (intv & BVM_MIN_LONG) == BVM_MIN_LONG)
			sprintf(buf, "-0.0");
		else
			sprintf(buf, "0.0");
	}
    else if (bvm_fp_is_dnan(intv)) {
        sprintf(buf, "NaN");
    } else if (bvm_fp_is_dninf(intv)) {
        sprintf(buf, "-Infinity");
    } else if (bvm_fp_is_dpinf(intv)) {
        sprintf(buf, "Infinity");
    } else {

    	/* "If m is greater than or equal to 10^-3 but less than 10^7, then it is represented as the
    	 * integer part of m, in decimal form with no leading zeroes, followed by '.' ('\u002E'), followed
    	 * by one or more decimal digits representing the fractional part of m". */
    	if ( (fabs(d) >= /*0.001*/ 10.0e-3) && (fabs(d) < /*10000000.0*/ 10.0e7) ) {
    		sprintf(buf, "%f", d);
    	} else {
			char *c, *e;

    		sprintf(buf, "%E", d);

			c = strchr(buf, 'E');

			/* if the end is "E+000" remove it by truncation */
			if (strncmp(c,"E+000", 5) == 0) {
				*c = '\0';
			} else {

				/* remove the '+' */
				if ( (c = strchr(buf, '+')) != NULL)

					while (*c != '\0') {
						*c = *(c+1);
						c++;
					}

				/* find the first number after the exponent */
				e = strchr(buf, 'E')+1;
				if (*e == '-') e++;

				/* remove any leading zeros from the exponent */
				while (*e == '0') {
					c = e;
					while  (*c != '\0') {
						*c = *(c+1);
						c++;
					}
				}
			}
    	}

		i = strlen(buf);

		/* Default ISO C sprintf float conversion is to 6 digits, so we'll remove empty zeros
		 * from the end of the buf - but we'll be cautious and leave a zero after
		 * the decimal so (say) 5.500000 becomes 5.5, and 5.000000 become 5.0*/
		while (buf[--i] == '0') {
			if (i != 0)
				if (buf[i-1] != '.') buf[i]=0;
		}
    }

	/* wrap the new buffer in utfstring */
	temp_utfstring = bvm_str_wrap_utfstring(buf);

	/* create a String object for our float number */
    return bvm_string_create_from_utfstring(&temp_utfstring, BVM_FALSE);
}

/*
 * static String toString()
 */
void java_lang_Float_toString(void *args) {
    jfloat f  = NI_GetParameterAsFloat(0);
	NI_ReturnObject(double_tostring((jdouble) f));
}

/*
 * static native int floatToIntBits(float value)
 */
void java_lang_Float_floatToIntBits(void *args) {
	NI_ReturnInt(NI_GetParameterAsInt(0));
}

#endif


/***************************************************************************************************
 * java.lang.Double
 **************************************************************************************************/


#if BVM_FLOAT_ENABLE

/*
 * static String toString()
 */
void java_lang_Double_toString(void *args) {
    bvm_double_t d = NI_GetParameterAsDouble(0);
	NI_ReturnObject(double_tostring(d));
}

/*
 * static native int doubleToLongBits(float value)
 */
void java_lang_Double_doubleToLongBits(void *args) {
	jlong l = NI_GetParameterAsLong(0);
	NI_ReturnLong(l);
}

/*
 * static native double sqrt(double value)
 */
void java_lang_Math_sqrt(void *args) {
	jdouble d = NI_GetParameterAsDouble(0);
	NI_ReturnDouble(sqrt(d));
}

/*
 * static native double floor(double value)
 */
void java_lang_Math_floor(void *args) {
	jdouble d = NI_GetParameterAsDouble(0);
	NI_ReturnDouble(floor(d));
}

/*
 * static native double log(double value)
 */
void java_lang_Math_log(void *args) {
	jdouble d = NI_GetParameterAsDouble(0);
	NI_ReturnDouble(log(d));
}

/*
 * static native double log10(double value)
 */
void java_lang_Math_log10(void *args) {
	jdouble d = NI_GetParameterAsDouble(0);
	NI_ReturnDouble(log10(d));
}

/*
 * static native double ceil(double value)
 */
void java_lang_Math_ceil(void *args) {
	jdouble d = NI_GetParameterAsDouble(0);
	NI_ReturnDouble(ceil(d));
}

/*
 * static native double exp(double value)
 */
void java_lang_Math_exp(void *args) {
	jdouble d = NI_GetParameterAsDouble(0);
	NI_ReturnDouble(exp(d));
}

/*
 * static native double IEEEremainder(double value)
 */
void java_lang_Math_IEEEremainder(void *args) {
	jdouble d1 = NI_GetParameterAsDouble(0);
	jdouble d2 = NI_GetParameterAsDouble(1);
	NI_ReturnDouble(fmod(d1,d2));
}

#endif

/***************************************************************************************************
 * java.io.Console
 **************************************************************************************************/

/*
 * void write(int b) throws IOException
 */
void java_io_Console_write(void *args) {

#if BVM_CONSOLE_ENABLE
	jbyte data = NI_GetParameterAsByte(1);
	bvm_pd_console_out("%c",data);
#endif

	NI_ReturnVoid();
}

/*
 * void println0(String s)
 */
void java_io_Console_println0(void *args) {
#if BVM_CONSOLE_ENABLE

	/* String object is second param */
	bvm_string_obj_t *string_obj = NI_GetParameterAsObject(1);

	/* no string?  Null pointer Exception */
	if (string_obj == NULL) bvm_throw_exception(BVM_ERR_NULL_POINTER_EXCEPTION, NULL);

	bvm_string_print_to_console(string_obj, "%s\n");
#endif
	NI_ReturnVoid();
}

/*
 * void print0(String s)
 */
void java_io_Console_print0(void *args) {
#if BVM_CONSOLE_ENABLE
	/* String object is second param */
	bvm_string_obj_t *string_obj = NI_GetParameterAsObject(1);

	/* no string?  Null pointer Exception */
	if (string_obj == NULL) bvm_throw_exception(BVM_ERR_NULL_POINTER_EXCEPTION, NULL);

	bvm_string_print_to_console(string_obj, NULL);
#endif
	NI_ReturnVoid();
}

/***************************************************************************************************
 * java.lang.Thread
 **************************************************************************************************/

/*
 * static int nextThreadId()
 */
void java_lang_Thread_nextThreadId(void *args) {
    UNUSED(args);
	NI_ReturnInt(bvm_thread_get_next_id());
}

/*
 * static native Thread currentThread0()
 */
void java_lang_Thread_currentThread0(void *args) {
    UNUSED(args);
	NI_ReturnObject(bvm_gl_thread_current->thread_obj);
}

/*
 * synchronized void start()
 */
void java_lang_Thread_start(void *args) {
	bvm_vmthread_t *vmthread;
	bvm_thread_obj_t *thread_obj = NI_GetParameterAsObject(0);

	vmthread = thread_obj->vmthread;

	/* if the Java thread does not have a vmthread yet, create it */
	if (vmthread == NULL)
		vmthread = bvm_thread_create_vmthread(thread_obj);

	bvm_thread_start(vmthread, BVM_TRUE);

	NI_ReturnVoid();
}

/*
 * void setPriority0(int newPriority)
 */
void java_lang_Thread_setPriority0(void *args) {

	bvm_vmthread_t *vmthread;
	bvm_thread_obj_t *thread_obj = NI_GetParameterAsObject(0);
	jint priority = NI_GetParameterAsInt(1);

	thread_obj->priority.int_value = priority;

	vmthread = thread_obj->vmthread;

	/* if the Java thread does not have a vmthread yet, create it */
	if (vmthread == NULL)
		vmthread = bvm_thread_create_vmthread(thread_obj);

	bvm_thread_calc_timeslice(vmthread);

	NI_ReturnVoid();
}

/*
 * boolean isAlive()
 */
void java_lang_Thread_isAlive(void *args) {

	bvm_thread_obj_t *thread_obj = NI_GetParameterAsObject(0);

	NI_ReturnInt(bvm_thread_is_alive(thread_obj->vmthread));
}

/*
 * boolean isInterrupted(boolean clearInterrupted)
 */
void java_lang_Thread_isInterrupted(void *args) {

	bvm_vmthread_t *vmthread;
	bvm_thread_obj_t *thread_obj = NI_GetParameterAsObject(0);

	jboolean clearit = NI_GetParameterAsInt(1);

	vmthread = thread_obj->vmthread;

	if (vmthread == NULL)
		vmthread = bvm_thread_create_vmthread(thread_obj);

	NI_ReturnInt(vmthread->is_interrupted);

	if (clearit) vmthread->is_interrupted = BVM_FALSE;
}

/*
 * static void yield()
 */
void java_lang_Thread_yield(void *args) {
    UNUSED(args);
	/* setting to zero will cause a thread switch before the next bytecode is executed */
	bvm_gl_thread_timeslice_counter = 0;
	NI_ReturnVoid();
}

/*
 * static void sleep(long millis) throws InterruptedException
 */
void java_lang_Thread_sleep(void *args) {

    bvm_int64_t sleep_time = BVM_INT64_from_cells((bvm_cell_t*)args);

	if (!BVM_INT64_zero_gt(sleep_time))
		bvm_throw_exception(BVM_ERR_ILLEGAL_ARGUMENT_EXCEPTION, "timeout negative");

	bvm_thread_sleep(sleep_time);
	NI_ReturnVoid();
}

/*
 * int getState()
 */
void java_lang_Thread_getState(void *args) {

	bvm_vmthread_t *vmthread;
	bvm_thread_obj_t *thread_obj = NI_GetParameterAsObject(0);

	vmthread = thread_obj->vmthread;

	NI_ReturnInt( (vmthread == NULL) ? BVM_THREAD_STATUS_NEW : vmthread->status );
}

/*
 * void interrupt0()
 */
void java_lang_Thread_interrupt0(void *args) {
	bvm_thread_obj_t *thread_obj = NI_GetParameterAsObject(0);
	bvm_thread_interrupt(thread_obj->vmthread);
	NI_ReturnVoid();
}

/***************************************************************************************************
 * java.lang.ref.WeakReference
 **************************************************************************************************/

/*
 * public native void makeweak();
 *
 */
void java_lang_ref_WeakReference_makeweak(void *args) {

	/* this */
	bvm_weak_reference_obj_t *weak_reference_obj = NI_GetParameterAsObject(0);

	/* change the GC alloc type from BVM_ALLOC_TYPE_OBJECT, to BVM_ALLOC_TYPE_WEAK_REFERENCE */
	bvm_heap_set_alloc_type(weak_reference_obj, BVM_ALLOC_TYPE_WEAK_REFERENCE);

	NI_ReturnVoid();
}


/***************************************************************************************************
 * java.nio.ByteOrder
 **************************************************************************************************/

/*
 * static boolean isLittleEndian()
 */
void java_nio_ByteOrder_isLittleEndian(void *args) {

    UNUSED(args);

#if (!BVM_BIG_ENDIAN_ENABLE)
		NI_ReturnInt(BVM_TRUE);
#else
		NI_ReturnInt(BVM_FALSE);
#endif
}

/***************************************************************************************************
 * babe.io.File
 **************************************************************************************************/

typedef struct fileobjstruct {
	BVM_COMMON_OBJ_INFO
	jint native_handle;
    jint flags;
    jint flush_pending;
} file_obj_t;

/*
 * flush a file if it needs it */
static void flushfile(file_obj_t *file_obj) {

	if (file_obj->flush_pending) {
		if (bvm_file_flush(file_obj->native_handle) != 0)
			bvm_throw_exception(BVM_ERR_IO_EXCEPTION, NULL);
		file_obj->flush_pending = BVM_FALSE;
	}
}

/*
 * static int open0(String filename, int flags)
 */
void babe_io_File_open0(void *args) {

	BVM_FILE file;
	char *name;

	/* the String name */
	bvm_string_obj_t *string_obj = NI_GetParameterAsObject(0);

	/* The open flags mode - note that no sanity checking is performed. */
	jint flags = NI_GetParameterAsInt(1);

	if (string_obj == NULL)
		bvm_throw_exception(BVM_ERR_NULL_POINTER_EXCEPTION, NULL);

	/* convert the name to a c string (comes from the heap, so we'll clean it up later */
	name = bvm_string_to_cstring(string_obj);

	/* make the name transient just in case a GC is caused */
	BVM_MAKE_TRANSIENT_ROOT(name);

	/* perform the file open.  We'll get NULL back it it does not work */
	file = bvm_file_open(bvm_gl_filetype_md, name, flags);

	/* no can do?  whoops */
	if (file == BVM_ERR)
		bvm_throw_exception(BVM_ERR_FILE_NOT_FOUND_EXCEPTION, NULL);

	bvm_heap_free(name);

	/* void* handle is returned as ref.  VM will interpret it as int */
	NI_ReturnInt(file);
}

/*
 * void close() throws IOException
 */
void babe_io_File_close(void *args) {

	/* the File object */
	file_obj_t *file_obj = NI_GetParameterAsObject(0);

	/* if the file is not closed, close it */
	if (file_obj->native_handle != -1) {

		/* flush if required */
		flushfile(file_obj);

		/* native file close will return -1 if any issues */
		if (bvm_file_close(file_obj->native_handle) < 0)
			bvm_throw_exception(BVM_ERR_IO_EXCEPTION, NULL);

		/* set handle to -1 to indicate the file is now closed */
		file_obj->native_handle = -1;
	}

	NI_ReturnVoid();
}

/*
 * int read(byte[] dst, int offset, int count) throws IOException
 */
void babe_io_File_read(void *args) {

	jint ret;

	/* the File object */
	file_obj_t *file_obj = NI_GetParameterAsObject(0);
	BVM_FILE file = file_obj->native_handle;

	/* the dest array and offset/count */
	bvm_jbyte_array_obj_t *dst_array_obj = NI_GetParameterAsObject(1);
	jint offset = NI_GetParameterAsInt(2);
	jint count  = NI_GetParameterAsInt(3);

	if (dst_array_obj == NULL)
		bvm_throw_exception(BVM_ERR_NULL_POINTER_EXCEPTION, NULL);

	if ( (count < 0) || (offset < 0) || (count > dst_array_obj->length.int_value - offset) )
		bvm_throw_exception(BVM_ERR_INDEX_OUT_OF_BOUNDS_EXCEPTION, NULL);

	/* the file is closed, */
	if (file_obj->native_handle == -1)
		bvm_throw_exception(BVM_ERR_IO_EXCEPTION, NULL);

	/* if file is write only */
	if (file_obj->flags & BVM_FILE_O_WRONLY)
		bvm_throw_exception(BVM_ERR_IO_EXCEPTION, NULL);

	/* flush if required */
	flushfile(file_obj);

	/* Read zero bytes ?  Stupid.  Exit */
	if (count == 0) {
		NI_ReturnInt(0);
		return;
	}

	/* native returns 0 if EOF, negative if error */
	ret = bvm_file_read(&dst_array_obj->data[offset], count, file);

	if (ret < 0)
		bvm_throw_exception(BVM_ERR_IO_EXCEPTION, NULL);

	/* return -1 to java if eof, otherwise return nr bytes read */
	NI_ReturnInt( ret == 0 ? -1 : ret);
}

/*
 * void write(byte[] dst, int offset, int count) throws IOException
 */
void babe_io_File_write(void *args) {

	/* the File object */
	file_obj_t *file_obj = NI_GetParameterAsObject(0);
	BVM_FILE file = file_obj->native_handle;

	/* the source array and offset/count */
	bvm_jbyte_array_obj_t *src_array_obj = NI_GetParameterAsObject(1);
	jint offset = NI_GetParameterAsInt(2);
	jint count  = NI_GetParameterAsInt(3);


	if (src_array_obj == NULL)
		bvm_throw_exception(BVM_ERR_NULL_POINTER_EXCEPTION, NULL);

	if ( (count < 0) || (offset < 0) || (count > src_array_obj->length.int_value - offset) )
		bvm_throw_exception(BVM_ERR_INDEX_OUT_OF_BOUNDS_EXCEPTION, NULL);

	/* the file is closed, */
	if (file_obj->native_handle == -1)
		bvm_throw_exception(BVM_ERR_IO_EXCEPTION, NULL);

	/* if file is read only */
	if ( (file_obj->flags & BVM_FILE_O_RDONLY) == BVM_FILE_O_RDONLY)
		bvm_throw_exception(BVM_ERR_IO_EXCEPTION, NULL);

	/* Write zero bytes ?  Stupid.  Exit */
	if (count == 0) return;

	/* native call returns less than count if error*/
	if ( (jint) bvm_file_write(&src_array_obj->data[offset],count,file) != count)
		bvm_throw_exception(BVM_ERR_IO_EXCEPTION, NULL);

	/* done a write, make sure we flush if required to */
	file_obj->flush_pending = BVM_TRUE;

	NI_ReturnVoid();
}

/*
 * void setPosition(int offset, int origin) throws IOException
 */
void babe_io_File_setPosition(void *args) {

	jint ret;

	/* the File object */
	file_obj_t *file_obj = NI_GetParameterAsObject(0);
	BVM_FILE file = file_obj->native_handle;

	jint offset = NI_GetParameterAsInt(1);
	jint origin = NI_GetParameterAsInt(2);

	/* the file is closed, */
	if (file_obj->native_handle == -1)
		bvm_throw_exception(BVM_ERR_IO_EXCEPTION, NULL);

	/* force a flush if required */
	flushfile(file_obj);

	/* returns new position, or -1 is error */
	ret = bvm_file_setpos(file, offset, origin);

	if (ret == -1)
		bvm_throw_exception(BVM_ERR_IO_EXCEPTION, NULL);

	NI_ReturnVoid();
}

/*
 * int getPosition() throws IOException
 */
void babe_io_File_getPosition(void *args) {

	jint ret;

	/* the File object */
	file_obj_t *file_obj = NI_GetParameterAsObject(0);
	BVM_FILE file = file_obj->native_handle;

	/* the file is closed, */
	if (file_obj->native_handle == -1)
		bvm_throw_exception(BVM_ERR_IO_EXCEPTION, NULL);

	/* force a flush if required */
	flushfile(file_obj);

	/* returns current position, or -1 is error */
	ret = bvm_file_getpos(file);

	if (ret == -1)
		bvm_throw_exception(BVM_ERR_IO_EXCEPTION, NULL);

	NI_ReturnInt(ret);
}

/*
 * int sizeOf() throws IOException
 */
void babe_io_File_sizeOf(void *args) {

	jint ret;

	/* the File object */
	file_obj_t *file_obj = NI_GetParameterAsObject(0);
	BVM_FILE file = file_obj->native_handle;

	/* the file is closed, */
	if (file_obj->native_handle == -1)
		bvm_throw_exception(BVM_ERR_IO_EXCEPTION, NULL);

	/* force a flush if required */
	flushfile(file_obj);

	/* returns size or -1 if error */
	ret = bvm_file_sizeof(file);

	if (ret == -1)
		bvm_throw_exception(BVM_ERR_IO_EXCEPTION, NULL);

	NI_ReturnInt(ret);
}

/*
 * static void rename(String oldname, String newname) throws IOException
 */
void babe_io_File_rename(void *args) {

	char *oldname;
	char *newname;

	/* the String file name */
	bvm_string_obj_t *old_string_obj = NI_GetParameterAsObject(0);
	bvm_string_obj_t *new_string_obj = NI_GetParameterAsObject(1);

	if (old_string_obj == NULL)
		bvm_throw_exception(BVM_ERR_NULL_POINTER_EXCEPTION, NULL);

	if (new_string_obj == NULL)
		bvm_throw_exception(BVM_ERR_NULL_POINTER_EXCEPTION, NULL);

	/* convert the old name to a c string */
	oldname = bvm_string_to_cstring(old_string_obj);
	BVM_MAKE_TRANSIENT_ROOT(oldname);

	/* convert the new name to a c string */
	newname = bvm_string_to_cstring(new_string_obj);
	BVM_MAKE_TRANSIENT_ROOT(newname);

	/* returns -1 if error */
	if (bvm_file_rename(oldname, newname) == -1)
		bvm_throw_exception(BVM_ERR_IO_EXCEPTION, NULL);

	bvm_heap_free(oldname);
	bvm_heap_free(newname);

	NI_ReturnVoid();
}

/*
 * static void remove(String filename) throws IOException
 */
void babe_io_File_remove(void *args) {

	char *name;

	/* the String file name */
	bvm_string_obj_t *string_obj = NI_GetParameterAsObject(0);

	if (string_obj == NULL)
		bvm_throw_exception(BVM_ERR_NULL_POINTER_EXCEPTION, NULL);

	/* convert the name to a c string */
	name = bvm_string_to_cstring(string_obj);
	BVM_MAKE_TRANSIENT_ROOT(name);

	/* returns -1 if error */
	if (bvm_file_remove(name) == -1)
		bvm_throw_exception(BVM_ERR_IO_EXCEPTION, NULL);

	bvm_heap_free(name);

	NI_ReturnVoid();
}

/*
 * static boolean exists(String filename) throws IOException
 */
void babe_io_File_exists(void *args) {

	jint ret;
	char *name;

	/* the String file name */
	bvm_string_obj_t *string_obj = NI_GetParameterAsObject(0);

	if (string_obj == NULL)
		bvm_throw_exception(BVM_ERR_NULL_POINTER_EXCEPTION, NULL);

	/* convert the name to a c string */
	name = bvm_string_to_cstring(string_obj);
	BVM_MAKE_TRANSIENT_ROOT(name);

	/* returns -1 if error */
	ret = bvm_file_exists(name);

	if ( ret == -1)
		bvm_throw_exception(BVM_ERR_IO_EXCEPTION, NULL);

	bvm_heap_free(name);

	NI_ReturnBoolean(ret);
}

/*
 * void truncate(int newlength) throws IOException
 */
void babe_io_File_truncate(void *args) {

	/* the File object */
	file_obj_t *file_obj = NI_GetParameterAsObject(0);
	BVM_FILE file = file_obj->native_handle;

	jint newlen = NI_GetParameterAsInt(1);

	/* the file is closed, */
	if (file_obj->native_handle == -1)
		bvm_throw_exception(BVM_ERR_IO_EXCEPTION, NULL);

	if (newlen < 0)
		bvm_throw_exception(BVM_ERR_ILLEGAL_ARGUMENT_EXCEPTION, NULL);

	/* read only?  Bang! */
	if ( (file_obj->flags & BVM_FILE_O_RDONLY) == BVM_FILE_O_RDONLY)
		bvm_throw_exception(BVM_ERR_IO_EXCEPTION, NULL);

	/* force a flush if required */
	flushfile(file_obj);

	if (newlen > (jint) bvm_file_sizeof(file))
		bvm_throw_exception(BVM_ERR_IO_EXCEPTION, NULL);

	/* returns -1 if error */
	if (bvm_file_truncate(file, newlen) == -1)
		bvm_throw_exception(BVM_ERR_IO_EXCEPTION, NULL);

	NI_ReturnVoid();
}


/***************************************************************************************************
 * java.lang.Throwable
 **************************************************************************************************/

/*
 * public synchronized native void fillInStackTrace();
 *
 */
void java_lang_Throwable_fillInStackTrace(void *args) {

#if BVM_STACKTRACE_ENABLE
	/* this */
	bvm_throwable_obj_t *throwable_obj = NI_GetParameterAsObject(0);

	/* populate the stack trace info */
	bvm_stacktrace_populate_backtrace(throwable_obj);
#endif

	NI_ReturnVoid();
}

/*
 * private native StackTraceElement[] getStackTrace0();
 */
void java_lang_Throwable_getStackTrace0(void *args) {

	/* this */
	bvm_throwable_obj_t *throwable_obj = NI_GetParameterAsObject(0);

#if BVM_STACKTRACE_ENABLE
	/* create the stack trace element array */
	bvm_stacktrace_populate(throwable_obj);
#endif

	NI_ReturnObject(throwable_obj->stack_trace_elements);
}

/***************************************************************************************************
 * java.security.SecurityManager
 **************************************************************************************************/

typedef struct {
	bvm_instance_array_obj_t *array;
	bvm_bool_t privileged;
} stackcontext_t;

// for the given array, return a pointer to the data section of the array
static void* bvm_get_array_data_offset_ptr(bvm_jbyte_array_obj_t *src_array_obj) {

//    bvm_jtype_t type = src_array_obj->clazz->component_jtype;

//    switch (type) {
//        case BVM_T_OBJECT: return ((bvm_instance_array_obj_t *) src_array_obj)->data;
//        case BVM_T_BOOLEAN: return ((bvm_jboolean_array_obj_t *) src_array_obj)->data;
//        case BVM_T_CHAR: return ((bvm_jchar_array_obj_t *) src_array_obj)->data;
//        case BVM_T_FLOAT: return ((bvm_jfloat_array_obj_t *) src_array_obj)->data;
//        case BVM_T_DOUBLE: return ((bvm_jdouble_array_obj_t *) src_array_obj)->data;
//        case BVM_T_BYTE: return ((bvm_jbyte_array_obj_t *) src_array_obj)->data;
//        case BVM_T_SHORT: return ((bvm_jshort_array_obj_t *) src_array_obj)->data;
//        case BVM_T_INT: return ((bvm_jint_array_obj_t *) src_array_obj)->data;
//        case BVM_T_LONG: return ((bvm_jlong_array_obj_t *) src_array_obj)->data;
//    }

    // should never get here ...
    return ((bvm_jbyte_array_obj_t *) src_array_obj)->data;
}

static bvm_bool_t stackcontextcallback(bvm_stack_frame_info_t *stackinfo, void *data) {

	int i;

	static bvm_utfstring_t *method_name;
	static bvm_clazz_t *clazz;
	static bvm_clazz_t *pd_clazz;

	stackcontext_t *context;
	bvm_protectiondomain_obj_t *pd;

	context = data;

	method_name = bvm_utfstring_pool_get_c("doPrivileged", BVM_TRUE);
	clazz = bvm_clazz_get_c(BVM_BOOTSTRAP_CLASSLOADER_OBJ, "java/security/SecurityManager");
	pd_clazz = bvm_clazz_get_c(BVM_BOOTSTRAP_CLASSLOADER_OBJ, "java/security/ProtectionDomain");

	/* if the clazz is a bootstrap clazz (it has NULL classloader) there will be no PD, otherwise, use the
	 * PD of the clazz's classloader. */
	pd = (stackinfo->method->clazz->classloader_obj == BVM_BOOTSTRAP_CLASSLOADER_OBJ) ? NULL :
	       stackinfo->method->clazz->classloader_obj->protection_domain;

	if (pd != NULL) {

		/* if the array is null, create a new default one of size 2. */
		if (context->array == NULL) {
			context->array = bvm_object_alloc_array_reference(2, pd_clazz);
			BVM_MAKE_TRANSIENT_ROOT(context->array);
		}

        // check if the pd is already in the array
        bvm_bool_t found = BVM_FALSE;
        for (i = 0; i < context->array->length.int_value; i++) {
            if (context->array->data[i] == (bvm_obj_t *) pd) {
                found = BVM_TRUE;
                break;
            }
        }

        if (found == BVM_FALSE) { // add it if not already in the array

            /* we'll need to add it to the first NULL spot (this will be at the end) - no duplicate checking as yet */
            for (i = 0; i < context->array->length.int_value; i++) {
                if (context->array->data[i] == NULL) {
                    context->array->data[i] = (bvm_obj_t *) pd;
                    break;
                }
            }

            /* if we got to the end of the array, then there was no space so we expand it and
             * add the pd to the first new cell in the expanded array */
            if (i == context->array->length.int_value) {

                // keep a handle to old array
                bvm_instance_array_obj_t *src_array = context->array;

                // create a new larger array
                context->array = bvm_object_alloc_array_reference(context->array->length.int_value * 2, pd_clazz);

                BVM_MAKE_TRANSIENT_ROOT(context->array);

                // copy old data into new array
                bvm_raw_copy_array_contents(src_array, context->array, 0, 0, src_array->length.int_value);

                // set the pd into the new array
                context->array->data[i] = (bvm_obj_t *) pd;
            }
        }
	}

	/* if the last frame was privileged, stop the stack visit */
	if (context->privileged) return BVM_FALSE;

	/* check the method to see if it is one of the "AccessController.doPrivileged" methods. */
	if ( (method_name == stackinfo->method->name) &&
		 (clazz == (bvm_clazz_t *) stackinfo->method->clazz) ) {

		context->privileged = BVM_TRUE;
	}

	return BVM_TRUE;
}

/*
 * private static native AccessControlContext getStackAccessControlContext();
 *
 */
void java_security_SecurityManager_getStackAccessControlContext(void *args) {

    UNUSED(args);

	/* Create a Java array of all the unique ProtectionDomain objects for classes in the current
	 * execution stack.  The array may have trailing nulls.  The array will start as NULL.  When a PD is encountered
	 * we'll default the array size to 2 and expand in twos as required.  It is pretty unlikely (but possible) that
	 * the number of simultaneous PDs in effect on the stack would be greater than 2. */

	/* Note that encountering a "privileged" frame will stop the search with the following frame.  A privileged
	 * frame is where the method name is "doPrivileged" and the method class is the java/security/AccessController.
	 *
	 * We use the frame *below* "doPrivileged" to ensure the protection domain of the caller of
	 * "AccessController.doPrivileged" is included in the context. */

	bvm_accesscontrolcontext_obj_t *acc = NULL;

	stackcontext_t data = {NULL, BVM_FALSE};

	bvm_stack_visit(bvm_gl_thread_current, 0, -1, NULL,  stackcontextcallback, &data);

	if (data.array != NULL) {
		/* create a new AccessControlContext object */
		acc = (bvm_accesscontrolcontext_obj_t *)
		   bvm_object_alloc( (bvm_instance_clazz_t *) bvm_clazz_get_c(BVM_BOOTSTRAP_CLASSLOADER_OBJ, "java/security/AccessControlContext"));

        // TODO: If the stack context is not privileged, add in the current thread's inherited context
//        if (!data.privileged) {
//            bvm_accesscontrolcontext_obj_t *inherited_context = (bvm_accesscontrolcontext_obj_t *) bvm_gl_thread_current->thread_obj->inherited_context;
//            if (inherited_context != NULL) {
//                int len = (inherited_context->context != NULL) ? inherited_context->context->length.int_value : 0;
//                if (len != 0) {
//                    // create a new array a copy both found pds + inherited context
//                    bvm_clazz_t *pd_clazz = bvm_clazz_get_c(BVM_BOOTSTRAP_CLASSLOADER_OBJ, "java/security/ProtectionDomain");
//                    bvm_instance_array_obj_t *new_array = bvm_object_alloc_array_reference(data.array->length.int_value + len, pd_clazz);
//                    bvm_raw_copy_array_contents(data.array, new_array, 0, 0, data.array->length.int_value);
//                    bvm_raw_copy_array_contents(inherited_context->context, new_array, 0, data.array->length.int_value, len);
////                    bvm_heap_free(data.array);
//                    data.array = new_array;
//                }
//            }
//        }

		acc->context = data.array;
	}

	NI_ReturnObject(acc);
}


/*
 * static native AccessControlContext getInheritedAccessControlContext();
 */
void java_security_SecurityManager_getInheritedAccessControlContext(void *args) {
    UNUSED(args);
	NI_ReturnObject(bvm_gl_thread_current->thread_obj->inherited_context);
}

/***************************************************************************************************
 *
 **************************************************************************************************/

static char *console_classname 			= "java/io/Console";
static char *object_classname  			= "java/lang/Object";
static char *string_classname  			= "java/lang/String";
static char *system_classname  			= "java/lang/System";
static char *class_classname   			= "java/lang/Class";
static char *classloader_classname   	= "java/lang/ClassLoader";
static char *thread_classname  			= "java/lang/Thread";
static char *runtime_classname  		= "java/lang/Runtime";
static char *integer_classname  		= "java/lang/Integer";
static char *stringbuffer_classname  	= "java/lang/StringBuffer";
static char *stringbuilder_classname  	= "java/lang/StringBuilder";
static char *throwable_classname  	    = "java/lang/Throwable";
static char *weakreference_classname  	= "java/lang/ref/WeakReference";
static char *byteorder_classname  		= "java/nio/ByteOrder";
static char *file_classname  			= "babe/io/File";
static char *securitymanager_classname  = "java/security/SecurityManager";

#if BVM_FLOAT_ENABLE
static char *float_classname   = "java/lang/Float";
static char *double_classname   = "java/lang/Double";
static char *math_classname   = "java/lang/Math";
#endif

void bvm_init_native() {

	bvm_native_method_pool_register(object_classname, "hashCode", "()I", java_lang_Object_hashCode);
	bvm_native_method_pool_register(object_classname, "getClass", "()Ljava/lang/Class;", java_lang_Object_getClass);
	bvm_native_method_pool_register(object_classname, "wait", "(J)V", java_lang_Object_wait);
	bvm_native_method_pool_register(object_classname, "notify", "()V", java_lang_Object_notify);
	bvm_native_method_pool_register(object_classname, "notifyAll", "()V", java_lang_Object_notifyAll);
	bvm_native_method_pool_register(object_classname, "clone", "()Ljava/lang/Object;", java_lang_Object_clone);

	bvm_native_method_pool_register(console_classname, "write", "(I)V", java_io_Console_write);
	bvm_native_method_pool_register(console_classname, "println0", "(Ljava/lang/String;)V", java_io_Console_println0);
	bvm_native_method_pool_register(console_classname, "print0", "(Ljava/lang/String;)V", java_io_Console_print0);

	bvm_native_method_pool_register(runtime_classname, "freeMemory", "()J", java_lang_Runtime_freeMemory);
	bvm_native_method_pool_register(runtime_classname, "totalMemory", "()J", java_lang_Runtime_totalMemory);
	bvm_native_method_pool_register(runtime_classname, "gc", "()V", java_lang_Runtime_gc);
	bvm_native_method_pool_register(runtime_classname, "exit0", "(I)V", java_lang_Runtime_exit0);

	bvm_native_method_pool_register(system_classname, "arraycopy", "(Ljava/lang/Object;ILjava/lang/Object;II)V", java_lang_System_arraycopy);
	bvm_native_method_pool_register(system_classname, "currentTimeMillis", "()J", java_lang_System_currentTimeMillis);
	bvm_native_method_pool_register(system_classname, "identityHashCode", "(Ljava/lang/Object;)I", java_lang_Object_hashCode); /* Yes, same as object */

	bvm_native_method_pool_register(integer_classname, "toString", "(I)Ljava/lang/String;", java_lang_Integer_toString);

	bvm_native_method_pool_register(string_classname, "intern", "()Ljava/lang/String;", java_lang_String_intern);
	bvm_native_method_pool_register(string_classname, "equals", "(Ljava/lang/Object;)Z", java_lang_String_equals);
	bvm_native_method_pool_register(string_classname, "hashCode", "()I", java_lang_String_hashCode);
	bvm_native_method_pool_register(string_classname, "startsWith", "(Ljava/lang/String;I)Z", java_lang_String_startsWith);
	bvm_native_method_pool_register(string_classname, "indexOf", "(II)I", java_lang_String_indexOf);
	bvm_native_method_pool_register(string_classname, "lastIndexOf", "(II)I", java_lang_String_lastIndexOf);
	bvm_native_method_pool_register(string_classname, "charAt", "(I)C", java_lang_String_charAt);

	bvm_native_method_pool_register(stringbuffer_classname, "insert0", "(I[CII)Ljava/lang/StringBuffer;", java_lang_StringBuffer_insert0);
	bvm_native_method_pool_register(stringbuffer_classname, "setLength", "(I)V", java_lang_StringBuffer_setLength);
	bvm_native_method_pool_register(stringbuffer_classname, "ensureCapacity", "(I)V", java_lang_StringBuffer_ensureCapacity);
	bvm_native_method_pool_register(stringbuffer_classname, "append", "(Ljava/lang/String;)Ljava/lang/StringBuffer;", java_lang_StringBuffer_append);

	bvm_native_method_pool_register(stringbuilder_classname, "insert0", "(I[CII)Ljava/lang/StringBuilder;", java_lang_StringBuffer_insert0);
	bvm_native_method_pool_register(stringbuilder_classname, "setLength", "(I)V", java_lang_StringBuffer_setLength);
	bvm_native_method_pool_register(stringbuilder_classname, "ensureCapacity", "(I)V", java_lang_StringBuffer_ensureCapacity);
	bvm_native_method_pool_register(stringbuilder_classname, "append", "(Ljava/lang/String;)Ljava/lang/StringBuilder;", java_lang_StringBuffer_append);

	bvm_native_method_pool_register(class_classname, "forName", "(Ljava/lang/String;)Ljava/lang/Class;", java_lang_Class_forName);
	bvm_native_method_pool_register(class_classname, "forName2", "(Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;", java_lang_Class_forName2);
	/* not a mistake - getPrimitiveClass() uses the same code as forName() */
	bvm_native_method_pool_register(class_classname, "getPrimitiveClass", "(Ljava/lang/String;)Ljava/lang/Class;", java_lang_Class_forName);
	bvm_native_method_pool_register(class_classname, "newInstance", "()Ljava/lang/Object;", java_lang_Class_newInstance);
	bvm_native_method_pool_register(class_classname, "getName", "()Ljava/lang/String;", java_lang_Class_getName);
	bvm_native_method_pool_register(class_classname, "getSuperclass", "()Ljava/lang/Class;", java_lang_Class_getSuperclass);
	bvm_native_method_pool_register(class_classname, "isInterface", "()Z", java_lang_Class_isInterface);
	bvm_native_method_pool_register(class_classname, "__doclinit", "()V", java_lang_Class_doclinit);
	bvm_native_method_pool_register(class_classname, "getState", "()I", java_lang_Class_getState);
	bvm_native_method_pool_register(class_classname, "setState", "(I)V", java_lang_Class_setState);
	bvm_native_method_pool_register(class_classname, "getInitThread", "()Ljava/lang/Thread;", java_lang_Class_getInitThread);
	bvm_native_method_pool_register(class_classname, "desiredAssertionStatus", "()Z", java_lang_Class_desiredAssertionStatus);
	bvm_native_method_pool_register(class_classname, "getClassLoader0", "()Ljava/lang/ClassLoader;", java_lang_Class_getClassLoader0);

	bvm_native_method_pool_register(classloader_classname, "getSystemClassLoader", "()Ljava/lang/ClassLoader;", java_lang_ClassLoader_getSystemClassLoader);

	bvm_native_method_pool_register(thread_classname, "nextThreadId", "()I", java_lang_Thread_nextThreadId);
	bvm_native_method_pool_register(thread_classname, "currentThread0", "()Ljava/lang/Thread;", java_lang_Thread_currentThread0);
	bvm_native_method_pool_register(thread_classname, "start", "()V", java_lang_Thread_start);
	bvm_native_method_pool_register(thread_classname, "setPriority0", "(I)V", java_lang_Thread_setPriority0);
	bvm_native_method_pool_register(thread_classname, "isAlive", "()Z", java_lang_Thread_isAlive);
	bvm_native_method_pool_register(thread_classname, "isInterrupted", "(Z)Z", java_lang_Thread_isInterrupted);
	bvm_native_method_pool_register(thread_classname, "yield", "()V", java_lang_Thread_yield);
	bvm_native_method_pool_register(thread_classname, "interrupt0", "()V", java_lang_Thread_interrupt0);
	bvm_native_method_pool_register(thread_classname, "sleep", "(J)V", java_lang_Thread_sleep);
	bvm_native_method_pool_register(thread_classname, "getState", "()I", java_lang_Thread_getState);

#if BVM_FLOAT_ENABLE
	bvm_native_method_pool_register(float_classname, "toString", "(F)Ljava/lang/String;", java_lang_Float_toString);
	// same code for both
	bvm_native_method_pool_register(float_classname, "floatToIntBits", "(F)I", java_lang_Float_floatToIntBits);
	bvm_native_method_pool_register(float_classname, "intBitsToFloat", "(I)F", java_lang_Float_floatToIntBits);

	bvm_native_method_pool_register(double_classname, "toString", "(D)Ljava/lang/String;", java_lang_Double_toString);
    // same code for both
	bvm_native_method_pool_register(double_classname, "doubleToLongBits", "(D)J", java_lang_Double_doubleToLongBits);
	bvm_native_method_pool_register(double_classname, "longBitsToDouble", "(J)D", java_lang_Double_doubleToLongBits);

	bvm_native_method_pool_register(math_classname, "floor", "(D)D", java_lang_Math_floor);
	bvm_native_method_pool_register(math_classname, "sqrt", "(D)D", java_lang_Math_sqrt);
	bvm_native_method_pool_register(math_classname, "log", "(D)D", java_lang_Math_log);
	bvm_native_method_pool_register(math_classname, "log10", "(D)D", java_lang_Math_log10);
	bvm_native_method_pool_register(math_classname, "ceil", "(D)D", java_lang_Math_ceil);
	bvm_native_method_pool_register(math_classname, "exp", "(D)D", java_lang_Math_exp);
	bvm_native_method_pool_register(math_classname, "IEEEremainder", "(DD)D", java_lang_Math_IEEEremainder);
#endif

	bvm_native_method_pool_register(byteorder_classname, "isLittleEndian", "()Z", java_nio_ByteOrder_isLittleEndian);

	bvm_native_method_pool_register(file_classname, "open0", "(Ljava/lang/String;I)I", babe_io_File_open0);
	bvm_native_method_pool_register(file_classname, "close", "()V", babe_io_File_close);
	bvm_native_method_pool_register(file_classname, "read", "([BII)I", babe_io_File_read);
	bvm_native_method_pool_register(file_classname, "write", "([BII)V", babe_io_File_write);
	bvm_native_method_pool_register(file_classname, "setPosition", "(II)V", babe_io_File_setPosition);
	bvm_native_method_pool_register(file_classname, "getPosition", "()I", babe_io_File_getPosition);
	bvm_native_method_pool_register(file_classname, "sizeOf", "()I", babe_io_File_sizeOf);
	bvm_native_method_pool_register(file_classname, "rename", "(Ljava/lang/String;Ljava/lang/String;)V", babe_io_File_rename);
	bvm_native_method_pool_register(file_classname, "remove", "(Ljava/lang/String;)V", babe_io_File_remove);
	bvm_native_method_pool_register(file_classname, "exists", "(Ljava/lang/String;)Z", babe_io_File_exists);
	bvm_native_method_pool_register(file_classname, "truncate", "(I)V", babe_io_File_truncate);

	bvm_native_method_pool_register(throwable_classname, "fillInStackTrace", "()V", java_lang_Throwable_fillInStackTrace);
	bvm_native_method_pool_register(throwable_classname, "getStackTrace0", "()[Ljava/lang/StackTraceElement;", java_lang_Throwable_getStackTrace0);

	bvm_native_method_pool_register(weakreference_classname, "makeweak", "()V", java_lang_ref_WeakReference_makeweak);

	bvm_native_method_pool_register(securitymanager_classname, "getStackAccessControlContext", "()Ljava/security/AccessControlContext;", java_security_SecurityManager_getStackAccessControlContext);
	bvm_native_method_pool_register(securitymanager_classname, "getInheritedAccessControlContext", "()Ljava/security/AccessControlContext;", java_security_SecurityManager_getInheritedAccessControlContext);
}
