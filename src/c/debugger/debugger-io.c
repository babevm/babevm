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

  JDWP Debugger IO functions.

  @author Greg McCreath
  @since 0.0.10

*/

#if BVM_DEBUGGER_ENABLE

/**
 * Global interface to debug transport.
 */
bvmd_transport_t bvmd_gl_transport;

/**
 * Global debug transport capabilities.
 */
bvmd_transport_capabilities_t bvmd_gl_transport_capabilities;

/**
 * Flag to report is the VM is in server mode and is actually listening.
 */
bvm_bool_t dbg_server_is_listening = BVM_FALSE;

/**
 * Flag to report is a debug session is currently open
 */
bvm_bool_t is_session_open = BVM_FALSE;

/**
 * Reports if a given packet is a reply packet.
 */
static bvm_bool_t dbg_is_reply_packet(const bvmd_packet_t *packet) {
	return ( (packet->type.cmd.flags & BVMD_TRANSPORT_FLAGS_REPLY) != 0);
}

/**
 * A default #bvmd_cmdhandler_t for unimplemented, unsupported, or invalid commands.
 *
 * @param in an inputstream
 * @param out an outputstream
 */
void bvmd_cmdhandler_default(bvmd_instream_t* in, bvmd_outstream_t* out) {
    out->error = JDWP_Error_NOT_IMPLEMENTED;
}

/**
 * Populates a given packet with data read from the debug transport interface.
 *
 * @param packet a given packet.
 *
 * @return BVMD_TRANSPORT_ERROR_NONE if all okay, or otherwise any error that may be returned from the debug interface byte
 * reading function.
 */
static bvmd_transport_error_t dbg_read_packet(bvmd_packet_t *packet) {

	bvm_int32_t i;
	bvmd_transport_error_t error;

	/* read the length of the packet and copy it as endian-safe to the packet */
	if ( (error = bvmd_gl_transport.read_bytes(&i, sizeof(i)))) {
		return error;
	}
	packet->type.cmd.length = bvm_htonl(i);

	/* read the id of the packet and copy it as endian-safe to the packet */
	if ( (error = bvmd_gl_transport.read_bytes(&i, sizeof(i)))) {
		return error;
	}
	packet->type.cmd.id = bvm_htonl(i);

	/* read the flags of the packet */
	if ( (error = bvmd_gl_transport.read_bytes(&(packet->type.cmd.flags), sizeof(bvm_uint8_t)))) {
		return error;
	}

	/* if it is a reply, get the error code, otherwise get the command set and command */
	if (packet->type.cmd.flags & BVMD_TRANSPORT_FLAGS_REPLY) {

		/* reply */

		/* even though we are ignoring replies, we still have to read the two bytes. */

		bvm_int16_t s;

		/* read the error code */
		if ( (error = bvmd_gl_transport.read_bytes(&s, sizeof(s)))) {
			return error;
		}

		packet->type.reply.error_code = bvm_htons(s);

	} else {

		/* command */

		/* read the command set */
		if ( (error = bvmd_gl_transport.read_bytes(&(packet->type.cmd.cmdset), sizeof(bvm_uint8_t)))) {
			return error;
		}

		/* read the command */
		if ( (error = bvmd_gl_transport.read_bytes(&(packet->type.cmd.cmd), sizeof(bvm_uint8_t)))) {
			return error;
		}
	}

	/* now, read the data portion of the inbound packet (if any).  The overhead of a packet is 11 bytes, so
	 * anything greater than that will be the data.*/
	{
		/* to keep track of the previous packet if we are creating a list of them */
		bvmd_packetdata_t *prev_packetdata = NULL;

		/* represents to number of bytes of data left to read.  11 is the size of a packet header - note we
		 * do not just use sizeof() to get it - the platform may byte align a packet struct to be more than
		 * 11 bytes - so we just code it here as 11. */
		bvm_int32_t nleft = packet->type.cmd.length - 11;

		/* while there are still bytes anticipated  */
		while (nleft > 0) {

			/* the size of the data to read will not exceed BVM_DEBUGGER_PACKETDATA_SIZE */
			int datasize = BVM_MIN(nleft, BVM_DEBUGGER_PACKETDATA_SIZE);

			/* grab some heap for the packetdata */
			bvmd_packetdata_t *packetdata = bvm_heap_calloc( sizeof(bvmd_packetdata_t), BVM_ALLOC_TYPE_STATIC);

			/* if this is the first packet data, set it as the head of the list */
			if (packet->type.cmd.packetdata == NULL) packet->type.cmd.packetdata = packetdata;

			/* if this is not the only packetdata, point the 'next' of the last one to this one - we are making
			 * a linked list */
			if (prev_packetdata != NULL) prev_packetdata->next = packetdata;

			packetdata->length = datasize;

			if ( (error = bvmd_gl_transport.read_bytes(packetdata->bytes, datasize))) {
				return error;
			}

			prev_packetdata = packetdata;
			nleft -= datasize;
		}
	}

	return BVMD_TRANSPORT_ERROR_NONE;
}

