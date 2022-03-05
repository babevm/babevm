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

 The Babe VM Native Interface

  @author Greg McCreath
  @since 0.0.10

 *
 * @section ov Overview
 *
 * This unit contains the code for the VM Native Interface.  In essence the NI, as it is
 * referred to in this document and in the VM, is Java Native Interface (JNI) like, but is not the JNI.  This
 * is a much simpler animal.
 *
 * The JNI is a generic interface for C and C++ developers to interface functionality written in
 * those language with Java.  Developing with the native interface is about implementing Java methods that
 * have the 'native' modifier in their method description in C or C++.
 *
 * The JNI provides a VM independent means to link C/C++ with Java for a particular platform.  It is
 * designed to be quite generic and exposes very useful functionality.
 *
 * Importantly, all java types defined by the JNI are opaque - the JNI developer does not know of
 * the internal structure or memory management techniques of the particular VM on which they
 * are operating.
 *
 * This VM, quite simply, does not require, or cannot support all the functionality defined in
 * the JNI.  This is why this interface is simply the 'NI'.  However, it is a robust subset of the JNI
 * and provides every JNI feature that can be support by this VM.  In some cases the JNI functionality
 * has been simplified to cut down on the amount of code and functions required - but mostly, the
 * functions implemented here are exactly the function prototypes of the JNI.
 *
 * The following is a point-by-point discussion of the key differences between the JNI and this NI.
 *
 * @li Static compile-time: Native method cannot be implemented in dynamically loaded modules - all
 * native functions are statically linked.
 * @li No argument marshalling: The JNI auto-magically figures the right native function to call based
 * on signature and cleverly passes parameters to the function.  In this VM, all native functions
 * satisfy the same function pointer typedef.
 * @li Class definition: It is not possible to have the VM load a class from its raw bytes via this NI.
 * @li Threads.  The JNI defines a number of functions for thread control such as acquiring and
 * releasing an object monitor.  This is not supported in the NI. The VM's green threads
 * implementation does not make it possible.
 * @li Method invocation: An additional side effect of green-threads is it also makes it
 * impossible to support calling a java method from the NI in a synchronous manner.  Unfortunately,
 * this is what method invocation requires.
 * @li Object construction: A flow-on from not being able to call java methods from the NI is that it
 * is not possible to call either an object's &lt;init&gt; method or a class's &lt;clinit&gt; method.
 * This limitation would mean that developers could not rely on the state on an new-created object
 * or just-loaded class.  However, creation of String and array objects is fully supported.
 * @li Global/local/weak references: the NI does not support the JNI memory management functions
 * for references.  However, it *is* possible to create temporary objects for use and free them.
 * @li Array: Not all JNI array access functions have been implemented, but all the same functionality
 * can be achieved albeit with some different, more generic function calls.
 * @li Reflection: Not support by the VM, therefore not supported by the NI.
 *
 * That might seem like a large list, but the remaining functionality is actually a very useful subset.
 *
 * @section param Method Parameters
 *
 * Unlike the JNI which has explicit arguments to native methods that match the method signature of the
 * Java method, the NI passes parameters to native methods as an array of \c void pointers.  This means
 * all NI native methods have the same signature:
 *
 @verbatim
 void methodname(void *args)
 @endverbatim
 *
 * NI native methods get their parameter values using a series of related NI 'get parameter' functions :
 *
 @verbatim
 NI_GetParameterAsTYPE(x)
 @endverbatim
 *
 * Where 'TYPE' is \c Object, \c Int, \c Boolean, \c Short, \c Byte etc, and \c x is the number of the
 * required parameter in the parameter list.
 *
 * The parameter list always starts at '0'.  For instance methods, the zero-th parameter will be the
 * 'this' equivalent in Java - it is the object that the method is being invoked upon.  For instance
 * methods thus, the first real 'value' parameter is parameter '1', not parameter '0'.  So, to get the 'this'
 * for a method use \c NI_GetParameterAsObject(0).
 *
 * For static methods however, the zero-th parameter will be the first value parameter - there is no
 * 'this' for static methods.  If you need to know the class of the executing method,
 * use #NI_GetCurrentClass().
 *
 * Using the correct parameter number and type is completely the responsibility of the
 * developer.  VM behaviour is undefined if invalid parameter number are used, or the incorrect type
 * of parameter is used.
 *
 * @section rv Return Values
 *
 * Values are returned from native methods using :
 *
 @verbatim
 NI_ReturnTYPE(v)
 @endverbatim
 *
 * Where 'TYPE' is \c Object, \c Int, \c Boolean, \c Short, \c Byte etc, and \c v is the value to return.  So,
 * returning the int value '10' from a native method would be \c NI_ReturnInt(10).
 *
 * Every native method must return a value - even if the java method is a 'void' method.  Use
 * \c NI_ReturnVoid() to return void to the VM.  Note that \c NI_ReturnVoid does not require a value
 * parameter.
 *
 * @section malloc Memory Management
 *
 * Some NI functions allocate memory for objects or data.  These are referred to a 'local' references.  Local
 * references are valid only during the execution of the current native method.  Local references created
 * by a native method may be passed safely to other functions during the native method execution.  However,
 * after the native method has finished executing, the value of a local reference is undefined.
 *
 * The only way a local reference can exist beyond the lifetime of a single native method execution is
 * by setting that value of an object or class field to the local reference.  Furthermore, that object must
 * be visible to the running Java program and must be 'reachable' by the GC.
 *
 * Developers may allocate specific local memory for a task by using #NI_MallocLocal.  This allocates a block of
 * memory that has the same characteristics as the local references returned by a function like
 * (say) #NI_NewByteArray - the handle to it is only valid during the current native method execution.
 *
 * Local references are transient and will be swept up by the GC the next time it runs after the current native
 * method execution terminates.
 *
 * The #NI_Free function may be used by the developer to free a local reference after use - this will
 * immediately place the memory back into the heap and make it available for further object creation etc.  It
 * is considered good practice for developers to clean up local references created during native method
 * execution.
 *
 * To allocate memory that exists beyond the lifetime of a native method execution, developers may use
 * the #NI_MallocGlobal function.  This allocated memory from the VM heap that can be used anywhere anytime
 * by the developer and is valid until free manually using #bvm_heap_free.
 *
 * Note that the NI provides no facilities to create a global object reference.  Global object references are
 * those object references that are in the running Java program and are 'reachable' by the GC.
 *
 * Developers must be extremely careful when using memory.  These functions permit access to the raw
 * VM-managed heap. Make a mistake and the consequences may be subtle and very hard to track down.
 *
 * No equivalent to 'realloc' is provided.
 *
 * There is a limited number of local references that may exist.  This number is determined by the value in
 * #bvm_gl_gc_transient_roots_depth - which default to the compile time constant #BVM_GC_TRANSIENT_ROOTS_DEPTH.
 *
 * @section classinit Class Initialisation
 *
 * The Java specification defines that classes must be initialised before their static members
 * or methods are used.  That initialisation may happen well after class loading, but must happen
 * before class usage.
 *
 * This VM has no means of initialising a class from running C code. This means that a call to #NI_FindClass
 * may return \c NULL (even though the class may be loaded and findable) if the class is not
 * yet initialised.
 *
 * Class initialisation must be performed at the Java level and must be performed before a class is used at
 * the native level.  It is the responsibility of the developer to ensure that any class they wish to use at
 * the native level has been initialised at the Java level.
 *
 * Initialising a class at the java level is simple.  Accessing any static field, calling any static method,
 * or instantiating an object of the given class will cause it to be initialised.
 *
 * @section exceptions Exceptions
 *
 * Unlike java, the native interface does not alter the flow of program control when an exception is thrown.  Functions
 * that throw exceptions will indicate via their return code if an exception was thrown during their execution.
 *
 * Until control returns to the VM, an exception is considered 'pending'.
 *
 * When control returns to the VM, if an exception is pending, it will then be thrown and visible to the running
 * java code.  Only the last pending exception will be thrown.
 *
 * Developers must check the return code of functions to determine if any errors occurred, or alternatively,
 * may use the #NI_ExceptionCheck function.
 *
 * Flow of control is not altered by exceptions for the simple reason that the JNI does not do so.  The JNI
 * specifies no BVM_TRY/BVM_CATCH mechanism at the native level and follows a more traditional approach of checking
 * return codes.
 */

#include "../h/bvm.h"

/*********************************************************************************************
 ** NI Related functions
 *********************************************************************************************/

/**
 * Returns the version of the NI interface. 1.0, returns 0x00010000. The upper
 * two bytes represent the major version and the lower two bytes represent the
 * minor version.
 *
 * @return the NI interface version.
 *
 * @throws none.
 */
jint NI_GetVersion() {
	return NI_VERSION;
}

/*********************************************************************************************
 ** Class functions
 *********************************************************************************************/

