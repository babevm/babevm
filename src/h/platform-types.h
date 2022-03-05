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

#ifndef BVM_PLATFORM_TYPES_H_
#define BVM_PLATFORM_TYPES_H_

/**
  @file

  Definitions of the platform types used in the VM.

  @author Greg McCreath
  @since 0.0.10

*/

#if (!BVM_NATIVE_INT64_ENABLE)

/* Define the emulated types for bvm_int64_t.
 */

#if (!BVM_BIG_ENDIAN_ENABLE)
//typedef struct { bvm_uint32_t low; bvm_uint32_t high; } bvm_int64_t, bvm_uint64_t;
#else
//typedef struct { bvm_uint32_t high; bvm_uint32_t low; } bvm_int64_t, bvm_uint64_t;
#endif
#endif /* bvm_int64_t */

#endif /*BVM_PLATFORM_TYPES_H_*/
