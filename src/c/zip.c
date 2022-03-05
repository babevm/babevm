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

#if BVM_JAR_INFLATE_ENABLE
#include "../thirdparty/tinf-master/src/tinf.h"
#endif

/**

 @file

 Jar file handling and functions

 @author Greg McCreath
 @since 0.0.10

 @section jarov Overview

 This unit provides functionality for the platform to read and understand jar (zip) files.  This is a basic
 jar reader and not a winzip equivalent.

 Each jar file opened is 'interned' and it not closed during the executing of the VM.  A jar is opened
 and kept open.  Open jars are held in an array of descriptors (#jars) defined in this unit.  Rather than create
 another pool, like that used for classes or interned string this is simple and self
 contained here.

 @section jar101 Jar Files 101

 A Java jar file is actually a renamed zip file.  They are equivalent.  The zip specification can be read here:

 http://www.pkware.com/documents/casestudies/APPNOTE.TXT

 It is very important to read that specification if you are attempting to understand this unit.

 Weirdly, a zip file has the files contained in it listed twice.  No doubt due to legacy.  The first list
 starts at the beginning of the file and proceeds as:

 [fileheader][filedata][fileheader][filedata] ... etc

 this list is referred as 'local' (for some reason).  The file headers in this list are referred to as
 'local' headers and each header has a standard 4 byte signature that lets you know you are reading a local
 file header.

 The other list is contained in what is called the Central Directory (CD).  This list is located at/near
 the end of the file.  A end-of-central-directory record is the first thing to be read, it gives the offset
 of the start of the CD.  The start of the CD represents the start of a list of file headers rather like the
 local file headers, except these are called 'central directory file headers'.  They contain some duplicated
 information from the local file headers.  Depending on a bit flag, some info may exist in either the
 local file header or the central file header.  Great.  Thanks.

 The key difference of entries in the central directory is that they are not followed by file data.
 Each central directory file header contains an offset to the local file header (which *is* followed by
 the data).  To get at the actual file data the local header can be located using the offset described
 in the central directory header.

 All integer values stored in a jar file are in little-endian format.  That explains the usage of
 the #READ_LE_INT and #READ_LE_SHORT macros defined below to read those little endian values into
 int/short in an endian independent manner.

 @section intern Jar Interning

 Jars are interned to make accessing them faster.  The jar format offers no optimised means of locating
 an entry in the jar file.  Worst case is a linear lookup through the jar file looking at directory entries
 one at a time until the correct one is found.  Nasty.

 Of course, a linear lookup like the one described above is memory efficient but certainly not fast.  The
 fastest way to access a jar file would be to keep it all in memory and deal with it there (some VM
 implementations do just this).  A less extreme way may be to cache just the directory information from the
 jar in memory and use that to speed up access to the actual file.  Trouble is, directories in jar files can
 be large.  Each directory entry (local+central) carries about 80 bytes of overhead - excluding the actual
 file and path name.  So a 100 file jar would take at least 8k of memory to cache the directory (plus the
 actual file names themselves - so it is more likely to be something like 10-12k).  Too much.

 (side note: memory requirement could be reduced to probably about 30 bytes + the file name size for
 each entry, but that is still quite a lot).

 Keeping some directory information cached is going to speed up access to the jar file entry a lot so a
 better scheme is required.

 The approach we have taken here is to cache *some* of the directory.  And then, only enough to identify
 a possible file match and where to look next.  The cache method we use here takes 4 bytes per jar file entry.  That
 100 file jar then has 400 bytes of overhead for caching and not 12k.

 How is it done?  When a jar is opened it is 'interned'.  The intern processing creates the descriptor for
 it and then scans the jar central directory.  For each file header in the central directory a hash is
 calculated on the file name (and full path), and along with the offset to that directory file header is
 stored with the jar descriptor.

 These two values are squeezed into a 4 byte int.  The top 3 bytes have the offset, the lower byte has the
 hash of the file name (mod 255 - to get a value between 0 and 255, so it goes into one byte).

 The interned jar descriptor then has an cache (array) of 4 byte values that represent the hash of the file
 name and where to find its central directory file header in the jar.

 When getting a file from the jar, a hash (mod 255) on the requested file name is calculated and then the
 cache is scanned looking for jar file entries with a matching hash.  When one is found, the central directory
 file header is read from the jar file by using the offset contained in the top 3 bytes of the jar desc cache
 entry.  If the file name at the given record matches, we have the file.  If not, keep scanning the jar desc
 cache looking for the next file with the same hash to test it also.

 Yes, it is true, this does mean that there may be collisions and the actual jar file central directory may
 have to be read more than once to get to the real file, but it a pretty good trade off.  With a hash range
 of 255 there will (hopefully) rarely be collisions and even more rarely will
 there be a double collision.  At any rate, I think it is a good compromise between memory usage and speed.

 Using 3 bytes for the offset does mean a max offset of 2^24 (16M).  This in practice means that jar files
 must be less than than 16M.  For this VM, I am sure that will not be an issue.

 @section note Note

 During interning the full central directory of the jar is loaded into memory.  The central directory overhead
 is about 50 bytes per file.  So using the 100 file example before, this would mean about 7-8k of heap is used
 temporarily during interning.  It is freed immediately after interning.

 The array used for interned jar files is allocated from the heap and grows automatically as required.  It never
 shrinks.

 At this point the scan through an interned jar file desc file entry cache is linear.

 @section nsupp Not Supported ...

 Some things to note (like .. what is not known to be supported):

 @li The ZIP64 format is not supported.  Only 32 bit zips are supported.
 @li Comments on a jar file are not supported.  In fact, they will cause the file not be read.  Why?  The
     comments are variable length and on the end of the file, so supporting them means scanning backwards
     from the end of the file to find the end-of-central-directory record.  Can't be bothered and there is
     already a fair bit of code here - so comments on a jar file will cause failure.  Don't comment jar files.
 @li full file names (including the directory path) cannot be longer than 255.
 @li a jar file cannot be greater than 16777215k (16M) in size.
 @li split jar files are not supported.  Yes, the zip spec says you can split them.  We don't.
 @li only the DEFLATE and STORE compression methods are supported.
 @li crc32 checksums are not being checked as the file is inflated.

*/


