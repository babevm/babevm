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

  JDWP ReferenceType commandset command handlers.

  @author Greg McCreath
  @since 0.0.10

*/

#if BVM_DEBUGGER_ENABLE

/**
 * Internal command handler logic for ReferenceType/Field and ReferenceType/FieldWithGeneric - both are the same - just
 * 'generic' outputs an extra attribute.
 *
 *  The compile time variable #BVM_DEBUGGER_SIGNATURES_ENABLE must be true to processes signatures.  If not true,
 *  \c NULL is always output for a signature.
 *
 * @param in the request input stream
 * @param out the response output stream
 * @param do_generic if BVM_TRUE the field generic signature (\c NULL) will be output.
 */
static void dbg_fields(bvmd_instream_t* in, bvmd_outstream_t* out, bvm_bool_t do_generic) {

	bvm_instance_clazz_t *clazz;
	bvm_int32_t fields_count;
	bvm_int32_t i;
	bvm_uint16_t flags;

	/* no point is dealing with fields if the given clazz ID is not valid */
	if ( (clazz = (bvm_instance_clazz_t *) bvmd_readcheck_reftype(in, out) ) == NULL) return;

	/* additionally, the debugger should not be asking for this information for a clazz that is
	 * not prepared */
	if (clazz->state < BVM_CLAZZ_STATE_LOADED) {
		out->error = JDWP_Error_CLASS_NOT_PREPARED;
		return;
	}

	/* additionally, the debugger should only ask this information from an instance clazz */
	if (!BVM_CLAZZ_IsInstanceClazz(clazz)) {
		out->error = JDWP_Error_INVALID_CLASS;
		return;
	}

	/* write out the number of field the clazz has */
	fields_count = clazz->fields_count;
	bvmd_out_writeint32(out, fields_count);

	/* for each field, in the same order as defined in the class file */
	for (i = 0; i < fields_count; i++) {

		bvm_field_t *field = &(clazz->fields[i]);

		bvmd_out_writeaddress(out, field);  					/* field id is the field address */
		bvmd_out_writestring(out, (char *) field->name->data);	/* name of the field */

		if (!BVM_FIELD_IsNative(field)) {
			bvmd_out_writestring(out, (char *) field->jni_signature->data);	/* JNI signature of the field */
		} else {
			/* write out native fields as INT */
			bvm_utfstring_t utfstring = {1, (bvm_uint8_t *) "I", NULL};
			bvmd_out_writeutfstring(out, &utfstring);
		}

		if (do_generic) {
#if BVM_DEBUGGER_SIGNATURES_ENABLE
			bvmd_out_writeutfstring(out, field->generic_signature);	/* if generic, then output field generic signature */
#else
			bvmd_out_writestring(out, NULL);			/* output NULL (not supported) */
#endif
		}

		/* remove VM specific additions to field flags before sending them ... just in case the
		 * debugger has troubles with the additional non-JVMS flags we use. */
		flags = field->access_flags & ~BVM_FIELD_ACCESS_FLAG_CONST;
		flags &= ~BVM_FIELD_ACCESS_FLAG_LONG;
		flags &= ~BVM_FIELD_ACCESS_FLAG_NATIVE;

		bvmd_out_writeint32(out, flags);				/* modBits */

	}
}

/**
 * Internal command handler logic for ReferenceType/Methods and ReferenceType/MethodsWithGeneric - both are the same - just
 * 'generic' outputs an extra attribute.
 *
 *  The compile time variable #BVM_DEBUGGER_SIGNATURES_ENABLE must be true to processes signatures.  If not true,
 *  \c NULL is always output for a signature.
 *
 * @param in the request input stream
 * @param out the response output stream
 * @param do_generic if BVM_TRUE the methods generic signature (\c NULL) will be output.
 */
