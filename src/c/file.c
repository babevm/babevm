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

#include "../h/bvm.h"

/**
 * @file
 *
 * VM File handing functions
 *
 * @section ov Overview
 *
 * Provides a common layer for the VM and native functions to access the file system.  The VM does not use the
 * md_file_* functions directly - it uses this thin layer on top of them.
 *
 * Yes, it seems almost superfluous but there is a key driving factor - some md_file calls need modifying
 * before the actual calls - mainly to do with using the 'home' directory when opening files.  Opening file paths can be
 * made relative to a 'home' path by using the \c -home command line argument.
 *
 * The design is simple and underneath uses an array of #filehandle_t to store info about each open file.  Each
 * open file has a file type.
 *
 * The file type (#bvm_filetypeintf_t) is a C interface (using function pointers) that has the IO functions for a
 * given file type.  This is like a small impl of the UNIX way of handling files.  At this point, only the md_file
 * type has been implemented, but the structures are there to allow other types as well (eg, reading from a zip
 * file but without buffering it all in memory first.
 *
 * So, the file type functions provide a consistent layer for the VM regardless of the type of file it is
 * dealing with.
 *
 * Additionally, this layer serves to abstract machine dependant 'file handles' to a simple integer that can be stored
 * on a Java class - this layer makes OS file handles opaque and maintains a mapping between a given file handle
 * int, and the underlying handle used by the OS.
 *
 * The size of the underlying file handles array is #bvm_gl_max_file_handles.  This number is defaulted to
 * #BVM_MAX_FILE_HANDLES if not set on the command line.
 *
  @author Greg McCreath
  @since 0.0.10

 */

/**
 * The default number of file handles.
 */
int bvm_gl_max_file_handles = BVM_MAX_FILE_HANDLES;

typedef struct _filehandlestruct {
	bvm_filetypeintf_t *type;
	void *handle;
} filehandle_t;

/** Pointer to an array of file handles */
filehandle_t *filehandles = NULL;

/** Pointer to file type definition for md files */
bvm_filetypeintf_t *bvm_gl_filetype_md = NULL;

/**
 * Initialise the VM file handling.  Creates the storage for files from the heap.  Note that size of the
 * file handles array is not determined at compile time - it is determined from the #bvm_gl_max_file_handles
 * global variable (which is defaulted from #BVM_MAX_FILE_HANDLES).
 */
void bvm_init_io() {

	filehandles = (filehandle_t *) bvm_heap_calloc(bvm_gl_max_file_handles * sizeof(filehandle_t), BVM_ALLOC_TYPE_STATIC);

	bvm_gl_filetype_md = bvm_heap_calloc(sizeof(bvm_filetypeintf_t), BVM_ALLOC_TYPE_STATIC);

	/* populate the platform-dependent file handing interface with functions */
	bvm_gl_filetype_md->open 	= bvm_pd_file_open;
	bvm_gl_filetype_md->close 	= bvm_pd_file_close;
	bvm_gl_filetype_md->read 	= bvm_pd_file_read;
	bvm_gl_filetype_md->write 	= bvm_pd_file_write;
	bvm_gl_filetype_md->flush 	= bvm_pd_file_flush;
	bvm_gl_filetype_md->exists 	= bvm_pd_file_exists;
	bvm_gl_filetype_md->getpos 	= bvm_pd_file_getpos;
	bvm_gl_filetype_md->setpos 	= bvm_pd_file_setpos;
	bvm_gl_filetype_md->remove 	= bvm_pd_file_remove;
	bvm_gl_filetype_md->rename 	= bvm_pd_file_rename;
	bvm_gl_filetype_md->size 	 = bvm_pd_file_sizeof;
	bvm_gl_filetype_md->truncate = bvm_pd_file_truncate;
}

/**
 * Finalise the file system.  Close all open files.  Releases the file handles array
 * back to the heap.
 */
void bvm_file_finalise() {
	int lc = bvm_gl_max_file_handles;

	if (filehandles != NULL) {
		while (lc--) {
			if (filehandles[lc].handle != NULL)
				bvm_file_close(lc);
		}

		bvm_heap_free(filehandles);
	}

	bvm_heap_free(bvm_gl_filetype_md);
}