/**
 * A cached descriptor of an already-opened jar file.  The jar descriptor is used to cache some
 * important information extracted from the jar file to make searching it faster.
 */
typedef struct _jarstruct {

	/** the file name of the jar */
	char *filename;

	/** a handle to the open file (if it is open) */
	BVM_FILE file;

	/** offset into the file of the first central directory file header */
	bvm_uint32_t central_dir_offset;

	/** the number of cached directory entries */
	bvm_uint32_t entries_count;

	/** array of jar entries. 24bits offset. 8 bits hash. Max jar size is therefore 16mb. */
	bvm_uint32_t entries[1];

} jardesc_t;


/** initial size of jar descriptors array */
int jars_sz = 10;

/** array of pointers to jar descriptors */
jardesc_t **jars = NULL;

/** Read a non-aligned little-endian 4 byte value */
#define READ_LE_INT(b)   ((b)[0]|((b)[1]<<8)|((b)[2]<<16)|((b)[3]<<24))

/** Read a non-aligned little-endian 2 byte value */
#define READ_LE_SHORT(b) ((b)[0]|((b)[1]<<8))

/* Offsets and lengths of fields within the zip file format */

#define MAX_JAR_SIZE			   0xFFFFFF  /* 24 bits */

/* End of central directory record */

/*
	end of central dir signature    4 bytes  (0x06054b50)
	number of this disk             2 bytes
	number of the disk with the
	start of the central directory  2 bytes
	total number of entries in the
	central directory on this disk  2 bytes
	total number of entries in
	the central directory           2 bytes
	size of the central directory   4 bytes
	offset of start of central
	directory with respect to
	the starting disk number        4 bytes
	.ZIP file comment length        2 bytes
	.ZIP file comment       (variable size)
*/