/**
 * This function loads a class. It searches the directories and zip files specified by the
 * classpath for the current classloader for the class with the specified name.
 *
 * FindClass locates the class loader associated with the current native method. If the
 * native code belongs to the \c NULL loader, then it uses the bootstrap class loader to load the
 * named class or interface.
 *
 * The function assumes the class to be found will be already loaded by the system and will be "initialised".
 * Initialisation is required when the class has static fields or static blocks.  Refer to the JVMS for
 * more details on class initialisation.  If the requested class is not initialised this function will
 * return \c NULL.
 *
 * Note that only instance classes require initialisation.  Array and primitive classes do not.  Array
 * classes for references type do not require initialisation, but, of course, the actual reference component
 * type does before it can be used.
 *
 * The \c name argument is a class descriptor. For example, the descriptor for the java.lang.String class
 * is:
 *
 @verbatim
 java/lang/String
 @endverbatim
 *
 * and the descriptor of the array class java.lang.object[] is:
 *
 @verbatim
 [Ljava/lang/Object;
 @endverbatim
 *
 * Refer the Java Virtual Machine Specification of the Java Native Interface documentation for
 * further examples.
 *
 * @param name a fully-qualified class name (that is, a package name, delimited by "/", followed by the
 * class name). If the name begins with "[" (the array signature character), it returns an
 * array class.  Must not be \c NULL.  The name is represented as a null-terminated UTF-8 string.
 *
 * @return a class object from a fully-qualified name, or \c NULL if the class cannot be found, or
 * is uninitialised, or an error occurred.
 *
 * @throws ClassFormatError if the class data does not specify a valid class.
 * @throws ClassCircularityError if a class or interface would be its own superclass or superinterface.
 * @throws NoClassDefFoundError if no definition for a requested class or interface can be found.
 * @throws OutOfMemoryError if the system runs out of memory.
 */
jclass NI_FindClass(const char *name) {

	bvm_class_obj_t *class_obj = NULL;

	BVM_TRY {
		/* get a handle to the class of the given name using the
		 * classloader of the currently executing native method */
		bvm_clazz_t *clazz = bvm_clazz_get_c(bvm_gl_rx_clazz->classloader_obj, name);

		/* we have no way of initialising a class with the NI, so if the class is
		 * not initialized, return NULL */
		class_obj = BVM_CLAZZ_IsInitialised(clazz) ? clazz->class_obj : NULL;
	} BVM_CATCH(e) {
		bvm_gl_thread_current->pending_exception = e;
		class_obj = NULL;
	} BVM_END_CATCH

	return class_obj;
}

/**
 *
 * If \c clazz represents any class other than the class Object, then this function returns the object
 * that represents the superclass of the class specified by \c clazz.
 *
 * If \c clazz specifies the class java/lang/Object, or \c clazz represents an interface, this function
 * returns \c NULL.
 *
 * @param clazz a Java class object.  Must not be \c NULL.
 * @return the superclass of the class represented by \c clazz , or \c NULL.
 *
 * @throws none.
 *
 */
jclass NI_GetSuperclass(jclass clazz) {

	bvm_clazz_t *refers_to_clazz = ((bvm_class_obj_t *) clazz)->refers_to_clazz;

	/* JVMS: "If this Class represents either the Object class, an interface,
	 * a primitive type, or void, then null is returned."*/
	if ( (refers_to_clazz == (bvm_clazz_t *) BVM_OBJECT_CLAZZ) ||
		 (BVM_CLAZZ_IsInterface(refers_to_clazz) ||
	     !BVM_CLAZZ_IsInstanceClazz(refers_to_clazz))) {
		return NULL;
	}
	/* return the class object for the superclazz */
	return refers_to_clazz->super_clazz->class_obj;
}

/**
 * Determines whether an object of class or interface clazz1 can be safely cast to
 * class or interface clazz2. Both clazz1 and clazz2 must not be \c NULL.
 *
 * @param clazz1 the first class or interface argument.
 * @param clazz2 the second class or interface argument.  Must not be \c NULL.
 *
 * @return \c NI_TRUE if any of the following is true:
 * @li The first and second arguments refer to the same class or interface.
 * @li The second argument refers to java/lang/Object.
 * @li The first argument refer to a subclass of the second argument.
 * @li The first argument refers to a class that has the second argument as one of its interfaces.
 * @li The first and second arguments both refer to array classes with element types X and Y, and
 *   NI_IsAssignableFrom(X, Y) is \c NI_TRUE.
 *
 * Otherwise, this function returns \c NI_FALSE.
 *
 * @throws none.
 */
jboolean NI_IsAssignableFrom(jclass clazz1, jclass clazz2) {

	/* the clazz1 may be a java null - null is assignment compatible with everything */
	bvm_clazz_t *clazz = (clazz1 == NULL) ? NULL : ((bvm_class_obj_t *) clazz1)->refers_to_clazz;

	return bvm_clazz_is_assignable_from(clazz, ((bvm_class_obj_t *) clazz2)->refers_to_clazz);
}

/**
 * Returns the java class object of the currently executing java native
 * method.
 *
 * @return the class object of the currently executing java native method.
 *
 * @throws none.
 */
jclass NI_GetCurrentClass() {
	return bvm_gl_rx_clazz->class_obj;
}

/*********************************************************************************************
 ** Exception functions
 *********************************************************************************************/

/**
 * Causes a java.lang.Throwable object to be thrown. The thrown exception will become pending in
 * the current thread, but does not immediately disrupt native code execution.
 *
 * If an exception is already pending the given throwable \c obj will simply replace the current
 * pending exception and #NI_OK will be returned.
 *
 * @param obj a java.lang.Throwable object.  Must not be \c NULL.

 * @return \c NI_OK on success; otherwise, returns \c NI_ERR.
 */
jint NI_Throw(jthrowable obj) {
	bvm_gl_thread_current->pending_exception = obj;
	return NI_OK;
}

/**
 * Constructs an exception object from the specified class with the message
 * specified by \c message and causes that exception to be thrown.
 *
 * Throwing an exception does not alter the flow of program control like java does.  This function
 * returns a value and control goes to the next C statement.
 *
 * Note: the no-args constructor of the throwable class specified by the \c clazz param is *NOT* called.
 * An object of the \c clazz type is created and thrown, but does not have &lt;init&gt; called on it before
 * it is thrown.
 *
 * @param clazz a subclass of java.lang.Throwable.  Must not be \c NULL.
 * @param msg the message used to construct the java.lang.Throwable object.  A \c NULL means no
 * message.  The msg is represented as a null-terminated UTF-8 string.
 *
 * @return \c NI_OK on success; otherwise, returns \c NI_ERR if the specified exception
 * cannot be thrown.
 *
 * @throws any exception that can occurs in constructing this object.
 */
jint NI_ThrowNew(jclass clazz, const char *msg) {

	BVM_TRY {
		bvm_gl_thread_current->pending_exception = bvm_create_exception( ((bvm_class_obj_t *) clazz)->refers_to_clazz, msg);
	} BVM_CATCH(e) {
		bvm_gl_thread_current->pending_exception = e;
		return NI_ERR;
	} BVM_END_CATCH

	return NI_OK;
}

/**
 * Raises a fatal error and does not expect the virtual machine implementation to recover. Prints the
 * message in a system debugging channel (if supported), such as stderr, and terminates the virtual
 * machine instance.
 *
 * This function does not return.
 *
 * @param msg an error message.  Must not be \c NULL.  The msg is represented as a null-terminated
 * UTF-8 string.
 */
void NI_FatalError(char *msg) {
	BVM_VM_EXIT(-1, msg);
}

/**
 * Determines if an exception has been thrown. The exception stays thrown until either the native
 * code calls #NI_ExceptionClear, or the caller of the native method handles the exception.
 *
 * The difference between this function and #NI_ExceptionOccurred is that this function returns a
 * jboolean to indicate whether there is a pending exception, whereas #NI_ExceptionOccurred returns a
 * reference to the pending exception, or returns \c NULL if there is no pending exception.
 *
 * @return \c NI_TRUE if there is a pending exception, or \c NI_FALSE if there is no pending exception.
 *
 * @throws none.
 */
jboolean NI_ExceptionCheck() {
	return (bvm_gl_thread_current->pending_exception != NULL);
}

/**
 * Clears any pending exception that is currently being thrown in the current thread. If no exception
 * is currently being thrown, this function has no effect. This function has no effect on
 * exceptions pending on other threads.
 *
 * @throws none.
 */
void NI_ExceptionClear() {
	bvm_gl_thread_current->pending_exception = NULL;
}

