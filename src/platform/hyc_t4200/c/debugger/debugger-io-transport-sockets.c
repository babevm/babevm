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

#ifdef BVM_PLATFORM_HYCT4200

#include "../../../../src/h/bvm.h"

#if BVM_DEBUGGER_ENABLE

#include <hycddl.h>

/**
  @file

  Sockets transport implementation for #bvmd_transport_t.

  @author Greg McCreath
  @since 0.0.10

*/

typedef int socklen_t;

static int bvmd_debugger_socket = 0;
static int bvmd_server_socket = 0;

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
 * @return BVM_TRUE if the connection to a debugger is opne, BVM_FALSE otherwise.
 */
static bvm_bool_t bvmd_sockets_is_open() {
	return (bvmd_debugger_socket != 0);
}

/**
 * Closes the current connection to a debugger.
 *
 * @return #BVMD_TRANSPORT_ERROR_NONE
 */
static bvmd_transport_error_t bvmd_sockets_close() {

	if (bvmd_debugger_socket != 0) {
		NU_Shutdown(bvmd_debugger_socket, SHUT_RDWR);
		NU_Close_Socket(bvmd_debugger_socket);
		bvmd_debugger_socket = 0;
	}

    // TODO, if cleaning up, do it here.

	return BVMD_TRANSPORT_ERROR_NONE;
}

/**
 * Stops a server socket listening if it is doing so.
 *
 * @return #BVMD_TRANSPORT_ERROR_NONE
 */
static bvmd_transport_error_t bvmd_socket_stop_listening() {

	if (bvmd_server_socket > 0) {
		NU_Shutdown(bvmd_server_socket, SHUT_RDWR);
		NU_Close_Socket(bvmd_server_socket);
		bvmd_server_socket = 0;
	}

	return BVMD_TRANSPORT_ERROR_NONE;
}

/**
 * Error handling for the sockets transport.
 *
 * @param error the error code
 * @param message a message to set the 'last error message' to.
 * @param do_close if BVM_TRUE, the current debug connection is shut down, and the server socket stopped listening.
 *
 * @return the error code passed in as the \c error param.
 */
static bvmd_transport_error_t bvmd_socket_transport_error(bvmd_transport_error_t error, char *message, bvm_bool_t do_close) {
	last_error = message;
	if (do_close) {
		bvmd_sockets_close();
		bvmd_socket_stop_listening();
	}
	return error;
}

/**
 * Write bytes to the debugger socket connection.
 *
 * @param src the source of data to write
 * @param size the size of data to write
 *
 * @return BVMD_TRANSPORT_ERROR_ILLEGAL_STATE is the connection is not open, BVMD_TRANSPORT_ERROR_IO_ERROR if an
 * error occurs, or BVMD_TRANSPORT_ERROR_NONE if all is okay.
 */
static bvmd_transport_error_t bvmd_socket_writebytes(const void *src, size_t size) {

	int nbytes = 0;

	/* cannot attempt a read if there is no connection */
	if (!bvmd_sockets_is_open())
		return bvmd_socket_transport_error(BVMD_TRANSPORT_ERROR_ILLEGAL_STATE, NULL, BVM_FALSE);

	nbytes = NU_Send(bvmd_debugger_socket, src, size, 0);

	return (nbytes < 0) ? bvmd_socket_transport_error(BVMD_TRANSPORT_ERROR_IO_ERROR, NULL, BVM_TRUE) : BVMD_TRANSPORT_ERROR_NONE;
}

/**
 * Read bytes from the debugger socket connection.  This is a blocking read, and will not return until the
 * given number of bytes has been read or an error occurs.
 *
 * @param dst where to put the read bytes
 * @param size how many bytes to read.
 *
 * @return #BVMD_TRANSPORT_ERROR_ILLEGAL_STATE is the socket is not open, BVMD_TRANSPORT_ERROR_IO_ERROR if something
 * bad happened, or #BVMD_TRANSPORT_ERROR_NONE if all is good.
 */
static bvmd_transport_error_t bvmd_socket_readbytes(void *dst, size_t size) {

	int nbytes;
	int tbytes = 0;
	int nleft = size;
	char *ptr = dst; /* a moving pointer into the destination buffer */

	if (nleft == 0) return BVMD_TRANSPORT_ERROR_NONE;

	if (!bvmd_sockets_is_open())
		return bvmd_socket_transport_error(BVMD_TRANSPORT_ERROR_ILLEGAL_STATE, NULL, BVM_FALSE);

	do {

		/* perform a blocking read for the given number of bytes */
		nbytes = NU_Recv(bvmd_debugger_socket,ptr,nleft,0);

		/* zero bytes means socket was closed */
		if (nbytes <= 0 )
			return bvmd_socket_transport_error(BVMD_TRANSPORT_ERROR_IO_ERROR, "Connection closed", BVM_TRUE);

		ptr += nbytes;
		nleft -= nbytes;
		tbytes += nbytes;

	} while (nleft > 0);

	return BVMD_TRANSPORT_ERROR_NONE;
}

