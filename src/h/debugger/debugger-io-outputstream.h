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

#ifndef BVM_DEBUGGER_OUTPUTSTREAM_H_
#define BVM_DEBUGGER_OUTPUTSTREAM_H_

/**

 @file

 Definitions for JDWP output stream handling.

 @author Greg McCreath
 @since 0.0.10

*/


bvmd_streampos_t bvmd_out_writebytes(bvmd_outstream_t *out, const void *src, size_t length);
void bvmd_out_rewritebytes(bvmd_streampos_t *pos, const void *src, size_t size);
bvmd_streampos_t bvmd_out_writebyte(bvmd_instream_t *out, bvm_uint8_t value);
bvmd_streampos_t bvmd_out_writeint32(bvmd_instream_t *out, bvm_int32_t value);
bvmd_streampos_t bvmd_out_writeaddress(bvmd_outstream_t *out, void *address);
bvmd_streampos_t bvmd_out_writelocation(bvmd_outstream_t *out, bvmd_location_t *location);
bvmd_streampos_t bvmd_out_writeint16(bvmd_outstream_t *out, bvm_int16_t value);
void bvmd_out_rewriteint32(bvmd_streampos_t *pos, bvm_int32_t value);
bvmd_streampos_t bvmd_out_writeint64(bvmd_outstream_t *out, bvm_uint32_t lower32, bvm_uint32_t upper32);
bvmd_streampos_t bvmd_out_writestring(bvmd_outstream_t *out, const char *string);
bvmd_streampos_t bvmd_out_writeutfstring(bvmd_outstream_t *out, const bvm_utfstring_t *utfstring);
bvmd_streampos_t bvmd_out_writestringobjectvalue(bvmd_outstream_t *out, bvm_string_obj_t *string_obj);
bvmd_streampos_t bvmd_out_writeobject(bvmd_outstream_t *out, void *obj);
void bvmd_out_writeboolean(bvmd_outstream_t *out, bvm_bool_t val);
void bvmd_out_writecell(bvmd_outstream_t *out, bvm_uint8_t tagtype, bvm_cell_t *value);
void bvmd_out_writeRaw64(bvmd_outstream_t *out, bvm_uint8_t jdwptag, void *value);

#endif /* BVM_DEBUGGER_OUTPUTSTREAM_H_ */
