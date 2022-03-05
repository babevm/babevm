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

#ifndef BVM_PD_MEMORY_H_
#define BVM_PD_MEMORY_H_

/**
  @file

  Platform-dependent memory handing functions and definitions.

  @author Greg McCreath
  @since 0.0.10

*/

/**
 *  Allocate memory from the platform.
 *
 *  @param size the size, in bytes, of the memory to allocate.
 *  @return a void* to the allocated memory or \c NULL if the platform was unable to
 *  allocate the requested memory successfully..
 */
void *bvm_pd_memory_alloc(size_t size);

/**
 * Return memory allocated with #bvm_pd_memory_alloc() back to the underlying platform.
 *
 * @param mem a handle to memory provided by #bvm_pd_memory_alloc().
 */
void bvm_pd_memory_free(void *mem);

#endif /*BVM_PD_MEMORY_H_*/
