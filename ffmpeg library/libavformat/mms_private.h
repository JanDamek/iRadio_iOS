/*
 * Native MMS client
 * Copyright (c) 2006,2007 Ryan Martell
 * Copyright (c) 2007 Björn Axelsson
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/** @file mms_private.h
 * Private data structures and definitions for the MMS protocols.
 * @author Ryan Martell
 * @author Björn Axelsson
 */
#ifndef FF_MMS_PRIVATE_H
#define FF_MMS_PRIVATE_H

#include "avformat.h"
#include "asf.h"

#define MMS_DEBUG_LEVEL 2
#define MMS_MAXIMUM_PACKET_LENGTH 512
#define MMS_KILO                  1024

/** Client to server packet types. */
typedef enum {
    CS_PACKET_INITIAL_TYPE= 0x01,
    CS_PACKET_PROTOCOL_SELECT_TYPE= 0x02,
    CS_PACKET_MEDIA_FILE_REQUEST_TYPE= 0x05,
    CS_PACKET_START_FROM_PACKET_ID_TYPE= 0x07,
    CS_PACKET_STREAM_PAUSE_TYPE= 0x09, // tcp left open, but data stopped.
    CS_PACKET_STREAM_CLOSE_TYPE= 0x0d,
    CS_PACKET_MEDIA_HEADER_REQUEST_TYPE= 0x15,
    CS_PACKET_TIMING_DATA_REQUEST_TYPE= 0x18,
    CS_PACKET_USER_PASSWORD_TYPE= 0x1a,
    CS_PACKET_KEEPALIVE_TYPE= 0x1b,
    CS_PACKET_STREAM_ID_REQUEST_TYPE= 0x33,
} MMSCSPacketType;

/** Server to client packet types. */
typedef enum {
    /** Control packets. */
    /*@{*/
    SC_PACKET_CLIENT_ACCEPTED= 0x01,
    SC_PACKET_PROTOCOL_ACCEPTED_TYPE= 0x02,
    SC_PACKET_PROTOCOL_FAILED_TYPE= 0x03,
    SC_PACKET_MEDIA_PACKET_FOLLOWS_TYPE= 0x05,
    SC_PACKET_MEDIA_FILE_DETAILS_TYPE= 0x06,
    SC_PACKET_HEADER_REQUEST_ACCEPTED_TYPE= 0x11,
    SC_PACKET_TIMING_TEST_REPLY_TYPE= 0x15,
    SC_PACKET_PASSWORD_REQUIRED_TYPE= 0x1a,
    SC_PACKET_KEEPALIVE_TYPE= 0x1b,
    SC_PACKET_STREAM_STOPPED_TYPE= 0x1e, // mmst, mmsh
    SC_PACKET_STREAM_CHANGING_TYPE= 0x20,
    SC_PACKET_STREAM_ID_ACCEPTED_TYPE= 0x21,
    /*@}*/

    /** Pseudo packets. */
    /*@{*/
    SC_PACKET_TYPE_CANCEL = -1, // mmst
    SC_PACKET_TYPE_NO_DATA = -2, // mmst
    SC_PACKET_HTTP_CONTROL_ACKNOWLEDGE = -3, // mmsh
    /*@}*/

    /** Data packets. */
    /*@{*/
    SC_PACKET_ASF_HEADER_TYPE= 0x81, // mmst, mmsh
    SC_PACKET_ASF_MEDIA_TYPE= 0x82, // mmst, mmsh
    /*@}*/
} MMSSCPacketType;

/** State machine states. */
typedef enum {
    AWAITING_SC_PACKET_CLIENT_ACCEPTED= 0,
    AWAITING_SC_PACKET_TIMING_TEST_REPLY_TYPE,
    AWAITING_CS_PACKET_PROTOCOL_ACCEPTANCE,
    AWAITING_PASSWORD_QUERY_OR_MEDIA_FILE,
    AWAITING_PACKET_HEADER_REQUEST_ACCEPTED_TYPE,
    AWAITING_STREAM_ID_ACCEPTANCE,
    AWAITING_STREAM_START_PACKET,
    AWAITING_ASF_HEADER,
    ASF_HEADER_DONE,
    AWAITING_PAUSE_ACKNOWLEDGE,
    AWAITING_HTTP_PAUSE_CONTROL_ACKNOWLEDGE,
    STREAMING,
    STREAM_DONE,
    STATE_ERROR,
    STREAM_PAUSED,
    USER_CANCELLED
} MMSState;

