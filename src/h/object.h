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

#ifndef BVM_OBJECT_H_
#define BVM_OBJECT_H_

/**
  @file

  Constants/Macros/Functions/Types for Java objects.

  @author Greg McCreath
  @since 0.0.10

*/

/* some helpful forward declarations */
struct _bvmstackbacktracestruct;
typedef struct _bvmstackbacktracestruct bvm_stack_backtrace_t;

/**
 * Stuff that goes at the top of all object structs.  Holds a pointer to  the object's #bvm_clazz_t, and
 * its thread locking monitor, if it has one.
 */
#define BVM_COMMON_OBJ_INFO										\
	/* The clazz of the object */								\
	bvm_clazz_t *clazz;


/**
 * A Java object.  Fields are represented as an array of #bvm_cell_t.
 */
struct _bvmobjstruct {
	BVM_COMMON_OBJ_INFO

	/** An array of fields containing the instance data for an object.  The actual size of
	 * this array is determined at runtime for each instance based on its class field definitions.
	 */
    bvm_cell_t fields[1];
} /* bvm_obj_t is forward defined */ ;


/**
 * A Java Weakreference object.
 */
typedef struct _bvmweakrefstruct {
	BVM_COMMON_OBJ_INFO

	/** The Java object the weakreference if referring to */
	bvm_obj_t *referent;

	/** Used for GC - the next reference in a list created during GC */
	struct _bvmweakrefstruct *next;

} bvm_weak_reference_obj_t ;


/**
 * A structure to hold information about array class types.
 */
typedef struct _bvmstructtypearrayinfo {
	/** If the array class is primitive, this is a pointer to its #bvm_clazz_t. Will be \c NULL for reference types. */
	bvm_array_clazz_t *primitive_array_clazz;

	/** The size in bytes of the elements this array can contains */
	int element_size;
} bvm_array_typeinfo_t;

/**
 * Array of information about array types - holds element size info, as well as handles to clazzes
 * for primitive arrays.  The array is indexed by the \c T_ type definitions from the JVMS,
 * like \c BVM_T_OBJECT = 1, \c BVM_T_ARRAY = 2 etc.  There is no zero-th element.  Index 1 is \c BVM_T_OBJECT,
 * index 11 is \c BVM_T_LONG.
 */
bvm_array_typeinfo_t bvm_gl_type_array_info[12];		/* 12 represents 0 .. BVM_T_LONG */

/**
 * Common fields used across java array object structs.
 */
#define BVM_COMMON_ARRAY_OBJ_INFO									\
	/* first fields here must match BVM_COMMON_OBJ_INFO */			\
	/** The class of the array */								    \
	bvm_array_clazz_t *clazz;										\
	/** The length of the array*/								    \
	bvm_cell_t length;


/**
 * A Java array object.
 */
typedef struct _bvmjarraystruct {
	BVM_COMMON_ARRAY_OBJ_INFO
} bvm_jarray_obj_t;

/**
 * A Java array object of \c short elements.
 */
typedef struct _bvmjshortarraystruct {
	BVM_COMMON_ARRAY_OBJ_INFO
    bvm_int16_t data[1];
} bvm_jshort_array_obj_t;

/**
 * A Java array object of \c char elements.
 */
typedef struct _bvmjchararraystruct {
	BVM_COMMON_ARRAY_OBJ_INFO
    bvm_uint16_t data[1];
} bvm_jchar_array_obj_t;

/**
 * A Java array object of \c int elements.
 */
typedef struct _bvmjintarraystruct {
	BVM_COMMON_ARRAY_OBJ_INFO
    bvm_int32_t data[1];
} bvm_jint_array_obj_t;

/**
 * A Java array object of \c long elements.
 */
typedef struct _bvmjlongarraystruct {
	BVM_COMMON_ARRAY_OBJ_INFO
    bvm_int64_t data[1];
} bvm_jlong_array_obj_t;

#if BVM_FLOAT_ENABLE
/**
 * A Java array object of \c float elements.
 */
typedef struct _bvmjfloatarraystruct {
	BVM_COMMON_ARRAY_OBJ_INFO
	bvm_float_t data[1];
} bvm_jfloat_array_obj_t;

/**
 * A Java array object of \c double elements.
 */
