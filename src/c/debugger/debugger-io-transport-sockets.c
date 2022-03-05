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

  Standard sockets transport implementation for #bvmd_transport_t.

  @author Greg McCreath
  @since 0.0.10

*/

#if BVM_DEBUGGER_ENABLE

static int bvmd_debugger_socket = -1;
static int bvmd_server_socket = -1;

char *last_error = NULL;

#if BVM_CONSOLE_ENABLE
static char *HANDSHAKE_READ_ERROR="Unable to read JDWP handshake";
static char *HANDSHAKE_WRITE_ERROR="Unable to write JDWP handshake";
static char *HANDSHAKE_INVALID_ERROR = "Invalid Handshake received";
static char *ADDRESS_INVALID = "Invalid address";
static char *SOCKET_INIT_ERROR = "Unable to initialise Sockets";
static char *SOCKET_CONNECT_ERROR = "Unable to connect to address";
static char *SOCKET_BIND_ERROR = "Cannot bind to socket";
static char *SOCKET_LISTEN_ERROR = "Cannot listen on socket";
static char *SOCKET_ACCEPT_SELECT_ERROR = "No selects on accept";
static char *SOCKET_ACCEPT_FAILURE_ERROR = "Socket accept failure";
static char *SOCKET_ALREADY_OPEN_ERROR = "Socket already open";
#else
static char *HANDSHAKE_READ_ERROR = NULL;
static char *HANDSHAKE_WRITE_ERROR = NULL;
static char *HANDSHAKE_INVALID_ERROR = NULL;
static char *ADDRESS_INVALID = NULL;
static char *SOCKET_INIT_ERROR = NULL;
static char *SOCKET_CONNECT_ERROR = NULL;
static char *SOCKET_BIND_ERROR = NULL;
static char *SOCKET_LISTEN_ERROR = NULL;
static char *SOCKET_ACCEPT_SELECT_ERROR = NULL;
static char *SOCKET_ACCEPT_FAILURE_ERROR = NULL;
static char *SOCKET_ALREADY_OPEN_ERROR = NULL;
#endif

/**
 * Reports if a sockets connection is open.
 *
 * @return #BVM_TRUE if the connection to a debugger is open, #BVM_FALSE otherwise.
 */
static bvm_bool_t dbg_sockets_is_open() {
	return (bvmd_debugger_socket > 0);
}

/**
 * Closes the current socket connection to a debugger.
 *
 * @return always #BVMD_TRANSPORT_ERROR_NONE
 */
static bvmd_transport_error_t dbg_sockets_close() {

	if (bvmd_debugger_socket > 0) {
		bvm_pd_socket_close(bvmd_debugger_socket);
		bvmd_debugger_socket = -1;
	}

	return BVMD_TRANSPORT_ERROR_NONE;
}

/**
 * Stops a server socket listening if it is doing so.
 *
 * @return always #BVMD_TRANSPORT_ERROR_NONE
 */
static bvmd_transport_error_t dbg_socket_stop_listening() {

	if (bvmd_server_socket > 0) {
		bvm_pd_socket_server_close(bvmd_server_socket);
		bvmd_server_socket = -1;
	}

	return BVMD_TRANSPORT_ERROR_NONE;
}

/**
 * Error handling for the sockets transport.
 *
 * @param error the error code
 * @param message a message to set the 'last error message' to.
 * @param do_close if #BVM_TRUE, the current debug connection is shut down, and the server socket
 * stopped listening.
 *
 * @return the error code passed in as the \c error param.
 */
static bvmd_transport_error_t dbg_socket_transport_error(bvmd_transport_error_t error, char *message, bvm_bool_t do_close) {
	last_error = message;
	if (do_close) {
		dbg_sockets_close();
		dbg_socket_stop_listening();
	}
	return error;
}

/**
 * Write bytes to the debugger socket connection.
 *
 * @param src the source of data to write
 * @param size the size of data to write
 *
 * @return #BVMD_TRANSPORT_ERROR_ILLEGAL_STATE is the connection is not open, #BVMD_TRANSPORT_ERROR_IO_ERROR
 * if an error occurs or not all the bytes could be written. Returns #BVMD_TRANSPORT_ERROR_NONE if all is okay.
 */
