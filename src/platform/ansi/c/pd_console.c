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

  ANSI console support.  Requires \c BVM_ANSI_CONSOLE_ENABLE be defined.

  @author Greg McCreath
  @since 0.0.10

*/

#include "../../../../src/h/bvm.h"

#if BVM_ANSI_CONSOLE_ENABLE

#if BVM_CONSOLE_ENABLE
/* TODO - what to do about buffer overrun here? */
static char console_buf[512];
#endif

int bvm_pd_console_out(const char* msg, ...) {

#if BVM_CONSOLE_ENABLE
    int res;
	FILE *fp;
    va_list vlist;
    va_start(vlist, msg);

	fp = stdout;

    res = vsprintf(console_buf, msg, vlist);

	fprintf(fp, "%s", console_buf);

    va_end(vlist);

	fflush(fp);

	return res;
#else
    return 0;
#endif
}

#endif