static void dbg_methods(bvmd_instream_t* in, bvmd_outstream_t* out, bvm_bool_t do_generic) {

	bvm_instance_clazz_t *clazz;
	bvm_int32_t methods_count;
	bvm_int32_t i;

	/* no point is dealing with methods if the given clazz ID is not valid */
	if ( (clazz = (bvm_instance_clazz_t *) bvmd_readcheck_reftype(in, out) ) == NULL) return;

	/* additionally, the debugger should not be asking for this information for a clazz that is
	 * not prepared */
	if (clazz->state < BVM_CLAZZ_STATE_LOADED) {
		out->error = JDWP_Error_CLASS_NOT_PREPARED;
		return;
	}

	/* additionally, the debugger should only ask this information from an instance clazz */
	if (!BVM_CLAZZ_IsInstanceClazz(clazz)) {
		out->error = JDWP_Error_INVALID_CLASS;
		return;
	}

	/* write out the number of methods the clazz has */
    methods_count = clazz->methods_count;
	bvmd_out_writeint32(out, methods_count);

	/* for each method, in the same order as defined in the class file */
	for (i = 0; i < methods_count; i++) {

		bvm_method_t *method = &(clazz->methods[i]);

		bvmd_out_writeaddress(out, method);  			    		/* method id */
		bvmd_out_writestring(out, (char *) method->name->data);	/* name of the method */
		bvmd_out_writestring(out, (char *) method->jni_signature->data);	/* JNI signature of the method */
		if (do_generic) {
#if BVM_DEBUGGER_SIGNATURES_ENABLE
			bvmd_out_writeutfstring(out, method->generic_signature);	/* if generic, then output signature */
#else
			bvmd_out_writestring(out, NULL);			/* output NULL (not supported) */
#endif
		}
		bvmd_out_writeint32(out, method->access_flags);				/* modBits */

	}
}

/**
 * Internal command handler for Signature/SignatureWithGeneric - the are both basically the same.
 *
 *  The compile time variable #BVM_DEBUGGER_SIGNATURES_ENABLE must be true to processes signatures.  If not true,
 *  \c NULL is always output for a signature.
 *
 * @param in the request input stream
 * @param out the response output stream
 * @param do_generic if true, the generic signature will be output (or \c NULL if not supported).
 */
static void do_signature(bvmd_instream_t* in, bvmd_outstream_t* out, bvm_bool_t do_generic) {
	bvm_clazz_t *clazz;

	if ( (clazz = bvmd_readcheck_reftype(in, out)) == NULL) return;

	bvmd_out_writeutfstring(out, clazz->jni_signature);

	if (do_generic) {
#if BVM_DEBUGGER_SIGNATURES_ENABLE
		if (BVM_CLAZZ_IsInstanceClazz(clazz)) {
			bvmd_out_writeutfstring(out, ((bvm_instance_clazz_t *) clazz)->generic_signature); /* if generic, then output signature */
		} else {
			bvmd_out_writestring(out, NULL);			/* output NULL (not supported) */
		}
#else
		bvmd_out_writestring(out, NULL);			/* output NULL (not supported) */
#endif
	}
}

/**
 * JDWP Command handler for ReferenceType/Signature.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_RT_Signature(bvmd_instream_t* in, bvmd_outstream_t* out) {
	do_signature(in, out, BVM_FALSE);
}

/**
 * JDWP Command handler for ReferenceType/ClassLoader.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_RT_ClassLoader(bvmd_instream_t* in, bvmd_outstream_t* out) {
	bvm_clazz_t *clazz;

	if ( (clazz = bvmd_readcheck_reftype(in, out)) == NULL) return;

	bvmd_out_writeobject(out, clazz->classloader_obj);
}

/**
 * JDWP Command handler for ReferenceType/Modifiers.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_RT_Modifiers(bvmd_instream_t* in, bvmd_outstream_t* out) {
	bvm_clazz_t *clazz;
	bvm_uint16_t flags;

	if ( (clazz = bvmd_readcheck_reftype(in, out)) == NULL) return;

	/* remove the VM specific flags from the class flags - just to be sure */
	flags = clazz->access_flags & ~BVM_CLASS_ACCESS_FLAG_ARRAY;
	flags &= ~BVM_CLASS_ACCESS_FLAG_INSTANCE;
	flags &= ~BVM_CLASS_ACCESS_FLAG_PRIMITIVE;

	bvmd_out_writeint32(out, flags);
}

