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

#ifndef BVM_CLAZZ_H_
#define BVM_CLAZZ_H_

/**
  @file

  Constants/Macros/Functions for Class file reading and clazz support.

  @author Greg McCreath
  @since 0.0.10

*/

struct _bvmobjstruct;
typedef struct _bvmobjstruct bvm_obj_t;

struct _bvmclassinstancestruct;
typedef struct _bvmclassinstancestruct bvm_class_obj_t;

struct _bvmclassloaderstruct;
typedef struct _bvmclassloaderstruct bvm_classloader_obj_t;

struct _bvmclazzstruct;
typedef struct _bvmclazzstruct bvm_clazz_t;

struct _bvminstanceclazzstruct;
typedef struct _bvminstanceclazzstruct bvm_instance_clazz_t;

struct _bvmarrayclazzstruct;
typedef struct _bvmarrayclazzstruct bvm_array_clazz_t;

struct _bvmprimitiveclazzstruct;
typedef struct _bvmprimitiveclazzstruct bvm_primitive_clazz_t;

/* ********************************************/
/* ********** JVMS Constants    **************/
/* ********************************************/

/* Class Access flags.  */
#define BVM_CLASS_ACCESS_PUBLIC       0x0001
#define BVM_CLASS_ACCESS_FINAL        0x0010
#define BVM_CLASS_ACCESS_SUPER        0x0020
#define BVM_CLASS_ACCESS_INTERFACE    0x0200
#define BVM_CLASS_ACCESS_ABSTRACT     0x0400
#define BVM_CLASS_ACCESS_SYNTHETIC    0x1000
#define BVM_CLASS_ACCESS_ANNOTATION   0x2000
#define BVM_CLASS_ACCESS_ENUM         0x4000

/* Non JVMS - Some extra access types squeezed into the above so we can tell what type of class we have */
#define BVM_CLASS_ACCESS_FLAG_ARRAY      0x0040
#define BVM_CLASS_ACCESS_FLAG_INSTANCE   0x0080
#define BVM_CLASS_ACCESS_FLAG_PRIMITIVE  0x0100

/* Field access flags */
#define BVM_FIELD_ACCESS_PUBLIC       0x0001
#define BVM_FIELD_ACCESS_PRIVATE      0x0002
#define BVM_FIELD_ACCESS_PROTECTED    0x0004
#define BVM_FIELD_ACCESS_STATIC       0x0008
#define BVM_FIELD_ACCESS_FINAL        0x0010
#define BVM_FIELD_ACCESS_VOLATILE     0x0040
#define BVM_FIELD_ACCESS_TRANSIENT    0x0080
#define BVM_FIELD_ACCESS_SYNTHETIC    0x1000
#define BVM_FIELD_ACCESS_ENUM         0x4000

/* Non JVMS - modifier for the field access flags to say it is a constant final field */
#define BVM_FIELD_ACCESS_FLAG_CONST        0x2000
/* Non JVMS - modifier for the field access flags to say it is a long field */
#define BVM_FIELD_ACCESS_FLAG_LONG         0x8000
/* Non JVMS - modifier for the field access flags to say it is a 'native' field.  'Native'
 * means it is used to hold a native pointer - and should be shown via JDWP to be an int.
 * Field with NATIVE set are described in the Java class as Object - but are
 * not actually an 'object' here */
#define BVM_FIELD_ACCESS_FLAG_NATIVE   0x0020

/* Method access flags */
#define BVM_METHOD_ACCESS_PUBLIC       0x0001
#define BVM_METHOD_ACCESS_PRIVATE      0x0002
#define BVM_METHOD_ACCESS_PROTECTED    0x0004
#define BVM_METHOD_ACCESS_STATIC       0x0008
#define BVM_METHOD_ACCESS_FINAL        0x0010
#define BVM_METHOD_ACCESS_SYNCHRONIZED 0x0020
#define BVM_METHOD_ACCESS_BRIDGE 	   0x0040
#define BVM_METHOD_ACCESS_VARARGS 	   0x0080
#define BVM_METHOD_ACCESS_NATIVE       0x0100
#define BVM_METHOD_ACCESS_ABSTRACT     0x0400
#define BVM_METHOD_ACCESS_STRICT       0x0800
#define BVM_METHOD_ACCESS_SYNTHETIC    0x1000

