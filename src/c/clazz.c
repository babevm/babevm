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

  Clazz loading and functions.

  @section clazz-overview Overview

  This unit contains the functions and facilities to load Java class files and internalise them for
  VM use.  It also contains utility functions for method/field/class constant resolution among other
  things.

  More detailed description of how functionality is performed is contained in the function comments
  in this unit.  This description will serve to provide an overview and some of the reasoning behind
  the way things are.

  @section clazz-cvc Class vz clazz

  Java deals with the notion of a 'class'.  The internal structure for a loaded java class is a #bvm_clazz_t
  struct.  Yes, there is a clazz for the java Class also ... it is just another class.  The outcome of loading
  a java class into the VM is a populated #bvm_clazz_t structure living happily in a pool known as the the clazz
  pool.  So when you see the word 'class' it should be referring to the notion of a java class - when you see
  'clazz' it is referring to the internal representation of a java class.

  @section clazz-loading Class loading overview

  Classes are loaded by scanning the classpath of the classloader of the class that is trying to use the class.
  Classes are thus namespaced by the classloader that loaded them.  This VM does not distinguish between
  an initiating class loader and the classloader that actually loaded it.  The classloader that loads it
  is its classloader.

  Classes are only loaded once.  As part of the loading process they are broken into more efficient structures and
  internalised for VM use.

  The 'bootstrap' classloader classpath is specified on the command line or in the #bvm_gl_boot_classpath
  global variable when the VM is initialised.  After VM init any changes made to the
  #bvm_gl_boot_classpath are ignored.

  VM init creates the 'user' classloader and gives it the classpath held in the #bvm_gl_user_classpath global
  variable.  This can also be passed in on the command line.  Also, after VM init, any change made to this variable
  is ignored.

  The VM init takes both the boot and user classpaths and breaks them into arrays of their respective component paths.
  It is possible to have multiple paths in a classpath.

  Java level classloaders have an array of String objects into which the parsed classpath segments go.  That array is
  scanned during class loading to try and find the to-be-loaded class.

  Like Java SE, all classloaders except the bootstrap classloader have a parent classloader.  Any class
  that has \c NULL as its classloader will have been loaded by the boostrap classloader.

  Unlike the JRE, the java level classloaders do not perform all the IO necessary to load a class's
  bytes and then call defineClass().  Here, all this stuff happens at the VM level. Actually, roughly the same
  thing happens but it is implemented in C.  The Java SE uses a delegation model where a classloader
  always delegates the class loading request to its parent - to give the parent a try first.  This has the
  effect of passing a class loading request all the way up the classloader parentage to the bootstrap classloader
  and then passing attempts to load it back down as each parent fails to load it.  When a classloader actually can load the
  class - cool, we have it, the search stops, and this classloader become the classloader of the newly loaded class.

  This VM does the same (but it is done in C).  If a class A uses a class B and B is not yet loaded, A's classloader
  will attempt to load the B class.  The first thing it does it pass the request to A's parent classloader,
  and on it goes.  This means that if B is a core runtime class, the bootstrap loader gets it first.

  The VM classloading protects against a user-level classloader redefining a system class
  to try to fool things.  All classes that begin with java/ and babe/ are loaded by the bootstrap loader only.
  No questions.

  The end result of a successful search for a class file is a temporary in-memory buffer holding the entire
  class file - ready for 'parsing' into an internal representation.

  @subsection clazz-buffer Why only load from buffers?

  This VM loads java class files only one way - using an in-memory buffer (#bvm_filebuffer_t).  The in-memory buffer is
  populated from one of two sources - either it is read from a platform file or read from an entry in a jar file.

  Why? Speed.  Loading a file or a jar entry can often be loaded into memory in a
  single io read.  Often they are not big.  When it is in memory we know we can whip through it at a fast rate and then
  discard the buffer afterwards.  There is no fundamental getc() being executed for every byte read.  In some tests
  run the total bytes loaded for the (rather simple) test was about 17500 bytes.  Using a stream
  approach would mean 17500 function getc() calls to fetch each byte from the stream.  That is a lot.  Using a
  specific buffered approach those 17500 function calls are not necessary.  Actually, about 4000 are used
  instead but operate at a higher level like read_integer() or read_utfstring().

  So, deep in the class loading functionality a basic fork is made to determine whether to buffer the
  class file from a jar, or from the native file system.

  When loaded, the buffer is handed to the class loading process for parsing and internalisation.

  @section clazz-jcload Java level class loading

  The Java SE basically uses Java to do all its class loading.  The Java URLClassLoader class actually loads
  the bytes and passes those onto the VM.  This is not so easy in this VM.  Because VMs like Open JDK and others
  use native threads it is possible for the VM to make methods calls to java class methods and block until
  the method finishes and proves a return value.  Unfortunately, this is not possible on a system that
  simulates threads.  There is no way for this VM to call a java method and then block until it finishes.
  So, the VM cannot call a java method to load a class's bytes.  Taking into account that for such a small
  VM there exist no real requirement to be able to load classes in any other way apart via file or jar I
  reckon we're okay.

  If you have a look at the ClassLoader class in the Babe runtime classes you see that it defines no
  methods for actually loading a class.  It serves a number of purposes however:

  @li It serves as a namespace for class loading, so that two classloaders can load the same named file and they
  will be different classes to the VM.
  @li It holds an array of the classpath entries to search on - this, of course, may include jar
  files.
  @li It holds an array of all the Class objects for classes it has loaded.  Having a reference to a Class
  object stops a loaded class from being unloaded and GC'd.  According to the Java spec, a class may only
  be unloaded when no references are held to its classloader.  So, if the classloader holds a reference
  to a class, the class will not be unloaded unless the classloader is unloaded.

  @section clazz-types Array, Primitive and Instance Classes

  Like the Java SE, this VM automatically creates class/clazz stuff for arrays and primitive types.  Refer the
  JVM spec on array and primitive classes.  If a class is not an array or primitive class it will be an
  instance class.  Instance classes are created from java class files - the others are created internally.

  All primitive type classes belong to the bootstrap classloader.  All arrays classes belong to the
  classloader of their component type.

  @section clazz-clinit Class Initialisation (&lt;clinit&gt; processing)

  All classes go though a lifecycle when being loaded.  The JVMS is very detailed in exactly how a class
  is to be initialised. Unlike many VMs, this one relies on the Class class to perform most of the
  initialisation.  That process is well documented in the Class.java file.  I have a feeling most VMs
  will make native -> java calls to perform class init, but strangely, things are made easier by the fact
  that this one doesn't.

  Primitive and array classes are created fully initialised.  Instances class must undergo initialisation
  only if they have a &lt;clinit&gt; method.  Before a class is initialised, all its parents must first be initialised.

  @section clazz-detailed Some notes about internal instance clazzes and structures.

  The end result of parsing and loading a java class file is a series of related structures
  representing parts of the class file.  The thing that hangs it all together is the #bvm_clazz_t structure.
  Every loaded class in the VM has one of these - even primitive and array classes.

  The \c bvm_clazz_t structure for an instance class is an #bvm_instance_clazz_t.  This structure has all the common
  stuff for a clazz but defines extra things for dealing with fields and methods and implemented interfaces
  and so on.  So in that sense, it really has the guts of it all.

  Each of the structures that the \c bvm_clazz_t references has been carefully design to be a good balance between space
  and speed.

  @author Greg McCreath
  @since 0.0.10

*/

/** Array of primitive names used for creating the clazzes for primitive arrays - null terminated */
static char *primitive_class_names[] = {
        "byte",
        "boolean",
        "long",
        "double",
        "char",
        "float",
        "int",
        "short",
        NULL};

/**
 * Buffer used during class loading that is associated to a #bvm_classloader_obj_t.  The act of loading a class may have to
 * search through a class loader hierarchy.  When one is found its classloader and its bytes are stored into
 * one of these structures ready for class loading.
 */
typedef struct _clazzfilebufferstruct {
	bvm_classloader_obj_t *classloader_obj;
	bvm_filebuffer_t *buffer;
} classfilebuffer_t;

/**
 * Find a method for a class.
 *
 * @param clazz the clazz to start the search in
 * @param name the name of the method to find
 * @param jni_signature the method signature to match it on
 * @param mode the search mode.  The search modes can be and'd together to form a combined search.
 * 			#BVM_METHOD_SEARCH_CLAZZ will include the given clazz.  #BVM_METHOD_SEARCH_SUPERS will include
 * 			the superclasses of the given clazz.  #BVM_METHOD_SEARCH_INTERFACES will include all
 * 			interfaces in the search.
 *
 * NOTE: This method does not match on the <i>value</i> of the utfstrings passed as name/desc, but on their
 * memory addresses.  It is assumed that the bvm_utfstring_t* for name/desc are pointers into the
 * utfstring pool (an equality operation is a lot faster than searching string all the time!).
 *
 * In the VM code you'll see examples like:
 *
  @verbatim
	my_method = get_method(BVM_SYSTEM_CLAZZ, bvm_utfstring_pool_get_c(",mymethod", BVM_TRUE),
			                         	    bvm_utfstring_pool_get_c("()V", BVM_TRUE),
			                         		BVM_METHOD_SEARCH_CLAZZ);
  @endverbatim
 *
 * Note the usage of #bvm_utfstring_pool_get_c.  It serves to both add the "mymethod" constant
 * to the pool as a utfstring and to get a pointer that pool entry to pass to this
 * get_method function.
 *
 * This is not a very smart search - it may have to motor its way through a fair few string
 * comparisons to get to the require method.  However, this generally happens for method
 * resolution, and that is only performed once for each method.  After having been found, the
 * method pointer is stored in the constant pool entry anyways.
 *
 * @returns bvm_method_t* if found or \c NULL if not.
 */
bvm_method_t *bvm_clazz_method_get(bvm_instance_clazz_t *clazz, bvm_utfstring_t *name, bvm_utfstring_t *jni_signature, int mode) {

	int lc;
	bvm_method_t *method;

	do {
		/* first search methods defined by the given class */
		if ( (mode & BVM_METHOD_SEARCH_CLAZZ) > 0) {

			for (lc = clazz->methods_count; lc--;) {

				method = &clazz->methods[lc];

				if ( (name == method->name) && (jni_signature == method->jni_signature) ) {
					return method;
				}
			}
		}

		/* if not found, search the interfaces of the given class (if search mode specifies it) */
		if ( (mode & BVM_METHOD_SEARCH_INTERFACES) > 0) {

			for (lc = clazz->interfaces_count; lc--;) {

				bvm_instance_clazz_t *interface_clazz = clazz->interfaces[lc];

				/* searching the interfaces is recursive - why not, they are just classes ... */
				method = bvm_clazz_method_get(interface_clazz, name, jni_signature, mode);

				if (method != NULL)
					return method;
			}
		}

		/* not found? determine superclass (if search mode specifies it)*/
		clazz = ( (mode & BVM_METHOD_SEARCH_SUPERS) > 0) ? clazz->super_clazz : NULL;

	} while (clazz != NULL);

	return NULL;
}

/**
 * Read the constant pool of a class.  An array of #bvm_clazzconstant_t is created
 * from the heap and each entry represents a class constant.  utfstring constants will
 * point into entries in the utfstring pool.  String constants will point to entries
 * in the intern string pool.  Integer/float/long constants are held in the
 * constant pool entry for their numbered constant.  Note that longs take up two constant
 * positions.
 *
 * Weirdly, constants are accessed by the VM as one-based, and not zero-based.  So the first constant
 * is constant number '1'.  To assist here, when we allocate the memory for class
 * constants we allocate enough to hold all the constants in a one-based scheme.  This leaves
 * the first constant empty.  Well, not really, we fill it with the count of the constants.
 *
 * As constants are accessed a lot during VM execution, using a one-based array means we do not
 * have to do an offset (-1) each constant access.  A small optimisation, but everything helps.
 *
 * A second optimisation is how the constant tags are held.  Classes may have *lots* of constants. For
 * example, the Object class does almost nothing and it has about 70 constants.  We want to pack all
 * those constants into the most effective space we can.
 *
 * So .... what we do is create memory for a structure that has the constant data at the front all
 * in nice size_t size blocks, and then appended to it on the end is a list of the corresponding
 * constant tags.  Like a constant array with a byte array at the end of it all malloc'd as one.
 *
 * This saves memory.  In order to help us get to the tag byte array faster, its address is
 * stored as the resolved pointer in constant '0'.  So constant '0' actually has two uses (the constant
 * count and a pointer to the tags).
 *
 * What?  The resolved pointer?  What is that all about.  Well, each constant is a union.  Its
 * contents may hold different data depending on where it is in its lifecycle.  The constant
 * lifecycle is one that progresses towards having its contents have a constant value (for
 * constants), or a pointer to the thing it is a constant for - eg, a method desc will end up
 * containing a pointer to a method, a field desc will contain a pointer to a field, a Class
 * constant likewise, a String constant will hold a pointer to an interned String in the
 * interned String pool, a utfstring constant will hold a pointer to a utfstring constant in the
 * utfstring pool.
 *
 * After this function has completed, all primitive constant values will be held in their
 * associated constant struct, all constant String pointers will point to an entry in the
 * interned string pool, and all Class constants will point to the class name in the utfstring
 * pool.  Class constants point to the class name as we do not yet know if the class has actually been
 * loaded, and frankly at this point do not care to find out.  When a Class constant is requested
 * by the \c ldc opcode it will be resolved/loaded and after that the constant \c resolved_ptr will
 * contain a direct pointer to the loaded clazz.  This is an example of the 'lifecycle' - a class
 * constant may start with one type of value and then progress to another as/when it is resolved.  The
 * JVMS may refer to this as 'linking'.
 *
 * To process class constants we actually loop though the constant list twice.  The first pass
 * reads the constants and resolves all primitive constants.  For Class and Strings this first
 * pass just finds out which constant has the real details for them - like a String constant at
 * index (say) 10, may have its actual textual content at (say) constant 11.  We cannot process the
 * String constant until we have processed its content.  Hence the second pass.  Ultimately what we want to
 * achieve is that the String constant at 10 has a direct resolved pointer into the String
 * pool - but we cannot be sure the string will be there until we have done a first pass on all the
 * constants.
 *
 * The second pass a) resolves the Class constant's utfstring pool pointers and b) ensure the String
 * constants have a #bvm_internstring_obj_t object created for them and added to the interned
 * String pool.
 *
 */
static void clazz_load_cp(bvm_instance_clazz_t *clazz, bvm_filebuffer_t *buffer) {

	bvm_uint16_t lc, count;
	bvm_clazzconstant_t *constant;
	bvm_uint8_t tag;

	/* read the constant count */
	count = bvm_file_read_uint16(buffer)-1;

	/* constant pool is count+1 size.  The constants in the class file do not start at 0, they
	 * are '1' based.  If we have the constants directly accessible, we'll not have
	 * an integer operation each time we access one.  We'll (smartly) use the first
	 * constant to hold the number of constants (so the space is not wasted) */

	clazz->constant_pool = bvm_heap_calloc( (count+1) * (sizeof(bvm_clazzconstant_t) + sizeof(bvm_uint8_t)), BVM_ALLOC_TYPE_STATIC);

	/* the 0'th constant's value holds the number of constants */
	clazz->constant_pool[0].data.value.int_value = count;

	/* the 0'th constant's resolved ptr value holds a pointer to the tags */
	clazz->constant_pool[0].resolved_ptr = ((bvm_uint8_t *) clazz->constant_pool) + ( (count+1) * sizeof(bvm_clazzconstant_t));

	/* for each constant in the constant pool ... */
	for (lc = 1; lc <= count; lc ++) {

		constant = &clazz->constant_pool[lc];

		/* get the tag (u1) */
		tag = bvm_read_byte(buffer);

		/* set the tag value in the constant pool structure */
		BVM_CONSTANT_RawTag(clazz, lc) = tag;

		switch (tag) {
			case BVM_CONSTANT_Utf8: {

				bvm_utfstring_t *str, *pooled_str;

				/* utf8string get resolved immediately - they are added to the utfstring pool if they are not there
				 * straight away */
				str = bvm_file_read_utfstring(buffer, BVM_ALLOC_TYPE_STATIC);

				/* Now we'll check to see if this new string is actually already contained in the
				 * utfstring pool.  If it is we'll use the one in the pool and discard this newly
				 * created one - if not, the BVM_TRUE param here will ensure it is added to the pool.
				 */
				pooled_str = bvm_utfstring_pool_get(str, BVM_TRUE);

				/* If the pointer we get back is not the one we passed in there must have already
				 * been a string in the pool, so we'll free the one just created.  */
				if (pooled_str != str) {
					bvm_heap_free(str);
					constant->data.value.ptr_value = pooled_str;
				} else {
					constant->data.value.ptr_value = str;
				}

				break;
			}
			case 2:
				goto err;
			case BVM_CONSTANT_Integer:
				/* integer constant are resolved immediately */
				constant->data.value.int_value = bvm_file_read_int32(buffer);
				break;
			case BVM_CONSTANT_Float: {
#if BVM_FLOAT_ENABLE
				/* float constants are resolved immediately */
				constant->data.value.int_value = bvm_file_read_int32(buffer);
				break;
#else
				bvm_throw_exception(BVM_ERR_INTERNAL_ERROR, "float constant not supported");
#endif
			}
			case BVM_CONSTANT_Long:

				/* long constants are resolved immediately - a long constant actually takes up
				 * the space of two constants */

				/* first half of long - hi bits */
				constant->data.value.int_value = bvm_file_read_int32(buffer);

				/* move constant loop counter along */
				lc++;
				constant = &clazz->constant_pool[lc];

				/* second half of long - low bits */
				constant->data.value.int_value = bvm_file_read_int32(buffer);

				break;
			case BVM_CONSTANT_Double:
#if BVM_FLOAT_ENABLE
				/* double constants are resolved immediately - double constants actually take up
				 * the space of two constant */

				/* first half of double - high bits */
				constant->data.value.int_value = bvm_file_read_int32(buffer);

				/* move constant loop counter along */
				lc++;
				constant = &clazz->constant_pool[lc];

				/* second half of double - low bits */
				constant->data.value.int_value = bvm_file_read_int32(buffer);

				break;
#else
				/* double is not supported */
				bvm_throw_exception(BVM_ERR_INTERNAL_ERROR, "Double constant not supported");
#endif

			case BVM_CONSTANT_Class:
				/* initially, resolve a Class constant to the index to the utfstring that contains the
				 * class name - later on, when accessed, this will be resolved further to a
				 * pointer to a bvm_clazz_t */
				constant->data.value.int_value = bvm_file_read_uint16(buffer);
				break;
			case BVM_CONSTANT_String:
				/* initially, resolve a String constant to the index of the utfstring that holds
				 * its content - in the second pass in this function this index will be replaced with
				 * a pointer to an interned string object */
				constant->data.value.int_value = bvm_file_read_uint16(buffer);
				break;
			case BVM_CONSTANT_Fieldref:
			case BVM_CONSTANT_Methodref:
			case BVM_CONSTANT_InterfaceMethodref:
			case BVM_CONSTANT_NameAndType:
				/* the above four types of constant all use two indexes to hold their data. Exactly what is stored at
				 * these is dependent on the above type, but as they are mutually exclusive we use the same two
				 * indexes in the constant struct for each.  Later, these may be resolved to direct pointers to
				 * (say) a bvm_method_t in the case of a methodfref.*/
				constant->data.ref_info.index1 = bvm_file_read_uint16(buffer);
				constant->data.ref_info.index2 = bvm_file_read_uint16(buffer);
				break;
			default:
				err: ;
				bvm_throw_exception(BVM_ERR_CLASS_FORMAT_ERROR, "unknown constant type");
		}
	}

	/* now, go through each constant again and resolve further as much as possible at this time */
	for (lc = 1; lc <= count; lc ++) {

		constant = &clazz->constant_pool[lc];

		switch (BVM_CONSTANT_Tag(clazz,lc)) {
			case BVM_CONSTANT_Class: {
				/* set the pointer value of the current constant to the utfstring pointer of the index it
				 * points to.  So, in an 'unresolved' state, a CLASS constant points to a
				 * pooled utfstring. Later, when accesses it will be further resolved to an bvm_clazz_t pointer. */
				bvm_uint16_t index = (bvm_uint16_t) constant->data.value.int_value;
				constant->data.value.ptr_value = clazz->constant_pool[index].data.value.ptr_value;
				break;
			}
			case BVM_CONSTANT_String: {
				/* create/use an interned string instance */
				/* The current constant index will give us a pooled utfstring pointer.  We'll use that
				 * utfstring to check for an interned string that matches the utf data */
				bvm_uint16_t index = (bvm_uint16_t) constant->data.value.int_value;
				bvm_utfstring_t *str = bvm_clazz_cp_utfstring_from_index(clazz, index);
				constant->data.value.ptr_value = bvm_internstring_pool_get(str, BVM_TRUE);
				break;
			}
		}
	}
}

/**
 * Get a handle to a constant #bvm_utfstring_t given a class and index.
 *
 * @param clazz the clazz
 * @param index the index into the constant pool of the utfstring.
 *
 * @return a pointer to a utfstring held in the utfstring pool.
 */
bvm_utfstring_t *bvm_clazz_cp_utfstring_from_index(bvm_instance_clazz_t *clazz, bvm_uint16_t index) {
	return clazz->constant_pool[index].data.value.ptr_value;
}

/**
 * Get the class index from a field ref_value at the given index, and from there get the name index of the class
 * and from there the actual class name.
 *
 * @param clazz the clazz
 * @param index the index of the class constant
 *
 * @return the class name as a pointer to a utfstring in the utfstring pool.
 */
bvm_utfstring_t *bvm_clazz_cp_ref_clazzname(bvm_instance_clazz_t *clazz, bvm_uint16_t index) {
	bvm_uint16_t classindex = clazz->constant_pool[index].data.ref_info.index1;
	return clazz->constant_pool[classindex].data.value.ptr_value;
}

/**
 * Get the field/method name from a field/method index.
 *
 * @param clazz the clazz
 * @param index the index of the ref's name and type constant
 *
 * @return the ref's name as a pointer to a utfstring in the utfstring pool.
 */
bvm_utfstring_t *bvm_clazz_cp_ref_name(bvm_instance_clazz_t *clazz, bvm_uint16_t index) {

	/* get the type and name index of the field */
	bvm_uint16_t typenameindex = clazz->constant_pool[index].data.ref_info.index2;

	/* typenamnameindex index1 is populated with the index of the field/method name */
	bvm_uint16_t nameindex = clazz->constant_pool[typenameindex].data.ref_info.index1;

	/* the value of nameindex will already be resolved to a utfstring in the
	 * utfstring pool.  Return that */
	return clazz->constant_pool[nameindex].data.value.ptr_value;
}

/**
 * Get the constant pool field/method desc from a field/method index.
 *
 * @param clazz the clazz
 * @param index the index of the ref's name and type constant
 *
 * @return the ref's desc as a pointer to a utfstring in the utfstring pool.
 */
bvm_utfstring_t *bvm_clazz_cp_ref_sig(bvm_instance_clazz_t *clazz, bvm_uint16_t index) {

	/* get the type and name index of the field */
	bvm_uint16_t typenamnameindex = clazz->constant_pool[index].data.ref_info.index2;

	/*  index is populated by the name index */
	bvm_uint16_t descindex = clazz->constant_pool[typenamnameindex].data.ref_info.index2;
	return clazz->constant_pool[descindex].data.value.ptr_value;
}


/**
 * Parse and load direct superinterfaces for a given clazz.  Note this will have the effect
 * of recursing up and loading all superinterfaces and their parents and so on.
 *
 * @param clazz the clazz
 * @param buffer the buffer that the class is being read from
 */
static void clazz_load_interfaces(bvm_instance_clazz_t *clazz, bvm_filebuffer_t *buffer) {

	bvm_uint16_t lc;

	/* number of interfaces implemented by this class */
	bvm_uint16_t interfaces_count = bvm_file_read_uint16(buffer);

	/* store the interface count with the clazz for later usage */
	clazz->interfaces_count = interfaces_count;

	if (interfaces_count > 0) {

		/* allocate memory for the array of interfaces */
		clazz->interfaces = bvm_heap_calloc(interfaces_count * sizeof(bvm_instance_clazz_t *), BVM_ALLOC_TYPE_STATIC);

		/* for each direct superinterface of the class */
		for (lc = 0; lc < interfaces_count; lc ++) {

			/* go get/load it */
			bvm_instance_clazz_t *interface_clazz =
				(bvm_instance_clazz_t *) bvm_clazz_get(clazz->classloader_obj, bvm_clazz_cp_utfstring_from_index(clazz, bvm_file_read_uint16(buffer)));

			/* JVMS - "If any of the interfaces named as direct superinterfaces is not in fact an
			 * interface, loading throws an IncompatibleClassChangeError." */
			if (!BVM_CLAZZ_IsInterface(interface_clazz))
				bvm_throw_exception(BVM_ERR_INCOMPATIBLE_CLASS_CHANGE_ERROR, NULL);

			/* JVMS - if the interface class failed to load, throw a NoClassDefFoundError */
	        if (interface_clazz->state == BVM_CLAZZ_STATE_ERROR)
				bvm_throw_exception(BVM_ERR_NO_CLASS_DEF_FOUND_ERROR, NULL);

	        /* JVMS - the interface must be accessible to the class that implements it */
			if (!bvm_clazz_is_class_accessible( (bvm_clazz_t *) clazz, (bvm_clazz_t *) interface_clazz))
				bvm_throw_exception(BVM_ERR_ILLEGAL_ACCESS_ERROR, NULL);

			/* store the interface clazz into the clazz interfaces array */
			clazz->interfaces[lc] = interface_clazz;
		}
	}
}

/**
 * Load class fields.  Resultant fields in the \c clazz->fields array will be sorted with the static fields
 * first, and the virtual fields last.  The first virtual field will start
 * at \c clazz->fields[class->static_field_count].  Static longs are treated differently.
 *
 * The two field types are separated so that during GC we can focus on traversing just static or virtual
 * fields rather as required rather than both.
 *
 * If there are static fields a second pass through the field list will be done to sort them into these
 * two halves.
 *
 * Static longs are held in a separate array on the clazz struct and are not held in the field array with
 * the other fields.  A long field then contains a pointer to its corresponding element in that list. This is a
 * trade-off - we add a extra (possibly unused) pointer on every clazz struct to hold constant long values, but the alternative
 * it to hold the long value in the field struct - which would mean an extra storage on each field to accommodate
 * the possibility of it being a long.
 *
 * Static final (constant) fields have their values resolved at this point.  Constant longs have their value stored
 * in that list with the clazz.
 *
 * @param clazz the clazz
 * @param buffer the buffer that the class is being read from
 */
static void clazz_load_fields(bvm_instance_clazz_t *clazz, bvm_filebuffer_t *buffer) {

	bvm_uint16_t lc, attrlc, attrcount;

	bvm_uint32_t attrlength;
	bvm_utfstring_t *attrname;
	bvm_field_t *tempfieldlist;
	bvm_field_t *field;
	int virtualfieldoffset = 0;
	int staticlongcount = 0;
	bvm_int64_t *int64_ptr = NULL;

	/* number of fields */
	bvm_uint16_t fields_count = bvm_file_read_uint16(buffer);

	/* store with class for later use */
	clazz->fields_count = fields_count;

	if (fields_count > 0) {

		/* allocate the class space for the class fields array */
		clazz->fields = bvm_heap_calloc(fields_count * sizeof(bvm_field_t), BVM_ALLOC_TYPE_STATIC);

		/* for each field in the class ... */
		for (lc = 0; lc < fields_count; lc ++) {

			/* get a pointer to the field struct in the fields array */
			field = (bvm_field_t *) &clazz->fields[lc];

			field->clazz = clazz;
			field->access_flags = bvm_file_read_uint16(buffer);

			/* resolve the name and desc to pointers in the utf string pool */
			field->name = bvm_clazz_cp_utfstring_from_index(clazz, bvm_file_read_uint16(buffer));
			field->jni_signature = bvm_clazz_cp_utfstring_from_index(clazz, bvm_file_read_uint16(buffer));

			/* if the field is a long, let's note that in the field access flags for
			 * convenience - actually, this tells us that the 'static_value' variable in the
			 * field contains a pointer to a long value and not the value itself. */
			if ( (field->jni_signature->data[0] == 'J') || (field->jni_signature->data[0] == 'D') )  {
				field->access_flags |= BVM_FIELD_ACCESS_FLAG_LONG;
				if (BVM_FIELD_IsStatic(field)) staticlongcount++;
			}

			/* Keep a count of non-static fields.  We use the number of non-static fields to calculate
			 * the size of an (object) instance of this class.  We also use the accumulated number of fields
			 * to create an offset into an instance's fields that this field will occupy. */
			if (!BVM_FIELD_IsStatic(field)) {
				field->value.offset = clazz->instance_fields_count;
				clazz->instance_fields_count += BVM_FIELD_IsLong(field) ? 2 : 1;  /* two for a long */
			} else {
				/* field is static, so increment the variable we use to give us an offset to
				 * the instance fields */
				virtualfieldoffset++;
			}

			/* the number of field attributes */
			attrcount = bvm_file_read_uint16(buffer);

			/* for each attribute of the field */
			for (attrlc = 0; attrlc < attrcount; attrlc++) {

				/* the name of the attribute */
				attrname = bvm_clazz_cp_utfstring_from_index(clazz, bvm_file_read_uint16(buffer));

				/* the length of the attribute */
				attrlength = bvm_file_read_uint32(buffer);

				/* If field is a const (static and final) field and it not a long, give it a value right now. Longs will
				 * get their values later after we know how many of them there are so we can allocate
				 * space for them - remember, they are kept in a list on the bvm_clazz_t */
				if ( (BVM_FIELD_IsStatic(field) &&
					  BVM_FIELD_IsFinal(field)) &&
					  strncmp((char *)attrname->data, "ConstantValue",13)==0) {

					bvm_uint16_t index = bvm_file_read_uint16(buffer);

					if (!BVM_FIELD_IsLong(field)) {

						if (BVM_CONSTANT_Tag(clazz, index) != BVM_CONSTANT_Class) {
							/* field is not a class, gets its static value */
							field->value.static_value = clazz->constant_pool[index].data.value;
						} else {
							/* field is a class, resolve pointer to class object and keep in field value */
							field->value.static_value.ref_value =
								(bvm_obj_t *) bvm_clazz_resolve_cp_clazz(clazz, index)->class_obj;
						}

					} else
						field->value.static_value.int_value = index;

					/* flag field as being a final constant */
					field->access_flags |= BVM_FIELD_ACCESS_FLAG_CONST;

					/* onto next attribute */
					continue;
				}

#if (BVM_DEBUGGER_ENABLE && BVM_DEBUGGER_SIGNATURES_ENABLE)

				/* is this the 'Signature' attribute ? */
				if (strncmp((char *) attrname->data, "Signature", 9) == 0) {
					field->generic_signature = bvm_clazz_cp_utfstring_from_index(clazz, bvm_file_read_uint16(buffer));
					continue;
				}
#endif
				bvm_file_skip_bytes(buffer, attrlength);
			}
		}

		/* if there are static long fields, create the space for them, and attach it to the class struct */
		if (staticlongcount != 0) {
			int64_ptr = bvm_heap_calloc(staticlongcount * sizeof(bvm_int64_t), BVM_ALLOC_TYPE_STATIC);
			clazz->static_longs = int64_ptr;
		}

		/* if there are static (non-long) fields sort the class fields to have the static
		 * ones first so that clazz->fields[class->virtual_field_offset] gives the first virtual
		 * field. We use a temp array to contain the sorted fields and then do a swap with
		 * the clazz->fields array. We sort the fields to make life easier for the GC to deal only
		 * with statics or virtuals. */
		if (virtualfieldoffset > 0 ) {

			int staticpos  = 0;
			int virtualpos = virtualfieldoffset;
			int index;

			/* allocate some temporary space for sorting the fields */
			tempfieldlist = bvm_heap_calloc(fields_count * sizeof(bvm_field_t), BVM_ALLOC_TYPE_STATIC);

			for (lc = 0; lc < fields_count; lc ++) {

				field = (bvm_field_t *) &clazz->fields[lc];

				index = BVM_FIELD_IsStatic(field) ? staticpos++ : virtualpos++;

				tempfieldlist[index] = clazz->fields[lc];

				/* while we're looping through all the fields again we'll do the static longs.
				 * Static longs point into the clazz->static_longs list.  Static final
				 * longs get a value as they go into that list.*/

				if (BVM_FIELD_IsStatic(field) && BVM_FIELD_IsLong(field)) {

					/* we'll perform operations on the newly copied field */
					bvm_field_t *newfield = (bvm_field_t *) &tempfieldlist[index];

					/* grab the index before we set the pointer - we'll use the index if it is a const */
					bvm_uint16_t idx2 = (bvm_uint16_t) newfield->value.static_value.int_value;

					/* set the static value pointer to an offset into the class's static longs array */
					newfield->value.static_value.ptr_value = int64_ptr++;

					/* if it is a static final long constant field, set the value right now */
					if (BVM_FIELD_IsConst(newfield)) {

						bvm_uint32_t hi = (bvm_uint32_t) clazz->constant_pool[idx2++].data.value.int_value;
						bvm_uint32_t lo = (bvm_uint32_t) clazz->constant_pool[idx2].data.value.int_value;

                        bvm_int64_t val = (bvm_int64_t) uint64Pack(hi, lo);
                        (*(bvm_int64_t*)(newfield->value.static_value.ptr_value)) = val;
					}
				}
			}

			/* get rid of the current fields list.  It will be used no more */
			bvm_heap_free(clazz->fields);

			/* replace it with the new sorted list just bought into life above */
			clazz->fields = tempfieldlist;

			/* set the class's offset to the above-calculated virtual field offset */
			clazz->virtual_field_offset = virtualfieldoffset;
		}
	}
}

/**
 * For a given class, find a field with the given name and desc.  The \c name and \c desc utfstring
 * pointers are to be pointers to utfstrings that exist in the utfstring constant pool.
 *
 * @param clazz the clazz
 * @param name the name of the field to search for
 * @param jni_signature the jni description of the field to search for
 *
 * @return the a pointer to a bvm_field_t for the requested field if it exists, or \c NULL otherwise.
 */
bvm_field_t *bvm_clazz_field_get(bvm_instance_clazz_t *clazz, bvm_utfstring_t *name, bvm_utfstring_t *jni_signature) {

	bvm_uint16_t lc;
	bvm_field_t *field;

	bvm_instance_clazz_t *search_clazz = clazz;

	/* Look for a field definition in the search class.  If none, look up through the direct
	 * interfaces of the class.  If not found, do the same thing recursively to the superclass of the
	 * search class until a field is found ... or not. */

	while (BVM_TRUE) {

		for (lc = search_clazz->fields_count; lc--;) {
			field = (bvm_field_t *) &search_clazz->fields[lc];
			if ( (field->name != name) || (field->jni_signature != jni_signature) ) continue;
			return field;
		}

		/* for each interface */
		for (lc = search_clazz->interfaces_count; lc--;) {
			field = bvm_clazz_field_get(search_clazz->interfaces[lc], name, jni_signature);
			if (field != NULL)
				return field;
		}

		/* look in superclass */
		search_clazz = search_clazz->super_clazz;

		/* no more super classes?  Exit */
		if (search_clazz == NULL)
			break;
	}

	return NULL;
}

/**
 * Given a clazz and a constant pool index, get the method.  If the method cannot be found
 * a #BVM_ERR_NO_SUCH_METHOD_ERROR will be thrown.
 *
 * @param clazz the clazz
 * @param index the constant pool index of the method
 *
 * @return a pointer to the #bvm_method_t for the requested method
 */
bvm_method_t *bvm_clazz_resolve_cp_method(bvm_instance_clazz_t *clazz, bvm_uint16_t index) {

	bvm_method_t *resolved_method;
	bvm_instance_clazz_t *search_clazz;
	bvm_utfstring_t *clazz_name;
	bvm_utfstring_t *method_name;
	bvm_utfstring_t *method_sig;

	/* get the clazz constant */
	bvm_clazzconstant_t *constant = &clazz->constant_pool[index];

	/* get the stuff that describes the method at the given index */
	clazz_name  = bvm_clazz_cp_ref_clazzname(clazz, index);
	method_name = bvm_clazz_cp_ref_name(clazz, index);
	method_sig  = bvm_clazz_cp_ref_sig(clazz, index);

	/* get the clazz where the method is defined - it may not actually be an instance clazz, but
	 * if it isn't, we substitute the Object class the next line. */
	search_clazz = (bvm_instance_clazz_t *) bvm_clazz_get(clazz->classloader_obj, clazz_name);

	/* if we are dealing with an array class or primitive class (which have no methods) we'll
	 * substitute the Object class instead.  Why? For example, because java arrays have methods
	 * like clone() that do not exist in bytecode. */
	if (!BVM_CLAZZ_IsInstanceClazz(search_clazz)) search_clazz = BVM_OBJECT_CLAZZ;

	/* get the method by searching the class, interface, super-class tree. */
	resolved_method = bvm_clazz_method_get(search_clazz, method_name, method_sig, BVM_METHOD_SEARCH_FULL_TREE);

	/* JVMS - "if no method matching the resolved name and signature is selected, throw
	 * a NoSuchMethodError". */
	if (resolved_method == NULL)
		bvm_throw_exception(BVM_ERR_NO_SUCH_METHOD_ERROR, (char *) method_name->data);

	/* JVMS - "if the referenced method is not accessible, throw an IllegalAccessError" */
	if (!bvm_clazz_is_member_accessible(BVM_METHOD_AccessFlags(resolved_method), (bvm_instance_clazz_t *) clazz, resolved_method->clazz))
		bvm_throw_exception(BVM_ERR_ILLEGAL_ACCESS_ERROR, NULL);

	/* store the resolved method pointer into the constant and set a flag to say it is now optimised */
	constant->resolved_ptr = resolved_method;
	BVM_CONSTANT_RawTag(clazz, index) |= BVM_CONSTANT_OPT_FLAG;

	return resolved_method;
}


/**
 * Given a clazz desc and an index to a constant pool field info get the
 * field definition.
 *
 * @param clazz the instace class to search
 * @param index the constant pool index of the field
 * @param is_static is the field we are looking for static.
 *
 * @return a pointer to a #bvm_field_t for the requested field.
 */
bvm_field_t *bvm_clazz_resolve_cp_field(bvm_instance_clazz_t *clazz, bvm_uint16_t index, bvm_bool_t is_static) {

	bvm_field_t *resolved_field;

	/* get the class name for field is defined in, then its name and desc */
	bvm_utfstring_t *clazz_name;
	bvm_utfstring_t *field_name;
	bvm_utfstring_t *field_sig;
	bvm_instance_clazz_t *cl;

	/* get the class constant for the field */
	bvm_clazzconstant_t *constant = &clazz->constant_pool[index];

	/* if we have already optimised this constant, return it */
	if (BVM_CONSTANT_IsOptimised(clazz,index))
		return constant->resolved_ptr;

	/* if the class was not in a good state. Error */
	if (clazz->state == BVM_CLAZZ_STATE_ERROR)
		bvm_throw_exception(BVM_ERR_NO_CLASS_DEF_FOUND_ERROR, "Class in ERROR state");

	/* get field descriptive info from the constant pool */
	clazz_name = bvm_clazz_cp_ref_clazzname(clazz, index);
	field_name = bvm_clazz_cp_ref_name(clazz, index);
	field_sig  = bvm_clazz_cp_ref_sig(clazz, index);

	/* get the field's class defined in the constant pool (this is not necessarily the class that
	 * defined the field, but the class we are accessing it via). */
	cl = (bvm_instance_clazz_t *) bvm_clazz_get(clazz->classloader_obj, clazz_name);

	/* JVMS - if the target clazz if not accessible, no good.  */
	if (!bvm_clazz_is_class_accessible((bvm_clazz_t *) clazz, (bvm_clazz_t *) cl))
		bvm_throw_exception(BVM_ERR_ILLEGAL_ACCESS_ERROR, (char *) cl->name->data);

	/* get the field, starting the search at the constant pool clazz we are accessing the field via */
	resolved_field = bvm_clazz_field_get(cl, field_name, field_sig);

	/* JVMS - not field found?  Throw a NoSuchFieldError exception */
	if (resolved_field == NULL) {
		bvm_throw_exception(BVM_ERR_NO_SUCH_FIELD_ERROR, (char *) field_name->data);
	}

	/* JVMS - "If the resolved field is NOT a static field, getfield throws
	 * an IncompatibleClassChangeError". Actually, we'll cross check here if
	 * expecting static and it is not, or vice-versa. */
	if ( is_static ? (!BVM_FIELD_IsStatic(resolved_field)) : (BVM_FIELD_IsStatic(resolved_field)))
		bvm_throw_exception(BVM_ERR_INCOMPATIBLE_CLASS_CHANGE_ERROR, NULL);

	/* JVMS - check this current class can access the field */
	if ( !bvm_clazz_is_member_accessible(resolved_field->access_flags, clazz, resolved_field->clazz) )
		bvm_throw_exception(BVM_ERR_ILLEGAL_ACCESS_ERROR, (char *) resolved_field->name->data);

	/* save the resolved ptr in the constant and set a flag to say it is now optimised */
	constant->resolved_ptr = resolved_field;
	BVM_CONSTANT_RawTag(clazz, index) |= BVM_CONSTANT_OPT_FLAG;

	return resolved_field;
}

/**
 * Given a clazz and an index into the constant pool of that clazz (which must point to a
 * class info structure) get and return the given clazz.
 *
 * @param clazz the clazz to search
 * @param index the constant pool index of the class info
 *
 * @return a pointer to a #bvm_clazz_t structure for the request clazz.
 */
bvm_clazz_t *bvm_clazz_resolve_cp_clazz(bvm_instance_clazz_t *clazz, bvm_uint16_t index) {

	bvm_clazz_t *resolved_clazz;
	bvm_utfstring_t *clazzname;

	/* get the clazz constant */
	bvm_clazzconstant_t *constant = &clazz->constant_pool[index];

	/* if we have already optimised this constant, return the clazz */
	if (BVM_CONSTANT_IsOptimised(clazz,index))
		return constant->resolved_ptr;

	/* get the class name from the constant pool */
	clazzname = bvm_clazz_cp_utfstring_from_index(clazz, index);

	/* get the clazz for the class at the given index, using the classloader of the current class as
	 * the starting point */
	resolved_clazz = bvm_clazz_get(clazz->classloader_obj, clazzname);

	/* JVMS - throw IllegalAccessError if access not permitted */
	if (!bvm_clazz_is_class_accessible( (bvm_clazz_t *) clazz, resolved_clazz))
		bvm_throw_exception(BVM_ERR_ILLEGAL_ACCESS_ERROR, (char *) resolved_clazz->name->data);

	/* save the resolved ptr in the constant and set a flag to say it is now optimised */
	constant->resolved_ptr = resolved_clazz;
	BVM_CONSTANT_RawTag(clazz, index) |= BVM_CONSTANT_OPT_FLAG;

	return resolved_clazz;
}

/**
 * Determine the type of an array from the char type representation.
 *
 * @param c the array type as a char
 *
 * @return the type of the array
 */
bvm_jtype_t bvm_clazz_get_array_type(char c) {

	switch (c) {
	case 'B':
		return BVM_T_BYTE;
	case 'C':
		return BVM_T_CHAR;
	case 'D':
		return BVM_T_DOUBLE;
	case 'F':
		return BVM_T_FLOAT;
	case 'I':
		return BVM_T_INT;
	case 'J':
		return BVM_T_LONG;
	case 'L':
		return BVM_T_OBJECT;
	case 'S':
		return BVM_T_SHORT;
	case 'Z':
		return BVM_T_BOOLEAN;
	case '[':
		return BVM_T_ARRAY;
	default :
		bvm_throw_exception(BVM_ERR_VIRTUAL_MACHINE_ERROR, "Invalid array type");
	}

	/* will not arrive here, but the compiler wants a return ...*/
	return BVM_ERR;
}

/**
 * Count the number of arguments a method has by parsing the method description.
 *
 * @param desc the array description to parse.
 *
 * @return the number of method arguments.
 */
static bvm_uint8_t clazz_count_method_args(bvm_utfstring_t *desc) {

	bvm_uint16_t retVal;

	char *c = (char *) desc->data;

	/* if we do not start at an open bracket something drastic
	 * has gone wrong.  Bang out. */
	if (*c++ != '(')
		bvm_throw_exception(BVM_ERR_CLASS_FORMAT_ERROR, NULL);

	/* assume no args as our starting count */
	retVal = 0;

	/* loop over the method description until the end bracket is encountered */
	while (*c != ')') {
		switch (*c) {
		/* standard primitive arguments */
		case 'B':
		case 'C':
		case 'F':
		case 'I':
		case 'S':
		case 'Z':
			/* primitives all get one */
			c++;
			retVal++;
			break;
		case 'D':
		case 'J':
			/* doubles and longs get two */
			c++;
			retVal += 2;
			break;
		case '[':
			/* move past the array markers until the first non-'[' char.  If that char is for a
			 * long type, increment the result and move the char one on. */
			while (*++c == '[');
			if (*c == 'D' || *c == 'J') {
				retVal++;
				c++;
			}
			break;
		case 'L':
			/* objects get one, and then read the object desc until the ';' at the end. */
			c++;
			while (*c++ != ';');
			retVal++;
			break;
		default:
			bvm_throw_exception(BVM_ERR_CLASS_FORMAT_ERROR, NULL);
		}
	}

	return retVal;
}

/**
 * Parse the attributes of the class.
 *
 * @param clazz the clazz
 * @param buffer the buffer the class is being read from
 */
static void clazz_load_attributes(bvm_instance_clazz_t *clazz, bvm_filebuffer_t *buffer) {

	bvm_uint16_t lc, attr_count;

	attr_count = bvm_file_read_uint16(buffer);

	for (lc = 0; lc < attr_count; lc++) {

		bvm_uint32_t attr_length;

		/* get the attribute name */
		bvm_utfstring_t *attr_name = bvm_clazz_cp_utfstring_from_index(clazz, bvm_file_read_uint16(buffer));

		/* skip the attr length, it is always '2' for a class attribute, and we know what follows. JVMS 4.7.7. */
		attr_length = bvm_file_read_uint32(buffer);

#if (BVM_LINE_NUMBERS_ENABLE || BVM_DEBUGGER_ENABLE)

		/* is this the 'SourceFile' class attribute? */
		if (strncmp((char *) attr_name->data, "SourceFile", 10) == 0) {
			clazz->source_file_name = bvm_clazz_cp_utfstring_from_index(clazz, bvm_file_read_uint16(buffer));
			continue;
		}
#endif

#if BVM_DEBUGGER_ENABLE

#if BVM_DEBUGGER_JSR045_ENABLE
		/* is this the 'SourceDebugExtension' class attribute? */
		if (strncmp((char *) attr_name->data, "SourceDebugExtension", 20) == 0) {
			clazz->source_debug_extension = bvm_str_new_utfstring(attr_length, BVM_ALLOC_TYPE_STATIC);
			bvm_file_read_bytes_into(buffer, attr_length, &(clazz->source_debug_extension->data));
			continue;
		}
#endif

#if BVM_DEBUGGER_SIGNATURES_ENABLE
		/* is this the 'Signature' class attribute? */
		if (strncmp((char *) attr_name->data, "Signature", 9) == 0) {
			clazz->generic_signature = bvm_clazz_cp_utfstring_from_index(clazz, bvm_file_read_uint16(buffer));
			continue;
		}
#endif

#endif

		bvm_file_skip_bytes(buffer, attr_length);
	}
}

/**
 * Parse the methods of the class file creating a #bvm_method_t structure for each one.
 *
 * @param clazz the clazz
 * @param buffer the buffer the class is being read from
 */
static void clazz_load_methods(bvm_instance_clazz_t *clazz, bvm_filebuffer_t *buffer) {

	bvm_uint16_t lc, lc2, lc3, methods_count, attr_count;
	char return_char;

	/* the method count */
	methods_count = bvm_file_read_uint16(buffer);

	/* store the method count with the clazz */
	clazz->methods_count = methods_count;

	if (methods_count > 0) {

		/* allocate memory for the array of bvm_method_t structures. */
		clazz->methods = bvm_heap_calloc(methods_count * sizeof(bvm_method_t), BVM_ALLOC_TYPE_STATIC);

		/* for each method ... */
		for (lc = 0; lc < methods_count; lc ++) {

			/* get a handle to the (still empty) method in the methods array */
			bvm_method_t *method = &clazz->methods[lc];

			/* populate it */
			method->clazz 	      = clazz;
			method->access_flags  = bvm_file_read_uint16(buffer);
			method->name          = bvm_clazz_cp_utfstring_from_index(clazz, bvm_file_read_uint16(buffer));
			method->jni_signature = bvm_clazz_cp_utfstring_from_index(clazz, bvm_file_read_uint16(buffer));
			method->num_args      = clazz_count_method_args(method->jni_signature);

			/* if the method returns a value, distinguish between a long/double return and a
			 * normal one. The method 'returns value' int is used both as a counter of the number
			 * of cells to return and as a boolean - Java void is returns_value = zero. */
			return_char = method->jni_signature->data[method->jni_signature->length - 1];
			if (return_char != 'V') {  /* not 'void' */
				if ( (return_char == 'J') || (return_char == 'D'))
					method->returns_value = 2;
				else
					method->returns_value = 1;
			}

			/* the number of attributes for the method */
			attr_count = bvm_file_read_uint16(buffer);

			/* if the method is native, resolve the native method right now. */
			if (BVM_METHOD_IsNative(method)) {

				bvm_native_method_desc_t *method_desc;

				/* get native descriptor method from the native method pool */
				method_desc = bvm_native_method_pool_get(clazz->name, method->name, method->jni_signature);

				/* .. and link it to the method.  No pooled descriptor means 'NULL' - that will cause
				 * an UnresolvedLinkError later on if the method is attempted to be used.  It would seem logical to
				 * fail class loading now if it cannot be resolved, but remember, the Open JDK VM assumes it can
				 * load native methods dynamically, so the failure actually occurs when it is
				 * to be used, not here.*/
				method->code.nativemethod = (method_desc == NULL) ? NULL : method_desc->method;
			}

			/* for each attribute of the method. */
			for (lc2 = 0; lc2 < attr_count; lc2++) {

				bvm_uint32_t attr_length;

				/* get the attribute name */
				bvm_utfstring_t *attr_name = bvm_clazz_cp_utfstring_from_index(clazz, bvm_file_read_uint16(buffer));

				/* get the attribute length */
				attr_length = bvm_file_read_uint32(buffer);

#if (BVM_DEBUGGER_ENABLE && BVM_DEBUGGER_SIGNATURES_ENABLE)

				/* is this the 'Signature' attribute ? */
				if (strncmp((char *) attr_name->data, "Signature", 9) == 0) {
					method->generic_signature = bvm_clazz_cp_utfstring_from_index(clazz, bvm_file_read_uint16(buffer));
					continue;
				}
#endif
				/* is this the 'Code' attribute? */
				if (strncmp((char *) attr_name->data, "Code", 4) == 0) {

					bvm_uint16_t code_attrcount;
					bvm_uint32_t code_length;

					/* populate the method descriptor */
					method->max_stack  = bvm_file_read_uint16(buffer);
					method->max_locals = bvm_file_read_uint16(buffer);

					code_length = bvm_file_read_uint32(buffer);
					method->code.bytecode = bvm_file_read_bytes(buffer, code_length, BVM_ALLOC_TYPE_STATIC);

#if BVM_DEBUGGER_ENABLE
					/* for the debugger, we need to store the total number of bytecodes */
					method->code_length = code_length;
#endif
					/* process the method's exception table - starting with 'how many are there?' */
					method->exceptions_count = bvm_file_read_uint16(buffer);
					if (method->exceptions_count > 0) {

						/* allocate memory for the exception table */
						method->exceptions = bvm_heap_calloc(method->exceptions_count * sizeof(bvm_exception_t), BVM_ALLOC_TYPE_STATIC);

						/* for each exception, populate an bvm_exception_t in the method's exception array */
						for (lc3=0; lc3 < method->exceptions_count; lc3++) {
							bvm_uint16_t catchtype_index;
							bvm_exception_t *exception = &method->exceptions[lc3];
							exception->start_pc   = bvm_file_read_uint16(buffer);
							exception->end_pc     = bvm_file_read_uint16(buffer);
							exception->handler_pc = bvm_file_read_uint16(buffer);

							catchtype_index = bvm_file_read_uint16(buffer);

							/* a catch type index of zero is a 'finally'. A non-zero catch type is a class - we do not
							 * resolve the clazz as yet, we just store the handle to the name of the class */
							if (catchtype_index != 0)
								exception->catch_type = bvm_clazz_cp_utfstring_from_index(clazz, catchtype_index);
							else
								exception->catch_type = NULL;
						}
					}

					/* number of code attributes */
					code_attrcount= bvm_file_read_uint16(buffer);

					/* for each code attribute */
					for (lc3 = 0; lc3 < code_attrcount; lc3++) {
						bvm_uint32_t codeattrlen;

						bvm_utfstring_t *codeAttrName = bvm_clazz_cp_utfstring_from_index(clazz, bvm_file_read_uint16(buffer));

						codeattrlen = bvm_file_read_uint32(buffer);

#if (BVM_LINE_NUMBERS_ENABLE || BVM_DEBUGGER_ENABLE)

						/* keep a reference to the line numbers if they are there. */
						if (strncmp((char *) codeAttrName->data, "LineNumberTable", 15) == 0) {
							method->line_number_count = bvm_file_read_uint16(buffer);

							if (method->line_number_count > 0) {
								bvm_uint16_t lc4;

								method->line_numbers = bvm_heap_calloc(method->line_number_count * sizeof(bvm_linenumber_t), BVM_ALLOC_TYPE_STATIC);

								for (lc4=0; lc4 < method->line_number_count; lc4++) {
									bvm_linenumber_t *line_number = &method->line_numbers[lc4];
									line_number->start_pc = bvm_file_read_uint16(buffer);
									line_number->line_nr  = bvm_file_read_uint16(buffer);
								}
							}

						} else
#endif

#if BVM_DEBUGGER_ENABLE
						/* load the local variable attribute table if it is there. */
						if (strncmp((char *) codeAttrName->data, "LocalVariableTable", 18) == 0) {
							method->local_variable_count = bvm_file_read_uint16(buffer);

							if (method->local_variable_count > 0) {
								bvm_uint16_t lc5;

								method->local_variables = bvm_heap_calloc(method->local_variable_count * sizeof(bvm_local_variable_t), BVM_ALLOC_TYPE_STATIC);

								for (lc5=0; lc5 < method->local_variable_count; lc5++) {
									bvm_local_variable_t *local_variable = &method->local_variables[lc5];
									local_variable->start_pc = bvm_file_read_uint16(buffer);
									local_variable->length = bvm_file_read_uint16(buffer);
									local_variable->name = bvm_clazz_cp_utfstring_from_index(clazz, bvm_file_read_uint16(buffer));
									local_variable->desc = bvm_clazz_cp_utfstring_from_index(clazz, bvm_file_read_uint16(buffer));
									local_variable->index  = bvm_file_read_uint16(buffer);
								}
							}

						} else
#endif
							bvm_file_skip_bytes(buffer, codeattrlen);
					}
				} else
					bvm_file_skip_bytes(buffer, attr_length);
			}
		}
	}
}

/**
 * Extract the package name from a qualified class name.  The algorithm is basic - search from
 * the right of the qualified class name until the first '/' is encountered or the beginning
 * of the name is reached.  All that remains to the left (even if nothing) is the package name.
 *
 * Then, check the UTF string pool for a matching value, if there is one, return a pointer to
 * that utfstring.
 *
 * @param clazzname the qualified name of the class to extract the package name from
 * @return a bvm_utfstring_t* package name, or \c NULL if the class has no package.
 */
bvm_utfstring_t *clazz_get_package(bvm_utfstring_t *clazzname) {

	bvm_int16_t lc;
	bvm_utfstring_t *package = NULL;
	bvm_utfstring_t *pooled_string = NULL;

	for (lc = clazzname->length; lc--;)
		if (clazzname->data[lc-1] == '/')
			break;

	if (lc != -1) {
		/* GC note: heap allocated by copy_utfstring, but no transient root required
		 * here (no further allocations) - but we do create the utfstring as STATIC in
		 * case it is added to the pool. */
		package = bvm_str_copy_utfstring(clazzname, 0, lc-1, 0, BVM_TRUE);
		pooled_string = bvm_utfstring_pool_get(package, BVM_TRUE);

		/* if the return pointer is not the same as the one we passed in, there must have
		 * been a utfstring cached already so we'll free the temp utfstring we created in this
		 * function */
		if (pooled_string != package) bvm_heap_free(package);
	}

	return pooled_string;
}

/**
 * Create the \c java/lang/Class object for a given clazz.
 *
 * @param clazz the clazz to create a Class object for.
 * @return a Class object as a pointer to a #bvm_class_obj_t.
 */
static bvm_class_obj_t *clazz_create_class(bvm_clazz_t *clazz) {

	/* get the Class clazz */
	bvm_instance_clazz_t *class_clazz =
		(bvm_instance_clazz_t *) bvm_clazz_get_c(BVM_BOOTSTRAP_CLASSLOADER_OBJ, "java/lang/Class");

	/* allocate memory for a new Class object */
	bvm_class_obj_t *class_obj = (bvm_class_obj_t *) bvm_object_alloc(class_clazz);

	/* reference the clazz from the new Class object */
	class_obj->refers_to_clazz = clazz;

	/* reference the class object from the clazz */
	clazz->class_obj = class_obj;

	/*
	 * Add the new Class object to the clazz ClassLoader's private _classes array.  Class objects are added to a
	 * classloader to stop them being GC'd and really nothing more. Class objects for bootstrap
	 * classes are push onto the permanent root stack (there is no concrete bootstrap classloader).
	 */
	if (clazz->classloader_obj == BVM_BOOTSTRAP_CLASSLOADER_OBJ) {
		BVM_MAKE_PERMANENT_ROOT(class_obj);
	}
	else {

		/* each class loader keep an array of Class objects that it has loaded.  We're going to add our new
		 * Class object to that array */

		bvm_class_instance_array_obj_t *new_array, *array = clazz->classloader_obj->class_array;
		bvm_uint32_t length = array->length.int_value;

		/* if the array is full, we'll expand it by a factor of 2 by creating a new
		 * array and copying existing elements (which are Class objects) into it. The old array is
		 * then effectively discarded. */
		if (clazz->classloader_obj->nr_classes.int_value == length) {

			BVM_BEGIN_TRANSIENT_BLOCK {

				/* make sure our new Class instance is not lost if we GC in the next alloc */
				BVM_MAKE_TRANSIENT_ROOT(class_obj);

				/* create a new larger array that is twice the size of the old one */
				new_array = (bvm_class_instance_array_obj_t *) bvm_object_alloc_array_reference(length << 1, array->clazz->component_clazz);

			} BVM_END_TRANSIENT_BLOCK

			/* copy the current array contents (Class objects) into the new array */
			memcpy(new_array->data, array->data, length * sizeof(bvm_obj_t *));

			/* set the classloader's array to the new array.  The old array will be
			 * GC'd at the next GC run. */
			clazz->classloader_obj->class_array = new_array;

			/* set the local working array */
			array = new_array;
		}

		/* store the new Class object into the classes array instance in the classloader */
		array->data[clazz->classloader_obj->nr_classes.int_value++] = (bvm_class_obj_t *) class_obj;
	}

	return class_obj;
}

#if BVM_DEBUGGER_ENABLE

/**
 * Copies a utfstring instance class name to a JNI signature utfstring.  The new
 * utfstring is allocated a #BVM_ALLOC_TYPE_STATIC.  A JNI signature is created from a
 * class name by inserting an 'L' at the front and appending a ';' at the end.
 *
 * @param instance_clazzname the name of an instance clazz.
 * @return a new #bvm_utfstring_t contain the JNI signature.
 */
static bvm_utfstring_t *clazz_create_jnisignature(bvm_utfstring_t *instance_clazzname)  {

	bvm_uint16_t length;
	bvm_utfstring_t *str;

	/* get the length of the string */
	length = instance_clazzname->length + 2;

	str = bvm_str_new_utfstring(length, BVM_ALLOC_TYPE_STATIC);

	str->data[0] = 'L';
	memcpy(&str->data[1], instance_clazzname->data, instance_clazzname->length);
	str->data[str->length-1] = ';';

	/* null terminate it */
	str->data[length] = '\0';

	return str;
}

#endif

/**
 * Create an instance clazz structure by reading a Java class file contents from a buffer.  The given
 * class loader is the starting point for class loading.
 *
 * @param classloader_obj the initiating classloader.
 * @param buffer the buffer to load the class from.
 *
 * @return a pointer to an #bvm_instance_clazz_t.
 */
static bvm_instance_clazz_t *clazz_load_instance_clazz(bvm_classloader_obj_t *classloader_obj, bvm_filebuffer_t *buffer) {

	/* the loaded class */
	bvm_instance_clazz_t *clazz = NULL;

	bvm_uint16_t superindex;

	/* Do some house keeping and check that the contents of the buffer has the JVMS 0xCAFEBABE magic number
	 * and then proceed to load the class by scanning through it and creating pointer structures to its
	 * internals.  JVMS - throw a ClassFormatError if it is not there. */
	if (bvm_file_read_uint32(buffer) != BVM_MAGIC_NUMBER)
		bvm_throw_exception(BVM_ERR_CLASS_FORMAT_ERROR, NULL);

	/* allocate memory for the new clazz structure */
	clazz = bvm_heap_calloc(sizeof(bvm_instance_clazz_t), BVM_ALLOC_TYPE_INSTANCE_CLAZZ);

	/* identify the memory as being a clazz - GC uses this signature to help
	 * determine if a memory pointer is really an object */
	clazz->magic_number = BVM_MAGIC_NUMBER;

	BVM_BEGIN_TRANSIENT_BLOCK {

		/* keep a handle to our new clazz.  Lots of memory manipulation goes on during class
		 * loading - we do not want it GC'd. */
		BVM_MAKE_TRANSIENT_ROOT(clazz);

		BVM_TRY {

			/* initial state for beginning of class load */
			clazz->state = BVM_CLAZZ_STATE_LOADING;

			/* .. and set the classloader of this new class to the initiating classloader */
			clazz->classloader_obj = classloader_obj;

#if BVM_DEBUGGER_ENABLE
			clazz->minor_version = bvm_file_read_uint16(buffer);
			clazz->major_version = bvm_file_read_uint16(buffer);
#else
			/* skip past the and minor and major versions */
			bvm_file_skip_bytes(buffer,4);
#endif

			/* process the constant pool */
			clazz_load_cp(clazz, buffer);

			/* .. read access flags, and also flag this clazz as instance clazz.  The usage of BVM_CLASS_ACCESS_FLAG_INSTANCE
			 * here is to squeeze some extra usage out of the 'holes' left in the JVMS access flags definitions - it is
			 * not a real JVMS access flag. */
			clazz->access_flags = bvm_file_read_uint16(buffer) | BVM_CLASS_ACCESS_FLAG_INSTANCE;

			/* the class name of the clazz.  We'll store it as a pointer in the clazz so we don't have to
			 * keep getting it over and over again.  The string content points into the class constant pool. */
			clazz->name = bvm_clazz_cp_utfstring_from_index(clazz, bvm_file_read_uint16(buffer));

			/* ... likewise for the package name */
			clazz->package = clazz_get_package(clazz->name);

#if BVM_DEBUGGER_ENABLE
			clazz->jni_signature = clazz_create_jnisignature(clazz->name);
#endif

			/* the super class of this class.*/
			superindex = bvm_file_read_uint16(buffer);

			/* the only class with superindex == 0 is the Object class */
			if (superindex > 0) {

				/* the name of the super class */
				bvm_utfstring_t *scname = bvm_clazz_cp_utfstring_from_index(clazz, superindex);

				/* get the super class, given the name */
				clazz->super_clazz = (bvm_instance_clazz_t *) bvm_clazz_get(classloader_obj, scname);

				/* JVMS - "if C is an interface it must have Object as its direct superclass" */
				if (BVM_CLAZZ_IsInterface(clazz) && (clazz->super_clazz != BVM_OBJECT_CLAZZ))
					bvm_throw_exception(BVM_ERR_INCOMPATIBLE_CLASS_CHANGE_ERROR, NULL);

				/* JVMS - "If the class or interface named as the direct superclass of C is in fact
				 * an interface, loading throws an IncompatibleClassChangeError" */
				if (!BVM_CLAZZ_IsInterface(clazz) && BVM_CLAZZ_IsInterface(clazz->super_clazz))
					bvm_throw_exception(BVM_ERR_INCOMPATIBLE_CLASS_CHANGE_ERROR, NULL);

				/* JVMS - "if any of the superclasses of C is C itself, loading throws
				 * a ClassCircularityError". We can tell this by testing the state of the superclass.  If
				 * it is also in the processes of being loaded while this class
				 * is being loaded - it must somehow be a superclass of itself. */
				if (clazz->super_clazz->state == BVM_CLAZZ_STATE_LOADING)
					bvm_throw_exception(BVM_ERR_CLASS_CIRCULARITY_ERROR, NULL);

				/* JVMS - superclazzes are not allowed to be final */
				if (BVM_CLAZZ_IsFinal(clazz->super_clazz))
					bvm_throw_exception(BVM_ERR_VERIFY_ERROR, NULL);

				/* JVMS - A super clazz must be accessible to its subclasses.  */
				if (!bvm_clazz_is_class_accessible( (bvm_clazz_t *) clazz, (bvm_clazz_t *) clazz->super_clazz))
					bvm_throw_exception(BVM_ERR_VERIFY_ERROR, NULL);

				/* default the field count of this class to be the field count of the super class. When we
				 * this read this class's fields, this count is increased to accommodate fields declared in this class.
				 * This field count is used to determine the memory requirements of an instance of this class */
				clazz->instance_fields_count = clazz->super_clazz->instance_fields_count;
			}

			/* load interfaces */
			clazz_load_interfaces(clazz, buffer);

			/* load fields */
			clazz_load_fields(clazz, buffer);

			/* load methods */
			clazz_load_methods(clazz, buffer);

#if (BVM_LINE_NUMBERS_ENABLE || BVM_DEBUGGER_ENABLE)
			/* load class attributes */
			clazz_load_attributes(clazz, buffer);
#endif

		} BVM_CATCH (e) {
			/* Something has gone wrong.  Mark clazz as being in error and rethrow the exception */
			clazz->state = BVM_CLAZZ_STATE_ERROR;
			BVM_THROW(e);
		} BVM_END_CATCH;

		/* no errors thrown so mark as loaded */
		clazz->state = BVM_CLAZZ_STATE_LOADED;

		/* add it to the clazz pool */
		bvm_clazz_pool_add((bvm_clazz_t *) clazz);

		/* create and store the Class object of newly loaded clazz */
		clazz->class_obj = clazz_create_class((bvm_clazz_t *) clazz);

	} BVM_END_TRANSIENT_BLOCK

#if BVM_DEBUGGER_ENABLE
	if (bvmd_is_session_open()) {

		bvmd_eventcontext_t *context = bvmd_eventdef_new_context(JDWP_EventKind_CLASS_PREPARE, bvm_gl_thread_current);
		context->location.clazz = (bvm_clazz_t *) clazz;

		bvmd_event_ClassPrepare(context);
	}
#endif

	return clazz;
}

/**
 * Create a clazz structure for a primitive clazz given the class name.
 *
 * @param clazzname the name of the clazz
 *
 * @return a pointer to a #bvm_primitive_clazz_t
 */
bvm_primitive_clazz_t *clazz_load_primitive_clazz(bvm_utfstring_t *clazzname) {

	/* create a new STATIC copy of the clazzname utfstring - this is done before
	 * the class memory allocation - this is STATIC, class memory is not.  Doing this first just saves us
	 * a transient block. */
	bvm_utfstring_t *newname = bvm_str_copy_utfstring(clazzname, 0, clazzname->length, 0, BVM_TRUE);

	/* allocate memory for the primitive class */
	bvm_primitive_clazz_t *clazz = bvm_heap_calloc(sizeof(bvm_primitive_clazz_t), BVM_ALLOC_TYPE_PRIMITIVE_CLAZZ);

	/* identify the memory as being a clazz - GC uses this signature to help
	 * determine if a memory pointer is really an object */
	clazz->magic_number = BVM_MAGIC_NUMBER;

	clazz->name = newname;

#if BVM_DEBUGGER_ENABLE
	{
		BVM_BEGIN_TRANSIENT_BLOCK {

			char s;
			bvm_utfstring_t *signature;

			/* make sure our clazz does not get GC'd if the create_class_for_clazz causes a GC */
			BVM_MAKE_TRANSIENT_ROOT(clazz);

			/* create the primitive class JNI signature as a string of length 1.  The
			 * primitive signature is not the same as its name.  The name may be
			 * (say) "boolean", but the signature is "Z". Long is 'J. The rest
			 * use the fist char from the primitive name. */
			signature = bvm_str_new_utfstring(1,BVM_ALLOC_TYPE_STATIC);
			s = newname->data[0];
			if (s == 'b') {
				s = (newname->data[1] == 'y') ? 'B' : 'Z';
			} else {
				if (s=='l') s = 'J';
				else s -= 32; /* convert to uppercase.  We have c,i,s,f,d to C,I,F,S,D */
			}

			/* set the first (and only) char of the signature string */
			signature->data[0] = s;

			/* assigned the new signature string to the class signature */
			clazz->jni_signature = signature;

		} BVM_END_TRANSIENT_BLOCK
	}
#endif

	/* primitive classes all descend from Object and belong to the system class loader
	 * regardless of which class initiated the load. */
	clazz->super_clazz = BVM_OBJECT_CLAZZ;
	clazz->classloader_obj = BVM_BOOTSTRAP_CLASSLOADER_OBJ;

	clazz->access_flags
			= BVM_CLASS_ACCESS_FLAG_PRIMITIVE |
			  BVM_CLASS_ACCESS_PUBLIC |
			  BVM_CLASS_ACCESS_FINAL |
			  BVM_CLASS_ACCESS_ABSTRACT;

	clazz->state = BVM_CLAZZ_STATE_INITIALISED;

	BVM_BEGIN_TRANSIENT_BLOCK {

		/* make sure our clazz does not get GC'd if the create_class_for_clazz causes a GC */
		BVM_MAKE_TRANSIENT_ROOT(clazz);

		/* add it to the clazz pool */
		bvm_clazz_pool_add( (bvm_clazz_t *) clazz);

		/* create and store the Class object for the new clazz */
		clazz->class_obj = clazz_create_class( (bvm_clazz_t *) clazz);

	} BVM_END_TRANSIENT_BLOCK

	return clazz;
}

/**
 * Create an array clazz struct given its name.  A handle is kept to the clazzname utfstring
 * pointer that is passed in.  Do not free it.
 *
 * Primitive arrays use the bootstrap loader. Reference arrays use the classloader of their component type.
 *
 * JVMS 5.3.3 Creating Array Classes. "If the component type is a reference type, C is
 * marked as having been defined by the defining class loader of the component type.
 * Otherwise, C is marked as having been defined by the bootstrap class loader."
 *
 * @param classloader_obj the classloader for the new clazz
 * @param clazzname the name of the clazz
 *
 * @return a pointer to a #bvm_array_clazz_t
 */
static bvm_array_clazz_t *clazz_load_array_clazz(bvm_classloader_obj_t *classloader_obj, bvm_utfstring_t *clazzname) {

	/* create a new STATIC copy of the clazzname utfstring - this is done before
	 * the class memory allocation - this is STATIC, it is not.  Doing this first just saves us
	 * a transient block. */
	bvm_utfstring_t *newname = bvm_str_copy_utfstring(clazzname, 0, clazzname->length, 0, BVM_TRUE);

	/* allocate memory for the new class */
	bvm_array_clazz_t *clazz = bvm_heap_calloc(sizeof(bvm_array_clazz_t), BVM_ALLOC_TYPE_ARRAY_CLAZZ);

	/* identify the memory as being a clazz - GC uses this signature to help
	 * determine if a memory pointer is an object */
	clazz->magic_number = BVM_MAGIC_NUMBER;

	clazz->name = newname;
	clazz->super_clazz = BVM_OBJECT_CLAZZ;

#if BVM_DEBUGGER_ENABLE
	clazz->jni_signature = newname;
#endif

	clazz->access_flags
			= BVM_CLASS_ACCESS_FLAG_ARRAY |
			  BVM_CLASS_ACCESS_PUBLIC |
			  BVM_CLASS_ACCESS_FINAL |
			  BVM_CLASS_ACCESS_ABSTRACT;

	/* the second char in the name will give us the type of the array.  For example, a char array will
	 * be "[C" and a String array (or any object array) will be "[L", an array of arrays will be "[["
	 * and so on. */
	clazz->component_jtype = (bvm_uint8_t) bvm_clazz_get_array_type(newname->data[1]);

	/* assume the bootstrap loader, if the array turns out not to be a primitive array
	 * we'll change this */
	clazz->classloader_obj = BVM_BOOTSTRAP_CLASSLOADER_OBJ;

	/* set class loading state to 'all-done'.  No class initialisation required for array classes */
	clazz->state = BVM_CLAZZ_STATE_INITIALISED;

	BVM_BEGIN_TRANSIENT_BLOCK {

		/* make sure our clazz does not get GC'd if any of the following causes a GC */
		BVM_MAKE_TRANSIENT_ROOT(clazz);

		/* if we are creating an array of arrays, remove the first '[' from clazzname to get
		 * the name of the element type. */
		if (clazz->component_jtype == BVM_T_ARRAY) {
			bvm_utfstring_t s;
			s.length = newname->length-1;
			s.data = &newname->data[1];

			/* get the class of the array's component class. This has the happy side effect of
			 * eventually recursing all the way down into an array (or array of arrays etc etc)
			 * components to get the instance class of the innermost component and loads it with
			 * the correct classloader. When we recurse back up, the classloader for that
			 * innermost class is propagated outwards.  Nice.  All array classes end up with
			 * the classloader of the inner-most component class - which is exactly how it
			 * should be according to the JVMS. Decent effort for one line of code ... :-) */
			clazz->component_clazz = bvm_clazz_get(classloader_obj, &s);

			/* array classes with array elements have the same classloader as their component type */
			clazz->classloader_obj = clazz->component_clazz->classloader_obj;

			/* we also give them the same package as their component type*/
			clazz->package = clazz->component_clazz->package;
		}

		/* Else, if we are creating an array of objects */
		else if (clazz->component_jtype == BVM_T_OBJECT) {

			/* remove the '[L' from the front of the name and the ';' from the end to
			 * determine the name of the element type */
			bvm_utfstring_t s;
			s.length = newname->length-3;
			s.data = &newname->data[2];
			clazz->component_clazz = bvm_clazz_get(classloader_obj, &s);

			/* array classes with object elements have the same loader as their component type */
			clazz->classloader_obj = clazz->component_clazz->classloader_obj;

			/* we also give them the same package as their component type */
			clazz->package = clazz->component_clazz->package;
		}

		/* add it to the pool */
		bvm_clazz_pool_add( (bvm_clazz_t *) clazz);

		/* and populate the class object for it */
		clazz->class_obj = clazz_create_class( (bvm_clazz_t *) clazz);

	} BVM_END_TRANSIENT_BLOCK

#if BVM_DEBUGGER_ENABLE

	if (bvmd_is_session_open()) {

		bvmd_eventcontext_t *context = bvmd_eventdef_new_context(JDWP_EventKind_CLASS_PREPARE, bvm_gl_thread_current);
		context->location.clazz = (bvm_clazz_t *) clazz;

		bvmd_event_ClassPrepare(context);
	}

#endif

	return clazz;
}

#if (BVM_LINE_NUMBERS_ENABLE || BVM_DEBUGGER_ENABLE)

/**
 * For a PC for a given method, find the source file line number if it is available.
 * If source line numbers are not enabled, or are not contained in the class file this
 * will return -1.
 *
 * @param method the method
 * @param pc the bytecode pointer within the given method to get the line of
 *
 * @return the line number if there is one, or -1 if there is no lines in the source or no line for
 * the given pc.
 */
bvm_int32_t bvm_clazz_get_source_line(bvm_method_t *method, bvm_uint8_t *pc) {

	int lc, current_pc;

	/* the current rx_pc can be calculated as the offset of the given pc less the address of
	 * the start of the method bytecode. */
	current_pc = (int) (pc - method->code.bytecode);

	/* loop *backwards* through the line numbers looking for the line number
	 * that is *less* that the given pc in the given method.  When we reach one, that is
	 * our line number. */
	for (lc = method->line_number_count; lc--;) {
		bvm_linenumber_t *linenr = &method->line_numbers[lc];
		if (current_pc >= linenr->start_pc)
			return linenr->line_nr;
	}

	return -1;
}

#endif

/**
 * Reports if a given class implements a given interface.  This include all superclasses and
 * superinterfaces.
 *
 * @param clazz the class to test
 * @param interface the interface to test for
 *
 * @return \c BVM_TRUE if the given class implements the given interface, \c BVM_FALSE otherwise.
 */
bvm_bool_t bvm_clazz_implements_interface(bvm_instance_clazz_t *clazz, bvm_instance_clazz_t *interface) {

	int lc;

	/* The same?  No need to check any further */
	if (clazz == interface)
		return BVM_TRUE;

	while (BVM_TRUE) {

		/* loop through each interface for the current "this" class.  */
		for (lc = clazz->interfaces_count; lc--;) {

			/* get the class of the interface */
			bvm_instance_clazz_t *interface_clazz = clazz->interfaces[lc];

			/* if we have a match, return true */
			if (interface_clazz == interface)
				return BVM_TRUE;

			/* else check the superinterfaces ... */
			if (bvm_clazz_implements_interface(interface_clazz, interface))
				return BVM_TRUE;
		}

		/* if we have reached the Object class then fail */
		if (clazz == BVM_OBJECT_CLAZZ)
			return BVM_FALSE;

		/* look in superclass */
		clazz = clazz->super_clazz;
	}
}

/**
 * Reports if a object is an instance of a given class or interface.
 *
 * @param obj the object to test
 * @param clazz the clazz/interface to test for
 *
 * @return \c BVM_TRUE is the object is an instance of the class, \c BVM_FALSE otherwise.
 */
bvm_bool_t bvm_clazz_is_instanceof(bvm_obj_t * obj, bvm_clazz_t * clazz) {
	return (obj == NULL) ? BVM_FALSE : bvm_clazz_is_assignable_from(obj->clazz, clazz);
}

/**
 * Reports if one class is assignment-compatible with another.  The parameters are 'from' and 'to' classes.  These
 * represent the R-value and L-value of an expression respectively.
 *
 * This method is complex.  It is an implementation of the rules describes in the JVMS
 * Section 2.6.7 Assignment Conversion.  Interested developers should read that section over very slowly
 * while inspecting this function - it covers all Java class assignment compatibility scenarios.
 *
 *
 * @param from_clazz the R-value class
 * @param to_clazz the L-value class
 *
 * @return \c BVM_TRUE if things from the from class is assignment compatible with the to class, \c BVM_FALSE
 * otherwise.
 */
bvm_bool_t bvm_clazz_is_assignable_from(bvm_clazz_t *from_clazz, bvm_clazz_t *to_clazz) {

	while (BVM_TRUE) {

		/* if classes are the same or class is comparing against Object class, forget it .. will
		 * always be BVM_TRUE.  NULL is assignment compatible with everything. */
		if ((from_clazz == NULL) || (from_clazz == to_clazz) || (to_clazz == (bvm_clazz_t *) BVM_OBJECT_CLAZZ)) {
			return BVM_TRUE;
		}

		if (BVM_CLAZZ_IsArrayClazz(to_clazz)) {

			/* only a compatible array can be assigned to an array */
			if (!BVM_CLAZZ_IsArrayClazz(from_clazz)) {
				/* a non-array class can't be a assigned to an array class */
				return BVM_FALSE;
			} else {
				/* assigning an array to an array */
				bvm_uint8_t fromType = ((bvm_array_clazz_t *)from_clazz)->component_jtype;
				bvm_uint8_t toType   = ((bvm_array_clazz_t *)to_clazz)->component_jtype;

				/* .. if either is a primitive both must be the same type */
				if ( (fromType > BVM_T_ARRAY) || (toType > BVM_T_ARRAY))
					return ((bvm_array_clazz_t *)from_clazz)->component_jtype == ((bvm_array_clazz_t *)to_clazz)->component_jtype;

				/* .. then both must be object types.  Could be arrays, could be objects.
				 * We'll get their component clazzes and return to the top of our
				 * checking loop and compare those ..*/
				from_clazz = ((bvm_array_clazz_t *)from_clazz)->component_clazz;
				to_clazz   = ((bvm_array_clazz_t *)to_clazz)->component_clazz;
				continue;

			}
		} else if (BVM_CLAZZ_IsInterface(to_clazz)) {

			/* ... assigning to an interface.  If from_clazz is any array and the to_clazz is either the
			 * cloneable or serializeable interfaces - all good.  Yes, the VM does not support
			 * serializeable as yet, but it is here for good measure.  */
			 if (BVM_CLAZZ_IsArrayClazz(from_clazz)) {

				 return ( (strncmp("java/lang/Cloneable", (char *) to_clazz->name->data, 19) == 0) ||
  					      (strncmp("java/io/Serializable", (char *) to_clazz->name->data, 20) == 0) );
			 }

			 /* ... otherwise from_clazz must be a non-array that implements the interface. */
			 return (bvm_clazz_implements_interface((bvm_instance_clazz_t *) from_clazz, (bvm_instance_clazz_t *) to_clazz));
		} else {

			/* ... assigning to a non-array, non-interface class that is not the Object class */

			if (BVM_CLAZZ_IsArrayClazz(from_clazz) ||
			   (BVM_CLAZZ_IsInterface(from_clazz))) {
				/* .. arrays and interfaces can only be assigned to
				 * Object ... and we already know that it's not */
				return BVM_FALSE;
			} else {
				/* ... from_clazz is also a non-array, non-interface class.  We
				 * need to check to see if from_clazz is a subclass of to_clazz.
				 * We already know from_clazz != to_clazz.
				 */
				bvm_instance_clazz_t *from_iclazz = (bvm_instance_clazz_t *) from_clazz;
				bvm_instance_clazz_t *to_iclazz   = (bvm_instance_clazz_t *) to_clazz;

				/* check for 'superness'. */
				while (from_iclazz != BVM_OBJECT_CLAZZ) {
					from_iclazz = from_iclazz->super_clazz;
					if (from_iclazz == to_iclazz) {
						return BVM_TRUE;
					}
				}
				return BVM_FALSE;
			}
		}
	}
}

/**
 * Internal class initialisation logic.
 *
 * @param clazz the class to initialise.
 * @param frame_pushed the address of a boolean.  Will be set to \c BVM_TRUE if clazz initialisation caused
 * any framed to be pushed onto the stack.
 */
void bvm_clazz_initialise0(bvm_instance_clazz_t *clazz, bvm_bool_t *frame_pushed) {

	bvm_method_t *clinit_method;
	bvm_method_t *doinit_method;

	/* Class is already initialised?  Do nothing */
	if (BVM_CLAZZ_IsInitialised(clazz))
		return;

	/* JVMS 2.17.5 - if class is in erroneous state, throw NoClassDefFoundError. */
	if (clazz->state == BVM_CLAZZ_STATE_ERROR) {
		bvm_throw_exception(BVM_ERR_NO_CLASS_DEF_FOUND_ERROR,  "Class in error state at init");
	}

	/* find the clazz initialisation method - looking only in the class (no supers or interfaces) */
	clinit_method = bvm_clazz_method_get(clazz, bvm_utfstring_pool_get_c("<clinit>", BVM_TRUE),
			                   bvm_utfstring_pool_get_c("()V", BVM_TRUE), BVM_METHOD_SEARCH_CLAZZ);

	if (clinit_method != NULL) {

		doinit_method = bvm_clazz_method_get( (bvm_instance_clazz_t *) clazz->class_obj->clazz,
			                       bvm_utfstring_pool_get_c("__doInit", BVM_TRUE),
				                   bvm_utfstring_pool_get_c("()V", BVM_TRUE), BVM_METHOD_SEARCH_CLAZZ);

		/* push a frame onto the stack to run the doinit method - the bvm_gl_rx_locals[0] thing is to push
		 * the instance onto the stack as the first argument to the method - it is 'this'. */
		bvm_frame_push(doinit_method, bvm_gl_rx_sp, bvm_gl_rx_pc, bvm_gl_rx_pc, NULL);
		bvm_gl_rx_locals[0].ref_value = (bvm_obj_t *) clazz->class_obj;

		*frame_pushed = BVM_TRUE;

	} else {

		/* no <clinit> method so we'll venture up the class hierarchy and do it
		 * in a more simple manner */

		/* set this class loading state to 'all okay' */
		clazz->state = BVM_CLAZZ_STATE_INITIALISED;

		/* if this clazz is not an interface and has an unitialised superclass, initialise the
		 * superclass (recursive).  Interfaces are not initialised. */
		if ( (clazz->super_clazz != NULL) &&
			 (!BVM_CLAZZ_IsInitialised(clazz->super_clazz)) &&
			 (!BVM_CLAZZ_IsInterface(clazz))) {
			bvm_clazz_initialise0((bvm_instance_clazz_t *) clazz->super_clazz, frame_pushed);
		}
	}
}

/**
 * Initialise a clazz according to JVMS section 5.5.  Classes that do not have a &lt;clinit&gt; method
 * are treated simply.  Classes that do have a &lt;clinit&gt; method have the Class.__doinit() method called
 * on their Class object - this means a new frame to execute this is pushed into the stack.
 *
 * @param clazz the class to initialise.
 * @return \c BVM_TRUE if class initialisation caused any frames to be pushed onto the stack,
 * \c BVM_FALSE otherwise.
 */
bvm_bool_t bvm_clazz_initialise(bvm_instance_clazz_t *clazz) {

	bvm_bool_t frame_pushed = BVM_FALSE;

	bvm_clazz_initialise0(clazz, &frame_pushed);

	return frame_pushed;
}

/**
 * Reports whether one class if the subclass of another.  This does not test for interfaces, only the superclass
 * parentage.  If the given sub_clazz and super_clazz are actually the same, this function will return BVM_TRUE.
 *
 * @param sub_clazz the potential subclass to test
 * @param super_clazz the potential superclass to test
 *
 * @return \c BVM_TRUE is \c sub_clazz is a subclass of \c super_clazz, \c BVM_FALSE otherwise.
 */
bvm_bool_t bvm_clazz_is_subclass_of(bvm_clazz_t *sub_clazz, bvm_clazz_t *super_clazz) {

	while (sub_clazz != NULL) {
		if (sub_clazz == super_clazz)
			return BVM_TRUE;
		sub_clazz = (bvm_clazz_t *) sub_clazz->super_clazz;
	}

	return BVM_FALSE;
}

/**
 * The reports whether one class is accessible from another.  The 'from' class is trying to access
 * something in the 'to' class.  JVMS 5.5 Access Control.
 *
 * @param from_clazz the class trying access something in the \c to_clazz.
 * @param to_clazz the class being accessed.
 *
 * @return \c BVM_TRUE if access if permitted, \c BVM_FALSE otherwise.
 */
bvm_bool_t bvm_clazz_is_class_accessible(bvm_clazz_t *from_clazz, bvm_clazz_t *to_clazz) {

	if ( (from_clazz == to_clazz) || (BVM_CLAZZ_IsPublic(to_clazz)) || (
			from_clazz->package == to_clazz->package))
		return BVM_TRUE;

	return BVM_FALSE;
}

/**
 * Check accessibility of a member of a clazz - JVMS 5.4.4 Access Control.  Accessibility rules for
 * fields and methods are the same.  The access_flags are the access flags of the member in
 * the defining class.
 *
 * A field or method R is accessible to a class or interface D if and only if any of the following conditions is true:
 *   - R is public.
 *   - R is protected and is declared in a class C, and D is either a subclass of C or C itself.
 *   - R is either protected or package private (that is, neither public nor protected nor private), and is
 * 		declared by a class in the same runtime package as D.
 *   - R is private and is declared in D.
 *
 * @param access_flags the to-be-accessed member access flags
 * @param from_clazz the clazz trying to access the member
 * @param defining_clazz the clazz where the member is defined
 *
 * @return \c BVM_TRUE if the member is accessible, \c BVM_FALSE otherwise.
 */
bvm_bool_t bvm_clazz_is_member_accessible(bvm_uint16_t access_flags, bvm_instance_clazz_t *from_clazz, bvm_instance_clazz_t *defining_clazz) {

	/* if public, who cares */
	if (BVM_ACCESS_IsPublic(access_flags))
		return BVM_TRUE;

	/* if the field is private, the requesting class must be the same as the defining class */
	if (BVM_ACCESS_IsPrivate(access_flags))
		return (from_clazz == defining_clazz);

	/* protected is a little more involved .... anything in the same package can access a
	 * protected member, as well as any subclass of the defining class (or the defining class itself) */
	if ( (BVM_ACCESS_IsProtected(access_flags)) && (bvm_clazz_is_subclass_of( (bvm_clazz_t *) from_clazz,
			(bvm_clazz_t *) defining_clazz))) return BVM_TRUE;

	/* so ... after all that the final catch all */
	return ( (BVM_ACCESS_IsProtected(access_flags) ||
			  BVM_ACCESS_IsPackagePrivate(access_flags)) &&
			  (from_clazz->package == defining_clazz->package));
}

/**
 * Given a class name and a path attempt to load its complete contents into a memory buffer.
 *
 * @param clazzname the name of the class to load.
 * @param pathname the classpath path to look for the class file.
 *
 */
static bvm_filebuffer_t *clazz_buffer_class_file(bvm_utfstring_t *clazzname, bvm_utfstring_t *pathname) {

	bvm_filebuffer_t *buffer;
	bvm_bool_t path_is_jar;
	int pathlen = pathname->length;

	/* is the pathname a jar file (it ends with ".jar") ? */
	path_is_jar = ( (pathname->length > 4) &&
			        (pathname->data[pathlen-4] == '.') &&
			        (pathname->data[pathlen-3] == 'j') &&
			        (pathname->data[pathlen-2] == 'a') &&
			        (pathname->data[pathlen-1] == 'r'));

	if (path_is_jar) {

		int fullpathlen = clazzname->length + 7; /* +7 for ".class", and '\0' */

		/* allocate memory to store the full path + classname */
		char *fullpath = bvm_heap_alloc(fullpathlen, BVM_ALLOC_TYPE_DATA);

        /* set the path to be the class */
        memcpy(fullpath, clazzname->data, clazzname->length);

        /* append the ".class" to the end of the full path */
        fullpath[clazzname->length] = '\0';
        strcat(fullpath, ".class");

		BVM_BEGIN_TRANSIENT_BLOCK {

			char *pname;

			/* make sure our path does not disappear if a GC occurs */
			BVM_MAKE_TRANSIENT_ROOT(fullpath);

			pname = bvm_str_utfstring_to_cstring(pathname);
			BVM_MAKE_TRANSIENT_ROOT(pname);

			buffer = bvm_zip_buffer_file_from_jar(pname, fullpath);

			bvm_heap_free(pname);
			bvm_heap_free(fullpath);

		} BVM_END_TRANSIENT_BLOCK


	} else {

		/* not a jar */

		/* calculate the length of the complete path */
		int fullpathlen = pathlen + clazzname->length + 8; /* +8 for '/', ".class", and '\0' */

		/* allocate memory to store the full path + classname */
		char *fullpath = bvm_heap_alloc(fullpathlen, BVM_ALLOC_TYPE_DATA);

		/* copy the given path to the front of the fullpath */
		size_t i = pathlen;
		memcpy(fullpath, pathname->data, i);

		/* if path does not end with a slash, append one - we've added the extra
		 * length to fullpathlen for it in case we need it */
		if (fullpath[i-1] != BVM_PLATFORM_FILE_SEPARATOR) {
            fullpath[i++] = BVM_PLATFORM_FILE_SEPARATOR;
        }

		/* append the class name to the path */
		memcpy(fullpath + i, clazzname->data, clazzname->length);

		/* append the ".class" to the end of the full path */
		i += clazzname->length;
		fullpath[i] = '\0';
		strcat(fullpath, ".class");

		/* replace all '/' in class name with the platform specific file separator */
		bvm_str_replace_char(fullpath, strlen(fullpath), '/', BVM_PLATFORM_FILE_SEPARATOR);

		BVM_BEGIN_TRANSIENT_BLOCK {

			/* make sure our path does not disappear if buffering causes a GC */
			BVM_MAKE_TRANSIENT_ROOT(fullpath);

			buffer = bvm_buffer_file_from_platform(fullpath);

		} BVM_END_TRANSIENT_BLOCK

		/* free up the temp full path */
		bvm_heap_free(fullpath);
	}

	return buffer;
}


/**
 * Given a class name and an initiating loader attempt to find and load the class file.
 * The loading mechanism emulates the Java SE class loading delegation model where a
 * class loader's parent is given first go at loading the class.  This has the effect
 * of starting the class loading at the bootstrap class loader and working back 'down' towards
 * the given class loader.  In Java SE, the class loader object is responsible for
 * performing this delegation -  here, it is all done natively.
 *
 * The #classfilebuffer_t is a handle to the loaded buffer and the classloader whose classpath the
 * file was found in. The buffer returned as \c classfilebuffer_t.buffer is allocated from
 * the heap.  It will be \c NULL if the buffering was unsuccessful.  It is the responsibility
 * of the calling function to keep the buffer from being GC'd until no longer needed.
 */
static classfilebuffer_t clazz_get_buffered_class(bvm_classloader_obj_t *classloader_obj, bvm_utfstring_t *clazzname) {

	bvm_uint32_t lc;

	classfilebuffer_t classbuffer;
	bvm_utfstring_t temppath;

	classbuffer.classloader_obj = classloader_obj;
	classbuffer.buffer = NULL;

	/* first thing, if the current loader is not the bootstrap loader, delegate (recursively)
	 * up to the parent classloader. */
	if (classloader_obj != BVM_BOOTSTRAP_CLASSLOADER_OBJ)
		classbuffer = clazz_get_buffered_class(classloader_obj->parent, clazzname);

	/* if the class was not loaded by any parent (or there is no parent). */
	if (classbuffer.buffer == NULL) {

		/* If we are at the bootstrap classloader then use the bootstrap classpath
		 * to attempt to locate and load the requested class */
		if (classloader_obj == BVM_BOOTSTRAP_CLASSLOADER_OBJ) {

			/* cycle through the classpath elements attempting to buffer the class */
			for (lc=0; (lc < BVM_MAX_CLASSPATH_SEGMENTS) && (classbuffer.buffer == NULL); lc++) {

				/* get the path segment.  A NULL segment means the end of the boot classpath.  Not so
				 * for user classpaths - see note below for user classpaths */
				char *segment;
				if ( (segment = bvm_gl_boot_classpath_segments[lc]) == NULL) break;

				temppath.length = (bvm_uint16_t) strlen(segment);
				temppath.data = (bvm_uint8_t *) segment;
				classbuffer.classloader_obj = BVM_BOOTSTRAP_CLASSLOADER_OBJ;
				classbuffer.buffer = clazz_buffer_class_file(clazzname, &temppath);
			}

		} else {

			/* ... so we are not using the bootstrap classloader.  Present impl is that a non-bootstrap
			 * classloader has an array of String objects that define its classpath
			 * segments.  Here we iterate over those classpath segments and attempt to load
			 * the class using each.
			 */

			for (lc=0; (classbuffer.buffer == NULL) && (lc < classloader_obj->paths_array->length.int_value); lc++ ) {

				/* get the String classpath segment */
				bvm_string_obj_t *path_string = (bvm_string_obj_t *) classloader_obj->paths_array->data[lc];

				/* as the classpath of a user classloader may be set programmatically it is
				 * possible a path segment may be NULL, yet still followed by valid path segment - unlike
				 * the bootstrap classpath (which is system maintained) where a NULL means the end of the
				 * classpath segments list ... */
				if (path_string == NULL) continue;

				/* The class path String unicode is first converted to utf and *that* is used as the file
				 * name. So file names can only be utf chars ... */
				BVM_BEGIN_TRANSIENT_BLOCK {

					/* get the utf representation of the String classpath segment */
					bvm_utfstring_t  *path_utfstring =
						bvm_str_unicode_to_utfstring(path_string->chars->data, path_string->offset.int_value, path_string->length.int_value);
					BVM_MAKE_TRANSIENT_ROOT(path_utfstring);

					classbuffer.classloader_obj = classloader_obj;
					classbuffer.buffer = clazz_buffer_class_file(clazzname, path_utfstring);
				} BVM_END_TRANSIENT_BLOCK;
			}
		}

	}

	return classbuffer;
}

/**
 * Given the string name of a class, find it.  That may mean loading and linking the class from a file on the
 * classpath, or simply returning it from the class pool if it has already been loaded.
 *
 * @param classloader_obj the class loader to use as the initiating class loader for the class loading.  Note that array classes of
 * primitive use the bootstrap class loader regardless of what is specified here.  Additionally, class in the "java" or "babe"
 * namespace will also always use the bootstrap classloader.  This is a security measure to stop developers creating classes in
 * these packages, or subpackages.
 * @param clazzname the fully qualified named of the class to load (contains "/" and not ".") - see JVMS docs on class naming
 * conventions.
 * @param is_reflective if \c BVM_TRUE any thrown classloading exceptions will the be the 'reflective' ones and not the
 * non-reflective ones.  Hmmm.  What does that mean?  Simply this - when loading a class in a reflective mode (like from
 * java \c Class.forName()) the exceptions thrown if the class cannot be found are different from normal.  That is it. Sounds
 * complex, but it isn't.
 * @return the loaded class.  If the class cannot be loaded this function will not return - an exception will be thrown.
 */
static bvm_clazz_t *clazz_find(bvm_classloader_obj_t *classloader_obj, bvm_utfstring_t *clazzname, bvm_bool_t is_reflective) {

	classfilebuffer_t classbuffer;

	/* Check the class pool for the class.  */
	bvm_clazz_t *clazz = bvm_clazz_pool_get(classloader_obj, clazzname);

	/* Got a class from the pool?  All done. */
	if (clazz != NULL)
		return clazz;

	/* No class already in pool?  Better try and load it ... */

	/* test for array class. The name of an array class begins with a '['. */
	if (clazzname->data[0] == '[') {

		/* load (create) a return a new clazz for the array name */
		return (bvm_clazz_t *) clazz_load_array_clazz(classloader_obj, clazzname);

	} else {

		/* we are not loading an array class - must be either a primitive class or an instance class */

		int i;

		/* Check for a primitive type.  If it is then return
		 * a primitive class.  Yes, a developer could write Class.forName("byte")
		 * and get a class back. The Java SE does not have this behaviour. */

		/* If the class name has no '/'s, look through each primitive class name and compare
		 * with the requested class name.  If we get a match, return the corresponding primitive
		 * class.*/
		if (bvm_str_utfstrchr( clazzname, '/') == NULL) {
			for (i = 0; primitive_class_names[i] != NULL; i++) {
				size_t len = strlen(primitive_class_names[i]);
				if ( (len == clazzname->length) &&
					 (strncmp((char *) clazzname->data, primitive_class_names[i], len) == 0)) {

					/* load (create) a return a new clazz for the primitive name */
					return (bvm_clazz_t *) clazz_load_primitive_clazz(clazzname);
				}
			}
		}

		/*  .... so not a primitive or array class then.  Must be a real instance class */

		/* if we are dealing with a class from a java or babe package, ensure we use
		 * the bootstrap classloader.  This will stop anybody redefining a system class. */
		if ( (classloader_obj != BVM_BOOTSTRAP_CLASSLOADER_OBJ) &&
			 ((strncmp((char *)clazzname->data, "java/",5) == 0) ||
		      (strncmp((char *)clazzname->data, "babe/",5) == 0)) ) classloader_obj = BVM_BOOTSTRAP_CLASSLOADER_OBJ;


		/* okay, so ... we're ready to attempt to load the instance class.*/

		/* delegate the locating and buffering of the class for the given loader ... */
		classbuffer = clazz_get_buffered_class(classloader_obj, clazzname);

		/* unable to find and buffer the class file?  Throw an exception ... */
		if (classbuffer.buffer == NULL) {

			const char *except_str;

			/* if the VM is not yet initialised then bad news - it is one of the classes the
			 * VM is trying to load - abend. */
			if (!bvm_gl_vm_is_initialised) {
#if BVM_CONSOLE_ENABLE
				bvm_pd_console_out("Fatal error: unable to load bootstrap class: %s\n", (char *)clazzname->data);
#endif
				BVM_VM_EXIT(BVM_FATAL_ERR_UNABLE_TO_REPORT_CLASS_LOADING_FAILURE, NULL)
			}

			/* Different exception is thrown depending on whether the class is being loaded reflectively.
			 * Also, special checking code to make sure the class that is not found is not the
			 * exception class we need to throw to say we couldn't find it! - we'd get into a knot
			 * then.  */
			except_str = (is_reflective) ? BVM_ERR_CLASS_NOT_FOUND_EXCEPTION : BVM_ERR_NO_CLASS_DEF_FOUND_ERROR;
			if (strncmp((char *)clazzname->data, except_str, strlen(except_str))==0) {
				BVM_VM_EXIT(BVM_FATAL_ERR_UNABLE_TO_REPORT_CLASS_LOADING_FAILURE, NULL);
			}
			else {
				bvm_throw_exception(except_str, (char *) clazzname->data);
			}
#if 0
			if (is_reflective) {
				if (strncmp((char *)clazzname->data,
						    BVM_ERR_CLASS_NOT_FOUND_EXCEPTION,
						    strlen(BVM_ERR_CLASS_NOT_FOUND_EXCEPTION))==0) {
					BVM_VM_EXIT(BVM_FATAL_ERR_UNABLE_TO_REPORT_CLASS_LOADING_FAILURE, NULL);
				}
				else {
					bvm_throw_exception(BVM_ERR_CLASS_NOT_FOUND_EXCEPTION, (char *) clazzname->data);
				}
			}
			else {
				if (strncmp((char *)clazzname->data,
						    BVM_ERR_NO_CLASS_DEF_FOUND_ERROR,
						    strlen(BVM_ERR_NO_CLASS_DEF_FOUND_ERROR))==0) {
					BVM_VM_EXIT(BVM_FATAL_ERR_UNABLE_TO_REPORT_CLASS_LOADING_FAILURE, NULL);
				}
				else {
				   bvm_throw_exception(BVM_ERR_NO_CLASS_DEF_FOUND_ERROR, (char *) clazzname->data);
				}
			}
#endif

		} else {

			/* cool.  All okay so far.  We have buffered a class file, so let's now attempt to
			 * load it using the loader we found it with ... */

			BVM_BEGIN_TRANSIENT_BLOCK {

				/* make sure the bytes we are using for the class are not GC'd during the loading
				 * process */
				BVM_MAKE_TRANSIENT_ROOT(classbuffer.buffer);

				/* and load the instance clazz. */
				clazz = (bvm_clazz_t *) clazz_load_instance_clazz(classbuffer.classloader_obj, classbuffer.buffer);

				/* well, we know we're not using this any more so we'll free it manually.  If an
				 * exception was thrown during load_instance_clazz, we'll not reach this line of code,
				 * but the memory will be freed up in the next GC anyways */
				bvm_heap_free(classbuffer.buffer);

			} BVM_END_TRANSIENT_BLOCK
		}

	}

	return clazz;
}

/**
 * Get the class of the given name non-reflectively.
 *
 * @return the loaded class - note this function will not return if the class cannot be loaded.  An
 *  exception will have been thrown.
 *
 */
bvm_clazz_t *bvm_clazz_get(bvm_classloader_obj_t *classloader_obj, bvm_utfstring_t *clazzname) {
	return clazz_find(classloader_obj, clazzname, BVM_FALSE);
}

/**
 * Get the class of the given name reflectively.
 *
 * @return the loaded class - note this function will not return if the class cannot be loaded.  An
 *  exception will have been thrown.
 *
 */
bvm_clazz_t *bvm_clazz_get_by_reflection(bvm_classloader_obj_t *classloader_obj, bvm_utfstring_t *clazzname) {
	return clazz_find(classloader_obj, clazzname, BVM_TRUE);
}

/**
 * Given a char* class name, go get it non-reflectively.
 */
bvm_clazz_t *bvm_clazz_get_c(bvm_classloader_obj_t *classloader_obj, const char *clazzname) {
	bvm_utfstring_t str;
	str.length = strlen(clazzname);
	str.data = (bvm_uint8_t *) clazzname;
	return clazz_find(classloader_obj, &str, BVM_FALSE);
}
