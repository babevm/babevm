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
  @file

  UTFString handling functions

  @author Greg McCreath
  @since 0.0.10

*/

/**
 * Replace a given char in a c string with another char.
 *
 * @param name the chars to search in
 * @param len the length to scan
 * @param oldchar the old char to replace
 * @param newchar the new char to replace with
 */
void bvm_str_replace_char(char *name, int len, char oldchar, char newchar) {
	while (len--) {
		if (name[len] == oldchar)
			name[len] = newchar;
	}
}

/**
 * Utfstring equivalent for 'strchr'.  Searches a utfstring left to right looking for a given byte.
 *
 * @param utfstring - the bvm_utfstring_t to scan
 * @param ch - a byte to scan for.
 *
 * @returns a pointer to the first occurrence of the byte within the utfstring or \c NULL if the byte is
 * not found.
 */
bvm_uint8_t *bvm_str_utfstrchr(bvm_utfstring_t *utfstring, bvm_uint8_t ch) {

	bvm_uint16_t lc;
	bvm_uint16_t len = utfstring->length;

	for (lc=0; lc < len; lc++) {
		if (utfstring->data[lc] == ch)
			return &utfstring->data[lc];
	}

	return NULL;
}

/**
 * Compare two utfstrings for equality on a length and byte-for-byte level.
 *
 * @param str1 the first utfstring to compare
 * @param str2 the second utfstring to compare
 *
 * @return the same return values as for ANSI C \c memcmp.
 */
int bvm_str_utfstringcmp(bvm_utfstring_t *str1, bvm_utfstring_t *str2) {
	if (str1->length != str2->length) return BVM_ERR;
    return memcmp(str1->data, str2->data, str1->length);
}

/**
 * Creates a new utfstring of a given length allocated from the heap using the given allocation type.  Only a
 * single heap allocation is performed to create both the string and its (empty) data.  The string's data pointer
 * actually points into this single allocation.  When freeing strings created with this function, only the
 * string needs to be freed - its data does not have to be freed separately.
 *
 * The string's char data is not zeroed (it may contain garbage), but it is actually one longer than the specified length and
 * this extra char is a \c NULL to make the char data null-terminated.
 *
 * A terminated null is placed in the last (extra) char of the string.
 *
 * @param length the length of the char data of the new string - the actual length will be one greater than this to allow
 * for a null-terminator.
 *
 * @param alloc_type the allocation type to use when getting heap memory.
 *
 * @return a new bvm_utfstring_t pointer.
 */
bvm_utfstring_t *bvm_str_new_utfstring(size_t length, int alloc_type) {

	bvm_utfstring_t *str = bvm_heap_alloc(sizeof(bvm_utfstring_t) + length + 1,  alloc_type);
	str->length = length;
	str->data = ( (bvm_uint8_t *) str) + sizeof(bvm_utfstring_t);
	str->next = NULL;

	/* null terminate it ... */
	str->data[length] = '\0';

	return str;
}

/**
 * Copies a utfstring.  This routine allocates the new utfstring
 * from the heap and copies contents of the given utfstring into it.
 *
 * @param utfstring the string to copy
 * @param offset the offset into the source utfstring to start the copy
 * @param length the size of the copy
 * @param destoffset the offset into the new string to start copying characters
 * @param is_static if BVM_TRUE, the new utfstring memory will be allocated as #BVM_ALLOC_TYPE_STATIC, otherwise it will
 * be #BVM_ALLOC_TYPE_DATA
 *
 * @return a new utfstring
 */
bvm_utfstring_t *bvm_str_copy_utfstring(
        bvm_utfstring_t *utfstring,
        bvm_uint16_t offset,
        bvm_uint16_t length,
        bvm_uint16_t destoffset,
        bvm_bool_t is_static) {

	/* alloc a single block to hold the utfstring struct and its data plus a null terminator - the single
	 * allocation is purely to avoid multiple allocs for this operation */
	bvm_utfstring_t *new_str = bvm_str_new_utfstring(length,  is_static ? BVM_ALLOC_TYPE_STATIC : BVM_ALLOC_TYPE_DATA);
	memcpy(new_str->data + destoffset, utfstring->data + offset, length);

	/* and make sure it is null terminated ... */
	new_str->data[length] = '\0';

	return new_str;
}

/**
 * Return the number of utf8 characters that would be needed to
 * encode a unicode string as a utf8 string.
 *
 * @param unicode pointer to array of unicode chars
 * @param offset the start within the unicode chars to being counting
 * @param length the number of unicode char to count
 * @return the number of bytes required to hold the unicode characters if they where converted
 */
