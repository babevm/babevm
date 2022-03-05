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

#include "../../h/bvm.h"

/**
  @file

  JDWP Debugger input stream functions.

  @author Greg McCreath
  @since 0.0.10

*/

#if BVM_DEBUGGER_ENABLE

/**
 * Read bytes from the input stream and write them to the given destination.
 *
 * @param in an input stream
 * @param dest where to write to
 * @param size the number of bytes to read/write.
 *
 * @return an error if a) the input stream is already in an error state, or b) an error occurs.
 */
int bvmd_in_readbytes(bvmd_instream_t *in, void *dest, size_t size) {

	size_t nleft = size;

    if (in->error) {
        return in->error;
    }

    while (nleft > 0) {
        long count = BVM_MIN(nleft, in->left);
        if (count == 0) {
            /* end of input */
            in->error = JDWP_Error_INTERNAL;
            return in->error;
        }
        if (dest) {
            memcpy(dest, &(in->segment->bytes[in->index]), count);
        }
        in->index += count;
        in->left -= count;
        if (in->left == 0) {
            /*
             * Move to the next segment
             */
            in->segment = in->segment->next;
            if (in->segment) {
                in->left = in->segment->length;
                in->index = 0;
            }
        }
        nleft -= count;
        if (dest) {
            dest = (char *)dest + count;
        }
    }

    return in->error;
}

/**
 * Read a boolean from an input stream.
 *
 * @param in an input stream
 *
 * @return a boolean
 */
bvm_bool_t bvmd_in_readboolean(bvmd_instream_t *in) {
    unsigned char flag;
    bvmd_in_readbytes(in, &flag, sizeof(flag));
    if (in->error) {
        return 0;
    } else {
        return flag ? BVM_TRUE : BVM_FALSE;
    }
}

/**
 * Read a byte from an input stream.
 *
 * @param in an input stream
 *
 * @return a byte
 */
bvm_uint8_t bvmd_in_readbyte(bvmd_instream_t *in) {
    unsigned char val = 0;
    bvmd_in_readbytes(in, &val, sizeof(val));
    return val;
}

/**
 * Read a two byte bvm_uint16_t from an input stream.
 *
 * @param in an input stream
 *
 * @return a bvm_uint16_t
 */
bvm_uint16_t bvmd_in_readuint16(bvmd_instream_t *in) {
    bvm_uint16_t val = 0;
    bvmd_in_readbytes(in, &val, sizeof(val));
    return bvm_ntohs(val);
}

/**
 * Read a two byte bvm_int16_t from an input stream.
 *
 * @param in an input stream
 *
 * @return a bvm_int16_t
 */
bvm_int16_t bvmd_in_readint16(bvmd_instream_t *in) {
    bvm_int16_t val = 0;
    bvmd_in_readbytes(in, &val, sizeof(val));
    return bvm_ntohs(val);
}

/**
 * Read a four byte bvm_int32_t from an input stream.
 *
 * @param in an input stream
 *
 * @return a bvm_int32_t
 */
bvm_int32_t bvmd_in_readint32(bvmd_instream_t *in) {
    bvm_int32_t val = 0;
    bvmd_in_readbytes(in, &val, sizeof(val));
    return bvm_ntohl(val);
}

/**
 * Read a memory address of \c size_t bytes from an input stream.
 *
 * @param in an input stream
 *
 * @return a void * address
 */
void *bvmd_in_readaddress(bvmd_instream_t *in) {
#if BVM_32BIT_ENABLE
    return (void *) bvmd_in_readint32(in);
#else
    bvm_int64_t val = 0;
    bvmd_in_readbytes(in, &val, sizeof(val));
    return (void *) bvm_ntohll(val);
#endif
}

/**
 * Read two halves of a 64 bit integer from an input stream.
 *
 * @param in an input stream
 * @param upper32 a pointer to a bvm_uint32_t to place the upper portion of the bvm_int64_t
 * @param lower32 a pointer to a bvm_uint32_t to place the lower portion of the bvm_int64_t

 */
void bvmd_in_readint64(bvmd_instream_t *in, bvm_uint32_t *upper32, bvm_uint32_t *lower32) {

    bvmd_in_readbytes(in, upper32, sizeof(bvm_uint32_t));
    bvmd_in_readbytes(in, lower32, sizeof(bvm_uint32_t));

    *upper32 = bvm_ntohl(*upper32);
    *lower32 = bvm_ntohl(*lower32);
}