struct MMSProtocol;
typedef struct MMSProtocol MMSProtocol;

/** Private MMS data. */
typedef struct {
    char local_guid[37]; ///< My randomly generated GUID.
    uint32_t local_ip_address; ///< Not ipv6 compatible, but neither is the protocol (sent, but not correct).
    int local_port; ///< My local port (sent but not correct).
    int sequence_number; ///< Outgoing packet sequence number.
    MMSState state; ///< Packet state machine current state.
    char path[256]; ///< Path of the resource being asked for.
    char host[128]; ///< Host of the resources.
    int port; ///< Port of the resource.

    URLContext *mms_hd; ///< TCP connection handle
    ByteIOContext incoming_io_buffer; ///< Incoming data on the socket

    /** Buffer for outgoing packets. */
    /*@{*/
    ByteIOContext outgoing_packet_data; ///< Outgoing packet stream
    uint8_t outgoing_packet_buffer[MMS_MAXIMUM_PACKET_LENGTH]; ///< Outgoing packet data
    /*@}*/

    /** Buffer for incoming control packets. */
    /*@{*/
    uint8_t incoming_buffer[8*MMS_KILO]; ///< Incoming buffer location.
    int incoming_buffer_length; ///< Incoming buffer length.
    /*@}*/

    /** Buffer for incoming media/header packets. */
    /*@{*/
    uint8_t media_packet_incoming_buffer[8*MMS_KILO]; ///< Either a header or media packet.
    uint8_t *media_packet_read_ptr; ///< Pointer for partial reads.
    int media_packet_buffer_length; ///< Buffer length.
    int media_packet_seek_offset;   ///< Additional offset into packet from seek.
    /*@}*/

    int incoming_packet_seq; ///< Incoming packet sequence number.
    int incoming_flags; ///< Incoming packet flags.

    int packet_id; ///< Identifier for packets in the current stream, incremented on stops, etc.
    unsigned int header_packet_id; ///< The header packet id (default is 2, can be reset)

    MMSProtocol *protocol; ///< Function pointers for the subprotocol (mmst/mmsh).
    int seekable; ///< This tells you if the stream is seekable.

    int http_client_id; ///< HTTP's client id.
    int http_play_rate; ///< Rate of playback (1 for normal, 5 or -5 for ffwd/rewind)

    /** Internal handling of the ASF header */
    /*@{*/
    uint8_t *asf_header; ///< Stored ASF header, for seeking into it and internal parsing.
    int asf_header_size; ///< Size of stored ASF header.
    int asf_header_read_pos; ///< Current read position in header. See read_packet().
    int header_parsed; ///< The header has been received and parsed.
    AVFormatContext private_av_format_ctx; ///< Private parsed header data (generic).
    ASFContext      asf_context;           ///< Private parsed header data (ASF-specific).
    AVFormatContext *av_format_ctx; ///< Optional external format context (for stream selection).
    /*@}*/

    int pause_resume_seq; ///< Last packet returned by mms_read. Useful for resuming pause.
} MMSContext;

/** Function table for subprotocols. */
struct MMSProtocol {
    int default_port;
    int (*packet_state_machine)(MMSContext *mms, MMSSCPacketType packet_type);
    int (*send_startup_message)(MMSContext *mms);
    int (*send_shutdown_message)(MMSContext *mms);
    int (*play_from)(MMSContext *mms, int rate, int64_t byte_offset, int64_t timestamp);
    int (*pause)(MMSContext *mms);
    int (*get_server_response)(MMSContext *mms);
};

extern MMSProtocol mmst_mmsprotocol, mmsh_mmsprotocol;

/** Shared utility functions for subprotocols. */
/*@{*/
MMSSCPacketType ff_mms_packet_state_machine(MMSContext *mms);
void ff_mms_set_state(MMSContext *mms, int new_state);
int ff_mms_open_connection(MMSContext *mms);
int ff_mms_stream_selection_code(AVStream *st);
/*@}*/
#endif
