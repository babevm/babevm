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

#ifndef BVM_INT64_H_
#define BVM_INT64_H_

/**
  @file

  Java long primitive support.

  This VM includes support for platforms that support a 64bit integer type, and those that do not.  For platforms that do not
  provide a 64bit integer the VM emulates it.

  In order to provide a common means of doing so for all platforms, 64 bit integer usage is all done through macros and functions.  Nowhere
  in the VM code will you see references to an \c '__int64' or \c 'long long' type - except for here when defining the #bvm_int64_t type for use
  with this VM.

  Essentially, all long stuff is achieved with \c 'INT64_' macros.

  @section long-native Native 64bit

  This is pretty straight forward - the INT64_ macros map to 'C' language structure for integer.  No big deal. If the target
  platform supports an 64bit integer, and performance is your concern, it is recommended that you do comparison testing between
  emulated and native 64 bit integers.  It is not always a given that native will be quicker - to my surprise on some informal
  testing (50,000,000 long multi/div/rem operations) on Win32 I found emulation to be a full 30% faster than native - go figure (my
  guess is that because the emulation uses two bvm_uint32_t the OS/CPU may just end up doing less work than with a 64.  Weird).

  @section long-emulated Emulated 64bit

  This is more complex.  64 integer emulation is achieved by using two unsigned 32 bit integers.  Underlying this is a very small XP
  (Extended Precision) library to operate on 64 bit integers as collection of 8 bit chars.  The XP library, if required, could be used
  to do any extended precision mathematics for the platform like 2048 bit integer maths (and so on).  My feeling is that emulated 64 bit
  integers could possibly be achieved with less code than is in the XP libs as it is made for arbitrary length integers.  Something specific
  for 64 bit emulation could be a future enhancement.

  In reality, the XP code is only used for multiplication, division, and bit shifting. All other operations including addition, subtraction, and
  comparisons are done using non-XP macros.

  Refer note above re native vs emulated performance.

  @author Greg McCreath
  @since 0.0.10

 */

bvm_int64_t  BVM_INT64_mul(bvm_int64_t a, bvm_int64_t b);
bvm_int64_t  BVM_INT64_div(bvm_int64_t a, bvm_int64_t b);
bvm_int64_t  BVM_INT64_rem(bvm_int64_t a, bvm_int64_t b);
bvm_int64_t  BVM_INT64_shl(bvm_int64_t a, int count);
bvm_int64_t  BVM_INT64_shr(bvm_int64_t a, int count);
bvm_int64_t  BVM_INT64_ushr(bvm_int64_t a, int count);

// to hold the upper and lower parts of a 64 bit in a platform independent way
typedef struct { bvm_uint32_t high; bvm_uint32_t low; } bvm_uint64_hilo_t;

// unpack a unit 64 bit into hi and low uint 32s
bvm_uint64_hilo_t uint64Unpack(bvm_uint64_t value);

// pack hi/low uint 32s into a unit64.
bvm_uint64_t uint64Pack(bvm_uint32_t hi, bvm_uint32_t low);

#if (!BVM_NATIVE_INT64_ENABLE)

#define BVM_MAX_LONG        ((bvm_int64_t){0x7FFFFFFF,0xFFFFFFFF})
#define BVM_MIN_LONG        ((bvm_int64_t){0x80000000,0})

#define BVM_INT64_ZERO      ((bvm_int64_t){0,0})
#define BVM_INT64_MINUS_ONE ((bvm_int64_t){0xFFFFFFFF,0xFFFFFFFF})

/**
 * Set a bvm_int64_t to zero.
 */
#define BVM_INT64_setZero(a)        ((a).high = 0, (a).low = 0)

/**
 * Increment \c a by the amount \c b.
 */
#define BVM_INT64_increase(a,b)          ((a).low += (b).low, (a).high += (b).high + ((a).low < (b).low))

/**
 * Decrement \c a by the amount \c b.
 */
#define BVM_INT64_decrease(a,b)          ((a).high -= ((b).high + ((a).low < (b).low)), (a).low  -= (b).low)

/**
 * Swap the sign of \c a.
 */
