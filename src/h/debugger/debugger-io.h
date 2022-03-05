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

#ifndef BVM_DEBUGGER_IO_H_
#define BVM_DEBUGGER_IO_H_

/**

 @file

 Definitions for JDWP IO.

 @author Greg McCreath
 @since 0.0.10

*/

/**
 * JDWP Packet flags.
 */
enum {
    BVMD_TRANSPORT_FLAGS_NONE	 = 0x0,
    BVMD_TRANSPORT_FLAGS_REPLY	 = 0x80
};

struct _bvmdpacketdatastruct;
typedef struct _bvmdpacketdatastruct bvmd_packetdata_t;

struct _bvmdpacketdatastruct {
    bvm_int32_t length;
    bvmd_packetdata_t *next;
    bvm_uint8_t bytes[BVM_DEBUGGER_PACKETDATA_SIZE];
};

/**
 * JDWP Command packet.
 */
typedef struct {
    bvm_int32_t length;
    bvm_int32_t id;
    bvm_uint8_t flags;
    bvm_uint8_t cmdset;
    bvm_uint8_t cmd;
    bvmd_packetdata_t *packetdata;
} bvmd_cmdpacket_t;

/**
 * JDWP Reply packet.
 */
typedef struct {
    bvm_int32_t length;
    bvm_int32_t id;
    bvm_uint8_t flags;
    bvm_int16_t error_code;
    bvmd_packetdata_t *packetdata;
} bvmd_replypacket_t;

/**
 * Union on command and reply packets struct to provide a single way to deal with both.   Command and
 * reply packets are the same length.
 */
typedef struct _bvmdpacketstruct {
    union {
    	bvmd_cmdpacket_t cmd;
        bvmd_replypacket_t reply;
    } type;
} bvmd_packet_t;


/**
 * A stream structure for reading from and writing to debugger packets.
 */
typedef struct _bvmdpacketstreamstruct {

	/** The JDWP packet this stream is reading or writing to */
	bvmd_packet_t *packet;

	/** The current #bvmd_packetdata_t being read from or written to */
    bvmd_packetdata_t *segment;

    /** the position within the \c segment member where reads/write are taking place */
    bvm_int32_t index;

    /** the number of bytes left in the current segment */
    bvm_int32_t left;

    /** a general error code */
	bvm_int32_t error;

	bvm_bool_t ignore_send;

} bvmd_packetstream_t, bvmd_instream_t, bvmd_outstream_t;

/**
 * A position reference within a debugger stream.
 */
typedef struct _bvmdstreampos {
    bvmd_packetdata_t *segment;
    bvm_int32_t index;
} bvmd_streampos_t;

bvm_thread_obj_t *bvmd_readcheck_threadref(bvmd_instream_t* in, bvmd_outstream_t* out);
bvm_clazz_t *bvmd_read_clazzref(bvmd_instream_t* in, bvmd_outstream_t* out);

bvmd_packetstream_t *bvmd_new_packetstream();
bvmd_packetstream_t *bvmd_new_commandstream();

void bvmd_sendstream(bvmd_packetstream_t* stream);

void dbg_free_packetstream(bvmd_packetstream_t *packetstream);

bvm_method_t *bvmd_readcheck_method(bvmd_instream_t* in, bvmd_outstream_t* out);
bvm_clazz_t *bvmd_readcheck_reftype(bvmd_instream_t* in, bvmd_outstream_t* out);

void bvmd_interact(bvm_int32_t timeout);

bvm_bool_t bvmd_is_session_open();
void bvmd_session_open();
void bvmd_session_close();

void bvmd_io_init();
void bvmd_io_shutdown();


#endif /* BVM_DEBUGGER_IO_H_ */
