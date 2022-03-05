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

  JDWP Debugger output stream functions.

  @author Greg McCreath
  @since 0.0.10

*/

#if BVM_DEBUGGER_ENABLE

/**
 * Extends an output stream by adding to its linked list of packetdata.  If the output stream
 * has no existing list, the new packetdata is the start of the list.
 *
 * @param out the output streamk to extend
 */
static void extend_outstream(bvmd_outstream_t *out) {

	/* get the current segment (if any) */
	bvmd_packetdata_t *current_segment = out->segment;

	/* create a new packetdata */
	bvmd_packetdata_t *packetdata = bvm_heap_calloc(sizeof(bvmd_packetdata_t), BVM_ALLOC_TYPE_STATIC);
	packetdata->length = 0;

	out->segment = packetdata;
	out->index = 0;
	out->left = BVM_DEBUGGER_PACKETDATA_SIZE;

	/* if there is an exiting one, make its 'next' point to the new one, otherwise make it
	 * the start of the list*/
	if (current_segment != NULL) {
		current_segment->next = packetdata;
	} else {
		/* make it the head of the packet's data list */
		out->packet->type.cmd.packetdata = packetdata;
	}
}

/**
 *
 * Overwrites bytes at a given point in an output stream.
 *
 * @param pos the bvmd_streampos_t bookmark that specifies the write location.
 * @param src the src data
 * @param size the number of bytes to write
 */
void bvmd_out_rewritebytes(bvmd_streampos_t *pos, const void *src, size_t size) {


	int nbytes;
	size_t nleft = size;
    bvmd_packetdata_t *segment = pos->segment;
    bvm_int32_t index = pos->index;
    char *ptr = (char *) src;

	while (nleft > 0) {

		nbytes = BVM_MIN(nleft, segment->length-index);

		memcpy( &(segment->bytes[index]), ptr, nbytes);

		/* cal howe many are left */
		nleft -= nbytes;

		/* increase the write position */
		index += nbytes;

		/* increase the read position */
		ptr += nbytes;

		/* if at the end of a segment, move onto the next segment */
		if (index == segment->length) {
			segment = segment->next;
			index = 0;
		}

	}
}

/**
 * Write a given number of bytes from a given src to an output stream.  The stream
 * is automatically extended.
 *
 * @param out the output stream to write to
 * @param src the src data.  If \c NULL, no data will be written to the stream, but the stream
 * write position will be moved on.
 * @param size the number of bytes to write
 *
 * @return the position within the stream of the first byte written.
 */
bvmd_streampos_t bvmd_out_writebytes(bvmd_outstream_t *out, const void *src, size_t size) {

	size_t nleft = size;
	int nbytes;
	bvmd_streampos_t writepos = {NULL, 0};
    char *ptr = (char *) src;
	int i =0;

	while (nleft > 0) {

		i++;
		/* if there is no more space in the current segment (or this is no current segment, extend the stream */
		if (out->left == 0)
			extend_outstream(out);

		nbytes = BVM_MIN(nleft, out->left);

		if (src != NULL) {
			memcpy( &(out->segment->bytes[out->index]), ptr, nbytes);
			ptr += nbytes;
		}

		/* keep the position of the first write */
		if (writepos.segment == NULL) {
			writepos.segment = out->segment;
			writepos.index = out->index;
		}

		out->left -= nbytes;
		out->index += nbytes;

		out->segment->length += nbytes;

		nleft -= nbytes;
	}

	out->packet->type.cmd.length += size;

	return writepos;
}

/**
 * Write a byte to an output stream.
 *
 * @param out the output stream to write to
 * @param value the value to write.
 *
 * @return the position within the stream of the first byte of the written value.
 */
bvmd_streampos_t bvmd_out_writebyte(bvmd_outstream_t *out, bvm_uint8_t value) {
	return bvmd_out_writebytes(out, &value, sizeof(value));
}

/**
 * Write a 4 byte integer to an output stream.  The
 * value is converted to 'network-order' for the write.
 *
 * @param out the output stream to write to
 * @param value the value to write.
 * @return the position within the stream of the first byte of the written value.
 */
bvmd_streampos_t bvmd_out_writeint32(bvmd_outstream_t *out, bvm_int32_t value) {
	bvm_int32_t i = bvm_htonl(value);
	return bvmd_out_writebytes(out, &i, sizeof(i));
}

/**
 * Write a memory address to an output stream.  The address is first converted to an integer.
 *
 * @param out the output stream to write to
 * @param address the address to write.
 * @return the position within the stream of the first byte of the written address.
 */
bvmd_streampos_t bvmd_out_writeaddress(bvmd_outstream_t *out, void *address) {
#if BVM_32BIT_ENABLE
    return bvmd_out_writeint32(out, (bvm_uint32_t) address);
#else
    bvm_uint64_t m64 = bvm_htonll((bvm_uint64_t)address);
    return bvmd_out_writebytes(out, &m64, sizeof(m64));
#endif
}