#define BVM_INT64_negate(a)         ((a).low = -(a).low, (a).high = -((a).high + ((a).low != 0)))

/**
 * Put the value of the \c bvm_uint32_t \c b into \c a.
 */
#define BVM_INT64_uint32_to_int64(a,b) ((a).high = 0, (a).low = b)

/**
 * Put the value of the \c bvm_int32_t \c b into \c a.
 */
#define BVM_INT64_int32_to_int64(a,b)  ((a).low = b, (a).high = ((bvm_int32_t)b) >> 31)

/**
 * Put the value of the low bits of \c a into \c b.
 */
#define BVM_INT64_int64_to_uint32(a,b) (b = (bvm_uint32_t)((a).low))

/**
 * Returns true if a < b.  Devs: note the casts to uint32.  The high/low int64 struct stores values as
 * unsigned 32 bits.  When comparing values, to take into account negative values, we cast to a signed
 * int 32.
 */
#define BVM_INT64_compare_lt(a, b)  (( ((bvm_int32_t)(a).high) < ((bvm_int32_t)(b).high)) || \
                                      (((a).high == (b).high) && ((a).low < (b).low)))

/**
 * Returns true is a == b.
 */
#define BVM_INT64_compare_eq(a, b)  (((a).high == (b).high) && ((a).low == (b).low))

/**
 * Returns true if a > b.
 */
#define BVM_INT64_compare_gt(a, b)  BVM_INT64_compare_lt(b, a)

/**
 * Returns true if a < 0.
 */
#define BVM_INT64_zero_lt(a)        ( (bvm_int32_t) (a).high < 0)

/**
 * Returns true if a == 0.
 */
#define BVM_INT64_zero_eq(a)        (((a).high | (a).low) == 0)

/**
 * Returns true if a > 0.
 */
#define BVM_INT64_zero_gt(a)        (!(BVM_INT64_zero_lt(a) || BVM_INT64_zero_eq(a)))

/**
 * Returns true if a <= 0.
 */
#define BVM_INT64_zero_le(a)        (!BVM_INT64_zero_gt(a))

/**
 * Returns true if a != 0.
 */
#define BVM_INT64_zero_ne(a)        (!BVM_INT64_zero_eq(a))

/**
 * Returns true if a >= 0.
 */
#define BVM_INT64_zero_ge(a)        (!BVM_INT64_zero_lt(a))

/**
 * Returns true if a <= b.
 */
#define BVM_INT64_compare_le(a,b)   (!BVM_INT64_compare_gt(a,b))

/**
 * Returns true if a != b
 */
#define BVM_INT64_compare_ne(a,b)   (!BVM_INT64_compare_eq(a,b))

/**
 * Returns true if a >= b
 */
#define BVM_INT64_compare_ge(a,b)   (!BVM_INT64_compare_lt(a,b))

/**
 * Sets a \c bvm_int64_t at a given address to contain the given high and low \c bvm_uint32_t value.
 */
#if (!BVM_BIG_ENDIAN_ENABLE)
#define BVM_INT64_setHilo(p, hi, low) \
        (((bvm_uint32_t *)p)[0] = (low), ((bvm_uint32_t *)p)[1] = (hi))
#else
#define BVM_INT64_setHilo(p, hi, low) \
        (((bvm_uint32_t *)p)[0] = (hi), ((bvm_uint32_t *)p)[1] = (low))
#endif


#else /* !BVM_NATIVE_INT64_ENABLE */

#define BVM_MAX_LONG     ((bvm_int64_t)0x7FFFFFFFFFFFFFFFLL)
#define BVM_MIN_LONG     ((bvm_int64_t)0x8000000000000000LL)

#define BVM_INT64_ZERO 0
#define BVM_INT64_MINUS_ONE -1

