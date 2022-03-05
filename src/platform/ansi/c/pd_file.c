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

  ANSI File support.   Requires \c BVM_ANSI_FILE_ENABLE be defined.

  @author Greg McCreath
  @since 0.0.10

*/

#include "../../../../src/h/bvm.h"

#if BVM_ANSI_FILE_ENABLE

void *bvm_pd_file_open(const char *filename, int flags) {

	/* default as read only, all creation modes are ignored.  The file must exist.  */
	char *mode = "rb";

	if (flags & BVM_FILE_O_RDWR) {

		/* Default is open for reading and writing.  The stream is
		   positioned at the beginning of the file. */
		mode = "r+b";

		/* Open for reading and writing.  The file is  created
          if  it does not exist.  The stream is positioned at
          the end of the file.*/
		if (flags & BVM_FILE_O_CREAT) {
			mode ="a+b";
		} else
			/* Open for reading and writing.  The file is created
               if it does not exist, otherwise it is truncated.
               The  stream  is  positioned at the beginning of the
               file.*/
			if (flags & BVM_FILE_O_TRUNC) {
				mode ="w+b";
			} else
				/* Open for reading and writing.  The file is created
          		   if it does not exist.  The stream is positioned at
                   the end of the file. */
				if (flags & BVM_FILE_O_APPEND) {
					mode ="a+b";
				}

	} else
		if (flags & BVM_FILE_O_WRONLY) {

			/* Open for writing.  The file is created if it does
               not  exist.  The stream is positioned at the end of
               the file. */
			mode = "ab";

			/* Truncate file to zero length or create text file
          	   for writing. The stream is positioned at the
               beginning of the file. */
			if (flags & BVM_FILE_O_TRUNC) {
				mode ="wb";
			}
		}

	return fopen(filename, mode);
}

int bvm_pd_file_close(void *handle) {
	return fclose( (FILE *) handle);
}

size_t bvm_pd_file_read(void *dst, size_t size, void *handle) {
	return fread(dst, 1, size, (FILE *) handle);
}

size_t bvm_pd_file_write(const void *src, size_t size, void *handle) {
	return fwrite(src, 1, size, (FILE *) handle);
}

int bvm_pd_file_flush(void *handle) {
	return fflush( (FILE *) handle);
}

bvm_int32_t bvm_pd_file_setpos(void *handle, bvm_int32_t offset, bvm_uint32_t origin) {
	return fseek( (FILE *) handle, offset, origin);
}

bvm_int32_t bvm_pd_file_getpos(void *handle) {
	return ftell( (FILE *) handle);
}

size_t bvm_pd_file_sizeof(void *handle) {

	size_t size;

	/* where are we now ?*/
	bvm_int32_t pos = bvm_pd_file_getpos(handle);

	/* go to the end of the file and get the position */
	bvm_pd_file_setpos(handle, 0, BVM_FILE_SEEK_END);
	size = (size_t) bvm_pd_file_getpos(handle);

	/* restore position */
	bvm_pd_file_setpos(handle, pos, BVM_FILE_SEEK_SET);

	return size;
}

int bvm_pd_file_rename(const char *oldname, const char *newname) {
	return rename(oldname, newname);
}

int bvm_pd_file_remove(const char *filename) {
	return remove(filename);
}

int bvm_pd_file_truncate(void *handle, size_t size) {
	/* truncate not supported on ANSI C */
    UNUSED(handle);
    UNUSED(size);
	return BVM_OK;
}

int bvm_pd_file_exists(const char *filename) {

	void *handle = bvm_pd_file_open(filename, BVM_FILE_O_RDONLY);
	if (handle != NULL) bvm_pd_file_close(handle);

	return (handle != NULL);
}

#endif