/* Access flags.  For classes methods and fields. */
#define BVM_ACCESS_PUBLIC       0x0001
#define BVM_ACCESS_PRIVATE      0x0002
#define BVM_ACCESS_PROTECTED    0x0004

/* Constant Pool tags */
#define BVM_CONSTANT_Utf8               1
#define BVM_CONSTANT_Integer            3
#define BVM_CONSTANT_Float              4
#define BVM_CONSTANT_Long               5
#define BVM_CONSTANT_Double             6
#define BVM_CONSTANT_Class              7
#define BVM_CONSTANT_String             8
#define BVM_CONSTANT_Fieldref           9
#define BVM_CONSTANT_Methodref          10
#define BVM_CONSTANT_InterfaceMethodref 11
#define BVM_CONSTANT_NameAndType        12

/** Java type constants */
typedef enum {
	BVM_T_OBJECT = 1,   	/* L...;	*/
	BVM_T_ARRAY = 2,   		/* [		*/
	BVM_T_BOOLEAN = 4,   	/* Z		*/
	BVM_T_CHAR = 5,   		/* C		*/
	BVM_T_FLOAT = 6,   		/* F		*/
	BVM_T_DOUBLE = 7,   	/* D		*/
	BVM_T_BYTE = 8,   		/* B		*/
	BVM_T_SHORT = 9,   		/* S		*/
	BVM_T_INT = 10,  		/* I		*/
	BVM_T_LONG = 11  		/* J		*/
} bvm_jtype_t;

/* ********************************************/
/* ********** Class search masks **************/
/* ********************************************/

/* Masks used when search classes to specify how broad the search is */
#define BVM_METHOD_SEARCH_CLAZZ          0x0001
#define BVM_METHOD_SEARCH_SUPERS 	     0x0002
#define BVM_METHOD_SEARCH_INTERFACES     0x0004

/** class search mask for the full interfaces and supers search */
#define BVM_METHOD_SEARCH_FULL_TREE             (BVM_METHOD_SEARCH_CLAZZ | BVM_METHOD_SEARCH_SUPERS | BVM_METHOD_SEARCH_INTERFACES)

/* ********************************************/
/* ********** Member Access flags Macros ******/
/* ********************************************/

/** Does a given access flag specify 'public' ? */
#define BVM_ACCESS_IsPublic(f)					( ((f) & BVM_ACCESS_PUBLIC) 	> 0)
/** Does a given access flag specify 'private' ? */
#define BVM_ACCESS_IsPrivate(f)					( ((f) & BVM_ACCESS_PRIVATE) 	> 0)
/** Does a given access flag specify 'protected' ? */
#define BVM_ACCESS_IsProtected(f)				( ((f) & BVM_ACCESS_PROTECTED) 	> 0)

/** Does a given access flag specify 'package private'? 'Package Private' is when the member is not protected, not public and not private - it can only be see
 * in the same package is is declared in. */
#define BVM_ACCESS_IsPackagePrivate(f)			( !BVM_ACCESS_IsProtected(f) && !BVM_ACCESS_IsPublic(f) && !BVM_ACCESS_IsPrivate(f) )

/* ********************************************/
/* ********** Field Access flags Macros ******/
/* ********************************************/

#define BVM_FIELD_AccessFlags(f) 				((f)->access_flags))
#define BVM_FIELD_IsStatic(f) 					(((f)->access_flags & BVM_FIELD_ACCESS_STATIC)    > 0)
#define BVM_FIELD_IsFinal(f) 					(((f)->access_flags & BVM_FIELD_ACCESS_FINAL) 	  > 0)
#define BVM_FIELD_IsPublic(f) 					(((f)->access_flags & BVM_FIELD_ACCESS_PUBLIC)    > 0)
#define BVM_FIELD_IsPrivate(f) 					(((f)->access_flags & BVM_FIELD_ACCESS_PRIVATE)   > 0)
#define BVM_FIELD_IsProtected(f) 				(((f)->access_flags & BVM_FIELD_ACCESS_PROTECTED) > 0)