typedef struct _bvmjdoublearraystruct {
	BVM_COMMON_ARRAY_OBJ_INFO
    bvm_double_t data[1];
} bvm_jdouble_array_obj_t;

#endif

/**
 * A Java array object of \c boolean elements.
 */
typedef struct _bvmjbooleanarraystruct {
	BVM_COMMON_ARRAY_OBJ_INFO
	bvm_uint8_t data[1];
} bvm_jboolean_array_obj_t;

/**
 * A Java array object of \c byte elements.
 */
typedef struct _bvmjbytearraystruct {
	BVM_COMMON_ARRAY_OBJ_INFO
    bvm_int8_t data[1];
} bvm_jbyte_array_obj_t;

/**
 * A Java array object of \c object elements.
 */
typedef struct _bvminstancearraystruct {
	BVM_COMMON_ARRAY_OBJ_INFO
    bvm_obj_t *data[1];
} bvm_instance_array_obj_t;

/**
 * A \c java.lang.String object.
 */
typedef struct _bvmstringinstancestruct {
	BVM_COMMON_OBJ_INFO
	/* maps to first field '_chars[]' */
    bvm_jchar_array_obj_t *chars;
	/* maps to field '_offset' */
    bvm_cell_t offset;
	/* maps to field '_length' */
    bvm_cell_t length;
} bvm_string_obj_t;

/**
 * A \c java.lang.StringBuffer object.
 */
typedef struct _bvmstringbufferinstancestruct {
	BVM_COMMON_OBJ_INFO
	/** maps to java field \c _chars[] */
    bvm_jchar_array_obj_t *chars;
	/** maps to java field \c _length */
    bvm_cell_t length;
	/** maps to java field \c _isShared */
    bvm_cell_t is_shared;
} bvm_stringbuffer_obj_t;

/**
 * Substruct of #bvm_string_obj_t that holds the info
 * required to have an string interned and maintained in a pool
 */
typedef struct _bvminternstringinstancestruct {

	/**** must be same as bvm_string_obj_t **/
	BVM_COMMON_OBJ_INFO
	/** maps to first java field \c _chars[] */
    bvm_jchar_array_obj_t *chars;
	/** maps to java field \c _offset */
    bvm_cell_t offset;
	/** maps to java field \c _length */
    bvm_cell_t length;
    /*************************************/

    /** handle to the bvm_utfstring_t this interned string represents */
    bvm_utfstring_t *utfstring;

    /** pointer to the next interned #bvm_internstring_obj_t in the same pool hash bucket */
    struct _bvminternstringinstancestruct *next;
} bvm_internstring_obj_t;

/**
 A Java \c java.security.CodeSource object.
 */
typedef struct _bvmcodesourceinstancestruct {
	BVM_COMMON_OBJ_INFO

	/** handle to the String location object for the codesource.  May be \c NULL. */
	bvm_string_obj_t *location;

	/** handle to array of \c java.security.Certificate objects. */
	bvm_instance_array_obj_t *stack_trace_elements;

} bvm_codesource_obj_t;


/**
 A Java \c java.security.CodeSource object.
 */
typedef struct _bvmprotectiondomaininstancestruct {
	BVM_COMMON_OBJ_INFO

	/** handle to the String location object for the codesource.  May be \c NULL. */
	bvm_codesource_obj_t *codesource;

	/** handle to the static Permissions of a protection domain */
	bvm_obj_t *permissions;

} bvm_protectiondomain_obj_t;


/**
 A Java \c java.security.AccessControlContext object.
 */
typedef struct _accesscontrolcontextinstancestruct {
	BVM_COMMON_OBJ_INFO

	/** handle to the array of ProtectionDomains.  May be \c NULL. */
	bvm_instance_array_obj_t *context;

} bvm_accesscontrolcontext_obj_t;


/**
 * A Java \c java.lang.Class object.
 */
struct _bvmclassinstancestruct {
	BVM_COMMON_OBJ_INFO

	/** A handle to the #bvm_clazz_t this Class object is associated with */
	bvm_clazz_t *refers_to_clazz;

	/** The initialising thread of a class - set in Java code */
	bvm_obj_t 	*init_thread;

} /*bvm_class_obj_t is forward defined */;

/**
 * A Java array object of \c java.lang.Class elements.
 */
