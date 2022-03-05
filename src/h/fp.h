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

#ifndef BVM_FP_H_
#define BVM_FP_H_

#if BVM_FLOAT_ENABLE

/**
  @file

  Constants/Macros/Functions/Types for floating point support.

  @author Greg McCreath
  @since 0.0.10

*/

/* Some notes:
 *
 *
 * According to IEEE 754 FP stuff:
 *
 * Positive zero is all zero bits.
 * Negative zero is one bit (sign bit) followed by 31 zero bits.
 * Infinity is zero bit, followed by 8 one bits, followed by 23 zero bits
 * -Infinity is one bit, followed by 8 one bits, followed by 23 zero bits
 */


/* For NaN (Not-A-Number) and Infinity for floats */
#define BVM_FLOAT_INFINITY_POS      ((bvm_int32_t)0x7F800000)
#define BVM_FLOAT_INFINITY_NEG      ((bvm_int32_t)0xFF800000)
#define BVM_FLOAT_NAN_POS_LOW       ((bvm_int32_t)0x7F800001)
#define BVM_FLOAT_NAN_POS_HI        ((bvm_int32_t)0x7FFFFFFF)
#define BVM_FLOAT_NAN_NEG_LOW       ((bvm_int32_t)0xFF800001)
#define BVM_FLOAT_NAN_NEG_HI        ((bvm_int32_t)0xFFFFFFFF)
#define BVM_FLOAT_NAN               ((bvm_int32_t)0x7FC00000)

bvm_bool_t bvm_fp_is_fnan(bvm_int32_t v);
bvm_bool_t bvm_fp_is_fpinf(bvm_int32_t v);
bvm_bool_t bvm_fp_is_fninf(bvm_int32_t v);

/* For NaN (Not-A-Number) and Infinity for doubles */
#define BVM_DOUBLE_INFINITY_POS      ((bvm_int64_t)0x7FF0000000000000LL)
#define BVM_DOUBLE_INFINITY_NEG      ((bvm_int64_t)0xFFF0000000000000LL)
#define BVM_DOUBLE_NAN_POS_LOW       ((bvm_int64_t)0x7FF0000000000001LL)
#define BVM_DOUBLE_NAN_POS_HI        ((bvm_int64_t)0x7FFFFFFFFFFFFFFFLL)
#define BVM_DOUBLE_NAN_NEG_LOW       ((bvm_int64_t)0xFFF0000000000001LL)
#define BVM_DOUBLE_NAN_NEG_HI        ((bvm_int64_t)0xFFFFFFFFFFFFFFFFLL)
#define BVM_DOUBLE_NAN        		 ((bvm_int64_t)0x7FF8000000000000LL)

#define BVM_DOUBLE_get(p) (*(bvm_double_t*)(p))
#define BVM_DOUBLE_set(p, a) (*(bvm_double_t*)(p) = (a))
#define BVM_DOUBLE_copy(pnew, pold) (*(bvm_double_t*)(pnew) = *(bvm_double_t*)(pold))

bvm_double_t BVM_DOUBLE_from_cells(bvm_cell_t *cell);
void BVM_DOUBLE_to_cells(bvm_cell_t *cell, bvm_double_t value);

bvm_bool_t bvm_fp_is_dnan(bvm_int64_t v);
bvm_bool_t bvm_fp_is_dpinf(bvm_int64_t v);
bvm_bool_t bvm_fp_is_dninf(bvm_int64_t v);

#endif


#endif /*BVM_FP_H_*/
