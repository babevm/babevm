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

  ANSI C heap support.    Requires \c BVM_ANSI_MEMORY_ENABLE be defined.

  @author Greg McCreath
  @since 0.0.10

*/

#include "../../../../src/h/bvm.h"

#if BVM_ANSI_MEMORY_ENABLE

void *bvm_pd_memory_alloc(size_t size) {
	return malloc(size);
}

void bvm_pd_memory_free(void *mem) {
	free(mem);
}

#endif
