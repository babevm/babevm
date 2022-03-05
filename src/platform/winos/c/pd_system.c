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

  WinOS platform-dependent system functions.  Requires \c BVM_PLATFORM_WINDOWS is defined.

  @author Greg McCreath
  @since 0.0.10

*/

#include "../../../../src/h/bvm.h"

#ifdef BVM_PLATFORM_WINDOWS

#include "windows.h"

bvm_int64_t bvm_pd_system_time() {

	/*
	 * On windows we must first get the time as at 01/01/1970 - all other comparison are based on the
	 * difference between the current local time and that.  The time at 01/01/1970 is a 'static' so we don't have to go
	 * back and get it each time.
	 */

	static bvm_int64_t time_1_1_70 = BVM_INT64_ZERO;
	static bvm_int64_t time_nano;
	bvm_int64_t now_time;

	SYSTEMTIME system_time;
	FILETIME   file_time;

	if (BVM_INT64_zero_eq(time_1_1_70)) {

		/* set time_1_1_70 to midnight 01/01/70. */
		memset(&system_time, 0, sizeof(system_time));
		system_time.wYear  = 1970;
		system_time.wMonth = 1;
		system_time.wDay   = 1;
		SystemTimeToFileTime(&system_time, &file_time);

		/* store the above time to a bvm_int64_t for later use */
		// TODO 64: pack 64, not hilo
		BVM_INT64_setHilo(&time_1_1_70, file_time.dwHighDateTime, file_time.dwLowDateTime);

		/* default a bvm_int64_t var to 10000, this is used reduce the windows return of 100ns blocks to milliseconds */
		BVM_INT64_uint32_to_int64(time_nano, 10000);
	}

	/* get the local time and convert it to a file time */
	GetLocalTime(&system_time);
	SystemTimeToFileTime(&system_time, &file_time);

	/* subtract the 01/01/1970 time from current local time to get the time diff since 01/01/1970. */
    // TODO 64: pack 64, not hilo
	BVM_INT64_setHilo(&now_time, file_time.dwHighDateTime, file_time.dwLowDateTime);
    BVM_INT64_decrease(now_time, time_1_1_70);

    /* current time is in nanos, so we'll dive by 10000 to get to millis */
	return (BVM_INT64_div(now_time, time_nano));
}

void bvm_pd_system_exit(int code) {
	exit(code);
}

#endif
