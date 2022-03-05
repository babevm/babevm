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


/**
  @file

  Linux32 Platform-dependent types and functions.  Requires \c BVM_PLATFORM_LINUX to be defined.

  @author Greg McCreath
  @since 0.0.10

*/
#ifdef BVM_PLATFORM_LINUX

#ifndef BVM_PD_H_
#define BVM_PD_H_

#define BVM_DEBUG_HEAP_GC_ON_ALLOC 0
#define BVM_DEBUG_CLEAR_HEAP_ON_EXIT 0
#define BVM_EXIT_ON_UNCAUGHT_EXCEPTION 1

#define BVM_DEBUGGER_ENABLE 1

// float support requires native 64 - so set them both when using float
#define BVM_FLOAT_ENABLE 1
#define BVM_NATIVE_INT64_ENABLE 1
//#define BVM_32BIT_ENABLE 1

typedef unsigned char bvm_uint8_t;
typedef signed char bvm_int8_t;
typedef unsigned short bvm_uint16_t;
typedef signed short bvm_int16_t;
typedef unsigned int bvm_uint32_t;
typedef signed int bvm_int32_t;
typedef float bvm_float_t;
typedef double bvm_double_t;

typedef unsigned long bvm_native_ulong_t;
typedef signed long bvm_native_long_t;

//typedef signed long long gmc_int64_t;
//typedef unsigned long long gmc_uint64_t;

#if BVM_NATIVE_INT64_ENABLE
typedef signed long long bvm_int64_t;
typedef unsigned long long bvm_uint64_t;
#else
#include "../../../../src/h/int64_emulated.h"
#endif

#endif /* BVM_PD_H_ */

#endif /* BVM_PLATFORM_LINUX */