/**
 * Performs the JDWP handshake between VM and debugger by exchanging the "JDWP-Handshake" chars.
 *
 * @return #BVMD_TRANSPORT_ERROR_IO_ERROR if any error occurs or #BVMD_TRANSPORT_ERROR_NONE if all okay.
 */
static bvmd_transport_error_t do_handshake() {

	char handshake[BVMD_JDWP_HANDSHAKE_SIZE + 1];

    /* read the "JDWP-Handshake" */
	if (bvmd_socket_readbytes(handshake, BVMD_JDWP_HANDSHAKE_SIZE)) {
		return bvmd_socket_transport_error(BVMD_TRANSPORT_ERROR_IO_ERROR, HANDSHAKE_READ_ERROR, BVM_TRUE);
	}

	/* NULL terminate it */
	handshake[BVMD_JDWP_HANDSHAKE_SIZE] = '\0';

	/* make sure it *is* "JDWP-Handshake" */
	if (strcmp(handshake, "JDWP-Handshake") != 0) {
		return bvmd_socket_transport_error(BVMD_TRANSPORT_ERROR_IO_ERROR, HANDSHAKE_INVALID_ERROR, BVM_TRUE);
	}

	/* echo the handshake chars back */
	if (bvmd_socket_writebytes(handshake, BVMD_JDWP_HANDSHAKE_SIZE)) {
		return bvmd_socket_transport_error(BVMD_TRANSPORT_ERROR_IO_ERROR, HANDSHAKE_WRITE_ERROR, BVM_TRUE);
	}

	return BVMD_TRANSPORT_ERROR_NONE;
}

/**
 * Return the capabilities of this sockets transport.
 *
 * @param capabilities a handle to a capabilities struct to populate.
 *
 * @return #BVMD_TRANSPORT_ERROR_NONE.
 */
bvmd_transport_error_t bvmd_socket_get_capabilities(bvmd_transport_capabilities_t *capabilities) {

	capabilities->can_attach = BVM_TRUE;
	capabilities->can_listen = BVM_TRUE;
	capabilities->can_timeout_accept = BVM_FALSE;
	capabilities->can_timeout_attach = BVM_FALSE;
	capabilities->can_timeout_handshake = BVM_FALSE;

	return BVMD_TRANSPORT_ERROR_NONE;
}

/**
 * Establish a connection to the given address.  The address must be of the format "host:port".  If the
 * The address is \NULL, a host of "127.0.0.1" and port of
 * #BVMD_SOCKET_PORT is assumed.
 *
 * The timeout values are ignored.
 *
 * @param address the address to connected to, or \c NULL for default.
 * @param attach_timeout ignored.
 * @param handshale_timeout ignored.
 *
 * @return #BVMD_TRANSPORT_ERROR_ILLEGAL_ARGUMENT the format of the address is invalid, #BVMD_TRANSPORT_ERROR_ILLEGAL_STATE if
 * a connection is already open,  #BVMD_TRANSPORT_ERROR_IO_ERROR if any IO error occurs, or #BVMD_TRANSPORT_ERROR_NONE if
 * all is good.
 */
static bvmd_transport_error_t bvmd_socket_attach(const char* address, int attach_timeout, int handshake_timeout) {

	struct addr_struct bvmd_sockaddr_in;
	const char *host = "127.0.0.1";
	int port = BVM_DEBUGGER_SOCKET_PORT;
	char *colonpos;

	if (address != NULL) {

		colonpos = strchr(address, ':');
		if (colonpos == NULL) {
			return bvmd_socket_transport_error(BVMD_TRANSPORT_ERROR_ILLEGAL_ARGUMENT, ADDRESS_INVALID, BVM_FALSE);
		}

		//*colonpos = '\0';

		port = atoi(colonpos+1);

		/* at this point we are not supporting host-name lookup.  But we'll test for localhost, because
		 * that is what most debuggers will likely send as a connect host */
		if (strcmp(address,"localhost") != 0) host = address;

	}

	/* cannot attempt to open if already open */
	if (bvmd_sockets_is_open())
		return bvmd_socket_transport_error(BVMD_TRANSPORT_ERROR_ILLEGAL_STATE, SOCKET_ALREADY_OPEN_ERROR, BVM_FALSE);

	/* get a configured socket */
	bvmd_debugger_socket = NU_Socket(NU_FAMILY_IP, NU_TYPE_STREAM, NU_NONE);

    memset(&bvmd_sockaddr_in, 0, sizeof(bvmd_sockaddr_in));
	bvmd_sockaddr_in.family = NU_FAMILY_IP;
	bvmd_sockaddr_in.port = port;

    bvmd_sockaddr_in.id.is_ip_addrs[0] = (unsigned char) 192;
    bvmd_sockaddr_in.id.is_ip_addrs[1] = (unsigned char) 168;
    bvmd_sockaddr_in.id.is_ip_addrs[2] = (unsigned char) 130;
    bvmd_sockaddr_in.id.is_ip_addrs[3] = (unsigned char) 102;

	if (NU_Connect(bvmd_debugger_socket, &bvmd_sockaddr_in , 0) < 0) {
		return bvmd_socket_transport_error(BVMD_TRANSPORT_ERROR_IO_ERROR, SOCKET_CONNECT_ERROR, BVM_TRUE);
	}

    {
    	/* set nodelay to true - unsure if this is necessary */
		int optval = 1;
		NU_Setsockopt(bvmd_debugger_socket, IPPROTO_TCP, TCP_NODELAY, (void *)&optval, sizeof(optval));
    }


    return do_handshake();
}

