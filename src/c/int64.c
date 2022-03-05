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
#include "../thirdparty/xp/xp.h"

/**

 @file

 Functions for #bvm_int64_t support.

  @author Greg McCreath
  @since 0.0.10


 Software implementation of Java 'long' arithmetic.  Uses the "Arbitrary Precision" (XP) code from
 "C Interfaces and Implementations: Techniques for Creating Reusable Software".  I'm sure all this can
 be done a lot more efficiently and with much less hassle and code.

 But for now this is it.

 The XP code is all little-endian and only works on positive numbers.  Hence there is a bit
 of gymnastics in here for negatives and positives and endianness.

 */

#if (!BVM_NATIVE_INT64_ENABLE)


/* number of bytes in a java 'long' */
#define BVM_INT64_BYTES 8

#if (BVM_BIG_ENDIAN_ENABLE)
/**
 * Toggles the 'endianness' of a bvm_int64_t.
 */
static void toggle_endian64(char *p_out, char *p_in) {
  p_out[0] = p_in[7];
  p_out[1] = p_in[6];
  p_out[2] = p_in[5];
  p_out[3] = p_in[4];

  p_out[4] = p_in[3];
  p_out[5] = p_in[2];
  p_out[5] = p_in[1];
  p_out[7] = p_in[0];
}
#endif

/**
 * Multiply two bvm_int64_t integers together.
 *
 * @param x the first bvm_int64_t to multiply
 * @param y the second bvm_int64_t to multiply
 *
 * @return the multiplication of x and y.
 */
static bvm_int64_t int64_mul(bvm_int64_t x, bvm_int64_t y) {

	/* a temporary scratch area for the multiplication.  The XP lib requires a space at least twice
	 * as large as the largest of x & y.  Considering that both x & y are 64 bits, we allocation two
	 * longs - 128 bits */
   bvm_int64_t temp[] = { {0,0} , {0,0} };
   bvm_int64_t result = {0,0};
   bvm_bool_t x_neg, y_neg;
   bvm_int64_t local_x;
   bvm_int64_t local_y;

   /* copy the arguments */
#if (!BVM_BIG_ENDIAN_ENABLE)
   local_x = x;
   local_y = y;
#else
   /* on big endian systems, convert longs to little endian for XP lib */
   toggle_endian64((char *) &local_x, (char *) &x);
   toggle_endian64((char *) &local_y, (char *) &y);
#endif

   /* the XP lib only works on positive numbers.  Remember the sign of x & y and make them positive
    * if need be */
   x_neg = BVM_INT64_zero_lt(x);
   y_neg = BVM_INT64_zero_lt(y);

   /* turn arguments into positives - XP only works with positive values */
   if (x_neg) BVM_INT64_negate(local_x);
   if (y_neg) BVM_INT64_negate(local_y);

   /* multiply x & y using 'temp' as the scratch area. */
   bvm_XP_mul( (unsigned char *) &temp, BVM_INT64_BYTES , (unsigned char *) &local_x, BVM_INT64_BYTES , (unsigned char *) &local_y) ;

   /* the result will come back from the XP lib as the first 64 bits of the temp area */
   result = temp[0];

   /* if the signs of x & y are not equal, negate the result */
   if (x_neg^y_neg) BVM_INT64_negate(result);

#if (!BVM_BIG_ENDIAN_ENABLE)
   return result;
#else
   {
	   bvm_int64_t be_result;
	   /* on big endian systems, convert the result back to big endian */
	   toggle_endian64((char *) &be_result, (char *) &result);

	   return be_result;
   }
#endif
}

/**
 *
 * Divide two bvm_int64_t integers and return either the result or the remainder.
 *
 * @param x the divisor
 * @param y the dividend
 * @param ret_rem specifies whether to return the result or the remainder
 *
 * @return if \c ret_rem is #BVM_FALSE return the result of the divide operation.  If \c ret_rem
 * is \c #BVM_TRUE return the remainder of the divide operation - equivalent to a 'mod' operation.
 *
 */