/**
 * Determines if an exception is pending in the current thread. The exception stays pending until either
 * the native code calls #NI_ExceptionClear, or the caller of the native method handles
 * the exception.
 *
 * The difference between this function and #NI_ExceptionCheck is that NI_ExceptionCheck returns a jboolean
 * to indicate whether there is a pending exception, whereas this function returns a reference
 * to the pending exception, or returns \c NULL if there is no pending exception.
 *
 * @return the exception object that is pending in the current thread, or \c NULL if no
 * exception is pending.
 *
 * @throws none.
 */
jthrowable NI_ExceptionOccurred() {
	return bvm_gl_thread_current->pending_exception;
}

/*********************************************************************************************
 ** Instance functions
 *********************************************************************************************/

/**
 * Returns the class of an object.
 *
 * @param obj a Java object. Must not be \c NULL.
 *
 * @return a java class object.
 *
 * @throws none.
 */
jclass NI_GetObjectClass(jobject obj) {
	return ((bvm_obj_t *)obj)->clazz->class_obj;
}

/**
 * Tests whether an object is an instance of a given class or interface. The \c clazz
 * reference must not be \c NULL.
 *
 * This functionality is subtly different from the \c instanceof functionality of the
 * Java programming language.  Java \c instanceof will return \c false for a \c NULL object.  This function
 * will return \c NI_TRUE for a \c NULL object - in this sense this is operating as a casting check as well.
 *
 * @param obj a reference to an object.
 * @param clazz a reference to a class or interface.
 *
 * @return \c NI_TRUE if \c obj can be cast to \c clazz; otherwise, returns \c NI_FALSE. A \c NULL
 * object can be cast to any class.
 *
 * @throws none.
 *
 */
jboolean NI_IsInstanceOf(jobject obj, jclass clazz) {
	return bvm_clazz_is_assignable_from(((bvm_obj_t *) obj)->clazz, ( (bvm_class_obj_t *) clazz)->refers_to_clazz);
}

/*********************************************************************************************
 ** Virtual getfield functions
 *********************************************************************************************/

/**
 * Returns the field ID for an instance field of a class. The field is specified by its name and
 * descriptor. The \c NI_Get<Type>Field and \c NI_Set<Type>Field families of accessor functions
 * use field IDs to retrieve instance fields. The field must be accessible from the class referred to by
 * \c clazz. The actual field, however, may be defined in one of clazz's superclasses. The clazz
 * reference must not be \c NULL.
 *
 * \c NI_GetFieldID cannot be used to obtain the length of an array. Use #NI_GetArrayLength instead.
 *
 * @param clazz a reference to the class object from which the field ID will be derived.
 * @param name the field name in a null-terminated UTF-8 string.
 * @param sig the field signature as a null-terminated UTF-8 string.
 *
 * @return a field ID, or \c NULL if the operation fails. Returns \c NULL if and only if
 * an invocation of this function has thrown an exception.
 *
 * @throws NoSuchFieldError if the specified field cannot be found.
 */
jfieldID NI_GetFieldID(jclass clazz, const char *name, const char *sig) {

	bvm_field_t *field = NULL;
	bvm_clazz_t *cl = ( (bvm_class_obj_t *) clazz)->refers_to_clazz;

	BVM_TRY {
		/* now get the field */
		field = bvm_clazz_field_get( (bvm_instance_clazz_t *) cl, bvm_utfstring_pool_get_c(name, BVM_TRUE),
				bvm_utfstring_pool_get_c(sig, BVM_TRUE));
	} BVM_CATCH (e) {
		bvm_gl_thread_current->pending_exception = e;
		return NULL;
	} BVM_END_CATCH

	/* not field found?  Nasty, create exception for thread */
	if (field == NULL)
		bvm_gl_thread_current->pending_exception = bvm_create_exception_c(BVM_ERR_NO_SUCH_FIELD_ERROR, name);

	return field;
}

/** macro to make getting at a virtual field cell more tidy */
#define VIRTUAL_FIELD_CELL(obj, fieldID)  ( ((bvm_obj_t *)(obj))->fields[((bvm_field_t *) (fieldID))->value.offset] )

/** macro to make getting at a static field cell more tidy */
#define STATIC_FIELD_CELL(fieldID)   ( ((bvm_field_t *) (fieldID) )->value.static_value)

/**
 * Returns the value of a reference field of an instance. The field to access is specified by a
 * field ID. The field ID must be valid in the class of the obj reference.
 *
 * No correctness checking is performed to determine if the given fieldID really belongs to the
 * given object's class.  
 *
 * @param obj a reference to the instance whose field are to be accessed. Must not be \c NULL.
 * @param fieldID a field ID of the given instance.
 *
 * @return the object value of the field.
 *
 * @throws none.
 */
jobject NI_GetObjectField(jobject obj, jfieldID fieldID) {
	return VIRTUAL_FIELD_CELL(obj, fieldID).ref_value;
}

/**
 * Returns the value of a boolean field of an instance. The field to access is specified by a
 * field ID. The field ID must be valid in the class of the obj reference.
 *
 * No correctness checking is performed to determine if the given fieldID really belongs to the
 * given object's class.  
 *
 * @param obj a reference to the instance whose field are to be accessed. Must not be \c NULL.
 * @param fieldID a field ID of the given instance.
 *
 * @return the boolean value of the field.
 *
 * @throws none.
 */
jboolean NI_GetBooleanField(jobject obj, jfieldID fieldID) {
	/* return cell int as 8 bit int making sure to truncate nicely */
	return (VIRTUAL_FIELD_CELL(obj, fieldID).int_value & 0xFF);
}

/**
 * Returns the value of a byte field of an instance. The field to access is specified by a
 * field ID. The field ID must be valid in the class of the obj reference.
 *
 * No correctness checking is performed to determine if the given fieldID really belongs to the
 * given object's class.  
 *
 * @param obj a reference to the instance whose field are to be accessed. Must not be \c NULL.
 * @param fieldID a field ID of the given instance.
 *
 * @return the byte value of the field.
 *
 * @throws none.
 */
jbyte NI_GetByteField(jobject obj, jfieldID fieldID) {
	/* return as 8 bit int making sure to truncate nicely */
	return (VIRTUAL_FIELD_CELL(obj, fieldID).int_value & 0xFF);
}

/**
 * Returns the value of a char field of an instance. The field to access is specified by a
 * field ID. The field ID must be valid in the class of the obj reference.
 *
 * No correctness checking is performed to determine if the given fieldID really belongs to the
 * given object's class.  
 *
 * @param obj a reference to the instance whose field are to be accessed. Must not be \c NULL.
 * @param fieldID a field ID of the given instance.
 *
 * @return the char value of the field.
 *
 * @throws none.
 */
jchar NI_GetCharField(jobject obj, jfieldID fieldID) {
	/* return as 16 bit making sure to truncate nicely */
	return (VIRTUAL_FIELD_CELL(obj, fieldID).int_value & 0xFFFF);
}

/**
 * Returns the value of a short field of an instance. The field to access is specified by a
 * field ID. The field ID must be valid in the class of the obj reference.
 *
 * No correctness checking is performed to determine if the given fieldID really belongs to the
 * given object's class.  
 *
 * @param obj a reference to the instance whose field are to be accessed. Must not be \c NULL.
 * @param fieldID a field ID of the given instance.
 *
 * @return the short value of the field.
 *
 * @throws none.
 */
jshort NI_GetShortField(jobject obj, jfieldID fieldID) {
	/* return as 16 bit int making sure to truncate nicely */
	return (VIRTUAL_FIELD_CELL(obj, fieldID).int_value & 0xFFFF);
}

/**
 * Returns the value of an int field of an instance. The field to access is specified by a
 * field ID. The field ID must be valid in the class of the obj reference.
 *
 * No correctness checking is performed to determine if the given fieldID really belongs to the
 * given object's class.  
 *
 * @param obj a reference to the instance whose field are to be accessed. Must not be \c NULL.
 * @param fieldID a field ID of the given instance.
 *
 * @return the int value of the field.
 *
 * @throws none.
 */
jint NI_GetIntField(jobject obj, jfieldID fieldID) {
	return VIRTUAL_FIELD_CELL(obj, fieldID).int_value;
}

/**
 * Returns the value of a long field of an instance. The field to access is specified by a
 * field ID. The field ID must be valid in the class of the obj reference.
 *
 * No correctness checking is performed to determine if the given fieldID really belongs to the
 * given object's class.  
 *
 * @param obj a reference to the instance whose field are to be accessed. Must not be \c NULL.
 * @param fieldID a field ID of the given instance.
 *
 * @return the long value of the field.
 *
 * @throws none.
 */
jlong NI_GetLongField(jobject obj, jfieldID fieldID) {
	/* create a long using starting at the first field cell of the long field */
	return BVM_INT64_from_cells(&VIRTUAL_FIELD_CELL(obj, fieldID));
}

#if BVM_FLOAT_ENABLE