#define BVM_INT64_mul(a,b) ( (bvm_int64_t) (a) * (bvm_int64_t) (b) )
#define BVM_INT64_div(a,b) ( (bvm_int64_t) (a) / (bvm_int64_t) (b) )
#define BVM_INT64_rem(a,b) ( (bvm_int64_t) (a) % (bvm_int64_t) (b) )
#define BVM_INT64_shl(a, i) ( (bvm_int64_t) (a) << (i) )
#define BVM_INT64_shr(a, i) ( (bvm_int64_t) (a) >> (i) )
#define BVM_INT64_ushr(a, i) ( (bvm_uint64_t) (a) >> (i) )

/**
 * Set a bvm_int64_t to zero.
 */
#define BVM_INT64_setZero(a)        ( (a) = 0)

/**
 * Increment \c a by the amount \c b..
 */
#define BVM_INT64_increase(a,b)          ( (a) += (b) )

/**
 * Decrement \c a by the amount \c b.
 */
#define BVM_INT64_decrease(a,b)          ( (a) -= (b) )

/**
 * Swap the sign of \c a.
 */
#define BVM_INT64_negate(a)         ((a) = -(a))

/**
 * Put the value of the \c bvm_uint32_t \c b into \c a.
 */
#define BVM_INT64_uint32_to_int64(a,b) ( (a) = (b) )

/**
 * Put the value of the \c bvm_int32_t \c b into \c a.
 */
#define BVM_INT64_int32_to_int64(a,b)  ( (a) = (b) )

/**
 * Put the value of the low bits of \c a into \c b.
 */
#define BVM_INT64_int64_to_uint32(a,b) ( (b) = (bvm_uint32_t)(a) )

/**
 * Returns true if a > b.
 */
#define BVM_INT64_compare_gt(a, b)  ( (a) > (b) )
/**
 * Returns true if a < b.
 */
#define BVM_INT64_compare_lt(a, b)  ( (a) < (b) )

/**
 * Returns true is a == b.
 */
#define BVM_INT64_compare_eq(a, b)  ( (a) == (b) )

/**
 * Returns true if a < 0.
 */
#define BVM_INT64_zero_lt(a)        ( (a) < 0)

/**
 * Returns true if a == 0.
 */
#define BVM_INT64_zero_eq(a)        ( (a) == 0)

/**
 * Returns true if a > 0.
 */
#define BVM_INT64_zero_gt(a)        ( (a) > 0)

/**
 * Returns true if a <= 0.
 */
#define BVM_INT64_zero_le(a)        ( (a) <= 0)

/**
 * Returns true if a != 0.
 */
#define BVM_INT64_zero_ne(a)        ( (a) != 0)

/**
 * Returns true if a >= 0.
 */
#define BVM_INT64_zero_ge(a)        ( (a) >= 0)

/**
 * Returns true if a <= b.
 */
#define BVM_INT64_compare_le(a,b)   ( (a) <= (b) )

/**
 * Returns true if a != b
 */
#define BVM_INT64_compare_ne(a,b)   ( (a) != (b) )

/**
 * Returns true if a >= b
 */
#define BVM_INT64_compare_ge(a,b)   ( (a) >= (b) )

/**
 * Sets a \c bvm_uint64_t at a given address to contain the given high and low \c bvm_uint32_t value.
 */
#define BVM_INT64_setHilo(p, hi, low) \
    (*(bvm_uint64_t *)(p) = ((((bvm_uint64_t) (hi) )<<32) | ((bvm_uint32_t) (low) )))
#endif

/**
 * Casts a given address to a \c bvm_int64_t.
 */
//#define BVM_INT64_get(p) (*(bvm_int64_t*)(p))

/* creates a bvm_int64_t from two adjacent cells */
bvm_int64_t BVM_INT64_from_cells(bvm_cell_t *cell);
void BVM_INT64_to_cells(bvm_cell_t *cell, bvm_int64_t value);

//gmc_int64_t get64(bvm_int64_t l);

/**
 * Places a \c bvm_int64_t at the given address.
 */
//#define BVM_INT64_set(p, a) (*(bvm_int64_t*)(p) = (a))

/**
 * Copies a \c bvm_int64_t value from one address to another.
 */
//#define BVM_INT64_copy(pnew, pold) (*(bvm_int64_t*)(pnew) = *(bvm_int64_t*)(pold))


#endif /* BVM_INT64_H_ */