#define END_CEN_SIG                0x06054b50
#define END_CEN_LEN                22
#define END_CEN_ENTRIES_OFFSET     10
#define END_CEN_DIR_SIZE_OFFSET    12
#define END_CEN_DIR_START_OFFSET   16

/* Central directory file header */

/*
	central file header signature   4 bytes  (0x02014b50)
	version made by                 2 bytes
	version needed to extract       2 bytes
	general purpose bit flag        2 bytes
	compression method              2 bytes
	last mod file time              2 bytes
	last mod file date              2 bytes
	crc-32                          4 bytes
	compressed size                 4 bytes
	uncompressed size               4 bytes
	file name length                2 bytes
	extra field length              2 bytes
	file comment length             2 bytes
	disk number start               2 bytes
	internal file attributes        2 bytes
	external file attributes        4 bytes
	relative offset of local header 4 bytes

    file name (variable size)
    extra field (variable size)
    file comment (variable size)
*/
#define CEN_FILE_HEADER_SIG        0x02014b50
#define CEN_FILE_HEADER_LEN        46
#define CEN_FILE_BITFLAG_OFFSET     8
#define CEN_FILE_COMPMETH_OFFSET   10
#define CEN_FILE_COMPLEN_OFFSET    20
#define CEN_FILE_UNCOMPLEN_OFFSET  24
#define CEN_FILE_PATHLEN_OFFSET    28
#define CEN_FILE_EXTRALEN_OFFSET   30
#define CEN_FILE_COMMENTLEN_OFFSET 32
#define CEN_FILE_LOCALHDR_OFFSET   42

/* Local file header */

/*
    local file header signature     4 bytes  (0x04034b50)
    version needed to extract       2 bytes
    general purpose bit flag        2 bytes
    compression method              2 bytes
    last mod file time              2 bytes
    last mod file date              2 bytes
    crc-32                          4 bytes
    compressed size                 4 bytes
    uncompressed size               4 bytes
    file name length                2 bytes
    extra field length              2 bytes

    file name (variable size)
    extra field (variable size)
*/
#define LOC_FILE_HEADER_SIG        0x04034b50
#define LOC_FILE_HEADER_LEN        30
#define LOC_FILE_BITFLAG_OFFSET     6
#define LOC_FILE_COMPLEN_OFFSET    18
#define LOC_FILE_UNCOMPLEN_OFFSET  22
#define LOC_FILE_PATHLEN_OFFSET    26
#define LOC_FILE_EXTRA_OFFSET      28

/* Supported compression methods */
#define COMP_STORED                0
#define COMP_DEFLATED              8

/**
 * Put a jar desc into the jar desc cache array.  Will grow the array if required.
 *
 * @param jardesc a jar description to store.
 */
static void zip_putjar(jardesc_t *jardesc) {

	int sz = jars_sz;

	/* find an empty array slot and use it */
	while (--sz) {
		if (jars[sz] == NULL) {
			jars[sz] = jardesc;
			return;
		}
	}

	/** oops none available.  Expand jars array */
	{
		jardesc_t **newjars;
		int newsz = sz * 2;
		/* create a new jars array twice the size */
		newjars = bvm_heap_calloc(newsz * sizeof(jardesc_t *), BVM_ALLOC_TYPE_STATIC);
		/* and copy old data into it */
		memcpy(newjars,jars, sz * sizeof(jardesc_t*));
		/* free up old one */
		bvm_heap_free(jars);
		/* make new one the current one */
		jars = newjars;
		/* and put it on the end */
		jars[newsz-1] = jardesc;
	}
}

/**
 * Intern a Jar identified by the given file name.  See unit-level description for how this
 * operates.
 *
 * @param filename the name of the jar file
 *
 * @return a handle to jar descriptor #jardesc_t.
 */