/**
 * Write a given packet to the debug transport interface.
 *
 * @param packet the packet to write
 *
 * @return BVMD_TRANSPORT_ERROR_NONE if all okay, or otherwise any error that may be returned from the debug interface byte
 * writing function.
 */
static bvmd_transport_error_t dbg_write_packet(const bvmd_packet_t* packet){

	bvmd_transport_error_t error;
	bvm_int32_t i;
	bvm_uint8_t b;

	/* write out length after converting it to network order */
	i = bvm_ntohl(packet->type.reply.length);
	if ( (error = bvmd_gl_transport.write_bytes(&i, sizeof(i)))) return error;

	/* write out id after converting it network order */
	i = bvm_ntohl(packet->type.reply.id);
	if ( (error = bvmd_gl_transport.write_bytes(&i, sizeof(i)))) return error;

	/* write out flags */
	b = packet->type.reply.flags;
	if ( (error = bvmd_gl_transport.write_bytes(&b, sizeof(b)))) return error;

	/* if the packet is a reply, write out the error (if any), otherwiede write out the commandset
	 * and command */
	if (dbg_is_reply_packet(packet)) {
		bvm_int16_t reply_err = bvm_ntohs(packet->type.reply.error_code);
		if ( (error = bvmd_gl_transport.write_bytes(&reply_err, sizeof(reply_err)))) return error;
	} else {
		/* command set */
		if ( (error = bvmd_gl_transport.write_bytes(&(packet->type.cmd.cmdset), sizeof(bvm_uint8_t )))) return error;
		/* command */
		if ( (error = bvmd_gl_transport.write_bytes(&(packet->type.cmd.cmd), sizeof(bvm_uint8_t)))) return error;
	}

	/* write out the packet data (if any).  Packet data forms a linked list of packetdata_t
	 * objects.  To write out the data we'll loop over the packet data linked list and output
	 * them one by one making sure only to write out the correct number of bytes.*/
	{
		bvmd_packetdata_t *packetdata = packet->type.reply.packetdata;

		while (packetdata != NULL) {
			if ( (error = bvmd_gl_transport.write_bytes(packetdata->bytes, packetdata->length))) return error;
			packetdata = packetdata->next;
		}
	}

	return BVMD_TRANSPORT_ERROR_NONE;
}

/**
 * Creates a new packetstream with a packet.  The newly created packet has no packetdata.
 *
 * @return a new packetstream
 */
bvmd_packetstream_t *bvmd_new_packetstream() {
	bvmd_packetstream_t *result = bvm_heap_calloc(sizeof(bvmd_packetstream_t), BVM_ALLOC_TYPE_STATIC);
	result->packet = bvm_heap_calloc(sizeof(bvmd_packet_t), BVM_ALLOC_TYPE_STATIC);
	result->packet->type.cmd.id = bvmd_nextid();
	result->packet->type.cmd.length = 11;
	return result;
}

/**
 * Create a new packetsream and configures it as a command for the given commandset and command.
 *
 * @param cmdset the command set
 * @param cmd the command
 *
 * @return a new packetstream
 */