#define BVM_FIELD_IsLong(f)						(((f)->access_flags & BVM_FIELD_ACCESS_FLAG_LONG)   > 0)
#define BVM_FIELD_IsConst(f)					(((f)->access_flags & BVM_FIELD_ACCESS_FLAG_CONST)  > 0)
#define BVM_FIELD_IsNative(f)					(((f)->access_flags & BVM_FIELD_ACCESS_FLAG_NATIVE) > 0)

/* ********************************************/
/* ********** Method Access flags Macros ******/
/* ********************************************/

#define BVM_METHOD_AccessFlags(m) 				((m)->access_flags)
#define BVM_METHOD_IsAbstract(m) 				(((m)->access_flags & BVM_METHOD_ACCESS_ABSTRACT)     > 0)
#define BVM_METHOD_IsFinal(m) 					(((m)->access_flags & BVM_METHOD_ACCESS_FINAL) 		  > 0)
#define BVM_METHOD_IsNative(m) 					(((m)->access_flags & BVM_METHOD_ACCESS_NATIVE) 	  > 0)
#define BVM_METHOD_IsPrivate(m) 				(((m)->access_flags & BVM_METHOD_ACCESS_PRIVATE) 	  > 0)
#define BVM_METHOD_IsProtected(m) 				(((m)->access_flags & BVM_METHOD_ACCESS_PROTECTED) 	  > 0)
#define BVM_METHOD_IsPublic(m) 					(((m)->access_flags & BVM_METHOD_ACCESS_PUBLIC) 	  > 0)
#define BVM_METHOD_IsStatic(m) 					(((m)->access_flags & BVM_METHOD_ACCESS_STATIC) 	  > 0)
#define BVM_METHOD_IsSynchronized(m) 			(((m)->access_flags & BVM_METHOD_ACCESS_SYNCHRONIZED) > 0)

/* ********************************************/
/* ********** Class Access flags Macros ******/
/* ********************************************/

#define BVM_CLAZZ_IsAbstract(c) 				( (( (bvm_clazz_t *) c)->access_flags & BVM_CLASS_ACCESS_ABSTRACT)  > 0)
#define BVM_CLAZZ_IsFinal(c) 					( (( (bvm_clazz_t *) c)->access_flags & BVM_CLASS_ACCESS_FINAL)     > 0)
#define BVM_CLAZZ_IsInterface(c) 				( (( (bvm_clazz_t *) c)->access_flags & BVM_CLASS_ACCESS_INTERFACE) > 0)
#define BVM_CLAZZ_IsPublic(c) 					( (( (bvm_clazz_t *) c)->access_flags & BVM_CLASS_ACCESS_PUBLIC)    > 0)
#define BVM_CLAZZ_IsPrivate(c) 					( (( (bvm_clazz_t *) c)->access_flags & CLASS_ACCESS_PRIVATE)       > 0)

#define BVM_CLAZZ_IsArrayClazz(c) 				( (( (bvm_clazz_t *) c)->access_flags & BVM_CLASS_ACCESS_FLAG_ARRAY)     > 0)
#define BVM_CLAZZ_IsInstanceClazz(c) 			( (( (bvm_clazz_t *) c)->access_flags & BVM_CLASS_ACCESS_FLAG_INSTANCE)  > 0)
#define BVM_CLAZZ_IsPrimitiveClazz(c) 			( (( (bvm_clazz_t *) c)->access_flags & BVM_CLASS_ACCESS_FLAG_PRIMITIVE) > 0)

/* ********************************************/
/* ********** Class lifecycle states **********/
/* ********************************************/

#define BVM_CLAZZ_STATE_ERROR           -1
#define BVM_CLAZZ_STATE_LOADING          0
#define BVM_CLAZZ_STATE_LOADED   		 1
#define BVM_CLAZZ_STATE_INITIALISING     2
#define BVM_CLAZZ_STATE_INITIALISED      3

/** To determine is a class is initialised - Refer JLS 2.17.5 Detailed Initialization Procedure */
#define BVM_CLAZZ_IsInitialised(c) ( ((c)->state == BVM_CLAZZ_STATE_INITIALISED) || ((c)->class_obj->init_thread == (bvm_obj_t *) bvm_gl_thread_current->thread_obj) )

