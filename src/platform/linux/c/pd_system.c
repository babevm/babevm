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

  Linux32 platform-dependent system functions.  Requires \c BVM_PLATFORM_LINUX be defined.

  @author Greg McCreath
  @since 0.0.10

*/

#include "../../../../src/h/bvm.h"

#ifdef BVM_PLATFORM_LINUX

#include <sys/time.h>

bvm_int64_t bvm_pd_system_time() {

	/*
	 * Under linux, use the gettimeofday() function to get the seconds/microseconds since 01/01/1970.
	 *
	 * We have to do bvm_int64_t manipulation to get our answer.
	 */

	struct timeval time_val;

	bvm_int64_t seconds;
	bvm_int64_t microseconds;
	bvm_int64_t result;
	bvm_int64_t thousand;

	/* create a bvm_int64_t of value 1000 */
	BVM_INT64_uint32_to_int64(thousand, 1000);

	/* get the linux system time since 01/01/1970 */
	gettimeofday(&time_val, NULL);

	/* get the seconds into a bvm_int64_t and multiply by 1000 */
	BVM_INT64_uint32_to_int64(seconds, time_val.tv_sec);
	result = BVM_INT64_mul(seconds, thousand);

	/* get the microseconds (not 'milli' seconds) into a bvm_int64_t and add to it the seconds bvm_int64_t - note the div by
	 * 1000 - this is converting microseconds to milliseconds */
	BVM_INT64_uint32_to_int64(microseconds, time_val.tv_usec);
	BVM_INT64_increase(result, BVM_INT64_div(microseconds, thousand) );

	return  result;
}

void bvm_pd_system_exit(int code) {
	exit(code);
}

#endif