static bvmd_transport_error_t dbg_socket_writebytes(const void *src, size_t size) {

	int nbytes = 0;

	/* cannot attempt a read if there is no connection */
	if (!dbg_sockets_is_open())
		return dbg_socket_transport_error(BVMD_TRANSPORT_ERROR_ILLEGAL_STATE, NULL, BVM_FALSE);

	nbytes = bvm_pd_socket_write(bvmd_debugger_socket, src, size);

	return (nbytes < size) ? dbg_socket_transport_error(BVMD_TRANSPORT_ERROR_IO_ERROR, NULL, BVM_TRUE) : BVMD_TRANSPORT_ERROR_NONE;
}

/**
 * Read bytes from the debugger socket connection.  This is a blocking read, and will not return until the
 * given number of bytes has been read or an error occurs.
 *
 * @param dst where to put the read bytes
 * @param size how many bytes to read.
 *
 * @return #BVMD_TRANSPORT_ERROR_ILLEGAL_STATE is the socket is not open, #BVMD_TRANSPORT_ERROR_IO_ERROR if
 * something bad happened, or #BVMD_TRANSPORT_ERROR_NONE if all is good.
 */
static bvmd_transport_error_t dbg_socket_readbytes(void *dst, size_t size) {

	if (size == 0) return BVMD_TRANSPORT_ERROR_NONE;

	if (!dbg_sockets_is_open())
		return dbg_socket_transport_error(BVMD_TRANSPORT_ERROR_ILLEGAL_STATE, NULL, BVM_FALSE);

	if (bvm_pd_socket_read(bvmd_debugger_socket, dst, size) <= 0)
		return dbg_socket_transport_error(BVMD_TRANSPORT_ERROR_IO_ERROR, "Connection closed", BVM_TRUE);

	return BVMD_TRANSPORT_ERROR_NONE;
}

/**
 * Performs the JDWP handshake between VM and debugger by exchanging the "JDWP-Handshake" chars.
 *
 * @param timeout the timeout value for the handshake (not implemented).
 * @return #BVMD_TRANSPORT_ERROR_IO_ERROR if any error occurs or #BVMD_TRANSPORT_ERROR_NONE if all okay.
 */
static bvmd_transport_error_t do_handshake(bvm_int32_t timeout) {

	char handshake[BVMD_JDWP_HANDSHAKE_SIZE + 1];

    /* read the "JDWP-Handshake" handshake chars */
	if (dbg_socket_readbytes(handshake, BVMD_JDWP_HANDSHAKE_SIZE)) {
		return dbg_socket_transport_error(BVMD_TRANSPORT_ERROR_IO_ERROR, HANDSHAKE_READ_ERROR, BVM_TRUE);
	}

	/* NULL terminate it */
	handshake[BVMD_JDWP_HANDSHAKE_SIZE] = '\0';

	/* make sure it actually *is* "JDWP-Handshake" */
	if (strcmp(handshake, "JDWP-Handshake") != 0) {
		return dbg_socket_transport_error(BVMD_TRANSPORT_ERROR_IO_ERROR, HANDSHAKE_INVALID_ERROR, BVM_TRUE);
	}

	/* echo the handshake chars back to the debugger */
	if (dbg_socket_writebytes(handshake, BVMD_JDWP_HANDSHAKE_SIZE)) {
		return dbg_socket_transport_error(BVMD_TRANSPORT_ERROR_IO_ERROR, HANDSHAKE_WRITE_ERROR, BVM_TRUE);
	}

	return BVMD_TRANSPORT_ERROR_NONE;
}

/**
 * Establish a connection to the given address.  The address must be of the format "host:port".
 *
 * The timeout values are ignored.
 *
 * @param address the address to connected to.  Max 128 chars.
 * @param attach_timeout ignored.
 * @param handshake_timeout ignored.
 *
 * @return #BVMD_TRANSPORT_ERROR_ILLEGAL_ARGUMENT the format of the address is invalid,
 * #BVMD_TRANSPORT_ERROR_ILLEGAL_STATE if a connection is already open,  #BVMD_TRANSPORT_ERROR_IO_ERROR
 * if any IO error occurs, or #BVMD_TRANSPORT_ERROR_NONE if all is good.
 */