/**
 * Reads a jdwp location from an input stream.
 *
 * @param in an input stream
 * @param location the address of a location to populate
 */
void bvmd_in_readlocation(bvmd_instream_t *in, bvmd_location_t *location) {
	bvm_uint32_t unused;
	location->type = bvmd_in_readbyte(in);
    location->clazz = bvmd_in_readobject(in);
    location->method = bvmd_in_readaddress(in);
    bvmd_in_readint64(in, &unused, &(location->pc_index));
}

/**
 * Reads a char string from an input stream.
 *
 * @param in an input stream
 * @return a null-terminated char string.
 */
char *bvmd_in_readstring(bvmd_instream_t *in) {
    bvm_int32_t length = bvmd_in_readint32(in);
    char *result = bvm_heap_alloc(length+1, BVM_ALLOC_TYPE_STATIC);

    bvmd_in_readbytes(in, result, length);
    result[length] = '\0';

    return result;
}

/**
 * Reads a utfstring from an input stream.
 *
 * @param in an input stream
 * @param utfstring a pointer to a utfstring to populate
 */
void bvmd_in_readutfstring(bvmd_instream_t *in, bvm_utfstring_t *utfstring) {

	bvm_int32_t length = bvmd_in_readint32(in);

	utfstring->length = length;
	utfstring->data = bvm_heap_alloc(length+1, BVM_ALLOC_TYPE_STATIC);

    bvmd_in_readbytes(in, utfstring->data, utfstring->length);
    utfstring->data[utfstring->length] = '\0';
}

/**
 * Read an object from the input stream.  Actually reads the ID and them uses the dgb id map to
 * locate the object.  If the object is not in the map \c NULL is returned.
 *
 * @param in an input stream
 * @return the object or \c NULL.
 */
void *bvmd_in_readobject(bvmd_instream_t *in) {
	bvm_int32_t id = bvmd_in_readint32(in);
    return (id == 0) ? NULL : bvmd_id_get_by_id(id);
}

/**
 * Read a value from an input stream and place it into a cell.  The type of read that is done on
 * the input stream is determined from the jdwptag.
 *
 * @param in an input stream
 * @param jdwptag the JDWP tag type of the data
 * @param value a bvm_cell_t pointer to place the read-in value into
 */
void bvmd_in_readcell(bvmd_instream_t *in, bvm_uint8_t jdwptag, bvm_cell_t *value) {

	switch (jdwptag) {
		case (JDWP_Tag_BYTE):
			value->int_value = bvmd_in_readbyte(in);
			break;
		case (JDWP_Tag_CHAR):
			value->int_value = bvmd_in_readuint16(in);
			break;
		case (JDWP_Tag_FLOAT):
		case (JDWP_Tag_INT):
			value->int_value = bvmd_in_readint32(in);
			break;
		case (JDWP_Tag_DOUBLE):
		case (JDWP_Tag_LONG):
#if (!BVM_BIG_ENDIAN_ENABLE)
			bvmd_in_readint64(in, (bvm_uint32_t*) &((value+1)->int_value), (bvm_uint32_t*) &(value->int_value));
#else
			bvmd_in_readint64(in, (bvm_uint32_t*) &(value->int_value), (bvm_uint32_t*) &((value+1)->int_value));
#endif
			break;
		case (JDWP_Tag_SHORT):
			value->int_value = bvmd_in_readint16(in);
			break;
		case (JDWP_Tag_VOID):
			/* erm ... */
			break;
		case (JDWP_Tag_BOOLEAN):
			value->int_value = bvmd_in_readboolean(in);
			break;
		default : {
			value->ref_value = bvmd_in_readobject(in);
		}
	}
}

/**
 * Read a value from an 64 bit value from the input stream.
 *
 * @param in an input stream
 * @param value a void pointer to the 64 bit value
 */
void bvmd_in_raw64(bvmd_instream_t *in, void *value) {
    bvm_uint64_hilo_t p;
    bvmd_in_readint64(in, (bvm_uint32_t*) &(p.high), (bvm_uint32_t*) &(p.low));
    bvm_uint64_t v = uint64Pack(p.high, p.low);
    *(bvm_uint64_t*) value = v;
}

#endif