/* ********************************************/
/* ********** Class constant macros  **********/
/* ********************************************/

/** Flag (mask) to indicate a class constant has been optimised */
#define BVM_CONSTANT_OPT_FLAG					 0x10

/** The raw tag is the constant tag including the optimised flag */
#define BVM_CONSTANT_RawTag(c,i) 			     ( ((bvm_uint8_t *) (c)->constant_pool[0].resolved_ptr)[i])

/** The constant tag excluding the optimised flag */
#define BVM_CONSTANT_Tag(c,i) 					 ( BVM_CONSTANT_RawTag(c,i) & 0xF )

/** Reports if a class constant has been optimised */
#define BVM_CONSTANT_IsOptimised(c,i)			 ( (BVM_CONSTANT_RawTag(c,i) & BVM_CONSTANT_OPT_FLAG) > 0 )

/* ********************************************/
/* *************** Conversions ****************/
/* ********************************************/

/** Convert a VM two byte array into a endian-safe signed 16 bit int */
#define BVM_VM2INT16(b)	 (bvm_int16_t) (((b)[0] << 8) | (b)[1])

/** Convert a VM two byte array into a endian-safe unsigned 16 bit int */
#define BVM_VM2UINT16(b) (bvm_uint16_t)(((b)[0] << 8) | (b)[1])

/** Convert a VM four byte array into a endian-safe signed 32 bit int */
#define BVM_VM2INT32(b)	 (bvm_int32_t) (((b)[0] << 24) | ((b)[1] << 16)| ((b)[2] << 8) | (b)[3])

/** Convert a VM four byte array into a endian-safe unsigned 32 bit int */
#define BVM_VM2UINT32(b) (bvm_uint32_t)(((b)[0] << 24) | ((b)[1] << 16)| ((b)[2] << 8) | (b)[3])

/** The Java class magic number */
#define BVM_MAGIC_NUMBER 0xCAFEBABE

/**
 * The basic unit of memory usage within the VM for objects and stack.  It only takes one value (hence
 * it is a union), but it provides a convenient way to be able to store any VM relevant value using
 * a single structure.
 */
typedef union _bvmcellunionstruct {
    bvm_native_long_t int_value;
	void *ptr_value;
#if BVM_FLOAT_ENABLE
	bvm_float_t float_value;
#endif
	bvm_obj_t *ref_value;
	bvm_obj_t *(*callback)(union _bvmcellunionstruct *, union _bvmcellunionstruct *, bvm_bool_t, void *);
} bvm_cell_t;

/**
 * A Java class field.
 */
typedef struct _bvmfieldstruct {

	/** The #bvm_instance_clazz_t this field belongs to */
	struct _bvminstanceclazzstruct *clazz;

	/** The access rights flags for the field */
	bvm_uint16_t access_flags;

	/** Handle to the pooled #bvm_utfstring_t name of the field */
	bvm_utfstring_t *name;

	/** Handle to the pooled #bvm_utfstring_t description of this field */
	bvm_utfstring_t *jni_signature;

#if (BVM_DEBUGGER_ENABLE && BVM_DEBUGGER_SIGNATURES_ENABLE)
	/** Handle to utf string Java 5 generic signature for the field (if any). */
	bvm_utfstring_t *generic_signature;
#endif

	/** Static final constants field will get a value at class load time in 'static_value', otherwise
	 * the 'offset' represents the index of this instance field among all other instance fields
	 * for the defining class - including superclasses.   These two are expressed as a union as they
	 * are mutually exclusive */
    union {
        bvm_uint16_t offset;
        bvm_cell_t static_value;
    } value;

} bvm_field_t;

/**
 * Function pointer definition of a native method.  Although defined here as a \c void*, arguments to
 * native methods are passed as #bvm_cell_t.
 */
typedef void (*bvm_native_method_t)(void *args);

/**
 * DOCME
 */
bvm_native_method_t bvm_native_method_lookup(bvm_uint32_t hash);

/**
 * A Java class constant.
 */