/**
 * Returns the value of a float field of an instance. The field to access is specified by a
 * field ID. The field ID must be valid in the class of the obj reference.
 *
 * No correctness checking is performed to determine if the given fieldID really belongs to the
 * given object's class.  
 *
 * @param obj a reference to the instance whose field are to be accessed. Must not be \c NULL.
 * @param fieldID a field ID of the given instance.
 *
 * @return the float value of the field.
 *
 * @throws none.
 */
jfloat NI_GetFloatField(jobject obj, jfieldID fieldID) {
	return VIRTUAL_FIELD_CELL(obj, fieldID).float_value;
}

/**
 * Returns the value of a double field of an instance. The field to access is specified by a
 * field ID. The field ID must be valid in the class of the obj reference.
 *
 * No correctness checking is performed to determine if the given fieldID really belongs to the
 * given object's class.  
 *
 * @param obj a reference to the instance whose field are to be accessed. Must not be \c NULL.
 * @param fieldID a field ID of the given instance.
 *
 * @return the double value of the field.
 *
 * @throws none.
 */
jdouble NI_GetDoubleField(jobject obj, jfieldID fieldID) {
	/* create a double using starting at the first field cell of the long field */
	return BVM_DOUBLE_from_cells(&VIRTUAL_FIELD_CELL(obj, fieldID));
}

#endif

/*********************************************************************************************
 ** Virtual setfield functions
 *********************************************************************************************/

/**
 * Sets the value of an instance field of an object. The obj reference must not be \c NULL.
 *
 * No correctness checking is performed to validate the given field ID actually belongs to
 * the given object reference.  
 *
 * @param obj a reference to the instance whose field is to be set. Must not be \c NULL.
 * @param fieldID a field ID of the given instance.
 * @param val the new value.
 *
 * @throws none.
 */
void NI_SetObjectField(jobject obj, jfieldID fieldID, jobject val) {
	VIRTUAL_FIELD_CELL(obj, fieldID).ref_value = val;
}

/**
 * Sets the value of a boolean field of an object. The obj reference must not be \c NULL.
 *
 * No correctness checking is performed to validate the given field ID actually belongs to
 * the given object reference.  
 *
 * @param obj a reference to the instance whose field is to be set. Must not be \c NULL.
 * @param fieldID a field ID of the given instance.
 * @param val the new value.
 *
 * @throws none.
*/
void NI_SetBooleanField(jobject obj, jfieldID fieldID, jboolean val) {
	VIRTUAL_FIELD_CELL(obj, fieldID).int_value = val;
}

/**
 * Sets the value of a byte field of an object. The obj reference must not be \c NULL.
 *
 * No correctness checking is performed to validate the given field ID actually belongs to
 * the given object reference.  
 *
 * @param obj a reference to the instance whose field is to be set. Must not be \c NULL.
 * @param fieldID a field ID of the given instance.
 * @param val the new value.
 *
 * @throws none.
*/
void NI_SetByteField(jobject obj, jfieldID fieldID, jbyte val) {
	VIRTUAL_FIELD_CELL(obj, fieldID).int_value = val;
}

/**
 * Sets the value of a char field of an object. The obj reference must not be \c NULL.
 *
 * No correctness checking is performed to validate the given field ID actually belongs to
 * the given object reference.  
 *
 * @param obj a reference to the instance whose field is to be set. Must not be \c NULL.
 * @param fieldID a field ID of the given instance.
 * @param val the new value.
 *
 * @throws none.
*/
void NI_SetCharField(jobject obj, jfieldID fieldID, jchar val) {
	VIRTUAL_FIELD_CELL(obj, fieldID).int_value = val;
}

/**
 * Sets the value of a short field of an object. The obj reference must not be \c NULL.
 *
 * No correctness checking is performed to validate the given field ID actually belongs to
 * the given object reference.  
 *
 * @param obj a reference to the instance whose field is to be set. Must not be \c NULL.
 * @param fieldID a field ID of the given instance.
 * @param val the new value.
 *
 * @throws none.
*/
void NI_SetShortField(jobject obj, jfieldID fieldID, jshort val) {
	VIRTUAL_FIELD_CELL(obj, fieldID).int_value = val;
}

/**
 * Sets the value of a int field of an object. The obj reference must not be \c NULL.
 *
 * No correctness checking is performed to validate the given field ID actually belongs to
 * the given object reference.  
 *
 * @param obj a reference to the instance whose field is to be set. Must not be \c NULL.
 * @param fieldID a field ID of the given instance.
 * @param val the new value.
 *
 * @throws none.
*/
void NI_SetIntField(jobject obj, jfieldID fieldID, jint val) {
	VIRTUAL_FIELD_CELL(obj, fieldID).int_value = val;
}

/**
 * Sets the value of a long field of an object. The obj reference must not be \c NULL.
 *
 * No correctness checking is performed to validate the given field ID actually belongs to
 * the given object reference.  
 *
 * @param obj a reference to the instance whose field is to be set. Must not be \c NULL.
 * @param fieldID a field ID of the given instance.
 * @param val the new value.
 *
 * @throws none.
*/
void NI_SetLongField(jobject obj, jfieldID fieldID, jlong val) {
	/* copy the long value to the field starting at the address of the first cell of the
	 * long field */
	BVM_INT64_to_cells(&VIRTUAL_FIELD_CELL(obj, fieldID), val);
//	BVM_INT64_copy(&VIRTUAL_FIELD_CELL(obj, fieldID), &val);
}

#if BVM_FLOAT_ENABLE

/**
 * Sets the value of a float field of an object. The obj reference must not be \c NULL.
 *
 * No correctness checking is performed to validate the given field ID actually belongs to
 * the given object reference.  
 *
 * @param obj a reference to the instance whose field is to be set. Must not be \c NULL.
 * @param fieldID a field ID of the given instance.
 * @param val the new value.
 *
 * @throws none.
*/
void NI_SetFloatField(jobject obj, jfieldID fieldID, jfloat val) {
	VIRTUAL_FIELD_CELL(obj, fieldID).float_value = val;
}

/**
 * Sets the value of a double field of an object. The obj reference must not be \c NULL.
 *
 * No correctness checking is performed to validate the given field ID actually belongs to
 * the given object reference.  
 *
 * @param obj a reference to the instance whose field is to be set. Must not be \c NULL.
 * @param fieldID a field ID of the given instance.
 * @param val the new value.
 *
 * @throws none.
*/
void NI_SetDoubleField(jobject obj, jfieldID fieldID, jdouble val) {
	/* copy the double value to the field starting at the address of the first cell of the
	 * field */
	BVM_DOUBLE_to_cells(&VIRTUAL_FIELD_CELL(obj, fieldID), val);
//	BVM_DOUBLE_copy(&VIRTUAL_FIELD_CELL(obj, fieldID), &val);
}


#endif

/*********************************************************************************************
 ** Static getfield functions
 *********************************************************************************************/

/**
 * Returns the field ID for a static field of a class or interface. The field may be defined in the
 * class or interface referred to by clazz or in one of its superclass or superinterfaces. The
 * clazz reference must not be \c NULL. The field is specified by its name and descriptor. The
 * \c NI_GetStatic<Type>Field and \c NI_SetStatic<Type>Field families of accessor functions use
 * static field IDs to retrieve static fields.
 *
 * @param clazz a reference to the class or interface object whose static field is to be accessed.
 * @param name the static field name in a null-terminated UTF-8 string.
 * @param sig the field descriptor in a null-terminated UTF-8 string.
 *
 * @return a field ID, or \c NULL if the operation fails. Returns \c NULL if and only if
 * an invocation of this function has thrown an exception.
 *
 * @throws NoSuchFieldError if the specified field cannot be found.
 * @throws OutOfMemoryError if the system runs out of memory.
 */
jfieldID NI_GetStaticFieldID(jclass clazz, const char *name, const char *sig) {
	return NI_GetFieldID(clazz, name, sig);
}

/**
 * Returns the value of a static reference field of an clazz. The field to access is specified by a
 * field ID. The field ID must be valid in the class or one of its superclasses or
 * implemented interfaces.
 *
 * No correctness checking is performed to determine if the given fieldID really belongs to the
 * given class.  
 *
 * @param clazz a reference to the class or interface object whose static field is to be accessed.
 * @param fieldID a field ID of the given instance.
 *
 * @return the reference value of the field.
 *
 * @throws none.
 */
jobject NI_GetStaticObjectField(jclass clazz, jfieldID fieldID) {
    UNUSED(clazz);
	return STATIC_FIELD_CELL(fieldID).ref_value;
}

/**
 * Returns the value of a static boolean field of an clazz. The field to access is specified by a
 * field ID. The field ID must be valid in the class or one of its superclasses or
 * implemented interfaces.
 *
 * No correctness checking is performed to determine if the given fieldID really belongs to the
 * given class.  
 *
 * @param clazz a reference to the class or interface object whose static field is to be accessed.
 * @param fieldID a field ID of the given instance.
 *
 * @return the boolean value of the field.
 *
 * @throws none.
 */
