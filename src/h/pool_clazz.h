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

#ifndef BVM_CLAZZ_POOL_H_
#define BVM_CLAZZ_POOL_H_

/**
  @file

  Constants/Macros/Functions/Types for the clazz pool.

  @author Greg McCreath
  @since 0.0.10

*/

extern bvm_clazz_t **bvm_gl_clazz_pool;
extern int bvm_gl_clazz_pool_bucketcount;

bvm_clazz_t *bvm_clazz_pool_get(bvm_classloader_obj_t *loader, bvm_utfstring_t *clazzname);
bvm_clazz_t *bvm_get_clazz_pool_c(bvm_classloader_obj_t *loader, char *clazzname);

void bvm_clazz_pool_add(bvm_clazz_t *clazz);
void bvm_clazz_pool_remove(bvm_clazz_t *clazz);

#endif /*BVM_CLAZZ_POOL_H_*/