/**
 * JDWP Command handler for ReferenceType/Fields.
 *
 * @param in the request input stream
 * @param out the response output stream
 *
 * @see #dbg_fields()
 */
void bvmd_ch_RT_Fields(bvmd_instream_t* in, bvmd_outstream_t* out) {
	dbg_fields(in, out, BVM_FALSE);
}

/**
 * JDWP Command handler for ReferenceType/Methods.
 *
 * @param in the request input stream
 * @param out the response output stream
 *
 * @see dbg_methods()
 */
void bvmd_ch_RT_Methods(bvmd_instream_t* in, bvmd_outstream_t* out) {
	dbg_methods(in, out, BVM_FALSE);
}

/**
 * Internal Command handler logic for getting and and setting values from/to a reference type.
 *
 * @param in the request input stream
 * @param out the response output stream
 * @param do_set if BVM_TRUE, function will set values with the reference type, otherwise will get values
 * and output them to the output stream.
 */
void bvmd_RT_GetSetValues(bvmd_instream_t* in, bvmd_outstream_t* out, bvm_bool_t do_set) {
	bvm_clazz_t *clazz;
	int fields_nr;
	int is_raw64 = BVM_FALSE;

	/* getting or setting field values is useless if the clazz is no longer valid */
	if ( (clazz = bvmd_readcheck_reftype(in, out)) == NULL) return;

	/* the number of fields to process */
	fields_nr = bvmd_in_readint32(in);

	/* if getting, write out the number of fields */
	if (!do_set)
		bvmd_out_writeint32(out, fields_nr);

	while (fields_nr--) {
		bvm_cell_t *value_ptr;
		bvm_uint8_t tagtype;
		bvm_field_t *field = bvmd_in_readaddress(in);

		/* get pointer to the cell that holds the static field's value.  For static long fields, the field
		 * actually holds a pointer to a 'static long' cell contained within a list of 'static longs' held on the
		 * clazz struct. All other static types have their value held in the field's static value.*/
		if (BVM_FIELD_IsLong(field)) {
		    // long fields are held as pointers into a 64bit array - not as field cell. We have to handle differently
		    // to a cell. A long or double occupies two cells and in 32 bit that is 64 bits, so all good - they kind of
		    // can 'overlay'.  In 64 bits a long/double is over two 64 bit cells .. so 128 bits - so not possible.
			value_ptr = field->value.static_value.ptr_value;
            is_raw64 = BVM_TRUE;
		} else {
            value_ptr = &(field->value.static_value);
		}

		/* if native, treat field type as an INT */
		if (BVM_FIELD_IsNative(field)) {
			tagtype = JDWP_Tag_INT;
		} else {
			/* otherwise test for primitive and array ... */
			tagtype = bvmd_get_jdwptag_from_char(field->jni_signature->data[0]);
		}

		/* if not primitive or array (or native), get field tag type from the cell ref value */
		if (!tagtype) tagtype = bvmd_get_jdwptag_from_object(value_ptr->ref_value);

		if (do_set)
		    if (is_raw64) {
                bvmd_in_raw64(in, value_ptr);
		    } else {
                bvmd_in_readcell(in, tagtype, value_ptr);
            }
		else
		    if (is_raw64) {
                bvmd_out_writeRaw64(out, tagtype, value_ptr);
		    } else {
                bvmd_out_writecell(out, tagtype, value_ptr);
		    }
	}
}


/**
 * JDWP Command handler for ReferenceType/GetValues.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_RT_GetValues(bvmd_instream_t* in, bvmd_outstream_t* out) {
	/* get and set are largely the same */
	bvmd_RT_GetSetValues(in, out, BVM_FALSE);
}

