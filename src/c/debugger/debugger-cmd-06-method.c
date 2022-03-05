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

  JDWP Method commandset command handlers.

  @author Greg McCreath
  @since 0.0.10

*/

#if BVM_DEBUGGER_ENABLE

/**
 * JDWP Command handler for Method/LineTable.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_M_LineTable(bvmd_instream_t* in, bvmd_outstream_t* out) {

	bvm_instance_clazz_t *clazz;
	bvm_method_t *method;

	if ( (clazz = (bvm_instance_clazz_t *) bvmd_readcheck_reftype(in, out)) == NULL) return;
	if ( (method = bvmd_readcheck_method(in, out)) == NULL) return;

	if (BVM_METHOD_IsNative(method)) {
		bvmd_out_writeint64(out, 0, (bvm_uint32_t) -1);
		bvmd_out_writeint64(out, 0, (bvm_uint32_t) -1);
		bvmd_out_writeint32(out, 0);
	} else {
		/* if there is bytecode, but there are no lines ... absent info ...*/
		if ( (method->line_number_count == 0) && (method->code_length != 0))  {
			out->error = JDWP_Error_ABSENT_INFORMATION;
		} else {

			/* ... else output each line */
			int i, nr = method->line_number_count;
			bvmd_out_writeint64(out, 0, 0);
			bvmd_out_writeint64(out, 0, method->code_length);
			bvmd_out_writeint32(out, nr);

			for (i=0; i<nr; i++) {
				bvm_linenumber_t *line = &(method->line_numbers[i]);
				bvmd_out_writeint64(out, 0, line->start_pc);
				bvmd_out_writeint32(out, line->line_nr);
			}
		}
	}
}

/**
 * Internal command handler logic for Method/VariableTable and Method/VariableTableWithGeneric - both are the
 * same but 'generic' outputs an extra attribute.
 *
 * Note that Babe does not support the generic attribute, so \c NULL is output in its stead.
 *
 * @param in the request input stream
 * @param out the response output stream
 * @param do_generic if BVM_TRUE the generic attribute will be output (always \c NULL).
 */
static void do_variable_table(bvmd_instream_t* in, bvmd_outstream_t* out, bvm_bool_t do_generic) {

	bvm_clazz_t *clazz;
	bvm_method_t *method;
	bvmd_streampos_t arg_count_pos;
	bvm_int32_t i;
    bvm_int32_t arg_count = 0;

	/* no use in processing the variable if the clazz is not valid */
	if ( (clazz = bvmd_readcheck_reftype(in, out)) == NULL) return;

	/* and likewise with the method */
	if ( (method = bvmd_readcheck_method(in, out)) == NULL) return;

	if (method->local_variables == NULL) {
		/* missing info on locals ? */
		out->error = JDWP_Error_ABSENT_INFORMATION;
	} else {

		/* write out a zero placeholder for argCnt */
		arg_count_pos = bvmd_out_writeint32(out, 0);

		/* the number of local variables */
		bvmd_out_writeint32(out, method->local_variable_count);

		/* for each local variable */
		for (i = 0; i < method->local_variable_count ; i++) {

			bvm_local_variable_t *local = &(method->local_variables[i]);

			bvmd_out_writeint64(out, 0, local->start_pc);
			bvmd_out_writeutfstring(out, local->name);
			bvmd_out_writeutfstring(out, local->desc);
			if (do_generic) bvmd_out_writeutfstring(out, NULL);
			bvmd_out_writeint32(out, local->length);
			bvmd_out_writeint32(out, local->index);

			/* up the arg counter by 2 for a long, or just 1 otherwise */
			arg_count += (local->desc->data[0] == 'J') ? 2 : 1;
		}

		/* rewrite the count if there is one */
        bvmd_out_rewriteint32(&arg_count_pos, arg_count);
	}
}

/**
 * JDWP Command handler for Method/VariableTable.
 *
 * @param in the request input stream
 * @param out the response output stream
 *
 * @see #do_variable_table()
 */
void bvmd_ch_M_VariableTable(bvmd_instream_t* in, bvmd_outstream_t* out) {
	do_variable_table(in, out, BVM_FALSE);
}

/**
 * JDWP Command handler for Method/Bytecodes.
 *
 * @param in the request input stream
 * @param out the response output stream
 */
void bvmd_ch_M_Bytecodes(bvmd_instream_t* in, bvmd_outstream_t* out) {

	bvm_instance_clazz_t *clazz;
	bvm_method_t *method;

	if ( (clazz = (bvm_instance_clazz_t *) bvmd_readcheck_reftype(in, out)) == NULL) return;
	if ( (method = bvmd_readcheck_method(in, out)) == NULL) return;

	bvmd_out_writeint32(out, method->code_length);
	bvmd_out_writebytes(out, method->code.bytecode, method->code_length);
}


/**
 * JDWP Command handler for Method/VariableTableWithGeneric.
 *
 * @param in the request input stream
 * @param out the response output stream
 *
 * @see #do_variable_table()
 */
void bvmd_ch_M_VariableTableWithGeneric(bvmd_instream_t* in, bvmd_outstream_t* out) {
	do_variable_table(in, out, BVM_TRUE);
}


#endif
