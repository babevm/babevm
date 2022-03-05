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

#ifndef BVM_INT64_EMULATED_H_
#define BVM_INT64_EMULATED_H_

/**
  @file

  For inclusion in a platform's \c bvm_md.h when emulated Java longs are utilised.  Only for when #BVM_32BIT_ENABLE is on, and
  #BVM_NATIVE_INT64_ENABLE and #BVM_FLOAT_ENABLE are off.

  @author Greg McCreath
  @since 0.0.10

 */

/**
 * Default the platform to little-endian. It is also defaulted in define.h, but, when long emulation is
 * enabled this will be loaded before that file.
 */
#ifndef BVM_BIG_ENDIAN_ENABLE
#define BVM_BIG_ENDIAN_ENABLE 0
#endif

#if (!BVM_NATIVE_INT64_ENABLE)
#if (!BVM_BIG_ENDIAN_ENABLE)
typedef struct { bvm_uint32_t low; bvm_uint32_t high; } bvm_int64_t, bvm_uint64_t;
#else
typedef struct { bvm_uint32_t high; bvm_uint32_t low; } bvm_int64_t, bvm_uint64_t;
#endif
#endif /* BVM_BIG_ENDIAN_ENABLE */
#endif /* BVM_NATIVE_INT64_ENABLE */