/**
 * JDWP Command handler for ReferenceType/SourceFile.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_RT_SourceFile(bvmd_instream_t* in, bvmd_outstream_t* out) {
	bvm_instance_clazz_t *clazz;

	if ( (clazz = (bvm_instance_clazz_t *) bvmd_readcheck_reftype(in, out)) == NULL) return;

	if (clazz->source_file_name == NULL) {
		out->error = JDWP_Error_ABSENT_INFORMATION;
	} else {
		bvmd_out_writeutfstring(out, clazz->source_file_name );
	}
}

/**
 * JDWP Command handler for ReferenceType/Status.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_RT_Status(bvmd_instream_t* in, bvmd_outstream_t* out) {
	bvm_clazz_t *clazz;

	if ( (clazz = bvmd_readcheck_reftype(in, out)) == NULL) return;

	bvmd_out_writeint32(out, bvmd_clazz_status(clazz));
}

/**
 * JDWP Command handler for ReferenceType/Interfaces.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_RT_Interfaces(bvmd_instream_t* in, bvmd_outstream_t* out) {

	bvm_instance_clazz_t *clazz;
	bvm_int32_t i;

	if ( (clazz = (bvm_instance_clazz_t *) bvmd_readcheck_reftype(in, out) ) == NULL) return;

	i = clazz->interfaces_count;
	bvmd_out_writeint32(out, i);

	while (i--) {
		bvmd_out_writeobject(out, clazz->interfaces[i]);
	}
}

/**
 * JDWP Command handler for ReferenceType/ClassObject.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_RT_ClassObject(bvmd_instream_t* in, bvmd_outstream_t* out) {
	bvm_clazz_t *clazz;

	if ( (clazz = bvmd_readcheck_reftype(in, out)) == NULL) return;

	bvmd_out_writeobject(out, clazz->class_obj);
}

/**
 * JDWP Command handler for ReferenceType/SourceDebugExtension.  Requires compile-time variable
 * #BVM_DEBUGGER_JSR045_ENABLE to be true.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_RT_SourceDebugExtension(bvmd_instream_t* in, bvmd_outstream_t* out) {
	bvm_instance_clazz_t *clazz;

	if ( (clazz = (bvm_instance_clazz_t *) bvmd_readcheck_reftype(in, out)) == NULL) return;

	/* the debugger should only ask this information from an instance clazz */
	if (!BVM_CLAZZ_IsInstanceClazz(clazz)) {
		out->error = JDWP_Error_INVALID_CLASS;
		return;
	}

	bvmd_out_writeutfstring(out, clazz->source_debug_extension);
}


/**
 * JDWP Command handler for ReferenceType/SignatureWithGeneric.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_RT_SignatureWithGeneric(bvmd_instream_t* in, bvmd_outstream_t* out) {
	do_signature(in, out, BVM_TRUE);
}

/**
 * JDWP Command handler for ReferenceType/FieldsWithGeneric.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_RT_FieldsWithGeneric(bvmd_instream_t* in, bvmd_outstream_t* out) {
	dbg_fields(in, out, BVM_TRUE);
}

/**
 * JDWP Command handler for ReferenceType/MethodsWithGeneric.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_RT_MethodsWithGeneric(bvmd_instream_t* in, bvmd_outstream_t* out) {
	dbg_methods(in, out, BVM_TRUE);
}

/**
 * JDWP Command handler for ReferenceType/ClassFileVersion.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_RT_ClassFileVersion(bvmd_instream_t* in, bvmd_outstream_t* out) {
	bvm_clazz_t *clazz;

	if ( (clazz = bvmd_readcheck_reftype(in, out)) == NULL) return;

	if (BVM_CLAZZ_IsInstanceClazz(clazz)) {
		bvmd_out_writeint32(out, ((bvm_instance_clazz_t* )clazz)->major_version);
		bvmd_out_writeint32(out, ((bvm_instance_clazz_t* )clazz)->minor_version);
	} else {
		out->error = JDWP_Error_ABSENT_INFORMATION;
	}
}


#endif
