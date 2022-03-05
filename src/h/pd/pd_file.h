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

   Platform-dependent file handing functions and definitions.

   @section ov Overview

   The functions in the platform-dependent file handing defines a high(ish) level means to access file on a given platform.
   Some structures are opaque and have no meaning to the VM.  They are simply held onto and passed pack
   when appropriate - eg, the file handle returned by #bvm_pd_file_open() is a \c void*.

   Directories are assumed not to be supported by the platform.  To this end, no directory access functions
   or structures are defined.  The #bvm_pd_file_list_open(), #bvm_pd_file_list_next(), and #bvm_pd_file_list_close()
   functions are intended to allow the VM to get a list all the files in its home dir.  From there, it can
   make any decisions it wants about those files.

   Implementation note:  The functions here are a little like ANSI C, and a little like UNIX.  The file open
   modes are UNIX and mappings have been provided in the docs into ANSI C file open modes.

  @author Greg McCreath
  @since 0.0.10

*/

#ifndef BVM_PD_FILE_H_
#define BVM_PD_FILE_H_

/* File handling */
/** The seek offset is set to the specified number of offset bytes. */
#define BVM_FILE_SEEK_SET SEEK_SET
/** The seek offset is set to its current location plus offset bytes. */
#define BVM_FILE_SEEK_CUR SEEK_CUR
/** The offset is set to the size of the file plus offset bytes. */
#define BVM_FILE_SEEK_END SEEK_END

/** For opening files in read only mode.  CREAT/TRUNC/APPEND will be ignored.  The file must exist. */
#define BVM_FILE_O_RDONLY   0x0000
/** For opening files in write only mode.  If any of the CREAT/TRUNC/APPEND flags are specified the file will
 * be created if it does not exist - otherwise the file must exist. */
#define BVM_FILE_O_WRONLY   0x0001
/** For opening files in read write mode.  If any of the CREAT/TRUNC/APPEND flags are specified the file will
 * be created if it does not exist - otherwise the file must exist. */
#define BVM_FILE_O_RDWR     0x0002

/** for write open modes, create the file if it does not exist.  If the file exists this flag has no effect.  */
#define BVM_FILE_O_CREAT    0x0100
/** for write open modes, create the file if it does not exist, or truncate an existing file on open and
 * set file position to the beginning of the file */
#define BVM_FILE_O_TRUNC    0x0200
/** for write open modes, create the file if it does not exist and set the file position to the
 * end of the file.  All write are at end of the file. */
#define BVM_FILE_O_APPEND   0x0400

/**
 * flags:
 *
 * The file open flags specify how the platform is to open the file.  There are two sorts of
 * flags: BVM_FILE_O_RDONLY, BVM_FILE_O_WRONLY and BVM_FILE_O_RDWR determine the read write mode of
 * the file.  The BVM_FILE_O_CREAT, BVM_FILE_O_TRUNC and BVM_FILE_O_APPEND determine where
 * the file position if set to on open.  In ansi C file open mode equivalents it goes like this:
 *
 * BVM_FILE_O_RDONLY is "rb".
 *
 * BVM_FILE_O_WRONLY default is "ab".
 * BVM_FILE_O_RDWR | BVM_FILE_O_CREAT is "ab" (default)
 * BVM_FILE_O_RDWR | BVM_FILE_O_TRUNC is "wb"
 * BVM_FILE_O_RDWR | BVM_FILE_O_APPEND is "ab" (default)
 *
 * BVM_FILE_O_RDWR default is "r+b".
 * BVM_FILE_O_RDWR | BVM_FILE_O_CREAT is "a+b"
 * BVM_FILE_O_RDWR | BVM_FILE_O_TRUNC is "w+b"
 * BVM_FILE_O_RDWR | BVM_FILE_O_APPEND is "a+b"
 *
 * Open a resource.  Return an opaque handle to a platform file or NULL if the file
 * could not be opened.
 *
 * For those platforms that require a 'home' folder for the VM to operate from, a global null-terminated char[]
 * called #bvm_gl_home_path can be set from the command line (or by any other programmatic means).  Platform
 * implementors will need to append the given filename arg to the home path to get the real file name.
 * The VM makes no assumption about home paths and simply passes a file open request to
 * md file handling.
 */
void *bvm_pd_file_open(const char *filename, int flags);

/**
 * Close the given file handle.
 *
 * @return #BVM_ERR if any errors occurred, and zero otherwise
 */
int bvm_pd_file_close(void *handle);

/**
 *
 * @return the number of bytes read; this may be less than the number requested.
 * negative if error.  zero if EOF was reached by previous read.  #BVM_ERR if error.
 */
size_t bvm_pd_file_read(void *dst, size_t size, void *handle);

/**
 *
 * @return the number of bytes written, which is less than count if issues.
 */
size_t bvm_pd_file_write(const void *src, size_t size, void *handle);

/**
 *
 * @return #BVM_ERR if an error occurred.
 */
int bvm_pd_file_flush(void *handle);

/**
 *
 * @return new file position or #BVM_ERR if an error occurred.
 */
bvm_int32_t bvm_pd_file_setpos(void *handle, bvm_int32_t offset, bvm_uint32_t origin);

/**
 *
 * @return position within file, #BVM_ERR on error
 */
bvm_int32_t bvm_pd_file_getpos(void *handle);

/**
 *
 * Provide the size of an open file.  File position must be preserved on
 * function exit.
 *
 * @return the size of the given file, or #BVM_ERR if an error occurs
 */
size_t bvm_pd_file_sizeof(void *handle);

/**
 *
 * @return #BVM_ERR if an error occurred
 */
int bvm_pd_file_rename(const char *oldname, const char *newname);

/**
 *
 * @return #BVM_ERR if an error occurred
 */
int bvm_pd_file_remove(const char *filename);

/**
 * Not yet supported.
 *
 * @return an opaque handle to the file list.  Meaningless to the VM.
 */
void *bvm_pd_file_list_open(const char *filename);

/**
 * Not yet supported.
 *
 * @return an char handle to the name of a file.  The caller is responsible for
 * freeing the handle when unused.
 */
char *bvm_pd_file_list_next(void *handle);

/**
 * Not yet supported.
 *
 * @return #BVM_ERR if an error occurred
 */
int bvm_pd_file_list_close(void *handle);

/**
 *
 * @return #BVM_ERR if an error occurred
 */
int bvm_pd_file_truncate(void *handle, size_t size);

/**
 * Tests for existence of a file.  Worth noting that some platforms may not support this
 * natively and therefore will probably attempt a read-only file open to test for existence.
 *
 * @return 1 if file exists and is a regular file (0 is also reserved if/when dirs are supported), #BVM_ERR
 * in case of an error.
 */
int bvm_pd_file_exists(const char *filename);

#endif /*BVM_PD_FILE_H_*/
