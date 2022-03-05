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

#ifndef BVM_DEBUGGER_H_
#define BVM_DEBUGGER_H_

/**

 @file

 Definitions for Babe / JDWP debug facilities.

 @author Greg McCreath
 @since 0.0.10

*/

#define BVMD_JDWP_HANDSHAKE_SIZE 14
#define BVMD_MAX_JDWP_CMDSETS    17

#define BVMD_THREADGROUP_SYSTEM_ID -11111111
#define BVMD_THREADGROUP_MAIN_ID   -11111112

#define BVMD_TIMEOUT_FOREVER -1

extern bvm_bool_t bvmd_gl_enabledebug;
extern bvm_bool_t bvmd_gl_server;
extern bvm_uint32_t bvmd_gl_timeout;
extern bvm_bool_t bvmd_gl_suspendonstart;
extern bvm_bool_t bvmd_gl_all_suspended;
extern bvm_bool_t bvmd_gl_holdevents;
extern char *bvmd_gl_address;

extern bvm_string_obj_t *bvmd_nosupp_tostring_obj;

/**
 * Represents a JDWP location.
 */
typedef struct _dbglocationstruct {
	bvm_uint8_t type;
	bvm_clazz_t *clazz;
	bvm_method_t *method;
	bvm_uint32_t pc_index;
} bvmd_location_t;

int bvmd_nextid();

void bvmd_init();
void bvmd_shutdown();

bvm_bool_t bvmd_is_vm_suspended();
void bvmd_spin_on_debugger();

bvm_uint8_t bvmd_clazz_reftype(bvm_clazz_t *clazz);
bvm_int32_t bvmd_clazz_status(bvm_clazz_t *clazz);
bvm_uint8_t bvmd_get_jdwptag_from_char(char ch);
bvm_uint8_t bvmd_get_jdwptag_from_clazz(bvm_clazz_t *clazz);
bvm_uint8_t bvmd_get_jdwptag_from_jvmstype(bvm_jtype_t type);
bvm_uint8_t bvmd_get_jdwptag_from_object(bvm_obj_t *obj);

#endif /* BVM_DEBUGGER_H_ */