typedef struct _bvmclassinstancearraystruct {
	BVM_COMMON_ARRAY_OBJ_INFO
    bvm_class_obj_t *data[1];
} bvm_class_instance_array_obj_t;


/**
 A Java \c java.lang.Throwable object.
 */
typedef struct _bvmthrowableinstancestruct {
	BVM_COMMON_OBJ_INFO

	/** handle to the String message object for the exception.  May be \c NULL. */
	bvm_string_obj_t *message;

	/** handle to the #bvm_throwable_obj_t that caused this exception. */
	struct _bvmthrowableinstancestruct *cause;

	/** handle to stack frame summary generated at throwable creation time.  Used to create
	 * \c java.lang.StackTraceElement objects. */
	bvm_stack_backtrace_t *backtrace;

	/** whether the exception needs to cause the VM to enter a new native try / catch block within
	 * exception handling.  Initially, all native exception will have this set to true.
	 */
	bvm_cell_t needs_native_try_reset;

	/** handle to array of \c java.lang.StackTraceElement objects.  Will be populated only when a throwable's
	 * stack elements are requested.
	 */
	bvm_instance_array_obj_t *stack_trace_elements;

} bvm_throwable_obj_t;


/**
 * A Java \c java.lang.ClassLoader object.
 */
struct _bvmclassloaderstruct {
	BVM_COMMON_OBJ_INFO

	/** The parent #bvm_classloader_obj_t */
	struct _bvmclassloaderstruct *parent;

	/** The number of class object maintained by this class loader */
	bvm_cell_t nr_classes;

	/** An array object of Class object held by this class loader */
	struct _bvmclassinstancearraystruct *class_array;

	/** A array of String objects represeting the classpath of this loader */
	struct _bvminstancearraystruct *paths_array;

	/** ProtectionDomain object (if any) for the class loader */
	bvm_protectiondomain_obj_t *protection_domain;

} /* bvm_classloader_obj_t is forward defined */;

/** Handle to clazz java/lang/Object for quick access */
bvm_instance_clazz_t *BVM_OBJECT_CLAZZ;

/** Handle to clazz java/lang/Class for quick access */
bvm_instance_clazz_t *BVM_CLASS_CLAZZ;

/** Handle to clazz Thread for quick access */
bvm_instance_clazz_t *BVM_THREAD_CLAZZ;

/** Handle to clazz java/lang/ClassLoader for quick access */
bvm_instance_clazz_t *BVM_CLASSLOADER_CLAZZ;

/** Handle to clazz java/lang/String for quick access */
bvm_instance_clazz_t *BVM_STRING_CLAZZ;

/** Handle to clazz java/lang/System for quick access */
bvm_instance_clazz_t *BVM_SYSTEM_CLAZZ;

/** Handle to clazz java/lang/Throwable for quick access */
bvm_instance_clazz_t *BVM_THROWABLE_CLAZZ;

/** Handle to the bootstrap classloader for quick access.  Will always be \c NULL but is defined to
 * explicitly show usage. */
bvm_classloader_obj_t *BVM_BOOTSTRAP_CLASSLOADER_OBJ;

/** The bootstrap classpath as a Java array of String objects */
bvm_instance_array_obj_t *BVM_BOOTSTRAP_CLASSPATH_ARRAY;

/** Handle to the system classloader object for quick access */
bvm_classloader_obj_t *BVM_SYSTEM_CLASSLOADER_OBJ;

/** Pre-built \c lava.lang.OutOfMemoryError object */
extern bvm_throwable_obj_t *bvm_gl_out_of_memory_err_obj;

bvm_obj_t *bvm_object_alloc(bvm_instance_clazz_t *clazz);
bvm_jarray_obj_t *bvm_object_alloc_array_primitive(bvm_uint32_t length, bvm_jtype_t type);
bvm_instance_array_obj_t *bvm_object_alloc_array_reference(bvm_uint32_t length, bvm_clazz_t *component_clazz);
bvm_instance_array_obj_t *bvm_object_alloc_array_multi(bvm_array_clazz_t *clazz, int dimensions, bvm_native_long_t lengths[]);
bvm_uint32_t bvm_calchash(bvm_uint8_t *key, bvm_uint16_t len);

#endif /*BVM_OBJECT_H_*/