jboolean NI_GetStaticBooleanField(jclass clazz, jfieldID fieldID) {
    UNUSED(clazz);
	return (STATIC_FIELD_CELL(fieldID).int_value & 0xFF);
}

/**
 * Returns the value of a static byte field of an clazz. The field to access is specified by a
 * field ID. The field ID must be valid in the class or one of its superclasses or
 * implemented interfaces.
 *
 * No correctness checking is performed to determine if the given fieldID really belongs to the
 * given class.  
 *
 * @param clazz a reference to the class or interface object whose static field is to be accessed.
 * @param fieldID a field ID of the given instance.
 *
 * @return the byte value of the field.
 *
 * @throws none.
 */
jbyte NI_GetStaticByteField(jclass clazz, jfieldID fieldID) {
    UNUSED(clazz);
	return (STATIC_FIELD_CELL(fieldID).int_value  & 0xFF);
}

/**
 * Returns the value of a static char field of an clazz. The field to access is specified by a
 * field ID. The field ID must be valid in the class or one of its superclasses or
 * implemented interfaces.
 *
 * No correctness checking is performed to determine if the given fieldID really belongs to the
 * given class.  
 *
 * @param clazz a reference to the class or interface object whose static field is to be accessed.
 * @param fieldID a field ID of the given instance.
 *
 * @return the char value of the field.
 *
 * @throws none.
 */
jchar NI_GetStaticCharField(jclass clazz, jfieldID fieldID) {
    UNUSED(clazz);
	return (STATIC_FIELD_CELL(fieldID).int_value  & 0xFFFF);
}

/**
 * Returns the value of a static short field of an clazz. The field to access is specified by a
 * field ID. The field ID must be valid in the class or one of its superclasses or
 * implemented interfaces.
 *
 * No correctness checking is performed to determine if the given fieldID really belongs to the
 * given class.  
 *
 * @param clazz a reference to the class or interface object whose static field is to be accessed.
 * @param fieldID a field ID of the given instance.
 *
 * @return the short value of the field.
 *
 * @throws none.
 */
jshort NI_GetStaticShortField(jclass clazz, jfieldID fieldID) {
    UNUSED(clazz);
	return (STATIC_FIELD_CELL(fieldID).int_value  & 0xFFFF);
}

/**
 * Returns the value of a static int field of an clazz. The field to access is specified by a
 * field ID. The field ID must be valid in the class or one of its superclasses or
 * implemented interfaces.
 *
 * No correctness checking is performed to determine if the given fieldID really belongs to the
 * given class.  
 *
 * @param clazz a reference to the class or interface object whose static field is to be accessed.
 * @param fieldID a field ID of the given instance.
 *
 * @return the int value of the field.
 *
 * @throws none.
 */
jint NI_GetStaticIntField(jclass clazz, jfieldID fieldID) {
    UNUSED(clazz);
	return STATIC_FIELD_CELL(fieldID).int_value;
}

/**
 * Returns the value of a static long field of an clazz. The field to access is specified by a
 * field ID. The field ID must be valid in the class or one of its superclasses or
 * implemented interfaces.
 *
 * No correctness checking is performed to determine if the given fieldID really belongs to the
 * given class.  
 *
 * @param clazz a reference to the class or interface object whose static field is to be accessed.
 * @param fieldID a field ID of the given instance.
 *
 * @return the long value of the field.
 *
 * @throws none.
 */
jlong NI_GetStaticLongField(jclass clazz, jfieldID fieldID) {
    UNUSED(clazz);
	return BVM_INT64_from_cells(STATIC_FIELD_CELL(fieldID).ptr_value);
}

#if BVM_FLOAT_ENABLE

/**
 * Returns the value of a static float field of an clazz. The field to access is specified by a
 * field ID. The field ID must be valid in the class or one of its superclasses or
 * implemented interfaces.
 *
 * No correctness checking is performed to determine if the given fieldID really belongs to the
 * given class.  
 *
 * @param clazz a reference to the class or interface object whose static field is to be accessed.
 * @param fieldID a field ID of the given instance.
 *
 * @return the float value of the field.
 *
 * @throws none.
 */
jfloat NI_GetStaticFloatField(jclass clazz, jfieldID fieldID) {
    UNUSED(clazz);
	return STATIC_FIELD_CELL(fieldID).float_value;
}

/**
 * Returns the value of a static double field of an clazz. The field to access is specified by a
 * field ID. The field ID must be valid in the class or one of its superclasses or
 * implemented interfaces.
 *
 * No correctness checking is performed to determine if the given fieldID really belongs to the
 * given class.  
 *
 * @param clazz a reference to the class or interface object whose static field is to be accessed.
 * @param fieldID a field ID of the given instance.
 *
 * @return the double value of the field.
 *
 * @throws none.
 */
jdouble NI_GetStaticDoubleField(jclass clazz, jfieldID fieldID) {
    UNUSED(clazz);
    return (*(bvm_double_t*)(STATIC_FIELD_CELL(fieldID).ptr_value));
}


#endif

/*********************************************************************************************
 ** Static setfield functions
 *********************************************************************************************/

/**
 * Sets the value of a static field of a class or interface. The field to access is specified by a
 * field ID.
 *
 * No correctness checking is performed to validate the given field ID actually belongs to
 * the given clazz reference.  
 *
 * @param clazz a reference to the clazz whose static field is to be set. Must not be \c NULL.
 * @param fieldID a field ID of the given instance.
 * @param value the new value.
 *
 * @throws none.
 */
void NI_SetStaticObjectField(jclass clazz, jfieldID fieldID, jobject value) {
    UNUSED(clazz);
	STATIC_FIELD_CELL(fieldID).ref_value = value;
}

/**
 * Sets the value of a static field of a class or interface. The field to access is specified by a
 * field ID.
 *
 * No correctness checking is performed to validate the given field ID actually belongs to
 * the given clazz reference.  
 *
 * @param clazz a reference to the clazz whose static field is to be set. Must not be \c NULL.
 * @param fieldID a field ID of the given instance.
 * @param value the new value.
 *
 * @throws none.
 */
void NI_SetStaticBooleanField(jclass clazz, jfieldID fieldID, jboolean value) {
    UNUSED(clazz);
	STATIC_FIELD_CELL(fieldID).int_value = value;
}

/**
 * Sets the value of a static field of a class or interface. The field to access is specified by a
 * field ID.
 *
 * No correctness checking is performed to validate the given field ID actually belongs to
 * the given clazz reference.  
 *
 * @param clazz a reference to the clazz whose static field is to be set. Must not be \c NULL.
 * @param fieldID a field ID of the given instance.
 * @param value the new value.
 *
 * @throws none.
 */
void NI_SetStaticByteField(jclass clazz, jfieldID fieldID, jbyte value) {
    UNUSED(clazz);
	STATIC_FIELD_CELL(fieldID).int_value = value;
}

/**
 * Sets the value of a static field of a class or interface. The field to access is specified by a
 * field ID.
 *
 * No correctness checking is performed to validate the given field ID actually belongs to
 * the given clazz reference.  
 *
 * @param clazz a reference to the clazz whose static field is to be set. Must not be \c NULL.
 * @param fieldID a field ID of the given instance.
 * @param value the new value.
 *
 * @throws none.
 */
void NI_SetStaticCharField(jclass clazz, jfieldID fieldID, jchar value) {
    UNUSED(clazz);
	STATIC_FIELD_CELL(fieldID).int_value = value;
}

/**
 * Sets the value of a static field of a class or interface. The field to access is specified by a
 * field ID.
 *
 * No correctness checking is performed to validate the given field ID actually belongs to
 * the given clazz reference.  
 *
 * @param clazz a reference to the clazz whose static field is to be set. Must not be \c NULL.
 * @param fieldID a field ID of the given instance.
 * @param value the new value.
 *
 * @throws none.
 */
void NI_SetStaticShortField(jclass clazz, jfieldID fieldID, jshort value) {
    UNUSED(clazz);
	STATIC_FIELD_CELL(fieldID).int_value = value;
}

/**
 * Sets the value of a static field of a class or interface. The field to access is specified by a
 * field ID.
 *
 * No correctness checking is performed to validate the given field ID actually belongs to
 * the given clazz reference.  
 *
 * @param clazz a reference to the clazz whose static field is to be set. Must not be \c NULL.
 * @param fieldID a field ID of the given instance.
 * @param value the new value.
 *
 * @throws none.
 */
void NI_SetStaticIntField(jclass clazz, jfieldID fieldID, jint value) {
    UNUSED(clazz);
	STATIC_FIELD_CELL(fieldID).int_value = value;
}

