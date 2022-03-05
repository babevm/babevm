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

#ifndef BVM_UTFSTRING_POOL_H_
#define BVM_UTFSTRING_POOL_H_

/**
  @file

  Constants/Macros/Functions/Types for the utf string pool.

  @author Greg McCreath
  @since 0.0.10

*/

/* a pool of utfstrings.  An array of bvm_utfstring_t pointers */
extern bvm_utfstring_t **bvm_gl_utfstring_pool;
extern int bvm_gl_utfstring_pool_bucketcount;

bvm_utfstring_t *bvm_utfstring_pool_get(bvm_utfstring_t *str, bvm_bool_t add_if_missing);
bvm_utfstring_t *bvm_utfstring_pool_get_c(const char *str, bvm_bool_t add_if_missing);

void bvm_utfstring_pool_add(bvm_utfstring_t *str);

#endif /*BVM_UTFSTRING_POOL_H_*/
