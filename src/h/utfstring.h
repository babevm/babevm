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

#ifndef BVM_UTFSTRING_H_
#define BVM_UTFSTRING_H_

/**
  @file

  Constants/Macros/Functions/Types for utfstrings.

  @author Greg McCreath
  @since 0.0.10

*/

#define BVM_MAX_UTFSTRING_LENGTH 0xFFFF

/**
 * A VM utf8 string (note: NOT a Java String object). VM utf strings are utf8 encoded strings.  The length is
 * given by the length field.
 *
 * As a standard within this VM all utfstrings are null-terminated.  The length field of utfstring struct
 * does not include the extra null character.
 *
 * Also, note the 'next' field.  As utfstrings mainly come from classes when they are loaded, and are pooled
 * for repeated 'constant' access, they go into linked lists within a hash-bucket.
 */
typedef struct _bvmutfstringstruct {

	/** The length of the utf8 chars - does not include the null-terminator character */
	bvm_uint16_t length;

	/** the encoded chars - plus a null-terminator*/
	bvm_uint8_t *data;

	/** pointer to the next #bvm_utfstring_t in the same hash bucket as this one in the utf string pool */
	struct _bvmutfstringstruct *next;

} bvm_utfstring_t ;



void bvm_str_replace_char(char *name, int len, char oldchar, char newchar);
bvm_uint8_t *bvm_str_utfstrchr(bvm_utfstring_t *str, bvm_uint8_t ch);
int bvm_str_utfstringcmp(bvm_utfstring_t *s1, bvm_utfstring_t *s2);
bvm_utfstring_t *bvm_str_new_utfstring(size_t length, int alloc_type);
bvm_utfstring_t *bvm_str_copy_utfstring(bvm_utfstring_t *string, bvm_uint16_t offset, bvm_uint16_t length, bvm_uint16_t destoffset, bvm_bool_t is_static);
bvm_uint32_t bvm_str_utf8_length_from_unicode(bvm_uint16_t *str, bvm_uint32_t offset, bvm_uint32_t length);
bvm_uint32_t bvm_str_unicode_length_from_utf8(bvm_uint8_t *utf8, bvm_uint32_t offset, bvm_uint32_t len);
void bvm_str_encode_unicode_to_utf8(bvm_uint16_t *chars, bvm_uint32_t offset, bvm_uint32_t length, bvm_int8_t *buf);
void bvm_str_decode_utf8_to_unicode(bvm_uint8_t *bytes, bvm_uint32_t offset, bvm_uint32_t length, bvm_uint16_t *chars);
bvm_utfstring_t bvm_str_wrap_utfstring(char *buf);
void bvm_str_itoa(bvm_int32_t value, char* str, int base);
char *bvm_str_utfstring_to_cstring(bvm_utfstring_t *str);
bvm_int8_t *bvm_str_unicode_to_chararray(bvm_uint16_t *unicode, bvm_int32_t offset, bvm_int32_t len);
bvm_utfstring_t *bvm_str_unicode_to_utfstring(bvm_uint16_t *unicode, bvm_int32_t offset, bvm_int32_t len);


#endif /* BVM_UTFSTRING_H_ */
