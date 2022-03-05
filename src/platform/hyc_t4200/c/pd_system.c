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

void bvm_pd_system_exit(int code) {
    exit(code);
}

/**
 * Returns the time since 00:00:00 GMT, January 1, 1970, measured in milliseconds.
 */
bvm_int64_t bvm_pd_system_time() {
    return (bvm_int64_t) HYC_GetStartTicks();
}

#endif
