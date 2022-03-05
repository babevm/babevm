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

#ifdef BVM_PLATFORM_HYCT4200

#ifndef BVM_PD_H_
#define BVM_PD_H_

#if defined(__cplusplus)
extern "C"
{
#endif

typedef unsigned char bvm_uint8_t;
typedef signed char bvm_int8_t;
typedef unsigned short bvm_uint16_t;
typedef signed short bvm_int16_t;
typedef unsigned long bvm_uint32_t;
typedef signed long bvm_int32_t;
typedef float bvm_float_t;
typedef double bvm_double_t;

//#define BVM_CONSOLE_ENABLE 1
//#define BVM_DEBUGGER_ENABLE 1

#define BVM_DEBUG_HEAP_GC_ON_ALLOC 1
#define BVM_EXIT_ON_UNCAUGHT_EXCEPTION 1

#define BVM_PLATFORM_PATH_SEPARATOR 	':'
#define BVM_PLATFORM_FILE_SEPARATOR 	'/'

#define BVM_FLOAT_ENABLE 1
#define BVM_NATIVE_INT64_ENABLE 1

#define BVM_ANSI_FILE_ENABLE 0
#define BVM_ANSI_MEMORY_ENABLE 0
#define BVM_ANSI_SYSTEM 0

/* if using floats, make the int64 native */
#if BVM_FLOAT_ENABLE || BVM_NATIVE_INT64_ENABLE
typedef long long bvm_int64_t;
typedef unsigned long long bvm_uint64_t;
#endif

#if defined(__cplusplus)
}
#endif

#endif

#endif /* BVM_PD_H_ */
