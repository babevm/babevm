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

#ifndef BVM_FILE_H_
#define BVM_FILE_H_

/**
  @file

  Constants/Macros/Functions for file access.

  @author Greg McCreath
  @since 0.0.10

*/

/**
 * A structure for a buffer of file data whose length is determined at runtime.
 */
typedef struct _bvmfilebufferstruct {

	/** the byte length of the data held in #data */
	bvm_int32_t length;

	/** The current reading/writing position within the buffer */
	bvm_int32_t position;

	/** An array of byte data.  The actual size of the data array is determined at runtime */
	bvm_uint8_t data[1];
} bvm_filebuffer_t;

/**
 * Interface structure for interaction with a file system.  The #bvm_gl_filetype_md global variable is
 * populated with an instance of this structure and populates it with handles to the implementations
 * of the function prototypes defined in this unit.
 */
typedef struct _bvmfiletype {
	void *(*open)(const char *, int);
	int (*close)(void *);
	size_t (*read)(void *, size_t, void *);
	size_t (*write)(const void *, size_t, void *);
	int (*flush)(void *);
	bvm_int32_t (*setpos)(void *, bvm_int32_t, bvm_uint32_t);
	bvm_int32_t (*getpos)(void *);
	size_t (*size)(void *);
	int (*rename)(const char *, const char *);
	int (*remove)(const char *);
	int (*truncate)(void *, size_t);
	int (*exists)(const char *);
} bvm_filetypeintf_t;

/**
 *  Read a byte from a #bvm_filebuffer_t.
 */
#define bvm_read_byte(f) ((f)->data[(f)->position++])

/* global limits and sizes */

extern int bvm_gl_max_file_handles;

extern bvm_filetypeintf_t *bvm_gl_filetype_md;

void bvm_init_io();
void bvm_file_finalise();

bvm_filebuffer_t *bvm_buffer_file_from_platform(char *filename);
bvm_filebuffer_t *bvm_create_buffer(size_t size, int alloc_type);

typedef int BVM_FILE;

/**
 * Open a resource.  Return a handle to a BVM_FILE, or -1 if the file
 * could not be opened.
 */
BVM_FILE bvm_file_open(bvm_filetypeintf_t *type, const char *filename, int flags);

/**
 *
 * @return -1 if any errors occurred, and zero otherwise
 */
int bvm_file_close(BVM_FILE fp);

/**
 *
 * @return the number of objects read; this may be less than the number requested.
 * negative if error.  zero if eof was reached by previous read.
 */
size_t  bvm_file_read(void *dst, size_t size, BVM_FILE fp);

/**
 *
 * @return the number of objects written, which is less than count on error. negative if error.
 */
size_t  bvm_file_write(const void *src, size_t size, BVM_FILE fp);

/**
 *
 * @return non-zero if an error occurred.
 */
int bvm_file_flush(BVM_FILE fp);

/**
 * Set the stream position for a file.
 *
 * Offset is one of #BVM_FILE_SEEK_SET, #BVM_FILE_SEEK_CURR or #BVM_FILE_SEEK_END
 *
 * @return non-zero if an error occurred.
 */
int bvm_file_setpos(BVM_FILE fp, bvm_int32_t offset, bvm_uint32_t origin);

/**
 * Provide the stream position for a file.
 *
 * @return position within file, non-zero on error
 */
bvm_int32_t bvm_file_getpos(BVM_FILE fp);

/**
 *
 * Provide the size of an open file.
 *
 * @return the size of the given file, or -1 if an error occurs
 */
size_t bvm_file_sizeof(BVM_FILE file);

/**
 *
 * @return non-zero if an error occurred
 */
int bvm_file_rename(const char *oldname, const char *newname);

/**
 *
 * @return non-zero if an error occurred
 */
int bvm_file_remove(const char *filename);

/**
 *
 * @return non-zero if an error occurred
 */
int bvm_file_truncate(BVM_FILE file, size_t size);

/**
 *
 * @return 1 if it exists and is a regular file (0 is reserved if/when dirs are supported), -1
 * in case of an error.
 */
int bvm_file_exists(const char *filename);

bvm_uint16_t bvm_file_read_uint16(bvm_filebuffer_t *buffer);
bvm_int32_t bvm_file_read_int32(bvm_filebuffer_t *buffer);
bvm_uint32_t bvm_file_read_uint32(bvm_filebuffer_t *buffer);
void bvm_file_skip_bytes(bvm_filebuffer_t *buffer, size_t count);
bvm_uint8_t *bvm_file_read_bytes(bvm_filebuffer_t *buffer, size_t size, int alloc_type);
void bvm_file_read_bytes_into(bvm_filebuffer_t *buffer, size_t size, void *dest);
bvm_utfstring_t *bvm_file_read_utfstring(bvm_filebuffer_t *buffer, int alloc_type);

#endif /*BVM_FILE_H_*/
