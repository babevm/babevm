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

  BSD sockets support.  Requires both \c BVM_SOCKETS_ENABLE and \c BVM_BSD_SOCKETS_ENABLE
  be defined as 1.  Contains code to deal with some small Windows differences (which
  require \c BVM_PLATFORM_WINDOWS to be defined).

  @author Greg McCreath
  @since 0.0.10

*/

#include "../../../h/bvm.h"

#if ( (BVM_SOCKETS_ENABLE) && (BVM_BSD_SOCKETS_ENABLE) )

#include <ctype.h>

#ifdef BVM_PLATFORM_WINDOWS

#include <winsock2.h>

/* indicates to the linker that the Ws2_32.lib file is needed. */
#pragma comment(lib, "Ws2_32.lib")

WSADATA wsdata;

/* winsock has no socklen_t type, so we make one .. */
typedef int socklen_t;

#endif

#ifdef BVM_PLATFORM_LINUX
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>  /* TODO - this right for gethostbyname ? */

/* linux/unix has no closesocket, so we make one .. */
#define closesocket close

#endif

/**
 * Returns the 32 bit IP address of a given hostname.  Names may be in dotted address notation, or as
 * non-numeric host names.
 *
 * @param hostname a given host name.
 * @return a 32 bit ip address, or #BVM_SOCKET_ERROR is the host name is invalid or the IP address
 * cannot be determined at this time.
 */
bvm_int32_t bvm_pd_socket_get_ip(const char *hostname) {

	bvm_int32_t result;

	struct hostent* hp;
	struct in_addr addr;

	if (isdigit(hostname[0])) {
		/* nice and easy .. it is a dot notation string */
		result = inet_addr(hostname);
	}

	/* attempt to get the hostent structure for the given address.  An error, or no addresses will be considered and
	 * error. */
	else if ( ((hp = gethostbyname(hostname)) == NULL) ||
			   (hp->h_addr_list == NULL) ||
			   (hp->h_addr_list[0] == NULL) ) {
		result = BVM_SOCKET_ERROR;
	} else {
		/* grab for the first address */
		memcpy(&addr.s_addr, *(hp->h_addr_list), sizeof (addr.s_addr));
		result = addr.s_addr;
	}
	return result;
}

/**
 * Open a socket connection to a given host and port.
 *
 * @param name a host name to connect to.
 * @param a port to connect to at the host.
 *
 * @return non-negative socket descriptor if OK, #BVM_SOCKET_ERROR on error.
 */
bvm_int32_t bvm_pd_socket_open(const char *hostname, bvm_int32_t port) {

	struct sockaddr_in addr_in;
	int optval = 1;

	/* get a configured socket */
	bvm_int32_t ip;
	bvm_int32_t fd = socket(AF_INET,SOCK_STREAM,0);

	if ( (fd == BVM_SOCKET_ERROR) ||
		 ( (ip = bvm_pd_socket_get_ip(hostname)) == BVM_SOCKET_ERROR ) )
		return BVM_SOCKET_ERROR;

	/* get the ip address associated with the given host name.  If not a valid hostname, error. */
	/*ip = bvm_pd_socket_get_ip(hostname);
	if (ip < 0)
		return BVM_SOCKET_ERROR; */

	memset(&addr_in, 0, sizeof(addr_in));
	addr_in.sin_family = AF_INET;
	addr_in.sin_port = htons(port);
	addr_in.sin_addr.s_addr = ip;

	if (connect(fd, (struct sockaddr *) &addr_in , sizeof(addr_in)) < 0) {
		return BVM_SOCKET_ERROR;
	}

	/* disable the nagle algorithm (set nodelay to true) - this makes a **BIG** difference when debugging
	 * across a network.  On Winos it seems to make no difference when using localhost - but when the
	 * VM is remote - wow, this needs to be one - without it, stuff is almost unusable. */
	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *)&optval, sizeof(optval));

	return fd;
}

/**
 * Performs a blocking read of bytes from a socket.  Function does not return until the given number
 * of bytes has been read, or an error occurs.
 *
 * @param fd the socket to read from.
 * @param buf the address to read data into.
 * @param len the number of bytes to read.
 *
 * @return the number of bytes read, If the connection has been gracefully closed, and all data
 * received, the return value is 0. Otherwise, a value of BVM_SOCKET_ERROR is returned.
 */