bvmd_packetstream_t *bvmd_new_commandstream(int cmdset, int cmd) {
	bvmd_packetstream_t *stream = bvmd_new_packetstream();
	stream->packet->type.cmd.cmdset = cmdset;
	stream->packet->type.cmd.cmd = cmd;
	return stream;
}

/**
 * Frees a packetstream and also frees all associated packet and packetdata memory back to the heap as well.
 *
 * @param packetstream the packet stream to free.
 */
void dbg_free_packetstream(bvmd_packetstream_t *packetstream) {

	if (packetstream->packet != NULL) {

		/* free all the packet data segments, if any */
		bvmd_packetdata_t *data = packetstream->packet->type.cmd.packetdata;
		while ( data != NULL) {
			bvmd_packetdata_t *next = data->next;
			bvm_heap_free(data);
			data = next;
		}

		/* free the packet */
		bvm_heap_free(packetstream->packet);
	}

	/* free the stream */
	bvm_heap_free(packetstream);
}


/**
 * Sends the contents of the given packetstream to the debug transport.  The packetstream is also freed after
 * sending.
 *
 * @param packetstream the packetstream to send.
 */
void bvmd_sendstream(bvmd_packetstream_t* packetstream) {

    bvmd_transport_error_t error;

	if (!bvmd_gl_transport.is_open()) return;

	if (packetstream->ignore_send) return;

	if (packetstream->error) {
		bvm_pd_console_out("STREAM ERROR!!\n");
	}

	error = dbg_write_packet(packetstream->packet);
	dbg_free_packetstream(packetstream);

	/* if there is an error, shut down the session */
	if (error != BVMD_TRANSPORT_ERROR_NONE) {
		bvmd_session_close();
	}
}

/**
 * Reads a thread object reference from the input stream.  If an error is encountered, or the thread object is
 * not valid the output stream error is set and \c NULL is returned.
 *
 * @param in an inputstream
 * @param out an outputstream
 *
 * @return a thread object, or \c NULL if the read thread ID is invalid.
 */
bvm_thread_obj_t *bvmd_readcheck_threadref(bvmd_instream_t* in, bvmd_outstream_t* out) {

	bvm_thread_obj_t *thread_obj;

	bvm_int32_t id = bvmd_in_readint32(in);

	if (id == 0) {
		out->error = JDWP_Error_INVALID_THREAD;
		return NULL;
	}

	thread_obj = bvmd_id_get_by_id(id);

	if (thread_obj == NULL)
		out->error = JDWP_Error_INVALID_OBJECT;
	else {
		if (!bvm_thread_is_alive(thread_obj->vmthread)) {
			out->error = JDWP_Error_INVALID_THREAD;
			thread_obj = NULL;
		}
	}

	return thread_obj;
}

/**
 * Reads a clazz from the input stream.  If an error is encountered, or the clazz is
 * not valid the output stream error is set and \c NULL is returned.
 *
 * @param in an inputstream
 * @param out an outputstream
 *
 * @return a clazz, or \c NULL if the read ID is invalid.
 */
bvm_clazz_t *bvmd_read_clazzref(bvmd_instream_t* in, bvmd_outstream_t* out) {

	bvm_clazz_t *clazz;

	bvm_int32_t id = bvmd_in_readint32(in);

	if (id == 0) {
		out->error = JDWP_Error_INVALID_CLASS;
		return NULL;
	}

	clazz = bvmd_id_get_by_id(id);

	if (clazz == NULL)
		out->error = JDWP_Error_INVALID_CLASS;

	return clazz;
}

/**
 * Reads a clazz ID from the input stream and returns the corresponding #bvm_clazz_t pointer.
 *
 * If an error is encountered, or the clazz is not a clazz, the output stream error value is set and
 * \c NULL is returned.
 *
 * @param in an inputstream
 * @param out an outputstream
 *
 * @return a clazz, or \c NULL if a problem occurred.
 */
