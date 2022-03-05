/*
 Copyright (c) 1994,1995,1996,1997 by David R. Hanson.

Permission is hereby granted, free of charge, to any person obtaining a copy of this
software and associated documentation files (the "Software"), to deal in the Software
without restriction, including without limitation the rights to use, copy, modify,
merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be included in all copies
or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

<http://www.opensource.org/licenses/mit-license.php>

 */

/* From "C Interfaces and Implementations: Techniques for Creating Reusable Software" (Addison-Wesley
 * Professional Computing Series, 1997, ISBN 0201498413, ISBN-13 9780201498417).  David Hanson.
 *
 * Recommended.
 *
 */



/**
  @file

  Typedefs and prototypes for extended precision mathematics.

  @since 0.0.10

*/

#ifndef BVM_XP_INCLUDED_
#define BVM_XP_INCLUDED_
#define T XP_T
typedef unsigned char *T;
extern int bvm_XP_add(int n, T z, T x, T y, int carry);
extern int bvm_XP_sub(int n, T z, T x, T y, int borrow);
extern int bvm_XP_mul(T z, int n, T x, int m, T y);
extern int bvm_XP_div(int n, T q, T x, int m, T y, T r,T tmp);
extern int bvm_XP_sum     (int n, T z, T x, int y);
extern int bvm_XP_diff    (int n, T z, T x, int y);
extern int bvm_XP_product (int n, T z, T x, int y);
extern int bvm_XP_quotient(int n, T z, T x, int y);
extern int bvm_XP_neg(int n, T z, T x, int carry);
extern int bvm_XP_cmp(int n, T x, T y);
extern void bvm_XP_lshift(int n, T z, int m, T x, int s, int fill);
extern void bvm_XP_rshift(int n, T z, int m, T x, int s, int fill);
extern int  bvm_XP_length (int n, T x);
extern unsigned long bvm_XP_fromint(int n, T z, unsigned long u);
extern unsigned long XP_toint  (int n, T x);
extern int bvm_XP_fromstr(int n, T z, const char *str, int base, char **end);
extern char *bvm_XP_tostr(char *str, int size, int base, int n, T x);
#undef T
#endif
