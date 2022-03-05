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

#ifndef BVM_PD_COMMON_H_
#define BVM_PD_COMMON_H_

/**
  @file

  Platform-dependent global functions and definitions.

  @author Greg McCreath
  @since 0.0.10

*/

#include "pd_memory.h"
#if BVM_CONSOLE_ENABLE
#include "pd_console.h"
#endif
#include "pd_file.h"
#include "pd_system.h"
#if BVM_SOCKETS_ENABLE
#include "pd_sockets.h"
#endif

#endif /* BVM_PD_COMMON_H_ */