/**
 * Scan the file descriptors array looking for an unused descriptor.
 *
 * @return an int that is the file handle to use or #BVM_ERR if no handles are available.
 */
static int get_next_descriptor() {
	int i;

	for (i=0;i < bvm_gl_max_file_handles ; i++) {
		if (filehandles[i].handle == NULL) return i;
	}

	return BVM_ERR;
}

/**
 * Create a full name of a given filename taking into account the 'home' directory for the VM.  The home
 * directory is contained in #bvm_gl_home_path.  The full name of a file is the filename appended to the
 * \c bvm_gl_home_path.  If \c bvm_gl_home_path is \c NULL this function will have no effect - the filename will be
 * returned in the dst char array.  If the full file name would exceed the indicated max length the dst array will
 * contain a zero-length null-terminated string.
 *
 * The \c bvm_gl_home_path variable may (or may not) end with a #BVM_PLATFORM_FILE_SEPARATOR character.  A
 * \c BVM_PLATFORM_FILE_SEPARATOR will only be placed in between the home path and the file name if neither has one. If
 * they both have one, the resulting full file name will have both separators in it.
 *
 * @param dst where the filename bytes will be written
 * @param filename a given filename
 * @param maxlength if the length of the home and filename exceed this amount, the dst will be zero-length and
 * the function will return.
 *
 */
static void get_full_name(char* dst, const char *filename, int maxlength) {

	static char sep[2] = {BVM_PLATFORM_FILE_SEPARATOR, '\0' };

	int len = strlen(filename);
	int homelen = (bvm_gl_home_path == NULL) ? 0 : strlen(bvm_gl_home_path);

	dst[0] = '\0';

	/* if the length is excessive, just return an empty string as 'dst' */
	if ( (len + homelen) > maxlength) return;

	/* if a home path is set, create a larger filename as home/filename */
	if (bvm_gl_home_path != NULL) {
		strcat(dst,bvm_gl_home_path);
		/* if the path has no file separator on the end of it, and the filename does not begin with one, put one
		 * there */
		if ( (dst[homelen-1] != BVM_PLATFORM_FILE_SEPARATOR) && (filename[0] != BVM_PLATFORM_FILE_SEPARATOR)) {
			strcat(dst,sep);
		}
		strcat(dst,filename);
	} else {
		/* just copy the file name */
		strcat(dst,filename);
	}
}

/**
 * Opens the file of the given name using the given flags.  The flags are described
 * in #md_file.h.
 *
 * @return a non-zero file handle to the file, or #BVM_ERR if unsuccessful.
 */
BVM_FILE bvm_file_open(bvm_filetypeintf_t *type, const char *filename, int flags) {

	void *handle;
	BVM_FILE file ;

	char fullname[255];
	get_full_name(fullname, filename, 255);

	handle = type->open(fullname, flags);

	if (handle == NULL)
		return BVM_ERR; /* file opening failed */

	/* Get an available file descriptor */
	file = get_next_descriptor();

	/* if there was a handle handle */
	if (file != BVM_ERR) {
		filehandles[file].type = type;
		filehandles[file].handle = handle;
	}

	return file;
}

/**
 * Close an open file.
 *
 * @return same return codes as #bvm_pd_file_close().
 */
int bvm_file_close(BVM_FILE file) {

	bvm_filetypeintf_t *type;
	void *handle = filehandles[file].handle;

	if (handle == NULL)
		return BVM_ERR;

	type = filehandles[file].type;

	filehandles[file].handle = NULL;
	filehandles[file].type   = NULL;

	return type->close(handle);
}

/**
 * Read from a file.
 *
 * @return same return codes as #bvm_pd_file_read().
 */
size_t bvm_file_read(void *dst, size_t size, BVM_FILE file) {

	void *handle = filehandles[file].handle;

	if (handle == NULL)
		return BVM_ERR;

	return filehandles[file].type->read(dst, size, handle);
}

/**
 * Write to a file.
 *
 * @return same return codes as #bvm_pd_file_write().
 */
size_t bvm_file_write(const void *src, size_t size, BVM_FILE file) {

	void *handle = filehandles[file].handle;

	if (handle == NULL)
		return BVM_ERR;

	return filehandles[file].type->write(src, size, handle);
}