/**
 * Write a bvmd_location_t to an output stream.  The location is written out according to JDWP spec
 * for a location on the wire.
 *
 * @param out the output stream to write to
 * @param value the location to write.
 * @return the position within the stream of the first byte of the written location.
 */
bvmd_streampos_t bvmd_out_writelocation(bvmd_outstream_t *out, bvmd_location_t *value) {
	bvmd_streampos_t pos;

	pos = bvmd_out_writebyte(out, (value->clazz != NULL) ? bvmd_clazz_reftype(value->clazz) : 0) ;
	bvmd_out_writeobject(out, value->clazz);
	bvmd_out_writeaddress(out, value->method);
	bvmd_out_writeint64(out, 0, value->pc_index);

	return pos;
}

/**
 * Write a two byte short to an output stream.  The
 * value is converted to 'network-order' for the write.
 *
 * @param out the output stream to write to
 * @param value the value to write.
 * @return the position within the stream of the first byte of the written value.
 */
bvmd_streampos_t bvmd_out_writeint16(bvmd_outstream_t *out, bvm_int16_t value) {
	bvm_int16_t s = bvm_htons(value);
	return bvmd_out_writebytes(out, &s, sizeof(s));
}

/**
 * Overwrite the contents of the outputput stream at the given location with a given 4 byte int value.  The
 * value is converted to 'network-order' for the write.
 *
 * @param pos the output stream position to write to
 * @param value the value to write.
 */
void bvmd_out_rewriteint32(bvmd_streampos_t *pos, bvm_int32_t value) {
	bvm_int32_t i = bvm_htonl(value);
	bvmd_out_rewritebytes(pos, &i, sizeof(bvm_int32_t));
}

/**
 * Write two halves of a 64 bit integer to an output stream.  The values are converted to 'network-order'
 * for the write.
 *
 * @param out the output stream to write to
 * @param upper32 the upper 32 bits.
 * @param lower32 the lower 32 bits.
 * @return the position within the stream of the first byte of the written value.
 */
bvmd_streampos_t bvmd_out_writeint64(bvmd_outstream_t *out, bvm_uint32_t upper32, bvm_uint32_t lower32) {

    bvmd_streampos_t pos;

    upper32 = bvm_htonl(upper32);
    lower32 = bvm_htonl(lower32);

    pos = bvmd_out_writebytes(out, &upper32, sizeof(upper32));
    bvmd_out_writebytes(out, &lower32, sizeof(lower32));

    return pos;
}


/**
 * Write a null-terminated char string to an output stream.  The format of the written string is according
 * to JDWP specs and is preceeded by its length.
 *
 * @param out the output stream to write to
 * @param string string to write.  If \c NULL a null-string (with zero length) will be written.
 * @return the position within the stream of the first byte of the written value.
 */
bvmd_streampos_t bvmd_out_writestring(bvmd_outstream_t *out, const char *string) {
	bvmd_streampos_t pos;
	bvm_int32_t len = (string == NULL) ? 0 : strlen(string);
	pos = bvmd_out_writeint32(out, len);
	if (len) bvmd_out_writebytes(out, string, len);
	return pos;
}

/**
 * Write a bvm_utfstring_t to an output stream.  The format of the written string is according
 * to JDWP specs and is preceeded by its length.
 *
 * @param out the output stream to write to
 * @param utfstring utfstring to write.  If \c NULL a null-string (with zero length) will be written.
 * @return the position within the stream of the first byte of the written value.
 */
bvmd_streampos_t bvmd_out_writeutfstring(bvmd_outstream_t *out, const bvm_utfstring_t *utfstring) {
	bvmd_streampos_t pos;
	bvm_int32_t len = (utfstring == NULL) ? 0 : utfstring->length;
	pos = bvmd_out_writeint32(out, len);
	if (len) bvmd_out_writebytes(out, utfstring->data, utfstring->length);
	return pos;
}

/**
 * Write a java string object's value to an output stream.  The format of the written string is according
 * to JDWP specs and is preceeded by its length.  The string contents are converted to UTF-8 before
 * being written.
 *
 * @param out the output stream to write to
 * @param string_obj the string object to write.  If \c NULL a null-string (with zero length) will be written.
 * @return the position within the stream of the first byte of the written value.
 */