bvm_uint32_t bvm_str_utf8_length_from_unicode(bvm_uint16_t *unicode, bvm_uint32_t offset, bvm_uint32_t length) {

	bvm_uint32_t res = 0;

    unicode += offset;

    for (; length > 0; unicode++, length--) {
    	bvm_uint16_t ch = *unicode;
        /* for 1 byte chars */
        if ((ch != 0) && (ch <= 0x7f))
            res++;
        /* for 2 byte chars */
        else if (ch <= 0x7FF)
            res += 2;
        else
        	/* else 3 byte chars */
            res += 3;
    }

    return res;
}

/**
 * Scan a utf8 string and count the number of unicode characters in it. If a non-utf8 encoded byte is encountered, the
 * length is truncated at the point of the bad char.
 *
 * @param utf8
 * @param offset
 * @param len
 * @return
 */
bvm_uint32_t bvm_str_unicode_length_from_utf8(bvm_uint8_t *utf8, bvm_uint32_t offset, bvm_uint32_t len) {

	bvm_uint32_t count = 0;
	bvm_uint32_t i;

	utf8 += offset;

	for (i = 0; i < len; i++, count++) {

		bvm_uint8_t ch = utf8[i];

		/* one byte char */
		if ((ch & 0x80) == 0)
		   ;
		else
			/* two byte char */
			if ((ch & 0xE0) == 0xC0)
				i++;
			else
				/*three byte char */
				if ((ch & 0xF0) == 0xE0)
					i += 2;
				else
					/* don't know */
					return len;
	}

	return count;
}

/**
 * Given an array of unicode chars, utf8 encode the char data into the given bvm_int8_t buffer.  Note that \c length
 * is the number of unicode characters to convert, not the number of utf characters to create.
 *
 * @param chars the uncode characters to decode to bytes
 * @param offset the offset within the unicode chars to start at
 * @param len the number of unicode chracters to convert
 * @param buf the byte array to populate with utf8 data.  Must be pre-sized.
 * See #bvm_str_utf8_length_from_unicode.
 */
void bvm_str_encode_unicode_to_utf8(bvm_uint16_t *chars, bvm_uint32_t offset, bvm_uint32_t len, bvm_int8_t *buf) {

	bvm_uint32_t i, pos = 0;

	chars += offset;

	for (i = 0; i < len; i++) {
		bvm_uint16_t ch = chars[i];
		if (ch >= 0x0001 && ch <= 0x007f) {
			buf[pos++] = (char) ch;
		} else if (ch <= 0x07ff) {
			buf[pos++] = (char) (0xc0 | (0x3f & (ch >> 6)));
			buf[pos++] = (char) (0x80 | (0x3f &  ch));
		} else {
			buf[pos++] = (char) (0xe0 | (0x0f & (ch >> 12)));
			buf[pos++] = (char) (0x80 | (0x3f & (ch >>  6)));
			buf[pos++] = (char) (0x80 | (0x3f &  ch));
		}
	}
}

/**
 * UTF8 encode the given bytes to unicode into the given char array.  There must be enough space in
 * the char array to accommodate the converted characters.  The char array can be pre-sized using the return
 * value from #bvm_str_unicode_length_from_utf8.
 *
 * @param bytes the utf bytes to encode
 * @param offset the starting offset within the utf8 bytes
 * @param len the length of utf8 bytes to process
 * @param chars the unicode char array to populate
 */
void bvm_str_decode_utf8_to_unicode(bvm_uint8_t *bytes, bvm_uint32_t offset, bvm_uint32_t len, bvm_uint16_t *chars) {

	/* two loops going on here, the 'i' loop is moving through the utf8
	 * bytes, and the 'count' loop is counting the number of unicode
	 * characters as we go though.  They may move at different rates ... */
	bvm_uint32_t i;
	bvm_uint32_t count = 0;

	bytes += offset;

	for (i = 0; i < len; i++, count++) {

		bvm_uint8_t ch = bytes[i];

		/* if char is single byte */
		if ((ch & 0x80) == 0)
		   chars[count] = bytes[i];
		else
		/* if char is double byte */
		if ((ch & 0xE0) == 0xC0) {
			chars[count] = (((bytes[i] & 0x1f) << 6) + (bytes[i+1] & 0x3f));
			i++;
		}
		else
		/* if char is triple byte */
		if ((ch & 0xF0) == 0xE0) {
			chars[count] = (((bytes[i] & 0xf) << 12) + ((bytes[i+1] & 0x3f) << 6) + (bytes[i+2] & 0x3f));
			i += 2;
		}
	}
}

