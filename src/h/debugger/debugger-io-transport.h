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

#ifndef BVM_DEBUGGER_IO_TRANSPORT_H_
#define BVM_DEBUGGER_IO_TRANSPORT_H_

/**

 @file

 Definitions for JDWP Transport Interface.

 @author Greg McCreath
 @since 0.0.10

*/



/**
 * Transport interface error codes.
 */
typedef enum {
    BVMD_TRANSPORT_ERROR_NONE = 0,
    BVMD_TRANSPORT_ERROR_ILLEGAL_ARGUMENT = 103,
    BVMD_TRANSPORT_ERROR_OUT_OF_MEMORY = 110,
    BVMD_TRANSPORT_ERROR_INTERNAL = 113,
    BVMD_TRANSPORT_ERROR_ILLEGAL_STATE = 201,
    BVMD_TRANSPORT_ERROR_IO_ERROR = 202,
    BVMD_TRANSPORT_ERROR_TIMEOUT = 203,
    BVMD_TRANSPORT_ERROR_MSG_NOT_AVAILABLE = 204
} bvmd_transport_error_t;


/**
 * Transport interface capabilities struct.
 */
typedef struct {
    unsigned int can_timeout_attach     :1;
    unsigned int can_timeout_accept     :1;
    unsigned int can_timeout_handshake  :1;
    unsigned int can_attach             :1;
    unsigned int can_listen             :1;
    unsigned int reserved5              :1;
    unsigned int reserved6              :1;
    unsigned int reserved7              :1;
    unsigned int reserved8              :1;
    unsigned int reserved9              :1;
    unsigned int reserved10             :1;
    unsigned int reserved11             :1;
    unsigned int reserved12             :1;
    unsigned int reserved13             :1;
    unsigned int reserved14				:1;
    unsigned int reserved15				:1;
} bvmd_transport_capabilities_t;


/**
 * Debugger transport interface.  The Babe VM uses a populated instance of this struct to call transport functions
 * for communicating with a debugger.
 */