static jardesc_t *zip_jar_intern(char *filename) {

	bvm_uint8_t intbuf[sizeof(bvm_uint32_t)];
    bvm_uint32_t entries, len, posn;

    bvm_uint8_t *cd = NULL, *pntr;
    bvm_uint8_t cd_end[END_CEN_LEN];

    BVM_FILE fd;

    jardesc_t *jardesc;

    /* attempt to open file.  No can do?  Exit */
    if ((fd = bvm_file_open(bvm_gl_filetype_md, filename, BVM_FILE_O_RDONLY)) == BVM_ERR)
        return NULL;

	/* Throw a wobbly on large jars.  The caching mechanism we use limits the
	 * size - yes, a trade off */
	if (bvm_file_sizeof(fd) > MAX_JAR_SIZE)
		bvm_throw_exception(BVM_ERR_VIRTUAL_MACHINE_ERROR, "Max Jar size (0xFFFFFF) exceeded");


    /* First 4 bytes must be the signature for the first local file header */
    if (bvm_file_read(&intbuf[0], sizeof(bvm_uint32_t), fd) != sizeof(bvm_uint32_t) || READ_LE_INT(intbuf) != LOC_FILE_HEADER_SIG) {
        bvm_file_close(fd);
		bvm_throw_exception(BVM_ERR_VIRTUAL_MACHINE_ERROR, "Not a valid Jar file (no head sig)");
    }

    /* Go back to the start of the CDR and read it in.  It will give us some important offsets.
     * IMPORTANT:  We assume that there is no JAR comment in the file.  Unsupported.  We do check
     * that the end of CDR signature is present though, so nothing naughty can happen. */

    /* set the file position pointer to the end of CDR marker */
    bvm_file_setpos(fd, -END_CEN_LEN, BVM_FILE_SEEK_END);

    /* read in the end of central directory structure */
    bvm_file_read(&cd_end, END_CEN_LEN, fd);

    /* end CDR sig not present?  Problem.  */
    if (READ_LE_INT(cd_end) != END_CEN_SIG) {
        bvm_file_close(fd);
		bvm_throw_exception(BVM_ERR_VIRTUAL_MACHINE_ERROR, "Not a valid Jar file (no end sig)");
    }

    /* Get the number of entries in the central directory */
    entries = READ_LE_SHORT(cd_end + END_CEN_ENTRIES_OFFSET);

    /* Get the offset from the start of the file of the first directory entry */
    posn = READ_LE_INT(cd_end + END_CEN_DIR_START_OFFSET);

    /* allocate some temp heap for the central directory - better to have in mem while we process
     * it than thrash the jar file pointer */
	len = READ_LE_INT(cd_end + END_CEN_DIR_SIZE_OFFSET);
    pntr = cd = bvm_heap_alloc(len, BVM_ALLOC_TYPE_STATIC);

    /* ... and read the central directory fully into it */
    bvm_file_setpos(fd, posn, BVM_FILE_SEEK_SET);
    bvm_file_read(cd, len, fd);

    /* we now have enough information and confidence to create and populate a jardesc */
    jardesc = bvm_heap_calloc(sizeof(jardesc_t) + (entries * sizeof(bvm_uint32_t)), BVM_ALLOC_TYPE_STATIC);

	/* make a copy of the filename for storage with the jardesc */
    len = strlen(filename);
    jardesc->filename = bvm_heap_alloc(len+1, BVM_ALLOC_TYPE_STATIC);
	memcpy(jardesc->filename, filename, len);
	jardesc->filename[len] = '\0';

    jardesc->entries_count = entries;
    jardesc->file = fd;
    jardesc->central_dir_offset = posn;

    /* Scan the directory list and add the entries to our jardesc */
    while (entries--) {

        char *pathname;
        bvm_uint32_t entry_hash, entry_offset;
        int path_len, comment_len, extra_len;

        /* Check directory entry signature is present */
        if (READ_LE_INT(pntr) != CEN_FILE_HEADER_SIG) goto error;

        /* Get the length of the pathname */
        path_len = READ_LE_SHORT(pntr + CEN_FILE_PATHLEN_OFFSET);

		/* Throw a wobbly if path_len > 255.  For speed, later the assumption is made that file
		 * names will not be longer than 255. */
        if (path_len > 255)
    		bvm_throw_exception(BVM_ERR_VIRTUAL_MACHINE_ERROR, "Max Jar entry length (255) exceeded");

        /* The pathname starts after the fixed part of the dir entry, so we'll use that as an
         * offset to calc the hash of the pathname at that point */
        entry_hash = bvm_calchash(pntr + CEN_FILE_HEADER_LEN, path_len) % 255;

        /* the offset of the local file header from the beginning of the jar */
        entry_offset = READ_LE_INT(pntr + CEN_FILE_LOCALHDR_OFFSET);

        pathname = (char *) (pntr + CEN_FILE_HEADER_LEN);

        /* Store the entry as top 24 bits is cd fileheader offset from start of cd, and lower 8 bits is
         * a hash of the path name. Not exact, but compact. */
        jardesc->entries[entries] = ( (pntr-cd) << 8) + entry_hash;

        /* Skip rest of variable fields */
        extra_len   = READ_LE_SHORT(pntr + CEN_FILE_EXTRALEN_OFFSET);
        comment_len = READ_LE_SHORT(pntr + CEN_FILE_COMMENTLEN_OFFSET);

        /* move the CD pointer onto the next file header */
        pntr += CEN_FILE_HEADER_LEN + path_len + extra_len + comment_len;
    }

    zip_putjar(jardesc);

    if (cd != NULL) bvm_heap_free(cd);

    return jardesc;

error:
    bvm_file_close(fd);
    if (cd != NULL) bvm_heap_free(cd);
    return NULL;
}