static bvm_int64_t int64_div(bvm_int64_t x, bvm_int64_t y, bvm_bool_t remainder_only) {

   char tmp[18];
   bvm_int64_t result = {0,0};
   bvm_int64_t remainder = {0,0};
   bvm_bool_t x_neg, y_neg;
   bvm_int64_t divisor;
   bvm_int64_t dividend;

   /* copy the arguments */
#if (!BVM_BIG_ENDIAN_ENABLE)
   divisor = x;
   dividend = y;
#else
   /* on big endian systems, convert longs to little endian for XP lib */
   toggle_endian64((char *) &divisor, (char *) &x);
   toggle_endian64((char *) &dividend, (char *) &y);
#endif

   /* the XP lib only works on positive numbers.  Remember the sign of x & y and make them positive
    * if need be */
   x_neg = BVM_INT64_zero_lt(x);
   y_neg = BVM_INT64_zero_lt(y);

   /* turn arguments into positives - XP only works with positive values */
   if (x_neg) BVM_INT64_negate(divisor);
   if (y_neg) BVM_INT64_negate(dividend);

   /* perform the divide operation */
   bvm_XP_div(8, (unsigned char *) &result, (unsigned char *) &divisor, BVM_INT64_BYTES, (unsigned char *) &dividend, (unsigned char *) &remainder, (unsigned char *) tmp ) ;

   /* return the remainder or the divide result depending on ret_rem - ensuring the
    * sign is correct.  */
   if (remainder_only) {
	   if (x_neg) BVM_INT64_negate(remainder);
   } else {
	   if (x_neg^y_neg) BVM_INT64_negate(result);
   }

#if (!BVM_BIG_ENDIAN_ENABLE)
   return (remainder_only) ? remainder : result;
#else
   {
	   bvm_int64_t be_result;
	   bvm_int64_t le_result = (remainder_only) ? remainder : result;

	   /* on big endian systems, convert the result back to big endian */
	   toggle_endian64((char *) &be_result, (char *) &le_result);

	   return be_result;
   }
#endif
}

/**
 * Long bitwise left shift.
 *
 * @param x the bvm_int64_t integer to perform the operation on
 * @param count the number of positions to shift
 * @param the value to fill at the LSB.
 *
 * @return the left shifted bvm_int64_t.
 */
static bvm_int64_t int64_shl(bvm_int64_t x, int count, int fill) {

   bvm_int64_t result = {0,0};
   bvm_int64_t local_x;

#if (!BVM_BIG_ENDIAN_ENABLE)
   local_x = x;
#else
   /* on big endian systems, convert longs to little endian for XP lib */
   toggle_endian64((char *) &local_x, (char *) &x);
#endif

   bvm_XP_lshift(BVM_INT64_BYTES, (unsigned char *) &result, BVM_INT64_BYTES, (unsigned char *) &local_x, count, fill);

#if (!BVM_BIG_ENDIAN_ENABLE)
   return result;
#else
   {
	   bvm_int64_t be_result;
	   /* on big endian systems, convert the result back to big endian */
	   toggle_endian64((char *) &be_result, (char *) &result);

	   return be_result;
   }
#endif
}

/**
 * Long bitwise right shift.  Can be used for both a signed right shift '>>' and
 * an unsigned right shift '>>>' operations depending on how the \c fill param is used.
 *
 * @param x the bvm_int64_t integer to perform the operation on
 * @param count the number of positions to shift
 * @param the value to fill at the MSB.
 *
 * @return the right shifted bvm_int64_t.
 */
static bvm_int64_t int64_shr(bvm_int64_t x, int count, int fill) {

   bvm_int64_t result = {0,0};
   bvm_int64_t local_x;

#if (!BVM_BIG_ENDIAN_ENABLE)
   local_x = x;
#else
   /* on big endian systems, convert longs to little endian for XP lib */
   toggle_endian64((char *) &local_x, (char *) &x);
#endif

   bvm_XP_rshift(BVM_INT64_BYTES, (unsigned char *) &result, BVM_INT64_BYTES, (unsigned char *) &local_x, count, fill);

#if (!BVM_BIG_ENDIAN_ENABLE)
   return result;
#else
   {
	   bvm_int64_t be_result;
	   /* on big endian systems, convert the result back to big endian */
	   toggle_endian64((char *) &be_result, (char *) &result);

	   return be_result;
   }
#endif
}

