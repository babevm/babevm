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

 String handling functions

 @author Greg McCreath
 @since 0.0.10

 */

/**
 * Create a String object from a utfstring.  The utfstring \c str argument is assumed to
 * be Java utf encoded.  The data from str is copied (not wrapped).
 *
 * If the \c intern argument is true, the actual memory allocated for the String will be
 * an #bvm_internstring_obj_t and not a vanilla #bvm_string_obj_t.  The new intern string
 * will have its utfstring field set to point to the location of \c str.
 *
 * @param utfstring the utfstring to make a String from
 * @param intern #BVM_TRUE will create a #bvm_internstring_obj_t and not a #bvm_string_obj_t
 *
 * @return a new String object allocated from the heap.
 */
bvm_string_obj_t *bvm_string_create_from_utfstring(bvm_utfstring_t *utfstring, bvm_bool_t intern) {

	bvm_uint32_t unicode_length, i;

	bvm_jchar_array_obj_t *char_array_obj;

	bvm_string_obj_t *string_obj;

	bvm_uint16_t alloc_size = (intern) ? sizeof(bvm_internstring_obj_t) : sizeof(bvm_string_obj_t);

	BVM_BEGIN_TRANSIENT_BLOCK {

		/* get the memory for the object */
		string_obj = bvm_heap_calloc(alloc_size, BVM_ALLOC_TYPE_STRING);

		/* clear the chars pointer in case a GC occurs during the array allocation */
		string_obj->chars = NULL;

		/* make sure our new string memory is not GC'd during char array array memory allocation */
		BVM_MAKE_TRANSIENT_ROOT(string_obj);

		/* set the class of the object.  Bummer, BVM_STRING_CLAZZ will be uninitialised if we
		 * are creating intern strings for some bootstrap classes (like the String class).
		 * As these classes are being loaded, the intern strings are created.  There is a manual
		 * fudge for this over in vm.c where initial classes are loaded. */
		string_obj->clazz = (bvm_clazz_t *) BVM_STRING_CLAZZ;

		/* how many unicode chars required ? */
		unicode_length = bvm_str_unicode_length_from_utf8(utfstring->data, 0, utfstring->length);

		/* Create the char array that will contain the string text */
		char_array_obj = (bvm_jchar_array_obj_t *) bvm_object_alloc_array_primitive(unicode_length, BVM_T_CHAR);

		/* if the lengths are the same then there are no encoded characters so we can just
		 * copy the data.  Otherwise, there is encoded data and we'll do it the hard way ... */
		if (unicode_length == utfstring->length) {
			for (i = unicode_length; i--;)
				char_array_obj->data[i] = (bvm_uint16_t) utfstring->data[i] & 0xFF;
		} else
			bvm_str_decode_utf8_to_unicode(utfstring->data, 0, utfstring->length, char_array_obj->data);

		/* set the array data of the new string object to the char array object */
		string_obj->chars  = char_array_obj;
		string_obj->length.int_value = unicode_length;
		string_obj->offset.int_value = 0;

		if (intern) ( (bvm_internstring_obj_t *) string_obj)->utfstring = utfstring;

	} BVM_END_TRANSIENT_BLOCK

	return string_obj;
}

/**
 * Create String object from a null-terminated char array.  The length of the char data may not
 * exceed \c BVM_MAX_UTFSTRING_LENGTH.
 *
 * @param data the chars that make up the String
 *
 * @return a new String object.
 */
bvm_string_obj_t *bvm_string_create_from_cstring(const char *data) {

	bvm_utfstring_t str;
	bvm_uint32_t length = strlen(data);

	/* there is an VM imposed upper-limit on the size of a utfstring - done to keep
	 * utfstring lengths to bvm_uint16_t */
	if (length > BVM_MAX_UTFSTRING_LENGTH)
		bvm_throw_exception(BVM_ERR_VIRTUAL_MACHINE_ERROR, "Max utfstring size (0xFFFF) exceeded");

	str.length = length;
	str.data = (bvm_uint8_t *) data;

	/* creating a string object makes a copy of the char data as unicode - it does
	 * not store it anywhere, so we're okay to just use a pointer to this local bvm_utfstring_t as a
	 * param to making the String object.
	 */
    return bvm_string_create_from_utfstring(&str, BVM_FALSE);
}


