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

#ifndef BVM_DEBUGGER_ROOTS_H_
#define BVM_DEBUGGER_ROOTS_H_

/**

 @file

 Definitions for JDWP debug root management.

 @author Greg McCreath
 @since 0.0.10

*/



#define BVMD_ROOTMAP_SIZE 16

typedef struct _bvmdrootstruct {
	void *addr;
	struct _bvmdrootstruct *next;
} bvmd_root_t;

extern bvmd_root_t **bvmd_root_map;

void bvmd_root_init();
void bvmd_root_free();

void bvmd_root_put(void *addr);
void bvmd_root_remove(void *addr);

#endif /* BVM_DEBUGGER_ROOTS_H_ */