/**
 * Sets the value of a static field of a class or interface. The field to access is specified by a
 * field ID.
 *
 * No correctness checking is performed to validate the given field ID actually belongs to
 * the given clazz reference.  
 *
 * @param clazz a reference to the clazz whose static field is to be set. Must not be \c NULL.
 * @param fieldID a field ID of the given instance.
 * @param value the new value.
 *
 * @throws none.
 */
void NI_SetStaticLongField(jclass clazz, jfieldID fieldID, jlong value) {
    UNUSED(clazz);
    (*(bvm_int64_t*)(STATIC_FIELD_CELL(fieldID).ptr_value)) = value;
}

#if BVM_FLOAT_ENABLE

/**
 * Sets the value of a static field of a class or interface. The field to access is specified by a
 * field ID.
 *
 * No correctness checking is performed to validate the given field ID actually belongs to
 * the given clazz reference.  
 *
 * @param clazz a reference to the clazz whose static field is to be set. Must not be \c NULL.
 * @param fieldID a field ID of the given instance.
 * @param value the new value.
 *
 * @throws none.
 */
void NI_SetStaticFloatField(jclass clazz, jfieldID fieldID, jfloat value) {
    UNUSED(clazz);
	STATIC_FIELD_CELL(fieldID).float_value = value;
}

/**
 * Sets the value of a static double field of a class or interface. The field to access is specified by a
 * field ID.
 *
 * No correctness checking is performed to validate the given field ID actually belongs to
 * the given clazz reference.  
 *
 * @param clazz a reference to the clazz whose static field is to be set. Must not be \c NULL.
 * @param fieldID a field ID of the given instance.
 * @param value the new value.
 *
 * @throws none.
 */
void NI_SetStaticDoubleField(jclass clazz, jfieldID fieldID, jdouble value) {
    UNUSED(clazz);
    BVM_DOUBLE_to_cells(&STATIC_FIELD_CELL(fieldID), value);
}

#endif

/*********************************************************************************************
 ** String functions
 *********************************************************************************************/

/**
 * Constructs a java.lang.String object from the given Unicode characters.
 *
 * @param unicode pointer to the Unicode sequence that makes up the string.
 * @param len length of the Unicode string.
 *
 * @return a local reference to a string object, or \c NULL if the string cannot be
 * constructed. Returns \c NULL if and only if an invocation of this function has thrown an exception.
 *
 * @throws OutOfMemoryError if the system runs out of memory.
 */
jstring NI_NewString(const jchar *unicode, jsize len) {

	bvm_string_obj_t *string = NULL;

	BVM_TRY {
		string = bvm_string_create_from_unicode(unicode, 0, len);
		BVM_MAKE_TRANSIENT_ROOT(string);
	} BVM_CATCH(e) {
		bvm_gl_thread_current->pending_exception = e;
		string = NULL;
	} BVM_END_CATCH

	return string;
}

/**
 * Returns the number of Unicode characters that constitute a string. The given string reference must
 * not be \c NULL.
 *
 * @param str a reference to the string object whose length is to be determined.
 *
 * @return the length of the given String reference
 *
 * @throws none.
 */
jsize NI_GetStringLength(jstring str) {
	return ((bvm_string_obj_t *) str)->length.int_value;
}

/**
 * Returns a pointer to the array of Unicode characters of the string.
 *
 * This gives raw access to the data that constitutes a String object - not a copy.  Although
 * tempting, developers must not to use this function as a means of changing the data of a String
 * object.  The VM optimises the usage of String char array by sharing them between String and
 * StringBuffer/StringBuilder instances - changing data and breaking the Java invariant that
 * String object are immutable is dangerous.
 *
 * @param str a reference to the string object whose elements are to be accessed.
 *
 * @return a pointer to a Unicode string
 *
 * @throws none.
 */
const jchar *NI_GetStringChars(jstring str) {

	bvm_string_obj_t *string = (bvm_string_obj_t *) str;

	/* return the offset into the string char array where the actual string data begins.
	 * Refer Java java/lang/String class for notes about this offset */
	return &string->chars->data[string->offset.int_value];
}

/**
 * Constructs a new java.lang.String object from an array of UTF-8 characters.
 *
 * @param utf the pointer to the null-terminated sequence of UTF-8 characters that makes up the string.  May not
 * exceed \c BVM_MAX_UTFSTRING_LENGTH in length, otherwise a VirtualMachineError will be thrown.
 *
 * @return a local reference to a string object, or \c NULL if the string cannot be constructed.
 * Returns \c NULL if and only if an invocation of this function has thrown an exception.
 *
 * @throws OutOfMemoryError if the system runs out of memory.
 * @throws VirtualMachineError if the size of the given utf string chars is greater than \c BVM_MAX_UTFSTRING_LENGTH.
 */
jstring NI_NewStringUTF(const char *utf) {

	bvm_string_obj_t *string = NULL;

	BVM_TRY {
		string = bvm_string_create_from_cstring(utf);
		BVM_MAKE_TRANSIENT_ROOT(string);
	} BVM_CATCH(e) {
		bvm_gl_thread_current->pending_exception = e;
		string = NULL;
	} BVM_END_CATCH

	return string;
}

/**
 * Returns the number of bytes needed to represent a string in the UTF-8 format. The length does not
 * include the trailing zero character. The given string reference must not be \c NULL.
 *
 * @param str a reference to the string object whose UTF-8 length is to be determined.
 *
 * @return the UTF-8 length of the string.
 *
 * @throws none.
 *
 */
jsize NI_GetStringUTFLength(jstring str) {
	bvm_string_obj_t *string = (bvm_string_obj_t *) str;
	return bvm_str_utf8_length_from_unicode(string->chars->data, string->offset.int_value, string->length.int_value);
}

/**
 * Returns a pointer to an array of UTF-8 characters of the string.
 *
 * @param str a reference to the string object whose elements are to be accessed.
 *
 * @return a pointer to a UTF-8 string, or \c NULL if the operation fails. Returns \c NULL if and only
 * if an invocation of this function has thrown an exception.
 *
 * @throws OutOfMemoryError if the system runs out of memory.
 */
const char* NI_GetStringUTFChars(jstring str) {

    jstring utf = NULL;

	BVM_TRY {
		bvm_string_obj_t *string = (bvm_string_obj_t *) str;
		utf = bvm_str_unicode_to_chararray(string->chars->data, string->offset.int_value, string->length.int_value);
		BVM_MAKE_TRANSIENT_ROOT(utf);
	} BVM_CATCH(e) {
		bvm_gl_thread_current->pending_exception = e;
		utf = NULL;
	} BVM_END_CATCH

	return utf;
}

/**
 * Copies len number of Unicode characters, beginning at \c start. Copies the characters to the
 * given buffer \c buf.
 *
 * @param str a reference to the string object to be copied.
 * @param start the offset within the string at which to start the copy.
 * @param len the number of Unicode characters to copy.
 * @param buf a pointer to a buffer to hold the Unicode characters.
 *
 * @return \c NI_OK if no error occurs, \c NI_ERR if one does.
 *
 * @throws StringIndexOutOfBoundsException if an index overflow error occurs.
 */
jint NI_GetStringRegion(jstring str, jsize start, jsize len, jchar *buf) {

	bvm_string_obj_t *string = (bvm_string_obj_t *) str;

	if ( (start < 0) || (len < 0) || ((start + len + string->offset.int_value) > string->length.int_value)) {
		bvm_gl_thread_current->pending_exception = bvm_create_exception_c(BVM_ERR_STRING_INDEX_OUT_OF_BOUNDS_EXCEPTION, NULL);
		return NI_ERR;
	} else
		memcpy(buf, &string->chars->data[string->offset.int_value + start], len * sizeof(jchar));

	return NI_OK;
}

/**
 *
 * Translates len number of Unicode characters into UTF-8 format. The function begins the translation
 * at offset \c start and places the result in the given buffer buf. The \c str reference and \c buf buffer
 * must not be \c NULL.
 *
 * Note that the \c len argument denotes the number of Unicode characters to be converted, not the number
 * of UTF-8 characters to be copied.  \c Buf will not be null-terminated by this function.
 *
 * @param str a reference to the string object to be copied.
 * @param start the offset within the string at which to start the copy.
 * @param len the number of Unicode characters to copy.
 * @param buf a pointer to a buffer to hold the UTF-8 characters.
 *
 * @return \c NI_OK if no error occurs, \c NI_ERR if one does.
 *
 * @throws StringIndexOutOfBoundsException if an index overflow error occurs.
 */
jint NI_GetStringUTFRegion(jstring str, jsize start, jsize len, char *buf) {
	bvm_string_obj_t *string = (bvm_string_obj_t *) str;

	if ( (start < 0) || (len < 0) || ((start + len + string->offset.int_value) > string->length.int_value)) {
		bvm_gl_thread_current->pending_exception = bvm_create_exception_c(BVM_ERR_STRING_INDEX_OUT_OF_BOUNDS_EXCEPTION, NULL);
		return NI_ERR;
	}

	bvm_str_encode_unicode_to_utf8( string->chars->data, start, len, (bvm_int8_t *) buf);
	return NI_OK;
}

