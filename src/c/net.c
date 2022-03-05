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

 Functions for converting network byte-order data to platform byte order.  Network data is
 always big endian.  If we're on a little endian system, the data is converted.

  @author Greg McCreath
  @since 0.0.10
*/

 #include "../h/bvm.h"

#if (!BVM_BIG_ENDIAN_ENABLE)

/**
 * equivalent of htonll().  Not all platforms have it - hence included in code here.
 *
 * @param val a val to convert - it is not mutated.
 * @return network order value
 */
bvm_uint64_t bvm_htonll(bvm_uint64_t val) {
#if BVM_NATIVE_INT64_ENABLE
    return ((((val) & 0xff00000000000000ull) >> 56)
       | (((val) & 0x00ff000000000000ull) >> 40)
       | (((val) & 0x0000ff0000000000ull) >> 24)
       | (((val) & 0x000000ff00000000ull) >> 8)
       | (((val) & 0x00000000ff000000ull) << 8)
       | (((val) & 0x0000000000ff0000ull) << 24)
       | (((val) & 0x000000000000ff00ull) << 40)
       | (((val) & 0x00000000000000ffull) << 56));
#else
    bvm_uint64_t ret = {bvm_htonl(val.high), bvm_htonl(val.low)};
    return ret;
#endif
}
/**
 * equivalent of htonl().  Not all platforms have it - hence included in code here.
 *
 * @param val a val to convert - it is not mutated.
 * @return network order value
 */
bvm_uint32_t bvm_htonl(bvm_uint32_t val) {
    return ((((val) & 0xff000000u) >> 24) | (((val) & 0x00ff0000u) >> 8) | (((val) & 0x0000ff00u) << 8) | (((val) & 0x000000ffu) << 24));
}

/**
 * equivalent of htons().  Not all platforms have it - hence included in code here.
 *
 * @param val a val to convert - it is not mutated.
 * @return network order value
 */
bvm_uint16_t bvm_htons(bvm_uint16_t val) {
    return ((bvm_uint16_t) ((((val) >> 8) & 0xff) | (((val) & 0xff) << 8)));
}

/**
 * equivalent of ntohll().  Not all platforms have it - hence included in code here.
 *
 * @param val a val to convert - it is not mutated.
 * @return host order value
 */
bvm_uint64_t bvm_ntohll(bvm_uint64_t val)  {return bvm_htonll(val); }

/**
 * equivalent of ntohl().  Not all platforms have it - hence included in code here.
 *
 * @param val a val to convert - it is not mutated.
 * @return host order value
 */
bvm_uint32_t bvm_ntohl(bvm_uint32_t val)  {return bvm_htonl(val); }

/**
 * equivalent of ntohs().  Not all platforms have it - hence included in code here.
 *
 * @param val a val to convert - it is not mutated.
 * @return host order value
 */
bvm_uint16_t bvm_ntohs(bvm_uint16_t val)  {return bvm_htons(val); }

#else

bvm_uint64_t bvm_htonll(bvm_uint64_t val) {return val};
bvm_uint32_t bvm_htonl(bvm_uint32_t val) {return val};
bvm_uint16_t bvm_htons(bvm_uint16_t val) {return val};

bvm_uint64_t bvm_ntohll(bvm_uint64_t val)  {return val};
bvm_uint32_t bvm_ntohl(bvm_uint32_t val)  {return val};
bvm_uint16_t bvm_ntohs(bvm_uint16_t val)  {return val};

#endif