/**
 * Given a char buffer, wrap it in an #bvm_utfstring_t and return it.  No heap allocation takes place -
 * the string is on the stack and does not need to be freed.
 *
 * The char data must be null-terminated.
 *
 * @param data the data to wrap
 * @return a new #bvm_utfstring_t that points at the given char data.
 */
bvm_utfstring_t bvm_str_wrap_utfstring(char *data) {
	bvm_utfstring_t str;
	str.length = strlen(data);
	str.data = (bvm_uint8_t *) data;
	return str;
}

/**
 * Reverse the chars in a char array.  The begin and end pointers must be
 * part of the same char string (of course).
 *
 * @param begin char pointer to first char to swap
 * @param end char pointer to last char to swap
 *
 */
static void str_reverse(char* begin, char* end) {
	char aux;
	while (end > begin)
		aux=*end, *end--=*begin, *begin++=aux;
}

/**
 * A re-implementation of \c itoa.  \c itoa is not ANSI C, so we'll include our own here.
 *
 * @param value the integer value convert to a c string
 * @param str pointer to char array to place result
 * @param base the radix
 */
void bvm_str_itoa(bvm_int32_t value, char* str, int base) {

	static char num[] = "0123456789abcdefghijklmnopqrstuvwxyz";

	char* wstr=str;

	int sign;

	div_t res;

	/* validate base */
	if (base<2 || base>35){ *wstr='\0'; return; }

	/* take care of sign */
	if ((sign=value) < 0)
		value = -value;

	/* Conversion. number is reversed. */
	do {
		res = div(value,base);
		*wstr++ = num[res.rem];
		value = res.quot;
	} while ( value );

	if (sign<0) *wstr++='-';

	*wstr='\0';

	/* Reverse string */
	str_reverse(str,wstr-1);
}

/**
 * Copy the char contents of a utf string to a null-terminated char array.  This
 * function allocates from the heap - it is the responsibility of the caller
 * to manage that memory.
 *
 * @param utfstring the string to convert.
 * @returns a ne char array allocated from the heap that has a copy of the char from the
 * given utfstring.
 */
char *bvm_str_utfstring_to_cstring(bvm_utfstring_t *utfstring) {

	char *chars = bvm_heap_alloc(utfstring->length + 1, BVM_ALLOC_TYPE_DATA);
	memcpy(chars, utfstring->data, utfstring->length+1); /* copy data plus null termintor */
	return chars;
}

/**
 * Create a null-terminated char array of unicode characters from an unicode array.
 *
 * @param unicode
 * @param offset
 * @param len
 * @return
 */
bvm_int8_t *bvm_str_unicode_to_chararray(bvm_uint16_t *unicode, bvm_int32_t offset, bvm_int32_t len) {

	bvm_uint32_t utf_length;
	bvm_int8_t *data;

	utf_length = bvm_str_utf8_length_from_unicode(unicode, offset, len);

	data = bvm_heap_alloc(utf_length + 1, BVM_ALLOC_TYPE_DATA); /* extra 1 is for null terminator */

	bvm_str_encode_unicode_to_utf8(unicode, offset, len, data);

	data[utf_length] = '\0';

	return data;
}


/**
 * Given a unicode array range, return a utfstring that contains the UTF-8 encoded chars.  The
 * utfstring is allocated from the heap.  Note the the utf_string is a single memory allocation and includes
 * the data.  The user of this function must manage the utfstring string - but to free it, only a
 * single #bvm_heap_free call is required.
 *
 * @param unicode unicode chars to convert
 * @param offset the offset into the given unicode chars to start the conversion.
 * @param len the number of unicode characters to convert
 * @return
 */
bvm_utfstring_t *bvm_str_unicode_to_utfstring(bvm_uint16_t *unicode, bvm_int32_t offset, bvm_int32_t len) {

	bvm_uint32_t utf_length;
	bvm_utfstring_t *utfstr;

	utf_length = bvm_str_utf8_length_from_unicode(unicode, offset, len);

	/* create memory for the new utfstring - just to the single alloc - the data pointer
	 * will point inside of that */
	utfstr = bvm_str_new_utfstring(utf_length, BVM_ALLOC_TYPE_DATA);

	bvm_str_encode_unicode_to_utf8(unicode, offset, len, (bvm_int8_t *) utfstr->data);

	utfstr->data[utfstr->length] = '\0'; // TODO.  not needed?  Null char is already there from bvm_str_new_utfstring.

	return utfstr;
}



