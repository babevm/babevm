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

#if BVM_FLOAT_ENABLE

/**
 * @file
 *
 * Floating point support functions
 *
 * @author Greg McCreath
 * @since 0.0.10
*/

bvm_bool_t bvm_fp_is_fnan(bvm_int32_t v)
{
	return (((v >= BVM_FLOAT_NAN_NEG_LOW) && (v <= BVM_FLOAT_NAN_NEG_HI)) ||
		    ((v >= BVM_FLOAT_NAN_POS_LOW) && (v <= BVM_FLOAT_NAN_POS_HI)));
}

bvm_bool_t bvm_fp_is_fpinf(bvm_int32_t v)  { return ( v == BVM_FLOAT_INFINITY_POS ); }

bvm_bool_t bvm_fp_is_fninf(bvm_int32_t v)  { return ( v == BVM_FLOAT_INFINITY_NEG ); }

bvm_bool_t bvm_fp_is_dnan(bvm_int64_t v)
{
	return (((v >= BVM_DOUBLE_NAN_NEG_LOW) && (v <= BVM_DOUBLE_NAN_NEG_HI)) ||
		    ((v >= BVM_DOUBLE_NAN_POS_LOW) && (v <= BVM_DOUBLE_NAN_POS_HI)));
}

bvm_bool_t bvm_fp_is_dpinf(bvm_int64_t v)
{
	return (v == BVM_DOUBLE_INFINITY_POS);
}

bvm_bool_t bvm_fp_is_dninf(bvm_int64_t v)
{
	return (v == BVM_DOUBLE_INFINITY_NEG);
}


bvm_double_t BVM_DOUBLE_from_cells(bvm_cell_t *cell) {
    // get the values of the two adjacent cells as uint32 and pack them into a 64.
    bvm_uint32_t high = (bvm_uint32_t) cell->int_value;
    bvm_uint32_t low  = (bvm_uint32_t) (cell+1)->int_value;
    bvm_uint64_t b64 = uint64Pack(high, low);

    // the pointer gymnastics here are to cast the _bytes_ at the u64
    // location to a double.  So .. get a double pointer and then dereference it.
    return (*((bvm_double_t *) &b64));
}

void BVM_DOUBLE_to_cells(bvm_cell_t *cell, bvm_double_t value) {
    // the pointer gymnastics here are to cast the _bytes_ at the double
    // location to a u64.  So .. get a u64 pointer and then dereference it.
    bvm_uint64_hilo_t p = uint64Unpack( (*(bvm_uint64_t*) &value));
    // set the values of the two adjacent cells as uint32
    cell->int_value = (bvm_native_ulong_t) p.high;
    (cell+1)->int_value = (bvm_native_ulong_t) p.low;
}

#endif