typedef struct _bvmclazzconstantstruct {

	/** A pointer to a resolved method or field.  Unused for primitive fields. After a field or method is resolved
	 * the resolved pointer is stored here to reduce lookup time. */
	void *resolved_ptr;

	union {

		/** holds a static value */
		bvm_cell_t value;

		// TODO 64: could this be an issue?  the two 16 bit number below as a union on the above cell if it is 64 bit.
		/** for those references that must be recalculated or available at runtime even after a constant
		 * has been resolved - like methodrefs, even after we have resolved it we still need access
		 * to the original method ref_value that was in the constant pool.*/
		struct {
			/** could be class index (for refs), or name index (for name and type info) */
			bvm_uint16_t index1;
			/** could be nameandtypeindex (for refs), or desc index (for name and type info) */
			bvm_uint16_t index2;
		} ref_info;
	} data;
} bvm_clazzconstant_t;

/**
 * A method exception attribute within a class file method definition.
 */
typedef struct _bvmexceptionstruct {

	/** Start PC of the exception's range */
	bvm_uint16_t start_pc;

	/** End PC of the exception's range */
	bvm_uint16_t end_pc;

	/** The PC of the handler code for the exception */
	bvm_uint16_t handler_pc;

	/** The catch type of exception as per JVMS.  When a class is loaded this will contain a
	 * pointer to the pooled class name of the exception class or \ NULL if the catch type is
	 * for a 'finally'. */
	bvm_utfstring_t *catch_type;

} bvm_exception_t;

#if (BVM_LINE_NUMBERS_ENABLE || BVM_DEBUGGER_ENABLE)

/**
 * A source code line number attribute within a class file method definition.  Also see the
 * #BVM_LINE_NUMBERS_ENABLE compile-time option.
 */
typedef struct _bvmlinenumberstruct {
	bvm_uint16_t start_pc;
	bvm_uint16_t line_nr;
} bvm_linenumber_t;

#endif

#if BVM_DEBUGGER_ENABLE

/**
 * A structure for method locals variables for use in debugging.
 */
typedef struct _bvmlocalvarsstruct {
	bvm_uint16_t start_pc;
	bvm_uint16_t length;
	bvm_utfstring_t *name;
	bvm_utfstring_t *desc;
	bvm_uint16_t index;
} bvm_local_variable_t;

#endif

/**
 * A Java class method.
 */
typedef struct _bvmmethodstruct {

	/** the #bvm_instance_clazz_t this method belongs to */
	struct _bvminstanceclazzstruct *clazz;

	/** The method access flags.  A number of macros exist to ease usage of this field.
	 * Refer macros starting with \c 'METHOD_Is' like #BVM_METHOD_IsAbstract.
	 */
	bvm_uint16_t access_flags;

	/** The number of exceptions caught within the method */
	bvm_uint16_t exceptions_count;

	/** handle to a list of exception definitions */
	bvm_exception_t *exceptions;

	/** handle to the pooled #bvm_utfstring_t name */
	bvm_utfstring_t *name;

	/** handle to the pooled #bvm_utfstring_t desc */
	bvm_utfstring_t *jni_signature;

	/** A pointer to either the Java bytecode or a native method. */
	union {
		bvm_uint8_t *bytecode;
		bvm_native_method_t nativemethod;
	} code;

	/** the number of arguments this method has */
	bvm_uint8_t num_args;

	/** Number of cells returned by this method for a return value? \c 0 (zero) if there is no return
	 * value, \c 1 for a primitive or reference return value, 2 for a long return value. */
	bvm_uint8_t returns_value;

	/** The maximum stack depth used by the method - with \c max_locals used to calculate stack frame size. */
	bvm_uint16_t max_stack;

	/** The number of locals used by the method - with \c max_stack used to calculate stack frame size. */
	bvm_uint16_t max_locals;

#if (BVM_LINE_NUMBERS_ENABLE || BVM_DEBUGGER_ENABLE)
	/** Number of #bvm_linenumber_t defs this method has */
	bvm_uint16_t line_number_count;
	/** Pointer to a list of #bvm_linenumber_t for the method */
	bvm_linenumber_t *line_numbers;
#endif

#if BVM_DEBUGGER_ENABLE

#if BVM_DEBUGGER_SIGNATURES_ENABLE
	/** Handle to utf string Java 5 generic signature for the method (if any). */
	bvm_utfstring_t *generic_signature;
#endif

	/** the number of bytecodes in the method */
	bvm_uint32_t code_length;

	/** the number of local variables the method has. */
	bvm_uint16_t local_variable_count;

	/** The local variable table */
	bvm_local_variable_t *local_variables;

#endif

} bvm_method_t;