static bvmd_transport_error_t dbg_socket_attach(const char* address, bvm_int32_t attach_timeout, bvm_int32_t handshake_timeout) {

	short port = BVM_DEBUGGER_SOCKET_PORT;
	char *colonpos;
	char host[128];
	int len;

	/* get the port (after the colon), and the address before the colon - only allow a host name of 128 chars */

	/* get the position of the colon */
	colonpos = strchr(address, ':');

	/* bad address is: no colon, no host before colon, a host len of more than 127, or a non-integer as a port */
	if ( (colonpos == NULL) ||
		 (colonpos == 0) ||
		 ( (len = colonpos-address) > 127) ||
		 ( (port = atoi(colonpos+1)) == 0) ) {
		return dbg_socket_transport_error(BVMD_TRANSPORT_ERROR_ILLEGAL_ARGUMENT, ADDRESS_INVALID, BVM_FALSE);
	}

	/* copy the text before the colon into the host name */
	memcpy(host, address, len);

	/* and null - terminate it */
	host[len] = '\0';

	/* cannot attempt to open if already open */
	if (dbg_sockets_is_open())
		return dbg_socket_transport_error(BVMD_TRANSPORT_ERROR_ILLEGAL_STATE, SOCKET_ALREADY_OPEN_ERROR, BVM_FALSE);

	/* attempt the connection */
	if ( (bvmd_debugger_socket = bvm_pd_socket_open(host, port)) <= 0)
		return dbg_socket_transport_error(BVMD_TRANSPORT_ERROR_IO_ERROR, SOCKET_CONNECT_ERROR, BVM_TRUE);

	/* finally, delegate the result of this function to the result of the handshake */
	return do_handshake(handshake_timeout);
}

/**
 * Reports if data is waiting to be read on the socket.  The socket interrogation will timeout
 * after \c timeout milliseconds.  To never timeout, specify a timeout of #BVMD_TIMEOUT_FOREVER.
 *
 * @param timeout the milliseconds to wait on the socket.
 *
 * @return #BVM_TRUE is data is available for reading, #BVM_FALSE otherwise.
 */
static bvm_bool_t dbg_socket_is_data_available(bvm_int32_t timeout) {
	return (bvm_pd_socket_available(bvmd_debugger_socket, timeout)) > 0 ? BVM_TRUE : BVM_FALSE;
}

/**
 * Start listening on a socket at the given address.  The address is to be a null-terminated char
 * string of the socket port to listen on.
 *
 * @param address the port to listen on.
 *
 * @return BVMD_TRANSPORT_ERROR_ILLEGAL_ARGUMENT if the address is not a valid integer port,
 * #BVMD_TRANSPORT_ERROR_ILLEGAL_STATE if the socket is already open or already listening,
 * #BVMD_TRANSPORT_ERROR_IO_ERROR for anything else, or #BVMD_TRANSPORT_ERROR_NONE if all goes well.
 */
static bvmd_transport_error_t dbg_socket_start_listening(char *address) {

	/* if an address has been supplied turn it into a port number, otherwise use the default port */
	bvm_int32_t port = (address == NULL) ? BVM_DEBUGGER_SOCKET_PORT : atoi(address);

	/* if the calculated port is incorrect, show an error and return - no listening */
	if (port <= 0) {
		return dbg_socket_transport_error(BVMD_TRANSPORT_ERROR_ILLEGAL_ARGUMENT, ADDRESS_INVALID, BVM_TRUE);
	}

	/* cannot attempt to open if already open */
	if (dbg_sockets_is_open())
		return dbg_socket_transport_error(BVMD_TRANSPORT_ERROR_ILLEGAL_STATE, NULL, BVM_FALSE);

	/* can't listen if it is already listening */
	if (bvmd_server_socket > 0)
		return dbg_socket_transport_error(BVMD_TRANSPORT_ERROR_ILLEGAL_STATE, NULL, BVM_FALSE);

    /* and attempt to start the listening server */
	if ( (bvmd_server_socket = bvm_pd_socket_server_open(port)) < 0)
			return dbg_socket_transport_error(BVMD_TRANSPORT_ERROR_IO_ERROR, SOCKET_LISTEN_ERROR, BVM_TRUE);

	return BVMD_TRANSPORT_ERROR_NONE;
}