bvmd_streampos_t bvmd_out_writestringobjectvalue(bvmd_outstream_t *out, bvm_string_obj_t *string_obj) {
	bvmd_streampos_t pos;

	if (string_obj == NULL) {
		pos = bvmd_out_writeint32(out, 0);
	} else {

		BVM_BEGIN_TRANSIENT_BLOCK {

			bvm_utfstring_t *utfstring = bvm_str_unicode_to_utfstring(string_obj->chars->data, string_obj->offset.int_value, string_obj->length.int_value);

			BVM_MAKE_TRANSIENT_ROOT(utfstring);

			pos = bvmd_out_writeint32(out, utfstring->length);
			if (utfstring->length) bvmd_out_writebytes(out, utfstring->data, utfstring->length);

			bvm_heap_free(utfstring);

		} BVM_END_TRANSIENT_BLOCK
	}

	return pos;
}

/**
 * Write a java object to an output stream.  This actually writes an object ID to the output stream.  The object is
 * added to the dbg id map (if it is not already there), and the ID from the dbg id map is written to the stream.
 *
 * A \c NULL object value will write a zero to the output stream.
 *
 * @param out the output stream to write to
 * @param obj the object to write.  If \c NULL, zeri is written.
 * @return the position within the stream of the first byte of the written value.
 */
bvmd_streampos_t bvmd_out_writeobject(bvmd_outstream_t *out, void *obj) {
	bvm_int32_t id = (obj == NULL) ? 0 : bvmd_id_put(obj);
	return bvmd_out_writeint32(out, id);
}

/**
 * Write a boolean value to an output stream.  The boolean is written as a single byte.
 *
 * @param out the output stream to write to
 * @param value the value to write.
 * @return the position within the stream of the first byte of the written value.
 */
void bvmd_out_writeboolean(bvmd_outstream_t *out, bvm_bool_t value) {
    bvm_uint8_t b = (value != 0) ? 1 : 0;
    bvmd_out_writebytes(out, &b, sizeof(b));
}

/**
 * Writes the value of a bvm_cell_t to an output stream.  How the value is written is determined by the
 * given JDWP tag.
 *
 * The value is written as a JDWP 'tagged value'.  For primitive type, the \c jdwptag represents the true
 * tag.  For reference type, the tag is re-calculated based on the type of object it is (as per JDWP spec -
 * Class, Thread, ClassLoader, String, Object).
 *
 * @param out the output stream to write to
 * @param jdwptag the value to write.
 * @param value the value to write.
 * @return the position within the stream of the first byte of the written value.
 */
void bvmd_out_writecell(bvmd_outstream_t *out, bvm_uint8_t jdwptag, bvm_cell_t *value) {

	/* assume value is primitive and write out the tag.  For all primitives, this value is fine.  But,
	 * for a non-primitive type, we'll recalc the tag type later and overwrite this
	 * byte in the stream.   ... just trying to save a little code space ... */
	bvmd_streampos_t pos = bvmd_out_writebyte(out, jdwptag);

	switch (jdwptag) {
		case (JDWP_Tag_BYTE):
			bvmd_out_writebyte(out, value->int_value);
			break;
		case (JDWP_Tag_CHAR):
			bvmd_out_writeint16(out, value->int_value);
			break;
		case (JDWP_Tag_FLOAT):
		case (JDWP_Tag_INT):
			bvmd_out_writeint32(out, value->int_value);
			break;
		case (JDWP_Tag_DOUBLE):
		case (JDWP_Tag_LONG): {
            bvm_int32_t hi = value->int_value;
            bvm_int32_t low = (value+1)->int_value;
#if (!BVM_BIG_ENDIAN_ENABLE)
            bvmd_out_writeint64(out, hi, low);
#else
            bvmd_out_writeint64(out, low, hi);
#endif
            break;
        }
		case (JDWP_Tag_SHORT):
			bvmd_out_writeint16(out, value->int_value);
			break;
		case (JDWP_Tag_VOID):
			/* erm ... */
			break;
		case (JDWP_Tag_BOOLEAN):
			bvmd_out_writeboolean(out, value->int_value);
			break;
		default : {
			/* calc the tag type for the object */
			bvm_uint8_t cltag = bvmd_get_jdwptag_from_object(value->ref_value);
			/* overwrite the one we did before */
			bvmd_out_rewritebytes(&pos, &cltag, sizeof(cltag));
			/* .. then then output the object */
			bvmd_out_writeobject(out, value->ref_value);
		}
	}
}


/**
 * Writes the value of a 64 bit pointer to an output stream.
 *
 * The value is written as a JDWP 'tagged value'.
 *
 * @param out the output stream to write to
 * @param jdwptag the value to write - will be either for long or double
 * @param value the value to write.
 */
void bvmd_out_writeRaw64(bvmd_outstream_t *out, bvm_uint8_t jdwptag, void *value) {
    bvmd_out_writebyte(out, jdwptag);
    bvm_uint64_hilo_t p = uint64Unpack( *(bvm_uint64_t*)value);
    bvmd_out_writeint64(out, p.high, p.low);
}

#endif
