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

#ifndef BVM_NET_H_
#define BVM_NET_H_

/**

 @file

 Definitions for IO.

 @author Greg McCreath
 @since 0.0.10

*/

bvm_uint64_t bvm_htonll(bvm_uint64_t val);
bvm_uint32_t bvm_htonl(bvm_uint32_t val);
bvm_uint16_t bvm_htons(bvm_uint16_t val);

bvm_uint64_t bvm_ntohll(bvm_uint64_t val);
bvm_uint32_t bvm_ntohl(bvm_uint32_t val);
bvm_uint16_t bvm_ntohs(bvm_uint16_t val);

#endif /* BVM_NET_H_ */
