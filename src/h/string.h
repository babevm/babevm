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

#ifndef BVM_STRING_H_
#define BVM_STRING_H_

/**
  @file

  Constants/Macros/Functions/Types for String types.

  @author Greg McCreath
  @since 0.0.10

*/

void bvm_string_print_to_console(bvm_string_obj_t *string, char *message);
bvm_string_obj_t *bvm_string_create_from_utfstring(bvm_utfstring_t *str, bvm_bool_t intern);
bvm_string_obj_t *bvm_string_create_from_cstring(const char *data);
bvm_string_obj_t *bvm_string_create_from_unicode(const bvm_uint16_t *unicode, bvm_int32_t offset, bvm_int32_t len);
char *bvm_string_to_cstring(bvm_string_obj_t *string);

#endif /*BVM_STRING_H_*/
