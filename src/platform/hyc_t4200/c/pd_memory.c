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

#include <hycddl.h>
#include "../../../../src/h/bvm.h"

NU_MEMORY_POOL  babe_memory_pool;

#define BABE_HEAP_SIZE (128*1024)

static char babe_heap[BABE_HEAP_SIZE];

void *start_addr;
void *my_heap;

void *bvm_pd_memory_alloc(size_t size) {

    return &babe_heap;
    /*my_heap = malloc(size);
    return my_heap;*/
}

void bvm_pd_memory_free(void *mem) {
    /*free(mem);*/
    /*delete mem;
    NU_Deallocate_Memory(mem);
    NU_Delete_Memory_Pool(&babe_memory_pool);*/
}

#endif