/**
 * Reports if data is waiting to be read on the socket.  The socket interrogation will timeout
 * after \c timeout milliseconds.  To never timeout, specify a timeout of #BVMD_TIMEOUT_FOREVER.
 *
 * @param timeout the milliseconds to wait on the socket.
 *
 * @return BVM_TRUE is data is available for reading, BVM_FALSE otherwise.
 */
static bvm_bool_t bvmd_socket_is_data_available(int timeout) {
	FD_SET rset;

    if (!bvmd_sockets_is_open()) return BVM_FALSE;

	NU_FD_Init(&rset);
	NU_FD_Set(bvmd_debugger_socket, &rset);

    /* if zero timeout, specify a small number - for sockets, zero actually
     * means 'wait forever'.   A small number gives us 'close to zero'. */
    if (timeout == 0) timeout = 10;

    /* if negative make ita large number   */
    if (timeout < 0) timeout = 0x7FFFFFFE;

	return (NU_Select(bvmd_debugger_socket+1, &rset, NULL, NULL, (UNSIGNED) timeout)) == OS_SUCCESS ? BVM_TRUE : BVM_FALSE;
}

/**
 * Start listening on a socket at the given address.  The address is to be a null-terminated char
 * string of the socket port to listen on.
 *
 * @param address the port to listen on.
 * @param actual_address - ignored and no value is returned in this impl.
 *
 * @return #BVMD_TRANSPORT_ERROR_ILLEGAL_STATE if the socket is already open or already
 * listening, #BVMD_TRANSPORT_ERROR_INTERNAL is the socket subsystem cannot be initialised,
 * #BVMD_TRANSPORT_ERROR_IO_ERROR for anything else, or BVMD_TRANSPORT_ERROR_NONE if all goes well.
 */