/*********************************************************************************************
 **  Array functions
 *********************************************************************************************/

/**
 * Returns the number of elements in a given array. The array argument may denote an array of any element
 * types, including primitive types such as int or double, or reference types such as the subclasses
 * of java.lang.Object or other array types.
 *
 * @param array a reference to the array object whose length is to be determined.  Must not be \c NULL.
 *
 * @return the length of the array.
 *
 * @throws none.
 */
jsize NI_GetArrayLength(jarray array) {
	return ( (bvm_jarray_obj_t *) array)->length.int_value;
}
/**
 *
 * Constructs a new array holding objects of class, array, or interface element type. All elements are initially
 * set to \c init. The length argument can be zero. The \c clazz reference must not be \c NULL.  The
 * \c len must not be negative.
 *
 * @param len the number of elements in the array to be created.
 * @param clazz class or interface of the elements in the array.
 * @param init a reference to initialization value object. This value can be NULL.
 *
 * @return a local reference to an array object, or \c NULL if the array cannot be
 * constructed. Returns \c NULL if and only if an invocation of this function has
 * thrown an exception.
 *
 * @throws NegativeArraySizeException if the len is negative.
 * @throws any exception that can be thrown during class creation.
 *
 */
jobjectArray NI_NewObjectArray(jsize len, jclass clazz, jobject init) {

	bvm_instance_array_obj_t *array = NULL;

	BVM_TRY {

		if (len < 0)
			bvm_throw_exception(BVM_ERR_NEGATIVE_ARRAY_SIZE_EXCEPTION, NULL);

		/* create the new array object */
		array = bvm_object_alloc_array_reference(len, ( (bvm_class_obj_t *) clazz)->refers_to_clazz);

		/* and init the contents if required */
		if (init != NULL) {
			while (len--)
				array->data[len] = init;
		}

		BVM_MAKE_TRANSIENT_ROOT(array);
	} BVM_CATCH(e) {
		bvm_gl_thread_current->pending_exception = e;
		array = NULL;
	} BVM_END_CATCH

	return array;
}

/**
 * Returns an element of a reference array. The array reference must not be \c NULL.  A \c NULL will be
 * returned if any error occurred, so after an invocation of this method the #NI_ExceptionCheck
 * function will indicate if any exception did occur.
 *
 * @param array a reference to the java.lang.Object array from which the element will be accessed.
 * @param index the array index.
 *
 * @return a reference to the element.  Will return \c NULL if an exception occurred.
 *
 * @throws ArrayIndexOutOfBoundsException: if index does not specify a valid index in the array
 */
jobject NI_GetObjectArrayElement(jobjectArray array, jsize index) {

	bvm_instance_array_obj_t *arr = (bvm_instance_array_obj_t *) array;

	if ( (index < 0) || (index >= arr->length.int_value) ) {
		bvm_gl_thread_current->pending_exception = bvm_create_exception_c(BVM_ERR_ARRAY_INDEX_OUT_OF_BOUNDS_EXCEPTION, NULL);
		return NULL;
	}

	return arr->data[index];
}

/**
 *
 * Sets an element of an Object array. The array reference must not be \c NULL.
 *
 * Although the NI gives the ability to access and manipulate the raw elements of an array, for reference
 * arrays this function is the preferred way to set the value of an element of such an array.
 *
 * Java has strict requirements on checking of type-compatibility when placing objects into a reference
 * array.  This function enforces those checks.  It is also possible to set a reference element value
 * to \c NULL.
 *
 * @param array a reference to an array whose element will be accessed.
 * @param index index of the array element to be accessed.
 * @param val the new value of the array element.
 *
 * @return \c NI_OK if no errors occur, \c NI_ERR if any do.
 *
 * @throws ArrayIndexOutOfBoundsException: if index does not specify a valid index in the array.
 * @throws ArrayStoreException: if the class of value is not assignment compatible with the component
 * class of the array.
 */
jint NI_SetObjectArrayElement(jobjectArray array, jsize index, jobject val) {

	bvm_instance_array_obj_t *arr = (bvm_instance_array_obj_t *) array;

	if ( (index < 0) || (index >= arr->length.int_value) ) {
		bvm_gl_thread_current->pending_exception = bvm_create_exception_c(BVM_ERR_ARRAY_INDEX_OUT_OF_BOUNDS_EXCEPTION, NULL);
		return NI_ERR;
	}

	/* same functionality as VM opcode 'aastore' */
	if ( (val != NULL) &&
		 (!bvm_clazz_is_assignable_from( ((bvm_obj_t *)val)->clazz, arr->clazz->component_clazz)) ) {
		bvm_gl_thread_current->pending_exception = bvm_create_exception_c(BVM_ERR_ARRAYSTORE_EXCEPTION, NULL);
		return NI_ERR;
	}

	arr->data[index] = val;

	return NI_OK;
}

/**
 * Helper function for NI primitive array creation.
 *
 * Wraps #bvm_object_alloc_array_primitive but with NI specific functionality. Basically, catches all exceptions
 * during the creation.  If any occurs it returns \c NULL - otherwise the created object is
 * made a local object and returned.
 *
 * @param len the length of the new array
 * @param type the type of the array according to the JVMS type codes.
 *
 * @return a local array reference or \c NULL if anything going wrong.
 *
 * @throws NegativeArraySizeException if the len is negative.
 */
static bvm_jarray_obj_t *create_primitive_array_helper(bvm_uint32_t len, int type) {

	bvm_jarray_obj_t *array = NULL;

	BVM_TRY {
		if (len < 0)
			bvm_throw_exception(BVM_ERR_NEGATIVE_ARRAY_SIZE_EXCEPTION, NULL);

		array = bvm_object_alloc_array_primitive(len, type);
		BVM_MAKE_TRANSIENT_ROOT(array);
	} BVM_CATCH(e) {
		bvm_gl_thread_current->pending_exception = e;
		array = NULL;
	} BVM_END_CATCH

	return array;
}

/**
 * Constructs a new array of boolean element types. All elements in the newly constructed array are
 * initialized to false.
 *
 * @param len the number of elements in the array to be created.
 *
 * @return a local reference to a primitive array, or \c NULL if the array cannot be
 * constructed. Returns \c NULL if and only if an invocation of this function has thrown an exception.
 *
 * @throws NegativeArraySizeException if the len is negative.
 * @throws OutOfMemoryError if the system runs out of memory.
 */
jbooleanArray NI_NewBooleanArray(jsize len) {
	return create_primitive_array_helper(len, BVM_T_BOOLEAN);
}

/**
 * Constructs a new array of byte element types. All elements in the newly constructed array are
 * initialized to zero.
 *
 * @param len the number of elements in the array to be created.
 *
 * @return a local reference to a primitive array, or \c NULL if the array cannot be
 * constructed. Returns \c NULL if and only if an invocation of this function has thrown an exception.
 *
 * @throws NegativeArraySizeException if the length is negative.
 * @throws OutOfMemoryError if the system runs out of memory.
 */
jbyteArray NI_NewByteArray(jsize len) {
	return create_primitive_array_helper(len, BVM_T_BYTE);
}

/**
 * Constructs a new array of char element types. All elements in the newly constructed array are
 * initialized to zero.
 *
 * @param len the number of elements in the array to be created.
 *
 * @return a local reference to a primitive array, or \c NULL if the array cannot be
 * constructed. Returns \c NULL if and only if an invocation of this function has thrown an exception.
 *
 * @throws NegativeArraySizeException if the length is negative.
 * @throws OutOfMemoryError if the system runs out of memory.
 */
jcharArray NI_NewCharArray(jsize len) {
	return create_primitive_array_helper(len, BVM_T_CHAR);
}

/**
 * Constructs a new array of short element types. All elements in the newly constructed array are
 * initialized to zero.
 *
 * @param len the number of elements in the array to be created.
 *
 * @return a local reference to a primitive array, or \c NULL if the array cannot be
 * constructed. Returns \c NULL if and only if an invocation of this function has thrown an exception.
 *
 * @throws NegativeArraySizeException if the length is negative.
 * @throws OutOfMemoryError if the system runs out of memory.
 */
jshortArray NI_NewShortArray(jsize len) {
	return create_primitive_array_helper(len, BVM_T_SHORT);
}

/**
 * Constructs a new array of int element types. All elements in the newly constructed array are
 * initialized to zero.
 *
 * @param len the number of elements in the array to be created.
 *
 * @return a local reference to a primitive array, or \c NULL if the array cannot be
 * constructed. Returns \c NULL if and only if an invocation of this function has thrown an exception.
 *
 * @throws NegativeArraySizeException if the length is negative.
 * @throws OutOfMemoryError if the system runs out of memory.
 */
