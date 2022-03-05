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

#ifndef BVM_BVM_H_
#define BVM_BVM_H_

/**
  @file

  Global Constants/Macros/Functions/Types.

  @author Greg McCreath
  @since 0.0.10

*/

#if defined(__cplusplus)
extern "C"
{
#endif

#define BVM_KB 1024
#define BVM_MB (BVM_KB*BVM_KB)

#define BVM_OK 0
#define BVM_ERR (-1)

typedef enum {
    BVM_FALSE = 0,
    BVM_TRUE = 1
} bvm_bool_t;

#include <setjmp.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <time.h>

#include <bvm_pd.h>

#include "define.h"

#include "platform-types.h"
#include "ni-types.h"

#include "vm.h"

#include "utfstring.h"
#include "file.h"
#include "zip.h"

#include "clazz.h"
#include "object.h"
#include "string.h"

#include "int64.h"

#if BVM_FLOAT_ENABLE
#	include <math.h>
#	include "fp.h"
#endif

#include "trycatch.h"

#include "pool_nativemethod.h"
#include "pool_clazz.h"
#include "pool_utfstring.h"
#include "pool_internstring.h"

#include "frame.h"
#include "stacktrace.h"
#include "thread.h"
#include "exec.h"

#include "pd/pd.h"

#include "net.h"

#if BVM_DEBUGGER_ENABLE
#	include "debugger/debugger.h"
#	include "debugger/debugger-jdwp.h"
#	include "debugger/debugger-id.h"
#	include "debugger/debugger-roots.h"
#	include "debugger/debugger-io.h"
#	include "debugger/debugger-io-inputstream.h"
#	include "debugger/debugger-io-outputstream.h"
#	include "debugger/debugger-io-transport.h"
#	include "debugger/debugger-events.h"
#	include "debugger/debugger-commands.h"
#endif

#include "collector.h"
#include "heap.h"

#include "ni.h"
#include "native.h"

// to assist with 'unused params' C compiler errors.  Some NI functions may not use all params to achieve the same result
// as the JNI, but, to keep the function signature JNI-like, we'll keep those params.
#define UNUSED(x) ((void)x)

#if defined(__cplusplus)
}
#endif

#endif /*BVM_BVM_H_*/