#define BVM_COMMON_CLAZZ_INFO										\
	/** Java Magic number - always \c 0xCAFEBABE */   				\
	bvm_uint32_t magic_number;										\
	/** Class lifecycle state - for class init */    				\
	bvm_int32_t state;										        \
	/** Class access flags */										\
	bvm_uint16_t access_flags;										\
	/** Class name in #bvm_utfstring_t pool */						\
	bvm_utfstring_t *name;											\
	/** Package name in #bvm_utfstring_t pool */					\
	bvm_utfstring_t *package;										\
	/** The \c java.lang.Class object for this class */				\
	struct _bvmclassinstancestruct *class_obj;						\
	/** The clazz of the superclass of this class.  \c NULL if class is \c java.lang.Object class */ \
	struct _bvminstanceclazzstruct *super_clazz;					\
	/** The next clazz in the #bvm_clazz_t pool */ 					\
	bvm_clazz_t *next;												\
	/** The class loader object reference of this class */			\
	bvm_classloader_obj_t *classloader_obj;


/**
 * Super-struct for clazz definitions.
 */
struct _bvmclazzstruct {
	BVM_COMMON_CLAZZ_INFO

#if BVM_DEBUGGER_ENABLE
	bvm_utfstring_t *jni_signature;
#endif

} /* bvm_clazz_t is forward defined */ ;

/**
 * A Java class for user-definable objects - not arrays or primitives.
 */
struct _bvminstanceclazzstruct {

	/** include all common class stuff */
	BVM_COMMON_CLAZZ_INFO

#if BVM_DEBUGGER_ENABLE
	bvm_utfstring_t *jni_signature;
	bvm_uint16_t minor_version;
	bvm_uint16_t major_version;
#endif

	/** The constant pool associated with the class.  The first constant is the number of constants and
	 * all subsequent constants are constants as defined in the class file.  Why?  The class
	 * file spec has constants as 1-based, not zero-based.  Using a structure like this means
	 * direct access to a constant from the index defined in bytecode. We gain 4 bytes per class, but
	 * the speed of access is worth it - we access the constant pool a lot. */
	bvm_clazzconstant_t *constant_pool;

	/** Number of interfaces implemented by this class */
	bvm_uint16_t interfaces_count;

	/** Pointer to first interface #bvm_instance_clazz_t for this class */
	struct _bvminstanceclazzstruct **interfaces;

	/** Number of fields defined in this class */
	bvm_uint16_t fields_count;

	/** The offset into the #fields table of the first virtual field - static
	 * fields are all first in the list - virtual field follow statics in the list */
	bvm_uint16_t virtual_field_offset;

	/** Pointer to the first element of an array of field defs for the class*/
	bvm_field_t *fields;

	/** Pointer to a list of static values for longs.  Putting them in this separate list
	 * means the field struct does not have to hold 64bit static values (albiet at the lesser
	 * expense of increased clazz size for those classes that do not have static longs */
	bvm_int64_t *static_longs;

	/** Number of methods in this class */
	bvm_uint16_t methods_count;

	/** Pointer to the first element of an array of methods for the class*/
	bvm_method_t *methods;

	/**
	 * Holds the cumulative number of non-static fields (including fields in superclasses)
	 * for this class def.  Using this count, we can easily calculate how much memory
	 * an instance of this class will consume.  memory = instance_fields_count * sizeof(bvm_cell_t).  Easy.
	 */
	bvm_uint16_t instance_fields_count;

#if (BVM_LINE_NUMBERS_ENABLE || BVM_DEBUGGER_ENABLE)
	/** The source file name in the #bvm_utfstring_t pool */
	bvm_utfstring_t *source_file_name;
#endif

#if BVM_DEBUGGER_ENABLE

#if BVM_DEBUGGER_JSR045_ENABLE
	/** The source file debug extension for JSR045 */
	bvm_utfstring_t *source_debug_extension;
#endif

#if BVM_DEBUGGER_SIGNATURES_ENABLE
	/** The Java 1.6 generic signature (if any) */
	bvm_utfstring_t *generic_signature;
#endif

#endif

} /* bvm_instance_clazz_t is forward defined */;