bvm_clazz_t *bvmd_readcheck_reftype(bvmd_instream_t* in, bvmd_outstream_t* out) {

	bvm_clazz_t *clazz = bvmd_in_readobject(in);

	if (clazz == NULL)
		out->error = JDWP_Error_INVALID_OBJECT;
	else {
		if (clazz->magic_number != BVM_MAGIC_NUMBER) {
			out->error = JDWP_Error_INVALID_CLASS;
			clazz = NULL;
		}
	}

	return clazz;
}

/**
 * Reads a method ID from the input stream and returns the corresponding #bvm_method_t pointer.
 *
 * If an error is encountered the output stream error value is set and \c NULL is returned.
 *
 * @param in an inputstream
 * @param out an outputstream
 *
 * @return a method, or \c NULL if a problem occurred.
 */
bvm_method_t *bvmd_readcheck_method(bvmd_instream_t* in, bvmd_outstream_t* out) {

	bvm_method_t *method = bvmd_in_readaddress(in);

	if (method == NULL)
		out->error = JDWP_Error_INVALID_METHODID;

	return method;
}

/**
 * Attempts to read a packet from the debugger and, if there is one, process a reply immediately.
 *
 * If any errors occur during reading or writing to the debugger the debug session will be closed.
 *
 * @param timeout the time to wait for data to be available from the debugger.  A value of
 * #BVMD_TIMEOUT_FOREVER will mean that this function blocks forever waiting for some data from the
 * debugger.
 *
 */
void bvmd_interact(bvm_int32_t timeout) {

	bvmd_instream_t *instream;
	bvmd_transport_error_t error;

	if (!bvm_gl_vm_is_initialised) return;

	/* if we do not have a debugger connection forget it - do nothing */
	if (!(bvmd_gl_transport.is_open())) return;

	/* check if data is waiting ... */
	if (!bvmd_gl_transport.is_data_available(timeout)) return;

	/* get the inbound packet */
	instream = bvmd_new_packetstream();
	error = dbg_read_packet(instream->packet);

	/* if there is an error, shut the session down and return */
	if (error != BVMD_TRANSPORT_ERROR_NONE) {
		bvmd_session_close();
		return;
	}

	/* we've got an in packet. Do some fiddling here to set the stream members to correctly
	 * reflect the state of the packet data it contains */
    instream->error = JDWP_Error_NONE;
    instream->segment = instream->packet->type.cmd.packetdata;
    if (instream->segment != NULL ) instream->left = instream->segment->length;

    /* we are not concerned with reply packets from a debugger - only command packets */
	if (!dbg_is_reply_packet(instream->packet)) {

		/* get the command set and the command, these are used to get a function pointer to the
		 * handler for the specific command. */
		int cmdset =  instream->packet->type.cmd.cmdset;
		int cmd =  instream->packet->type.cmd.cmd;

		bvmd_cmdhandler_t handler_func;

		/* create a new output stream for the reply */
		bvmd_outstream_t* outstream = bvmd_new_packetstream();

		/* the id of the reply is the same as the id of the request command */
		outstream->packet->type.reply.id = instream->packet->type.cmd.id;

		/* the reply flags are set to show it as a 'reply' */
		outstream->packet->type.reply.flags = BVMD_TRANSPORT_FLAGS_REPLY;

		/* if the command request falls outside the supported cmdset/cmd bounds use the default handler,
		 * otherwise use the nominated command handler function. */
		if ( (cmdset < 1 || cmdset > BVMD_MAX_JDWP_CMDSETS) ||
			 (cmd < 1 || cmd > bvmd_gl_commandset_sizes[cmdset]) ) {
			handler_func = bvmd_cmdhandler_default;
		} else {
			bvmd_cmdhandler_t* handlerSet =  bvmd_gl_command_sets[cmdset];
			handler_func = handlerSet[cmd];
		}

		/* have the handler function execute and do the real business */
		handler_func(instream, outstream);

		/* if the outstream is in an error state after handler execution, set the error code in the
		 * packet to be that same error */
		outstream->packet->type.reply.error_code = outstream->error;

		/* blocking write to the debug IO - also frees the output stream*/
		bvmd_sendstream(outstream);
	}

	/* and free the input stream, we no longer require it */
	dbg_free_packetstream(instream);
}