/**
 * Find a cached jar descriptor (if any), or create and cache one (if not) for the given jar name.
 *
 * @param jarname the name of the jar file
 *
 * @return a handle to jar descriptor #jardesc_t.
 */
static jardesc_t *zip_get_jar_desc(char *jarname) {

	int sz = jars_sz;

	/* not yet created?  Make it so */
	if (jars == NULL) {
		jars = bvm_heap_calloc(jars_sz * sizeof(jardesc_t *), BVM_ALLOC_TYPE_STATIC);
	}

	while (--sz) {
		if ( (jars[sz] != NULL) && (strcmp(jars[sz]->filename, jarname) == 0))  return jars[sz];
	}

	/* none found, better attempt to intern it */
	return zip_jar_intern(jarname);
}

/**
 * Load a given file name in a jar into an #bvm_filebuffer_t.
 *
 * @param jarname the name of the jar file
 * @param pathname the path of the file in the jar to load
 *
 * @return a handle to file buffer #bvm_filebuffer_t.
 */
bvm_filebuffer_t *bvm_zip_buffer_file_from_jar(char *jarname, char *pathname) {

	bvm_uint32_t hash;
	bvm_int32_t lc;
	bvm_uint32_t pathlen;
	bvm_filebuffer_t *buffer = NULL;

	/* let's get a jar descriptor - this will intern the jar if it is not
	 * already interned */
	jardesc_t *jar = zip_get_jar_desc(jarname);

	/* Trouble loading jar or it is non existent will mean 'jar' is NULL. */
	if (jar == NULL) return NULL;

	/* calc an 8 bit hash of the required file name */
	pathlen = strlen(pathname);
	hash = bvm_calchash( (bvm_uint8_t *) pathname, pathlen) % 255;

	lc = jar->entries_count;

	/* scan the directory entries looking for a match of the hash.*/

	while (lc--) {

		/* if there is a matching hash, let's go to the jar file at the offset stored in the cache and load
		 * the central directory file header and compare the name in it with the requested one.
		 * If they match, we have found our file entry.  If not, keep looking. We'll be optimistic
		 * and assume few hash collisions (even though it is only hashing on 8 bits) and begin with the assumption
		 * this is probably the file we're after. */

		/* when we do find a matching file we have to load the local file header to get some further
		 * info from it, not least where to get the actual file data. */

		if ( (jar->entries[lc] & 0xFF) == hash) {		/* the 0xFF mask is to get the lower 8 bits */

			bvm_uint8_t fileheader[CEN_FILE_HEADER_LEN+255];	/* temp area cd file header + 255 chars for path name.*/
			bvm_uint8_t localfileheader[LOC_FILE_HEADER_LEN];   /* temp area for local file header */
			bvm_uint32_t jar_pathlen;							/* the path name len in the cd file header */
			char *jar_pathname;							        /* temp debug pointer to cd file header path name */
			bvm_uint32_t local_pathlen;						    /* length of path name in local file header */

			bvm_uint32_t local_header_offset;					/* offset of local header in jar */
			bvm_uint32_t file_data_offset;					    /* offset of actual file data in jar */
			bvm_uint32_t comp_len;							    /* compressed length */
			bvm_uint32_t uncomp_len;							/* uncompressed length */
			bvm_uint16_t comp_method;							/* compression methd */
			bvm_uint16_t local_extralen;						/* 'extra len' in local file header */

			/* the offset of the CD file header - the right shift is to get rid of the file name hash. */
			bvm_uint32_t offset = jar->central_dir_offset + (jar->entries[lc] >> 8);

			/* seek to the offset of the cd header entry in the jar file */
			bvm_file_setpos(jar->file, offset, BVM_FILE_SEEK_SET);

			/* let's grab the central directory file header and grab also along with 255 bytes that will have the
			 * file name in it */
			bvm_file_read(&fileheader, sizeof(fileheader), jar->file);

			/* Check directory entry signature is present */
			if (READ_LE_INT(fileheader) != CEN_FILE_HEADER_SIG) return NULL;

			/* get the length of the path name in the jar file */
	        jar_pathlen = READ_LE_SHORT(fileheader + CEN_FILE_PATHLEN_OFFSET);

			/* can't be the one we're after if the path lengths are not equal ...) */
			if (jar_pathlen != pathlen) continue;

			/* get a pointer to the path name in the jar */
	        jar_pathname = (char *) (fileheader + CEN_FILE_HEADER_LEN);

			/* compare the path names.  Not equal?  Keep looking.  Note we use strncmp here - the
			 * pathnames in the jar file are not null-terminated. */
			if (strncmp(pathname, jar_pathname, pathlen) != 0) continue;

			/*** At this point, we have a found the file header we are looking for.  Let gather all the
			info we can about the file from the CD record.  The file offset in the CD is only to the
			beginning of the local file header - not to the actual file data.  */

			/* what is the compression method used on the file */
			comp_method = READ_LE_SHORT(fileheader + CEN_FILE_COMPMETH_OFFSET);

			/* the offset of the local file header */
	        local_header_offset = READ_LE_INT(fileheader + CEN_FILE_LOCALHDR_OFFSET);

			/* read the local file header into a buffer */
			bvm_file_setpos(jar->file, local_header_offset, BVM_FILE_SEEK_SET);
			bvm_file_read(&localfileheader, LOC_FILE_HEADER_LEN, jar->file);

			/* Check local file header signature is present - to be sure to be sure */
			if (READ_LE_INT(localfileheader) != LOC_FILE_HEADER_SIG) {
				bvm_throw_exception(BVM_ERR_VIRTUAL_MACHINE_ERROR, "Invalid Jar file header sig");
			}

			/* the compressed/uncompressed lengths from the CD record.  If zero, try the local header. */
			/* what is the uncompressed length of the file */
			uncomp_len = READ_LE_INT(fileheader + CEN_FILE_UNCOMPLEN_OFFSET);
			if (uncomp_len == 0)
				uncomp_len = READ_LE_INT(localfileheader + LOC_FILE_UNCOMPLEN_OFFSET);

			/* the compressed length of the file.  If zero, try the local header.  */
			comp_len = READ_LE_INT(fileheader + CEN_FILE_COMPLEN_OFFSET);
			if (comp_len == 0)
				comp_len = READ_LE_INT(localfileheader + LOC_FILE_COMPLEN_OFFSET);

			/* the path len again from the local header - should be the same as
			 * the CD path len, but the zip spec says it can be different - so we'll make no
			 * assumptions here */
			local_pathlen = READ_LE_SHORT(localfileheader + LOC_FILE_PATHLEN_OFFSET);

			/* the extra len in the local file header  */
			local_extralen = READ_LE_SHORT(localfileheader + LOC_FILE_EXTRA_OFFSET);

	        /* calc where the file data actual begins and seek to it */
			file_data_offset = local_header_offset + LOC_FILE_HEADER_LEN + local_pathlen + local_extralen;
			bvm_file_setpos(jar->file, file_data_offset, BVM_FILE_SEEK_SET);

			/* we have now got all the information required to read the file from the jar.  We'll allocate a
			 * buffer for the uncompressed size of the file and then get the file data into that buffer - how
			 * that is done depends on the compression method.  At this point only DEFLATE and STORE
			 * are supported. */

			BVM_BEGIN_TRANSIENT_BLOCK {

				buffer = bvm_create_buffer(uncomp_len, BVM_ALLOC_TYPE_DATA);

				BVM_MAKE_TRANSIENT_ROOT(buffer);

				switch(comp_method) {

					case COMP_STORED:

						/* easy. data is not compressed. just return it "as is" */
						bvm_file_read(buffer->data, comp_len, jar->file);
						return buffer;

					case COMP_DEFLATED: {

#if BVM_JAR_INFLATE_ENABLE

						int inflate_result;
						bvm_uint8_t *compressed_data;				/* buffer for compressed data */

                        /* read compressed data into buffer */
                        compressed_data = bvm_heap_alloc(comp_len, BVM_ALLOC_TYPE_STATIC);
                        bvm_file_read(compressed_data, comp_len, jar->file);

                        inflate_result = tinf_uncompress(buffer->data,
                                                         &uncomp_len,
                                                         compressed_data,
                                                         comp_len);

                        if ((inflate_result != TINF_OK)) {
                            BVM_VM_EXIT(BVM_FATAL_ERR_INFLATE_FAILED, NULL)
                        }

                        bvm_heap_free(compressed_data);
                        return buffer;
#else
                        BVM_VM_EXIT(BVM_FATAL_ERR_INFLATE_NOT_ENABLED, NULL)
						bvm_z_stream_tiny stream;
						bvm_uint8_t *comp_data;						/* buffer for compressed data */

						/* read compressed data into buffer */
						comp_data = bvm_heap_alloc(comp_len, BVM_ALLOC_TYPE_STATIC);
						bvm_file_read(comp_data, comp_len, jar->file);

						/* TODO.  inflate will only decompress up to 64k (decompressed) data at a time.
						 *  So, for now, max uncompressed file size is 64k - resolve (or work with) and test. */

						stream.next_in   = comp_data;
						stream.avail_in  = comp_len;
						stream.next_out  = buffer->data;
						stream.avail_out = uncomp_len;

						/* do the business! */
						if (bvm_inflatedata(&stream) != 0) {
							/* problem inflating ... */
							bvm_heap_free(buffer);
							bvm_heap_free(comp_data);
							return NULL;
						}

						bvm_heap_free(comp_data);
						return buffer;
#endif // BVM_JAR_TINYF_ENABLE
					}
					default:
                        BVM_VM_EXIT(BVM_FATAL_ERR_UNKNOWN_COMPRESSION_METHOD, NULL)
//						bvm_throw_exception(BVM_ERR_INTERNAL_ERROR,  "Unknown Jar compression mode");
				} BVM_END_TRANSIENT_BLOCK

			}
			break;
		}
	}

	return buffer;
}