bvm_int32_t bvm_pd_socket_read(bvm_int32_t fd, char *buf, bvm_int32_t len) {

	bvm_int32_t nbytes;
	bvm_int32_t nleft = len;
	char *ptr = buf; /* a moving pointer into the destination buffer */

	do {

		/* perform a blocking read for the given number of bytes */
		nbytes = recv(fd,ptr,nleft,0);

		/* zero or negative bytes means socket was closed */
		if (nbytes <= 0 )
			return BVM_SOCKET_ERROR; /* "Connection closed" */

		ptr += nbytes;
		nleft -= nbytes;

	} while (nleft > 0);

	return len;
}

/**
 * Reports if data is waiting to be read on the socket.  The socket interrogation will timeout
 * after \c timeout milliseconds.  To never timeout, specify a timeout
 * of #BVM_TIMEOUT_FOREVER.
 *
 * @param fd the socket to test
 * @param timeout a timeout value in milliseconds for the length of time to test.
 *
 * @return the number of bytes waiting to be read from the socket, or #BVM_SOCKET_ERROR for
 * any error.
 */
bvm_int32_t bvm_pd_socket_available(bvm_int32_t fd, bvm_int32_t timeout) {

	fd_set rset;
	struct timeval tv = {0,0};  /* default in case of BVM_TIMEOUT_FOREVER timeout value */
	struct timeval *tvp = NULL; /* for a default, will mean 'wait forever' */
	bvm_int32_t result;

	FD_ZERO(&rset);
	FD_SET((unsigned int) fd, &rset);

	/* if zero timeout, specify a small number - for sockets, zero actually
	 * means 'wait forever'.   A small number gives us 'close to zero'. */
	if (timeout == 0) timeout = 10;

	/* if greater than zero timeout */
	if (timeout > 0) {
		tv.tv_sec  = timeout / 1000;
		tv.tv_usec = (timeout % 1000) * 1000;
		tvp = &tv;
	}

	/* note: a negative timeout value gets the default value of 'tv' */

	/* check the socket for data and get the number of bytes available */
	result = select(fd+1, &rset, NULL, NULL, tvp);

	if (result < 0)
		return BVM_SOCKET_ERROR;

	return result;
}

/**
 * Perform a blocking write of data to a socket.  Function does not return until all data has been
 * written to the socket.
 *
 * @param fd the socket to write to
 * @param buf the source address to write data from
 * @param len the number of byte to write
 *
 * @return number of bytes written (which should always be the same as \c len), or
 * #BVM_SOCKET_ERROR for any error.
 */
bvm_int32_t bvm_pd_socket_write(bvm_int32_t fd, const char *src, bvm_int32_t len) {
	bvm_int32_t result = send(fd, src, len, 0);
	return (result < 0) ? BVM_SOCKET_ERROR : result;
}

/**
 * Close a socket.
 *
 * @param the socket to close
 * @return 0 (zero) on success, or #BVM_SOCKET_ERROR for any error.
 */
bvm_int32_t bvm_pd_socket_close(bvm_int32_t fd) {
	shutdown(fd, 2);
	closesocket(fd);
	return BVM_SOCKET_OK;
}

/**
 * Open a server sockets and listens on the given port for connections.
 *
 * @param port the port to listen on.
 * @return 0 (zero) on success, or #BVM_SOCKET_ERROR for any error.
 */
bvm_int32_t bvm_pd_socket_server_open(bvm_int32_t port) {

	struct sockaddr_in addr_in;
	bvm_int32_t fd;
	int optval = 1;

	/* get the server socket */
	fd = socket(AF_INET,SOCK_STREAM,0);
	if (fd < 0) {
		return BVM_SOCKET_ERROR; /* BVMD_TRANSPORT_ERROR_IO_ERROR/SOCKET_INIT_ERROR */
	}

	memset(&addr_in, 0, sizeof(addr_in));
	addr_in.sin_family = AF_INET;
	addr_in.sin_addr.s_addr = htonl(INADDR_ANY);
	addr_in.sin_port = htons(port);

	/* set the socket to re-use addresses */
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *)&optval, sizeof(optval));

	/* bind the socket */
    if (bind(fd, (struct sockaddr *)&addr_in, sizeof(addr_in)) < 0) {
    	bvm_pd_socket_server_close(fd);
		return BVM_SOCKET_ERROR; /* BVMD_TRANSPORT_ERROR_IO_ERROR/SOCKET_BIND_ERROR */
    }

    /* and now actually start listening */
    if (listen(fd, 1) < 0) {
    	bvm_pd_socket_server_close(fd);
		return BVM_SOCKET_ERROR; /* BVMD_TRANSPORT_ERROR_IO_ERROR/SOCKET_LISTEN_ERROR */
    }

	return fd;
}