/**
 * Commit outstanding writes.
 *
 * @return same return codes as #bvm_pd_file_flush().
 */
int bvm_file_flush(BVM_FILE file) {

	void *handle = filehandles[file].handle;

	if (handle == NULL)
		return BVM_ERR;

	return filehandles[file].type->flush(handle);
}


/**
 * Set the file position.
 *
 * @return same return codes as #bvm_pd_file_setpos().
 */
int bvm_file_setpos(BVM_FILE file, bvm_int32_t offset, bvm_uint32_t origin) {

	void *handle = filehandles[file].handle;

	if (handle == NULL)
		return BVM_ERR;

	return filehandles[file].type->setpos(handle, offset, origin);
}

/**
 * Get the file position.
 *
 * @return same return codes as #bvm_pd_file_getpos().
 */
bvm_int32_t bvm_file_getpos(BVM_FILE file) {

	void *handle = filehandles[file].handle;

	if (handle == NULL)
		return BVM_ERR;

	return filehandles[file].type->getpos(handle);
}

/**
 * Get the size of an open file.
 *
 * @return same return codes as #bvm_pd_file_sizeof().
 */
size_t bvm_file_sizeof(BVM_FILE file) {

	void *handle = filehandles[file].handle;

	if (handle == NULL)
		return BVM_ERR;

	return filehandles[file].type->size(handle);
}


/**
 * Rename an file.
 *
 * @return same return codes as #bvm_pd_file_rename().
 */
int bvm_file_rename(const char *oldname, const char *newname) {

	char fulloldname[255];
	char fullnewname[255];

	get_full_name(fulloldname, oldname, 255);
	get_full_name(fullnewname, newname, 255);

	return bvm_pd_file_rename(fulloldname, fullnewname);
}

/**
 * Remove a file.
 *
 * @return same return codes as #bvm_pd_file_remove().
 */
int bvm_file_remove(const char *filename) {

	char fullname[255];
	get_full_name(fullname, filename, 255);

	return bvm_pd_file_remove(fullname);
}

/**
 * Truncate an open file.
 *
 * @return same return codes as #bvm_pd_file_truncate().
 */
int bvm_file_truncate(BVM_FILE file, size_t size) {
	void *handle = filehandles[file].handle;

	if (handle == NULL)
		return BVM_ERR;

	return filehandles[file].type->truncate(handle, size);
}


/**
 * Determine is a file exists.
 *
 * @return same return codes as #bvm_pd_file_exists().
 */
int bvm_file_exists(const char *filename) {

	char fullname[255];
	get_full_name(fullname, filename, 255);

	return bvm_pd_file_exists(fullname);
}

/**
 * Load a file of the given name completely into memory and returns a initialised #bvm_filebuffer_t that
 * contains the file.  Will return \c NULL if any problems.
 */
bvm_filebuffer_t *bvm_buffer_file_from_platform(char *filename) {

	BVM_FILE file;
	bvm_filebuffer_t *buffer = NULL;
	size_t size;

	file = bvm_file_open(bvm_gl_filetype_md, filename, BVM_FILE_O_RDONLY);

	if (file == BVM_ERR)
		return NULL;

	size = bvm_file_sizeof(file);

	buffer = bvm_create_buffer(size, BVM_ALLOC_TYPE_DATA);

	if (size != bvm_file_read(buffer->data, size, file)) {
		bvm_heap_free(buffer);
		buffer = NULL;
	}

	bvm_file_close(file);

	return buffer;
}

/**
 * Allocate a buffer of a given size from the heap.  The data is not zeroed.
 */
bvm_filebuffer_t *bvm_create_buffer(size_t size, int alloc_type) {

	/* allocate from heap enough mem to hold the buffer struct + the data */
	bvm_filebuffer_t *buffer = bvm_heap_alloc(sizeof(bvm_filebuffer_t) + size, alloc_type);
	buffer->length = size;
	buffer->position = 0;

	return buffer;
}

/**
 * Read an endian-safe unsigned 16bit integer from a #bvm_filebuffer_t.
 *
 * @param buffer the buffer to read
 *
 * @return an bvm_uint16_t of the read bytes.
 */