static bvmd_transport_error_t bvmd_socket_start_listening(const char* address,  char** actual_address) {

	struct addr_struct bvmd_sockaddr_in;
	int port = BVM_DEBUGGER_SOCKET_PORT;

	/* cannot attempt to open if already open */
	if (bvmd_sockets_is_open())
		return bvmd_socket_transport_error(BVMD_TRANSPORT_ERROR_ILLEGAL_STATE, NULL, BVM_FALSE);

	/* can listen if it is already listening */
	if (bvmd_server_socket > 0)
		return bvmd_socket_transport_error(BVMD_TRANSPORT_ERROR_ILLEGAL_STATE, NULL, BVM_FALSE);

    // TODO startup sockets if need be.

	/* get the server socket */
	bvmd_server_socket = NU_Socket(NU_FAMILY_IP, NU_TYPE_STREAM, NU_NONE);
	if (bvmd_server_socket < 0) {
		return bvmd_socket_transport_error(BVMD_TRANSPORT_ERROR_IO_ERROR, SOCKET_INIT_ERROR, BVM_TRUE);
	}

	/* use the given address - if none was provided, the default BVMD_SOCKET_PORT is used */
	if (address != NULL) port = atoi(address);

    memset(&bvmd_sockaddr_in, 0, sizeof(bvmd_sockaddr_in));
	bvmd_sockaddr_in.family = NU_FAMILY_IP;
	/*bvmd_sockaddr_in.id = IP_ADDR_ANY;*/
	bvmd_sockaddr_in.port = port;

    bvmd_sockaddr_in.id.is_ip_addrs[0] = (unsigned char) 192;
    bvmd_sockaddr_in.id.is_ip_addrs[1] = (unsigned char) 168;
    bvmd_sockaddr_in.id.is_ip_addrs[2] = (unsigned char) 130;
    bvmd_sockaddr_in.id.is_ip_addrs[3] = (unsigned char) 110;

	{
		/* set the socket to re-use addresses */
		int optval = 1;
		NU_Setsockopt(bvmd_server_socket, SOL_SOCKET, SO_REUSEADDR, (void *)&optval, sizeof(optval));
	}

	/* bding the socket */
    if (NU_Bind(bvmd_server_socket, &bvmd_sockaddr_in, 0) < 0) {
    	bvmd_socket_stop_listening();
		return bvmd_socket_transport_error(BVMD_TRANSPORT_ERROR_IO_ERROR, SOCKET_BIND_ERROR, BVM_FALSE);
    }

    /* and now actually start listening */
    if (NU_Listen(bvmd_server_socket, 1) < 0) {
    	bvmd_socket_stop_listening();
		return bvmd_socket_transport_error(BVMD_TRANSPORT_ERROR_IO_ERROR, SOCKET_LISTEN_ERROR, BVM_FALSE);
    }

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
static bvmd_transport_error_t bvmd_socket_accept(int accept_timeout, int handshake_timeout) {

    struct addr_struct addr;
    int addr_len = sizeof(addr);
    FD_SET rset, wset, eset;

	/* cannot attempt to open if already open */
	if (bvmd_sockets_is_open())
		return bvmd_socket_transport_error(BVMD_TRANSPORT_ERROR_ILLEGAL_STATE, NULL, BVM_FALSE);

	/* cant accept if we're not listening */
	if (bvmd_server_socket <= 0)
		return bvmd_socket_transport_error(BVMD_TRANSPORT_ERROR_ILLEGAL_STATE, NULL, BVM_FALSE);

	/* if zero timeout, specify a small number - for sockets, zero actually
	 * means 'wait forever'.   A small number gives us 'close to zero'. */
	if (accept_timeout == 0) accept_timeout = 10;

	/* if negative make it a large number   */
	if (accept_timeout < 0) accept_timeout = 0x7FFFFFFE;

    NU_FD_Init(&rset);
    NU_FD_Init(&wset);
    NU_FD_Init(&eset);
    NU_FD_Set(bvmd_server_socket, &rset);
    memset(&addr, 0, sizeof(addr));

    if (NU_Select(bvmd_server_socket+1, &rset, &wset, &eset, accept_timeout) != OS_SUCCESS) {
    	bvmd_socket_stop_listening();
		return bvmd_socket_transport_error(BVMD_TRANSPORT_ERROR_IO_ERROR, SOCKET_ACCEPT_SELECT_ERROR, BVM_FALSE);
    }

    if (NU_FD_Check(bvmd_server_socket, &rset)) {

    	bvmd_debugger_socket = NU_Accept(bvmd_server_socket, &addr, 0);
        if (bvmd_debugger_socket < 0) {
    		return bvmd_socket_transport_error(BVMD_TRANSPORT_ERROR_IO_ERROR, SOCKET_ACCEPT_FAILURE_ERROR, BVM_TRUE);
        }

        {
        	/* set nodelay to true - unsure if this is necessary */
			int optval = 1;
			NU_Setsockopt(bvmd_debugger_socket, IPPROTO_TCP, TCP_NODELAY, (void *)&optval, sizeof(optval));
        }

        return do_handshake();

    } else {
		return bvmd_socket_transport_error(BVMD_TRANSPORT_ERROR_TIMEOUT, SOCKET_ACCEPT_FAILURE_ERROR, BVM_TRUE);
    }

    return BVMD_TRANSPORT_ERROR_NONE;
}

/**
 * Returns the last error for the transport, then NULLs the last error.
 *
 * @param error a pointer to a char pointer.
 *
 * @return #BVMD_TRANSPORT_ERROR_NONE
 */
static bvmd_transport_error_t bvmd_socket_get_last_error(char** error) {

	*error = last_error;

	last_error = NULL;

    return BVMD_TRANSPORT_ERROR_NONE;
}


/**
 * Initialises the transport to the socket functions.
 *
 * @param transport a handle to a transport interface struct to populate.
 */
void bvmd_transport_init(bvmd_transport_t *transport) {
	transport->attach = bvmd_socket_attach;
	transport->close = bvmd_sockets_close;
	transport->is_data_available = bvmd_socket_is_data_available;
	transport->read_bytes = bvmd_socket_readbytes;
	transport->write_bytes = bvmd_socket_writebytes;
	transport->is_open = bvmd_sockets_is_open;
	transport->start_listening = bvmd_socket_start_listening;
	transport->stop_listening = bvmd_socket_stop_listening;
	transport->accept = bvmd_socket_accept;
	transport->get_last_error = bvmd_socket_get_last_error;
	transport->get_capabilities = bvmd_socket_get_capabilities;
}

#endif

#endif