/**
 * A Java array class.
 */
struct _bvmarrayclazzstruct {
	BVM_COMMON_CLAZZ_INFO

#if BVM_DEBUGGER_ENABLE
	bvm_utfstring_t *jni_signature;
#endif

	/** The component type of the array (BVM_T_OBJECT, TARRAY etc) */
	bvm_jtype_t component_jtype;

	/** the #bvm_clazz_t of the component reference type of the array if #component_jtype is
	 * #BVM_T_OBJECT or #BVM_T_ARRAY.  If #component_jtype is not one of these values, the value
	 * this member has is undefined (should be \c NULL). */
	bvm_clazz_t *component_clazz;

} /* bvm_array_clazz_t is forward defined */;

/**
 * A Java primitive class.
 */
struct _bvmprimitiveclazzstruct {
	BVM_COMMON_CLAZZ_INFO

#if BVM_DEBUGGER_ENABLE
	bvm_utfstring_t *jni_signature;
#endif

} /* bvm_primitive_clazz_t is forward defined */;


/* prototypes */
bvm_clazz_t *bvm_clazz_get(bvm_classloader_obj_t *loader, bvm_utfstring_t *clazzname);
bvm_clazz_t *bvm_clazz_get_by_reflection(bvm_classloader_obj_t *loader, bvm_utfstring_t *clazzname);
bvm_clazz_t *bvm_clazz_get_c(bvm_classloader_obj_t *loader, const char *clazzname);
bvm_method_t *bvm_clazz_method_get(bvm_instance_clazz_t *clazz, bvm_utfstring_t *name, bvm_utfstring_t *desc, int mode);
bvm_field_t *bvm_clazz_field_get(bvm_instance_clazz_t *clazz, bvm_utfstring_t *name, bvm_utfstring_t *desc);
bvm_field_t *bvm_clazz_resolve_cp_field(bvm_instance_clazz_t *clazz, bvm_uint16_t index, bvm_bool_t is_static);
bvm_clazz_t *bvm_clazz_resolve_cp_clazz(bvm_instance_clazz_t *clazz, bvm_uint16_t index);
bvm_method_t *bvm_clazz_resolve_cp_method(bvm_instance_clazz_t *clazz, bvm_uint16_t index);
bvm_jtype_t bvm_clazz_get_array_type(char c);
bvm_int32_t bvm_clazz_get_source_line(bvm_method_t *method, bvm_uint8_t *pc);

bvm_utfstring_t *bvm_clazz_cp_utfstring_from_index(bvm_instance_clazz_t *c, bvm_uint16_t i);
bvm_utfstring_t *bvm_clazz_cp_ref_name(bvm_instance_clazz_t *c, bvm_uint16_t i);
bvm_utfstring_t *bvm_clazz_cp_ref_clazzname(bvm_instance_clazz_t *c, bvm_uint16_t i);
bvm_utfstring_t *bvm_clazz_cp_ref_sig(bvm_instance_clazz_t *c, bvm_uint16_t i);

bvm_bool_t bvm_clazz_implements_interface(bvm_instance_clazz_t *clazz, bvm_instance_clazz_t *interface);
bvm_bool_t bvm_clazz_is_assignable_from(bvm_clazz_t *from_clazz, bvm_clazz_t *to_clazz);
bvm_bool_t bvm_clazz_is_instanceof(bvm_obj_t * obj, bvm_clazz_t * clazz);
bvm_bool_t bvm_clazz_is_member_accessible(bvm_uint16_t access_flags, bvm_instance_clazz_t *from_clazz, bvm_instance_clazz_t *defining_clazz);
bvm_bool_t bvm_clazz_is_class_accessible(bvm_clazz_t *from_clazz, bvm_clazz_t *to_clazz);
bvm_bool_t bvm_clazz_is_subclass_of(bvm_clazz_t *from_clazz, bvm_clazz_t *to_clazz);

bvm_bool_t bvm_clazz_initialise(bvm_instance_clazz_t *clazz);

#endif /*BVM_CLAZZ_H_*/