bvm_uint16_t bvm_file_read_uint16(bvm_filebuffer_t *buffer) {
	bvm_uint8_t b1 = bvm_read_byte(buffer);
	bvm_uint8_t b2 = bvm_read_byte(buffer);
	bvm_uint16_t c = b1 << 8 | b2;
	return c;
}

/**
 * Read an endian-safe signed 32bit integer from a #bvm_filebuffer_t.
 *
 * @param buffer the buffer to read
 *
 * @return an bvm_int32_t of the read bytes.
 */
bvm_int32_t bvm_file_read_int32(bvm_filebuffer_t *buffer) {
	bvm_uint8_t b1 = bvm_read_byte(buffer);
	bvm_uint8_t b2 = bvm_read_byte(buffer);
	bvm_uint8_t b3 = bvm_read_byte(buffer);
	bvm_uint8_t b4 = bvm_read_byte(buffer);

	bvm_int32_t c = b1 << 24 | b2 << 16 | b3 << 8 | b4;
	return c;
}

/**
 * Read an endian-safe unsigned 32bit integer from a #bvm_filebuffer_t.
 *
 * @param buffer the buffer to read
 *
 * @return a bvm_uint32_t of the read bytes.
 */
bvm_uint32_t bvm_file_read_uint32(bvm_filebuffer_t *buffer) {
	bvm_uint8_t b1 = bvm_read_byte(buffer);
	bvm_uint8_t b2 = bvm_read_byte(buffer);
	bvm_uint8_t b3 = bvm_read_byte(buffer);
	bvm_uint8_t b4 = bvm_read_byte(buffer);

	bvm_uint32_t c = b1 << 24 | b2 << 16 | b3 << 8 | b4;
	return c;
}

/**
 * Read a #bvm_filebuffer_t for 'count' bytes and ignore them.
 *
 * @param buffer the buffer to read
 * @param count the number of bytes to skip
 */
void bvm_file_skip_bytes(bvm_filebuffer_t *buffer, size_t count) {
	buffer->position += count;
}

/**
 * Read a #bvm_filebuffer_t for 'size' bytes and return a pointer to new heap allocated memory that
 * holds those just-read bytes.
 *
 * @param buffer the buffer to read
 * @param size the number of bytes to read
 * @param alloc_type the type of memory to ask the allocator to allocate for these newly-read bytes.
 *
 * @return a #bvm_uint8_t pointer to the read bytes.
 */
bvm_uint8_t *bvm_file_read_bytes(bvm_filebuffer_t *buffer, size_t size, int alloc_type) {
    size_t lc;
	bvm_uint8_t *result = bvm_heap_alloc(size, alloc_type);
	for (lc=0;lc<size;lc++)
		result[lc] = bvm_read_byte(buffer);
	return result;
}

/**
 * Read a #bvm_filebuffer_t for 'size' bytes into existing memory.  The given destination memory must be able to
 * hold the number of requested bytes.
 *
 * @param buffer the buffer to read
 * @param size the number of bytes to read
 * @param dest a destination to write the bytes to
 */
void bvm_file_read_bytes_into(bvm_filebuffer_t *buffer, size_t size, void *dest) {
    size_t lc;
	for (lc=0; lc < size; lc++)
		((bvm_uint8_t *)dest)[lc] = bvm_read_byte(buffer);
}

/**
 * Read a #bvm_utfstring_t from a #bvm_filebuffer_t.  The returned utfstring and its data may be newly
 * allocated (as STATIC) from the heap.  All read utfstrings are added to the utfstring pool if they
 * are not already in it.
 *
 * @param buffer the buffer to read
 * @param alloc_type the GC allocation type of the memory allocated for the string
 *
 * @return a new #bvm_utfstring_t
 */
bvm_utfstring_t *bvm_file_read_utfstring(bvm_filebuffer_t *buffer, int alloc_type) {

	bvm_uint16_t lc, length;
	bvm_utfstring_t *str;

	/* get the length of the string */
	length = bvm_file_read_uint16(buffer);

    // create a new utfstring from the heap
	str = bvm_str_new_utfstring(length, alloc_type);

	/* populate the data of the new string from the file - one byte at a time */
	for (lc=0; lc < length; lc++)
		str->data[lc] = bvm_read_byte(buffer);

	/* zero terminate it */
	str->data[length] = '\0';

	return str;
}

