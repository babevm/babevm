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

#ifndef BVM_DEBUGGERID_H_
#define BVM_DEBUGGERID_H_
/**

 @file

 Definitions for JDWP ID management.

 @author Greg McCreath
 @since 0.0.10

*/


void bvmd_id_init();
void bvmd_id_free();

bvm_int32_t bvmd_id_put(void *addr);
void bvmd_id_remove_addr(void *addr);
void bvmd_id_remove_id(bvm_int32_t id);
bvm_int32_t bvmd_id_get_by_address(void *addr);
void *bvmd_id_get_by_id(bvm_int32_t id);

bvm_uint32_t calc_hash_for_ptr(void* addr, int mod);

#endif /* BVM_DEBUGGERID_H_ */