/**
 * Accept a connection on the currently listening server socket.
 *
 * @param accept_timeout the number of milliseconds to wait for an incoming connection.  To never timeout,
 * specify a timeout of #BVMD_TIMEOUT_FOREVER.
 * @param handshake_timeout ignored
 *
 * @return #BVMD_TRANSPORT_ERROR_ILLEGAL_STATE if a socket connection is already open, or the server
 * socket is not listening,  #BVMD_TRANSPORT_ERROR_TIMEOUT if an accept did not happen with the
 * timeout period, #BVMD_TRANSPORT_ERROR_IO_ERROR for everything else, or
 * #BVMD_TRANSPORT_ERROR_NONE if all goes well.
 */
static bvmd_transport_error_t dbg_socket_accept(bvm_int32_t accept_timeout, bvm_int32_t handshake_timeout) {

	/* cannot attempt to open if already open */
	if (dbg_sockets_is_open())
		return dbg_socket_transport_error(BVMD_TRANSPORT_ERROR_ILLEGAL_STATE, NULL, BVM_FALSE);

	/* can't accept if we're not listening */
	if (bvmd_server_socket <= 0)
		return dbg_socket_transport_error(BVMD_TRANSPORT_ERROR_ILLEGAL_STATE, NULL, BVM_FALSE);

	/* and attempt the socket accept */
	if ( (bvmd_debugger_socket = bvm_pd_socket_server_accept(bvmd_server_socket, accept_timeout)) < 0)
		return dbg_socket_transport_error(BVMD_TRANSPORT_ERROR_IO_ERROR, SOCKET_ACCEPT_SELECT_ERROR, BVM_FALSE);

	/* finally, delegate the result of this function to the handshake */
    return do_handshake(handshake_timeout);
}

/**
 * Returns the last error for the transport, then NULLs the last error.
 *
 * @param error a pointer to a char pointer.
 *
 * @return #BVMD_TRANSPORT_ERROR_NONE
 */
static bvmd_transport_error_t dbg_socket_get_last_error(char** error) {

	*error = last_error;

	last_error = NULL;

    return BVMD_TRANSPORT_ERROR_NONE;
}

/**
 * Describes the capabilities of this sockets transport.
 *
 * @see #bvmd_transport_t.get_capabilities
 *
 * @param capabilities a handle to a capabilities struct to populate.
 *
 * @return #BVMD_TRANSPORT_ERROR_NONE.
 */
static bvmd_transport_error_t dbg_socket_get_capabilities(bvmd_transport_capabilities_t *capabilities) {

	capabilities->can_attach = BVM_TRUE;
	capabilities->can_listen = BVM_TRUE;
	capabilities->can_timeout_accept = BVM_FALSE;
	capabilities->can_timeout_attach = BVM_FALSE;
	capabilities->can_timeout_handshake = BVM_FALSE;

	return BVMD_TRANSPORT_ERROR_NONE;
}


/**
 * Initialises the transport to the socket functions.
 *
 * @param transport a handle to a transport interface struct to populate.
 */
void bvmd_transport_init(bvmd_transport_t *transport) {
	transport->attach = dbg_socket_attach;
	transport->close = dbg_sockets_close;
	transport->is_data_available = dbg_socket_is_data_available;
	transport->read_bytes = dbg_socket_readbytes;
	transport->write_bytes = dbg_socket_writebytes;
	transport->is_open = dbg_sockets_is_open;
	transport->start_listening = dbg_socket_start_listening;
	transport->stop_listening = dbg_socket_stop_listening;
	transport->accept = dbg_socket_accept;
	transport->get_last_error = dbg_socket_get_last_error;
	transport->get_capabilities = dbg_socket_get_capabilities;
}

#endif
