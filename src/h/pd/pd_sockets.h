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

  Platform-dependent socket functions and definitions.

  @author Greg McCreath
  @since 0.0.10

*/

#if BVM_SOCKETS_ENABLE

#ifndef BVM_PD_SOCKET_H_
#define BVM_PD_SOCKET_H_

#define BVM_SOCKET_OK 0
#define BVM_SOCKET_ERROR -1
#define BVM_SOCKET_CONNECT_ERROR -2

/**
 * Returns the IP address of a given host name.
 *
 * @param hostname a given host name
 *
 * @return the non-negative 4 byte IP address of the given host name or #BVM_SOCKET_ERROR on
 * any error.
 */
bvm_int32_t bvm_pd_socket_get_ip(const char *hostname);

/**
 * Open a socket connection to a given host and port.
 *
 * @param name a host name to connect to.
 * @param a port to connect to at the host.
 *
 * @return non-negative socket descriptor if OK, #BVM_SOCKET_ERROR on error.
 */
bvm_int32_t bvm_pd_socket_open(const char *hostname, bvm_int32_t  port);

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
bvm_int32_t bvm_pd_socket_read(bvm_int32_t fd, char *buf, bvm_int32_t len);

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
bvm_int32_t bvm_pd_socket_available(bvm_int32_t fd, bvm_int32_t timeout);

/**
 * Perform a blocking write of data to a socket.  Function does not return until all data has been
 * written to the socket.
 *
 * @param fd the socket to write to
 * @param buf the source address to write data from
 * @param len the number of byte to write
 *
 * @return 0 (zero), or #BVM_SOCKET_ERROR for any error.
 */
bvm_int32_t bvm_pd_socket_write(bvm_int32_t fd, const char *src, bvm_int32_t len);

/**
 * Close a socket.
 *
 * @param the socket to close
 * @return 0 (zero) on success, or #BVM_SOCKET_ERROR for any error.
 */
bvm_int32_t bvm_pd_socket_close(bvm_int32_t fd);

/**
 * Open a server sockets and listens on the given port for connections.
 *
 * @param port the port to listen on.
 * @return 0 (zero) on success, or #BVM_SOCKET_ERROR for any error.
 */
bvm_int32_t bvm_pd_socket_server_open(bvm_int32_t port);

/**
 * Accept a connection on the listening server socket.
 *
 * @param fd the listening server socket.
 * @param timeout the time, in milliseconds, to wait for a connection.  A timeout
 * of #BVM_TIMEOUT_FOREVER must block awaiting a connection.
 *
 * @return the accepted socket connection, or #BVM_SOCKET_ERROR for any error.
 */
bvm_int32_t bvm_pd_socket_server_accept(bvm_int32_t fd, bvm_int32_t timeout);

/**
 * Closes a server listening socket.
 *
 * @param fd the listening server socket to close.
 * @return 0 (zero) on success, or #BVM_SOCKET_ERROR for any error.
 */
bvm_int32_t bvm_pd_socket_server_close(bvm_int32_t fd);

/**
 * Initialise the OS socket system.  If sockets has already been initialised or does not require
 * initialisation this function has no effect.
 *
 * @return 0 (zero) on success, or #BVM_SOCKET_ERROR for any error.
 */
bvm_int32_t bvm_pd_socket_init();

/**
 * Shutdown OS sockets.  If sockets is already shutdown, or does not require shutting down
 * this function has no effect.
 */
void bvm_pd_socket_finalise();

/**
 * Returns the last error of the underlying socket impl.  This is a one call operation - after return
 * an error code this function should return zero until the next time a error occurs.  Effectively, it returns the
 * last error code since the last time this function was called.
 *
 * @return the system socket code since the last call.
 */
bvm_int32_t bvm_pd_socket_error();

#endif


#endif /* SOCKET_H_ */