/**
 * Reports if a current debug connection is open.
 *
 * @return BVM_TRUE/BVM_FALSE.
 */
bvm_bool_t bvmd_is_session_open() {
	return is_session_open;
}

/**
 * Opens a session with the debugger.  If the VM is in debug 'server' mode (#bvmd_gl_server is BVM_TRUE) and
 * listening the debug transport is asked to accept a connection - it will likely block at this point.
 *
 * Otherwise, (if not in server mode) the transport layer will be asked to 'attach' to a
 * waiting debugger.
 */
void bvmd_session_open() {

	if (bvmd_gl_server) {

		if (dbg_server_is_listening) {

		    bvm_pd_console_out("Waiting for debugger connection (%s) ...\n", bvmd_gl_address);

			if (bvmd_gl_transport.accept(bvmd_gl_timeout, 4000) != BVMD_TRANSPORT_ERROR_NONE) {
			    bvm_pd_console_out("Error accepting debugger connection, or timeout. \n");
			    return;
			}
		} else {
		    bvm_pd_console_out("Not listening, unable to wait for debugger.\n");
		    return;
		}

	} else {

		if (bvmd_gl_transport.attach(bvmd_gl_address, bvmd_gl_timeout, 4000) != BVMD_TRANSPORT_ERROR_NONE) {
		    bvm_pd_console_out("Error attaching to debugger. \n");
		    return;
		}
	}

    bvm_pd_console_out("Debugger connected.\n");

    /* ... now establish all the things a debug session needs ... */

	bvmd_id_init();

	bvmd_root_init();

	/* create a dummy string to give back when the 'toString()' method is called from the
	 * degugger.  We create it then put it in dbg root so it doesn't get cleaned up.
	 */
	BVM_BEGIN_TRANSIENT_BLOCK {

		bvmd_nosupp_tostring_obj = bvm_string_create_from_cstring("<< toString() not supported in VM >>");

		BVM_MAKE_TRANSIENT_ROOT(bvmd_nosupp_tostring_obj);

		bvmd_root_put(bvmd_nosupp_tostring_obj);

	} BVM_END_TRANSIENT_BLOCK

	is_session_open = BVM_TRUE;
}

/**
 * Closes a communication session with a debugger.
 */
void bvmd_session_close() {

	bvmd_gl_transport.close();

	bvmd_thread_resume_all(BVM_TRUE, BVM_TRUE);

	bvmd_eventdef_clearall();

	bvmd_id_free();

	bvmd_root_free();

	bvmd_nosupp_tostring_obj = NULL;

    bvm_pd_console_out("Debugger disconnected.\n");

	is_session_open = BVM_FALSE;
}

/**
 * Initialises the debug transport interface, and if in server mode, starts the transport interface listening.
 */
void bvmd_io_init() {

	/* zero the transport interface */
	memset(&bvmd_gl_transport, 0, sizeof(bvmd_gl_transport));

	/* init the transport interface */
	bvmd_transport_init(&bvmd_gl_transport);

	/* get the capabilities ... TODO, unsure quite what to do with this !*/
	bvmd_gl_transport.get_capabilities(&bvmd_gl_transport_capabilities);

	/* if the VM is acting as a debug server, start listening at the given address */
	if (bvmd_gl_server) {

		if (bvmd_gl_transport.start_listening(bvmd_gl_address) == BVMD_TRANSPORT_ERROR_NONE) {
			dbg_server_is_listening = BVM_TRUE;
		} else {
		    bvm_pd_console_out("Error start debug server listening. \n");
			return;
		}
	}

	/* and attempt to open a session with the debugger */
	bvmd_session_open();
}

/**
 * Shuts down all debug communications and (if in server mode) stop the transport interface listening.
 */
void bvmd_io_shutdown() {

	if (bvmd_is_session_open()) {
		bvmd_session_close();
	}

	/* if a 'stop listening' function has been assigned, invoke it */
	if (bvmd_gl_transport.stop_listening != NULL)
		bvmd_gl_transport.stop_listening();

	dbg_server_is_listening = BVM_FALSE;
}


#endif

