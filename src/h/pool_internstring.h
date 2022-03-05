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

#ifndef BVM_INTERNSTRING_POOL_H_
#define BVM_INTERNSTRING_POOL_H_

/**
  @file

  Constants/Macros/Functions/Types for the intern String pool.

  @author Greg McCreath
  @since 0.0.10

*/

/* a pool of utfstrings.  An array of bvm_utfstring_t pointers */
extern bvm_internstring_obj_t **bvm_gl_internstring_pool;
extern int bvm_gl_internstring_pool_bucketcount;

bvm_internstring_obj_t *bvm_internstring_pool_get(bvm_utfstring_t *str, bvm_bool_t add_if_missing);
bvm_internstring_obj_t *bvm_internstring_pool_add(bvm_utfstring_t *str);

#endif /*BVM_INTERNSTRING_POOL_H_*/