jintArray NI_NewIntArray(jsize len) {
	return create_primitive_array_helper(len, BVM_T_INT);
}

/**
 * Constructs a new array of long element types. All elements in the newly constructed array are
 * initialized to zero.
 *
 * @param len the number of elements in the array to be created.
 *
 * @return a local reference to a primitive array, or \c NULL if the array cannot be
 * constructed. Returns \c NULL if and only if an invocation of this function has thrown an exception.
 *
 * @throws NegativeArraySizeException if the length is negative.
 * @throws OutOfMemoryError if the system runs out of memory.
 */
jlongArray NI_NewLongArray(jsize len) {
	return create_primitive_array_helper(len, BVM_T_LONG);
}

#if BVM_FLOAT_ENABLE

/**
 * Constructs a new array of float element types. All elements in the newly constructed array are
 * initialized to zero.
 *
 * @param len the number of elements in the array to be created.
 *
 * @return a local reference to a primitive array, or \c NULL if the array cannot be
 * constructed. Returns \c NULL if and only if an invocation of this function has thrown an exception.
 *
 * @throws NegativeArraySizeException if the length is negative.
 * @throws OutOfMemoryError if the system runs out of memory.
 */
jfloatArray NI_NewFloatArray(jsize len) {
	return create_primitive_array_helper(len, BVM_T_FLOAT);
}

/**
 * Constructs a new array of double element types. All elements in the newly constructed array are
 * initialized to zero.
 *
 * @param len the number of elements in the array to be created.
 *
 * @return a local reference to a primitive array, or \c NULL if the array cannot be
 * constructed. Returns \c NULL if and only if an invocation of this function has thrown an exception.
 *
 * @throws NegativeArraySizeException if the length is negative.
 * @throws OutOfMemoryError if the system runs out of memory.
 */
jdoubleArray NI_NewDoubleArray(jsize len) {
	return create_primitive_array_helper(len, BVM_T_DOUBLE);
}

#endif

/**
 * Returns a pointer to the first element of the raw array data of the the given array.
 *
 * @param array the array to get a handle to the data of.
 *
 * @return a pointer to the array data.
 *
 * @throws none.
 */
jboolean *NI_GetBooleanArrayElements(jbooleanArray array) {
	return ((bvm_jboolean_array_obj_t *) array)->data;
}

/**
 * Returns a pointer to the first element of the raw array data of the the given array.
 *
 * @param array the array to get a handle to the data of.
 *
 * @return a pointer to the array data.
 *
 * @throws none.
 */
jbyte *NI_GetByteArrayElements(jbyteArray array) {
	return ((bvm_jbyte_array_obj_t *) array)->data;
}

/**
 * Returns a pointer to the first element of the raw array data of the the given array.
 *
 * @param array the array to get a handle to the data of.
 *
 * @return a pointer to the array data.
 *
 * @throws none.
 */
jchar *NI_GetCharArrayElements(jcharArray array) {
	return ((bvm_jchar_array_obj_t *) array)->data;
}

/**
 * Returns a pointer to the first element of the raw array data of the the given array.
 *
 * @param array the array to get a handle to the data of.
 *
 * @return a pointer to the array data.
 *
 * @throws none.
 */
jshort *NI_GetShortArrayElements(jshortArray array) {
	return ((bvm_jshort_array_obj_t *) array)->data;
}

/**
 * Returns a pointer to the first element of the raw array data of the the given array.
 *
 * @param array the array to get a handle to the data of.
 *
 * @return a pointer to the array data.
 *
 * @throws none.
 */
jint *NI_GetIntArrayElements(jintArray array) {
	return ((bvm_jint_array_obj_t *) array)->data;
}

/**
 * Returns a pointer to the first element of the raw array data of the the given array.
 *
 * @param array the array to get a handle to the data of.
 *
 * @return a pointer to the array data.
 *
 * @throws none.
 */
jlong *NI_GetLongArrayElements(jlongArray array) {
	return ((bvm_jlong_array_obj_t *) array)->data;
}

#if BVM_FLOAT_ENABLE

/**
 * Returns a pointer to the first element of the raw array data of the the given array.
 *
 * @param array the array to get a handle to the data of.
 *
 * @return a pointer to the array data.
 *
 * @throws none.
 */
jfloat *NI_GetFloatArrayElements(jfloatArray array) {
	return ((bvm_jfloat_array_obj_t *) array)->data;
}

/**
 * Returns a pointer to the first element of the raw array data of the the given array.
 *
 * @param array the array to get a handle to the data of.
 *
 * @return a pointer to the array data.
 *
 * @throws none.
 */
jdouble *NI_GetDoubleArrayElements(jdoubleArray array) {
	return ((bvm_jdouble_array_obj_t *) array)->data;
}

#endif

/*********************************************************************************************
 **  Memory management functions
 *********************************************************************************************/

/**
 * Allocate some memory from the VM-managed heap.  Memory returned here is guaranteed not to be GC'd
 * during the execution of the current native method.  Developers must not create static handles to
 * memory returned using this function - it will be freed the next time the GC is run after the
 * native method execution is finished.  Developers wishing to allocate memory that will not be GC'd
 * should use #NI_MallocGlobal.
 *
 * Typically, this function is used to get memory for some transient purpose.  After usage, it is good
 * practice to perform an #NI_Free on the handle returned by this function.  This puts the memory back
 * in the VM-managed heap immediately and makes it available for allocation again.
 *
 * Reference fields of objects or classes may *not* be set to the handle returned by this function.  The handle returned
 * here is *not* a handle to a java object -  it is a handle to raw memory
 *
 * Memory returned by this function is not zeroed or initialised in any way.
 *
 * @param size the amount of memory (in bytes) to allocate.
 *
 * @return a void pointer handle to allocated memory or \ c NULL if an error occurs.  Returns \c NULL if and
 * only if an invocation of this function has thrown an exception.
 *
 * @throws OutOfMemoryError if the system runs out of memory.
 */
void *NI_MallocLocal(jint size) {

	void *ptr = NULL;

	BVM_TRY {
		ptr = bvm_heap_alloc(size, BVM_ALLOC_TYPE_DATA);
		BVM_MAKE_TRANSIENT_ROOT(ptr);
	} BVM_CATCH(e) {
		bvm_gl_thread_current->pending_exception = e;
		ptr = NULL;
	} BVM_END_CATCH;

	return ptr;
}

/**
 * Allocate memory from the VM-managed heap that will not be GC'd during the VM lifetime.  Memory allocated with
 * this function will be ignored by the VM GC mechanisms.  Developers may assign this memory to static variables
 * or use it again after the termination of the currently executing native method.
 *
 * To release the memory back to the VM-managed heap, the returned handle must be passed to the #NI_Free
 * function.  Developer mis-management of static memory may cause memory leaks.
 *
 * Reference fields of objects or classes may *not* be set to the handle returned by this function.  The handle returned
 * here is *not* a handle to a java object -  it is a handle to raw memory
 *
 * Memory returned by this function is not zeroed or initialised in any way.

 * @param size the amount of static memory (in bytes) to allocate.
 *
 * @return a void pointer handle to allocated memory or \ c NULL if an error occurs.  Returns \c NULL if and
 * only if an invocation of this function has thrown an exception.
 *
 * @throws OutOfMemoryError if the system runs out of memory.
 */
void *NI_MallocGlobal(jint size) {

	void *ptr =  NULL;

	BVM_TRY {
		ptr = bvm_heap_alloc(size, BVM_ALLOC_TYPE_STATIC);
	} BVM_CATCH(e) {
		bvm_gl_thread_current->pending_exception = e;
	} BVM_END_CATCH;

	return ptr;
}

/**
 * Returns memory allocated by #NI_MallocLocal or #NI_MallocGlobal back to the VM-managed heap.  The handle
 * provided must be a memory handle provided as the return value from either #NI_MallocLocal or
 * #NI_MallocGlobal.  No other memory handle can be used.
 *
 * Some limited validation checking is performed on the handle to determine if it is valid.  If handle is
 * not valid the VM will attempt to exit gracefully with an exit code of #BVM_FATAL_ERR_INVALID_MEMORY_CHUNK.
 *
 * @param handle a memory handle as provided by either #NI_MallocLocal or #NI_MallocGlobal.  The value of the
 * handle will be undefined afterwards and should not be used.
 *
 * @throws none.
 */
void NI_Free(void *handle) {
	bvm_heap_free(handle);
}

#if BVM_FLOAT_ENABLE
// some special handling for getting a double param.  It is a long, then cast as the value of the
// same memory space.
jdouble __NI_GetParameterAsDouble(void *args) {
    bvm_int64_t l = NI_GetParameterAsLong(0);
    return *(bvm_double_t*)((bvm_int64_t *) &l);
}
#endif