/**
 * Create String object from an array of Unicode characters. The unicode characters will be
 * copied into a new char array for the String.
 *
 * @param unicode a pointer to an array of Unicode characters
 * @param offset the offset into the unicode array for beginning the String
 * @param len the number of unicode characters to put into the new String
 *
 * @return a new string object
 */
bvm_string_obj_t *bvm_string_create_from_unicode(const bvm_uint16_t *unicode, bvm_int32_t offset, bvm_int32_t len) {

	bvm_jchar_array_obj_t *char_array_obj;

	bvm_string_obj_t *string_obj;

	BVM_BEGIN_TRANSIENT_BLOCK {

		/* get the memory for the object */
		string_obj = bvm_heap_calloc(sizeof(bvm_string_obj_t), BVM_ALLOC_TYPE_STRING);

		/* clear the chars pointer in case a GC occurs during the array allocation */
		string_obj->chars = NULL;

		/* make sure our new string memory is not GC'd during char array array memory allocation */
		BVM_MAKE_TRANSIENT_ROOT(string_obj);

		string_obj->clazz = (bvm_clazz_t *) BVM_STRING_CLAZZ;

		/* Create the char array that will contain the string text */
		char_array_obj = (bvm_jchar_array_obj_t *) bvm_object_alloc_array_primitive(len, BVM_T_CHAR);

		/* copy the unicode data to the new char array */
		memcpy(char_array_obj->data, &unicode[offset], len * sizeof(bvm_uint16_t));

		/* set the array data of the new string object to the char array object */
		string_obj->chars  = char_array_obj;
		string_obj->length.int_value = len;
		string_obj->offset.int_value = 0;

	} BVM_END_TRANSIENT_BLOCK

	return string_obj;
}


/**
 * Create a null-terminated C String from a Java string by simply using the lower
 * byte of each of the java chars in the java string.  Nothing unicode here - just
 * truncation to a byte.  The returned char[] is allocated from the heap.
 *
 * @param string_obj a string object to convert
 *
 * @return a null-terminated C string
 */

char *bvm_string_to_cstring(bvm_string_obj_t *string_obj) {

	bvm_jchar_array_obj_t *char_array_obj;
	bvm_uint32_t len;
	bvm_uint32_t offset;
	char *buf;

	/* The char array of the string to print. */
	char_array_obj = string_obj->chars;
	len = string_obj->length.int_value;
	offset  = string_obj->offset.int_value;

	/* Create a temp buffer for it. Extra char is for null terminator */
	buf = bvm_heap_alloc(len+1, BVM_ALLOC_TYPE_DATA);

	buf[len] = '\0';

	/* copy the bytes to a buffer (chars are 16bit so we're taking only the lower 8 bits). */
	while (len--)
		buf[len] = (bvm_uint8_t) (char_array_obj->data[len+offset] & 0xff);

	return buf;
}


#if BVM_CONSOLE_ENABLE

/**
 * Prints a given java String object to the platform console optionally using it as a param
 * to a output string (like printf).  Unicode is not output to the console - the output is the lower
 * 8 bits of each char in the String.  No encoding.
 *
 * If the input String in \c NULL, this function does nothing.
 *
 * Note that this function allocates (and frees) memory from the heap.
 *
 * @param string_obj the String object to print
 * @param message an option message to embed the string in - the message should include a \c %s at the
 * position where the string is to be printed.
 */
void bvm_string_print_to_console(bvm_string_obj_t *string_obj, char *message) {

	char *buf;

	/* no string?  Do nothing. */
	if (string_obj == NULL) return;

	/* create a C string from the Java String */
	buf = bvm_string_to_cstring(string_obj);

	/* if the message is null, just output the string, otherwise, use the string as a param
	 * to the message. */
	if (message == NULL) {
		bvm_pd_console_out(buf);
	} else {
		bvm_pd_console_out(message, buf);
	}

	/* free the buffer - it is no longer used */
	bvm_heap_free(buf);
}

#endif

