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

#ifndef BVM_DEBUGGER_INPUTSTREAM_H_
#define BVM_DEBUGGER_INPUTSTREAM_H_

/**

 @file

 Definitions for JDWP inputstream handling.

 @author Greg McCreath
 @since 0.0.10

*/


int bvmd_in_readbytes(bvmd_instream_t *in, void *dest, size_t size);
bvm_bool_t bvmd_in_readboolean(bvmd_instream_t *in);
bvm_uint8_t bvmd_in_readbyte(bvmd_instream_t *in);
bvm_uint16_t bvmd_in_readuint16(bvmd_instream_t *in);
bvm_int16_t bvmd_in_readint16(bvmd_instream_t *in);
bvm_int32_t bvmd_in_readint32(bvmd_instream_t *in);
void *bvmd_in_readaddress(bvmd_instream_t *in);
void bvmd_in_readint64(bvmd_instream_t *in, bvm_uint32_t *lower32, bvm_uint32_t *upper32);
void bvmd_in_readlocation(bvmd_instream_t *in, bvmd_location_t *loc);
void *bvmd_in_readobject(bvmd_instream_t *in);
char *bvmd_in_readstring(bvmd_instream_t *in);
void bvmd_in_readutfstring(bvmd_instream_t *in, bvm_utfstring_t *utfstring);
void bvmd_in_readcell(bvmd_instream_t *in, bvm_uint8_t tagtype, bvm_cell_t *value);
void bvmd_in_raw64(bvmd_instream_t *in, void *value);

#endif /* BVM_DEBUGGER_INPUTSTREAM_H_ */