typedef struct _bvmdtransportstruct {

    /**
     * Describes the capabilities of a transport implementation.
	 *
	 * @param capabilities a handle to a capabilities struct to populate.
	 *
	 * @return #BVMD_TRANSPORT_ERROR_NONE.
     */
    bvmd_transport_error_t (*get_capabilities)(bvmd_transport_capabilities_t *capabilities);

    /**
     * Function pointer for attaching to a remote debugger at a given address.  The meaning of the address is
     * transport dependent.
     *
	 * @param address the address to connected to.  Max 128 chars.
	 * @param attach_timeout suggested timeout for attaching to the debugger.  Implementations may use this
	 * suggestion and follow it is possible, or do what they can.
	 * @param handshake_timeout suggested timeout for the handshake to the debugger. Implementations may use
	 * this suggestion and follow it is possible, or do what they can.
	 *
	 * @return #BVMD_TRANSPORT_ERROR_ILLEGAL_ARGUMENT the format of the address is invalid,
	 * #BVMD_TRANSPORT_ERROR_ILLEGAL_STATE if a connection is already open,  #BVMD_TRANSPORT_ERROR_IO_ERROR if any
	 * IO error occurs, or #BVMD_TRANSPORT_ERROR_NONE if all is good.
     */
    bvmd_transport_error_t (*attach)(const char* address, bvm_int32_t attach_timeout, bvm_int32_t handshake_timeout);

    /**
     * Start Listening for an incoming debugger connection at the given address.  The actual meaning of the
     * address is transport dependent.
	 *
	 * @param address the null-terminated address to listen on.
	 *
	 * @return BVMD_TRANSPORT_ERROR_ILLEGAL_ARGUMENT if the address is not a address,
	 * #BVMD_TRANSPORT_ERROR_ILLEGAL_STATE if the address is already open or already listening,
	 * #BVMD_TRANSPORT_ERROR_IO_ERROR for anything else, or #BVMD_TRANSPORT_ERROR_NONE if all goes well.
     */
    bvmd_transport_error_t (*start_listening)(char *address);

    /**
     * Stops listening for a debugger connection.  If the transport is not currently listening
     * for a connection this function has not effect.
     *
     * @return always #BVMD_TRANSPORT_ERROR_NONE.
     */
    bvmd_transport_error_t (*stop_listening)();

    /**
     * Accepts in incoming debugger connection.
	 *
	 * @param accept_timeout suggested milliseconds to wait for an incoming connection.  To never
	 * timeout, specify a timeout of #BVMD_TIMEOUT_FOREVER.
	 *
	 * @param handshake_timeout suggested timeout milliseconds for debugger handshake.
	 *
	 * @return #BVMD_TRANSPORT_ERROR_ILLEGAL_STATE if a debugger connection is already open, or the
	 * VM is not listening for connections,  #BVMD_TRANSPORT_ERROR_TIMEOUT if an accept did not happen within the
	 * timeout period, #BVMD_TRANSPORT_ERROR_IO_ERROR for everything else, or
	 * #BVMD_TRANSPORT_ERROR_NONE if all goes well.
     */
    bvmd_transport_error_t (*accept)(bvm_int32_t accept_timeout, bvm_int32_t handshake_timeout);

    /**
     * Reports if a connection is currently established with a debugger.
	 *
	 * @return #BVM_TRUE if the connection to a debugger is open, #BVM_FALSE otherwise.
     */
    bvm_bool_t (*is_open)();

    /**
     * Close the connection with a debugger.  If the transport has no open
     * connection this function has no effect.
     *
     * @return always #BVMD_TRANSPORT_ERROR_NONE.
     */
    bvmd_transport_error_t (*close)();

    /**
     * Reports whether data is available to be read from the transport.  The interrogation will timeout
     * after \c timeout milliseconds.  To never timeout, specify a timeout of #BVMD_TIMEOUT_FOREVER.
	 *
	 * @param timeout the milliseconds to wait on the socket.
	 *
	 * @return #BVM_TRUE is data is available for reading, #BVM_FALSE otherwise.
     */
    bvm_bool_t (*is_data_available)(bvm_int32_t timeout);

    /**
     * Perform a blocking read for a given number of bytes from the transport.  This is a blocking read, and will
     * not return until the given number of bytes has been read or an error occurs.
	 *
	 * @param dst where to put the read bytes
	 * @param size how many bytes to read.
	 *
	 * @return #BVMD_TRANSPORT_ERROR_ILLEGAL_STATE is the socket is not open, #BVMD_TRANSPORT_ERROR_IO_ERROR
	 * if something bad happened, or #BVMD_TRANSPORT_ERROR_NONE if all is good.
     */
    bvmd_transport_error_t (*read_bytes)(void *dst, size_t size);

    /**
     * Write a number of bytes to the transport.
	 *
	 * @param src the source of data to write
	 * @param size the size of data to write
	 *
	 * @return #BVMD_TRANSPORT_ERROR_ILLEGAL_STATE if the debugger connection is not open,
	 * #BVMD_TRANSPORT_ERROR_IO_ERROR if an error occurs or not all the bytes could be written. Returns
	 * #BVMD_TRANSPORT_ERROR_NONE if all is okay.
     */
    bvmd_transport_error_t (*write_bytes)(const void *src, size_t size);

    /**
     * Returns a textual description of the last error the occurred in a transport - calling
     * this function will also NULL the last error.
	 *
	 * @param error a pointer to a char pointer.
	 *
	 * @return #BVMD_TRANSPORT_ERROR_NONE
	 */
    bvmd_transport_error_t (*get_last_error)(char** error);

} bvmd_transport_t;

extern bvmd_transport_t bvmd_gl_transport;
extern bvmd_transport_capabilities_t bvmd_gl_transport_capabilities;

void bvmd_transport_init(bvmd_transport_t *transport);

#endif /* BVM_DEBUGGER_IO_TRANSPORT_H_ */