/**
 * Accept a connection on the listening server socket.
 *
 * @param fd the listening server socket.
 * @param timeout the time, in milliseconds, to wait for a connection.  A timeout
 * of #BVM_TIMEOUT_FOREVER must block awaiting a connection.
 *
 * @return the accepted socket connection, or #BVM_SOCKET_ERROR for any error.
 */
bvm_int32_t bvm_pd_socket_server_accept(bvm_int32_t serverfd, bvm_int32_t timeout) {

    struct sockaddr_in addr_in;
    int addr_len = sizeof(addr_in);
    fd_set rset, wset, eset;
	struct timeval tv = {0,0};  /* default for negative (BVMD_TIMEOUT_FOREVER) value */
	struct timeval *tvp = NULL; /* for a default, will mean 'wait forever' */
	bvm_int32_t fd = BVM_SOCKET_ERROR;
	int optval = 1;

	/* if zero timeout, specify a small number - for sockets, zero actually
	 * means 'wait forever'.   A small number gives us 'close to zero'. */
	if (timeout == 0) timeout = 10;

	/* if we have a timeout value, convert it to milliseconds */
	if (timeout > 0) {
		tv.tv_sec  = timeout / 1000;
		tv.tv_usec = (timeout % 1000) * 1000;
		tvp = &tv;
	}

    FD_ZERO(&rset);
    FD_ZERO(&wset);
    FD_ZERO(&eset);
    FD_SET((unsigned int)serverfd, &rset);

	memset(&addr_in, 0, sizeof(addr_in));

    if (select(serverfd+1, &rset, &wset, &eset, tvp) <= 0) {
    	bvm_pd_socket_server_close(serverfd);
    	return BVM_SOCKET_ERROR; /* BVMD_TRANSPORT_ERROR_IO_ERROR/SOCKET_ACCEPT_SELECT_ERROR */
    }

    if (FD_ISSET(serverfd, &rset)) {

    	fd = accept(serverfd, (struct sockaddr *)&addr_in, (socklen_t *) &addr_len);
        if (fd < 0) {
        	return BVM_SOCKET_ERROR; /* BVMD_TRANSPORT_ERROR_IO_ERROR/SOCKET_ACCEPT_FAILURE_ERROR */
        }

		/* disable the nagle algorithm (set nodelay to true) - this makes a **BIG** difference when debugging
		 * across a network.  On Winos it seems to make no difference when using localhost - but when the
		 * VM is remote - wow, this needs to be one - without it, stuff is almost unusable. */
		setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *)&optval, sizeof(optval));

    } else {
    	return BVM_SOCKET_ERROR; /* BVMD_TRANSPORT_ERROR_TIMEOUT/SOCKET_ACCEPT_FAILURE_ERROR */
    }

    return fd;
}

/**
 * Closes a server listening socket.
 *
 * @param fd the listening server socket to close.
 * @return 0 (zero) on success, or #BVM_SOCKET_ERROR for any error.
 */
bvm_int32_t bvm_pd_socket_server_close(bvm_int32_t fd) {
	bvm_pd_socket_close(fd);
	return BVM_SOCKET_OK;
}

/**
 * Initialise the OS socket system.
 */
bvm_int32_t bvm_pd_socket_init() {

#ifdef BVM_PLATFORM_WINDOWS
	/* initialise Winsock IP */
	if (WSAStartup(0x101,&wsdata) != 0) {
		return BVM_SOCKET_ERROR; /*BVMD_TRANSPORT_ERROR_INTERNAL/SOCKET_INIT_ERROR*/
	}
#endif

	return BVM_SOCKET_OK;
}

/**
 * Shutdown the OS socket systems
 */
void bvm_pd_socket_finalise() {

#ifdef BVM_PLATFORM_WINDOWS
	WSACleanup();
#endif
}

/**
 * Returns the last error of the underlying socket impl.  This is a one call operation - after return
 * an error code this function should return zero until the next time a error occurs.
 *
 * @return the system socket code since the last call.
 */
bvm_int32_t bvm_pd_socket_error() {
  return BVM_SOCKET_OK;
  /* TODO - is this necessary ? */
}

#endif /* BVM_SOCKETS_ENABLE */