/**
 * Multiplies two bvm_int64_t values.
 *
 * @param a the first multiplicand
 * @param b the second multiplicand
 *
 * @return the result
 */
bvm_int64_t BVM_INT64_mul(bvm_int64_t a, bvm_int64_t b) {
	return int64_mul(a,b);
}

/**
 * Divides one long by another.
 *
 * @param dividend the (erm) dividend
 * @param divisor the (erm) divisor
 *
 * @return the quotient
 */
bvm_int64_t BVM_INT64_div(bvm_int64_t dividend, bvm_int64_t divisor) {
	return int64_div(dividend,divisor, BVM_FALSE);
}

/**
 * The remainder (mod) of the divide of the dividend and divisor.
 *
 * @param dividend the (erm) dividend
 * @param divisor the (erm) divisor
 *
 * @return the remainder
 */
bvm_int64_t BVM_INT64_rem(bvm_int64_t a, bvm_int64_t b) {
	return int64_div(a,b, BVM_TRUE);
}

/**
 * Performs a bvm_int64_t left shift inserting zeroes into the LSB.
 *
 * @param a the long to act upon
 * @param count the number of bits to shift
 *
 * @return the left shifted long.
 */
bvm_int64_t BVM_INT64_shl(bvm_int64_t a, int count) {
	return int64_shl(a, count, 0);
}

/**
 * Performs a bvm_int64_t right shift preserving the original sign by filling the MSB
 * with the sign of the being-shifted long.
 *
 * @param a the long to act upon
 * @param count the number of bits to shift
 *
 * @return the right shifted long.
 */
bvm_int64_t BVM_INT64_shr(bvm_int64_t a, int count) {
	return int64_shr(a, count, (( bvm_int32_t) a.high < 0));
}

/**
 * Perform a bvm_int64_t right shift filling the MSB with zeroes.
 *
 * @param a the long to act upon
 * @param count the number of bits to shift
 *
 * @return the right shifted long.
 */
bvm_int64_t BVM_INT64_ushr(bvm_int64_t a, int count) {
	return int64_shr(a, count, 0);
}

#endif

/**
 * Unpack a bvm_uint64_t into hi and low bvm_uint32_t.
 *
 * @param value the long to act upon
 *
 * @return the unpacked 64 bit
 */
bvm_uint64_hilo_t uint64Unpack(bvm_uint64_t value) {

    bvm_uint64_hilo_t sp;

#if (!BVM_NATIVE_INT64_ENABLE)
    sp.high = value.high;
    sp.low = value.low;
#else
    sp.high = (bvm_uint32_t) (value >> 32);
    sp.low = (bvm_uint32_t) value;
#endif
    return sp;
}


/**
 * Pack hi/low bvm_uint32_t into a bvm_uint64_t
 *
 * @param hi most significant 32 bits
 * @param low least signifigant 32 bits
 *
 * @return the packed bvm_uint64_t.
 */
bvm_uint64_t uint64Pack(bvm_uint32_t high, bvm_uint32_t low) {
    bvm_uint64_t p;
    BVM_INT64_setHilo(&p, high, low);
    return p;
}

bvm_int64_t BVM_INT64_from_cells(bvm_cell_t *cell) {
    // get the values of the two adjacent cells as uint32 and pack them into a 64.
    bvm_uint32_t high = (bvm_uint32_t) cell->int_value;
    bvm_uint32_t low  = (bvm_uint32_t) (cell+1)->int_value;
    return (bvm_int64_t) uint64Pack(high, low);
}

void BVM_INT64_to_cells(bvm_cell_t *cell, bvm_int64_t value) {
    bvm_uint64_hilo_t p = uint64Unpack( (bvm_uint64_t) value);
    // set the values of the two adjacent cells
    cell->int_value = (bvm_native_long_t) p.high;
    (cell+1)->int_value = (bvm_native_long_t) p.low;
}

//gmc_int64_t get64(bvm_int64_t l) {
//    return (((gmc_uint64_t) (l.high) )<<32) | ((bvm_uint32_t) (l.low) );
//}
