 /* Native MMS client
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

/** @file mms_tcp.c
 * MMS protocol over TCP.
 * @author Ryan Martell
 * @author Björn Axelsson
 */
#include "mms_private.h"
#include "libavutil/intreadwrite.h"

#define DEFAULT_MMS_PORT      1755

/** Debug function that dumps info about a MMST command packet. */
static void print_command (uint8_t *data, int len) {
    // N/A
}

/** Write a utf-16 string in little-endian order.
 * @note This is NOT the same as static int ascii_to_wc (ByteIOContext *pb, uint8_t *b), because ascii_to_wc is big endian, and this is little endian.
 */
static void put_le_utf16(ByteIOContext *pb, char *utf8)
{
    int val;

    while(*utf8) {
        GET_UTF8(val, *utf8++, break;); // goto's suck, but i want to make sure it's terminated.
        put_le16(pb, val);
    }

    put_le16(pb, 0x00);

    return;
}

/** MMST commands */

/** Create MMST command packet header */
static void start_command_packet(MMSContext *mms, MMSCSPacketType packet_type)
{
    ByteIOContext *context= &mms->outgoing_packet_data;

    url_fseek(context, 0, SEEK_SET); // start at the beginning...
    put_le32(context, 1); // start sequence?
    put_le32(context, 0xb00bface);
    put_le32(context, 0); // Length of command until the end of all data  Value is in bytes and starts from after the protocol type bytes
    put_byte(context, 'M'); put_byte(context, 'M'); put_byte(context, 'S'); put_byte(context, ' ');
    put_le32(context, 0);
    put_le32(context, mms->sequence_number++);
    put_le64(context, 0); // timestmamp
    put_le32(context, 0);
    put_le16(context, packet_type);
    put_le16(context, 3); // direction- to server
}

/** Add prefixes to MMST command packet. */
static void insert_command_prefixes(MMSContext *mms,
        uint32_t prefix1, uint32_t prefix2)
{
    ByteIOContext *context= &mms->outgoing_packet_data;

    put_le32(context, prefix1); // first prefix
    put_le32(context, prefix2); // second prefix
}

/** Send a prepared MMST command packet. */
static int send_command_packet(MMSContext *mms)
{
    ByteIOContext *context= &mms->outgoing_packet_data;
    int exact_length= url_ftell(context);
    int first_length= exact_length - 16;
    int len8= (first_length+7)/8;
    int write_result;

    // first adjust the header fields (the lengths)...
    url_fseek(context, 8, SEEK_SET);
    put_le32(context, first_length);
    url_fseek(context, 16, SEEK_SET);
    put_le32(context, len8);
    url_fseek(context, 32, SEEK_SET);
    put_le32(context, len8-2);

    // seek back to the end (may not be necessary...)
    url_fseek(context, exact_length, SEEK_SET);

    // print it out...
    print_command(context->buffer, exact_length);

    // write it out...
    write_result= url_write(mms->mms_hd, context->buffer, exact_length);
    if(write_result != exact_length) {
#if (MMS_DEBUG_LEVEL>0)
        fprintf(stderr, "url_write returned: %d != %d\n", write_result, exact_length);
#endif

        ff_mms_set_state(mms, STATE_ERROR);
        return AVERROR_IO;
    }

    return 0;
}

/** Read incoming MMST media, header or command packet. */
static MMSSCPacketType get_tcp_server_response(MMSContext *mms)
{
    // read the 8 byte header...
    int read_result;
    MMSSCPacketType packet_type= -1;
    int done;

    // use url_fdopen & url_fclose...
    do {
        done= 1; // assume we're going to get a valid packet.
        if((read_result= get_buffer(&mms->incoming_io_buffer, mms->incoming_buffer, 8))==8) {
            // check if we are a command packet...
            if(AV_RL32(mms->incoming_buffer + 4)==0xb00bface) {
                mms->incoming_flags= mms->incoming_buffer[3];
                if((read_result= get_buffer(&mms->incoming_io_buffer, mms->incoming_buffer+8, 4)) == 4) {
                    int length_remaining= AV_RL32(mms->incoming_buffer+8) + 4;

#if (MMS_DEBUG_LEVEL>0)
                    fprintf(stderr, "Length remaining is %d\n", length_remaining);
#endif
                    // FIXME? ** VERIFY LENGTH REMAINING HAS SPACE
                    // read the rest of the packet....
                    read_result = get_buffer(&mms->incoming_io_buffer, mms->incoming_buffer + 12, length_remaining) ;
                    if (read_result == length_remaining) {
                        // we have it all; get the stuff out of it.
                        mms->incoming_buffer_length= length_remaining+12;

                        // get the packet type...
                        packet_type= AV_RL16(mms->incoming_buffer+36);

                        print_command(mms->incoming_buffer, mms->incoming_buffer_length);
                    } else {
#if (MMS_DEBUG_LEVEL>0)
                        // read error...
                        fprintf(stderr, "3 read returned %d!\n", read_result);
#endif
                    }
                } else {
#if (MMS_DEBUG_LEVEL>0)
                    // read error...
                    fprintf(stderr, "2 read returned %d!\n", read_result);
#endif
                }
            } else {
                int length_remaining= (AV_RL16(mms->incoming_buffer + 6) - 8) & 0xffff;
                uint8_t *dst= mms->media_packet_incoming_buffer;
                int packet_id_type;

                assert(mms->media_packet_buffer_length==0); // assert all has been consumed.

                //** VERIFY LENGTH REMAINING HAS SPACE
                // note we cache the first 8 bytes, then fill up the buffer with the others
                mms->incoming_packet_seq        = AV_RL32(mms->incoming_buffer);
                packet_id_type                  = mms->incoming_buffer[4]; // NOTE: THIS IS THE ONE I CAN CHANGE
                mms->incoming_flags             = mms->incoming_buffer[5];
                mms->media_packet_buffer_length = length_remaining;
                mms->media_packet_read_ptr      = mms->media_packet_incoming_buffer;

                if(mms->media_packet_buffer_length>=sizeof(mms->media_packet_incoming_buffer)) {
                    fprintf(stderr, "Incoming Buffer Length exceeds buffer: %d>%d\n", mms->media_packet_buffer_length, (int) sizeof(mms->media_packet_incoming_buffer));
                }
                assert(mms->media_packet_buffer_length<sizeof(mms->media_packet_incoming_buffer));
                read_result= get_buffer(&mms->incoming_io_buffer, dst, length_remaining);
                if(read_result != length_remaining) {
#if (MMS_DEBUG_LEVEL>0)
                    fprintf(stderr, "read_bytes result: %d asking for %d\n", read_result, length_remaining);
#endif
                    break;
                } else {
                    // if we successfully read everything....
                    if(packet_id_type == mms->header_packet_id) {
                        // asf header
                        // fprintf(stderr, "asf header: %d\n", mms->incoming_buffer_length);
                        packet_type = SC_PACKET_ASF_HEADER_TYPE;

                        // Store the asf header
                        if(!mms->header_parsed) {
                            mms->asf_header = av_realloc(mms->asf_header, mms->asf_header_size + mms->media_packet_buffer_length);
                            memcpy(mms->asf_header + mms->asf_header_size, mms->media_packet_read_ptr, mms->media_packet_buffer_length);
                            mms->asf_header_size += mms->media_packet_buffer_length;
                        }
                    } else if(packet_id_type == mms->packet_id) {
                        //                    fprintf(stderr, "asf packet: %d\n", mms->incoming_buffer_length);
                        packet_type = SC_PACKET_ASF_MEDIA_TYPE;
                    } else {
#if (MMS_DEBUG_LEVEL>0)
                        fprintf(stderr, "packet id type %d which must be old, getting another one.", packet_id_type);
#endif
                        done= 0;
                    }
                }
            }
        } else {
            // read error...
            if(read_result<0) {
#if (MMS_DEBUG_LEVEL>0)
                fprintf(stderr, "Read error (or cancelled) returned %d!\n", read_result);
#endif
                packet_type = SC_PACKET_TYPE_CANCEL;
            } else {// 0 is okay, no data received.
#if (MMS_DEBUG_LEVEL>0)
                fprintf(stderr, "Read result of zero?!\n");
#endif
                packet_type = SC_PACKET_TYPE_NO_DATA;
            }
            done = 1;
        }
    } while(!done);

    return packet_type;
}

/** Send MMST stream selection command based on the AVStream->discard values. */
static int send_stream_selection_request(MMSContext *mms)
{
    int ii;
    int err;

    //  send the streams we want back...
    start_command_packet(mms, CS_PACKET_STREAM_ID_REQUEST_TYPE);
    put_le32(&mms->outgoing_packet_data, mms->av_format_ctx->nb_streams);

    for(ii= 0; ii<mms->av_format_ctx->nb_streams; ii++) {
        AVStream *st= mms->av_format_ctx->streams[ii];

        put_le16(&mms->outgoing_packet_data, 0xffff); // flags
        put_le16(&mms->outgoing_packet_data, st->id); // stream id
        put_le16(&mms->outgoing_packet_data, ff_mms_stream_selection_code(st)); // selection
    }

    put_le16(&mms->outgoing_packet_data, 0); /* Extra zeroes */

    err = send_command_packet(mms);
    ff_mms_set_state(mms, AWAITING_STREAM_ID_ACCEPTANCE);
    return err;
}

static int send_keepalive_packet(MMSContext *mms)
{
    // respond to a keepalive with a keepalive...
    start_command_packet(mms, CS_PACKET_KEEPALIVE_TYPE);
    insert_command_prefixes(mms, 1, 0x100FFFF);
    return send_command_packet(mms);
}

static int send_media_file_request(MMSContext *mms)
{
    int err= 0;
    start_command_packet(mms, CS_PACKET_MEDIA_FILE_REQUEST_TYPE);
    insert_command_prefixes(mms, 1, 0xffffffff);
    put_le32(&mms->outgoing_packet_data, 0);
    put_le32(&mms->outgoing_packet_data, 0);
    put_le_utf16(&mms->outgoing_packet_data, mms->path+1); // +1 because we skip the leading /
    put_le32(&mms->outgoing_packet_data, 0); /* More zeroes */

    err = send_command_packet(mms);
    ff_mms_set_state(mms, AWAITING_PASSWORD_QUERY_OR_MEDIA_FILE);
    return err;
}

static int send_media_header_request(MMSContext *mms)
{
    int err= 0;
    start_command_packet(mms, CS_PACKET_MEDIA_HEADER_REQUEST_TYPE);
    insert_command_prefixes(mms, 1, 0);
    put_le32(&mms->outgoing_packet_data, 0);
    put_le32(&mms->outgoing_packet_data, 0x00800000);
    put_le32(&mms->outgoing_packet_data, 0xffffffff);
    put_le32(&mms->outgoing_packet_data, 0);
    put_le32(&mms->outgoing_packet_data, 0);
    put_le32(&mms->outgoing_packet_data, 0);

    // the media preroll value in milliseconds?
    put_le32(&mms->outgoing_packet_data, 0);
    put_le32(&mms->outgoing_packet_data, 0x40AC2000);
    put_le32(&mms->outgoing_packet_data, 2);
    put_le32(&mms->outgoing_packet_data, 0);

    err = send_command_packet(mms);
    ff_mms_set_state(mms, AWAITING_PACKET_HEADER_REQUEST_ACCEPTED_TYPE);
    return err;
}

static int send_protocol_select(MMSContext *mms)
{
    char data_string[256];
    int err= 0;

    // send the timing request packet...
    start_command_packet(mms, CS_PACKET_PROTOCOL_SELECT_TYPE);
    insert_command_prefixes(mms, 0, 0);
    put_le32(&mms->outgoing_packet_data, 0);  // timestamp?
    put_le32(&mms->outgoing_packet_data, 0);  // timestamp?
    put_le32(&mms->outgoing_packet_data, 2);
    snprintf(data_string, sizeof(data_string), "\\\\%d.%d.%d.%d\\%s\\%d",
            (mms->local_ip_address>>24)&0xff,
            (mms->local_ip_address>>16)&0xff,
            (mms->local_ip_address>>8)&0xff,
            mms->local_ip_address&0xff,
            "TCP", // or UDP
            mms->local_port);
    put_le_utf16(&mms->outgoing_packet_data, data_string);
    put_le16(&mms->outgoing_packet_data, 0x30);

    err = send_command_packet(mms);
    ff_mms_set_state(mms, AWAITING_CS_PACKET_PROTOCOL_ACCEPTANCE);
    return err;
}

static void handle_packet_media_file_details(MMSContext *mms)
{
    ByteIOContext pkt;
    uint16_t broadcast_flags;
    int64_t total_file_length_in_seconds;
    uint32_t total_length_in_seconds;
    uint32_t packet_length;
    uint32_t total_packet_count;
    uint32_t highest_bit_rate;
    uint32_t header_size;
    uint32_t flags;
    double duration;

    // read these from the incoming buffer.. (48 is the packet header size)
    init_put_byte(&pkt, mms->incoming_buffer+48, mms->incoming_buffer_length-48, 0, NULL, NULL, NULL, NULL);
    flags= get_le32(&pkt); // flags?
    if(flags==0xffffffff) {
        // this is a permission denied event.
#if (MMS_DEBUG_LEVEL>0)
        fprintf(stderr, "Permission denied!\n");
#endif
        ff_mms_set_state(mms, STATE_ERROR);
    } else {
        get_le32(&pkt);
        get_le32(&pkt);
        get_le16(&pkt);
        broadcast_flags= get_le16(&pkt);

        total_file_length_in_seconds= get_le64(&pkt);
        duration= av_int2dbl(total_file_length_in_seconds);
        total_length_in_seconds= get_le32(&pkt);
        get_le32(&pkt);
        get_le32(&pkt);
        get_le32(&pkt);
        get_le32(&pkt);
        packet_length= get_le32(&pkt);
        total_packet_count= get_le32(&pkt);
        get_le32(&pkt);
        highest_bit_rate= get_le32(&pkt);
        header_size= get_le32(&pkt);
#if (MMS_DEBUG_LEVEL>0)
        fprintf(stderr, "Broadcast flags: 0x%x\n", broadcast_flags); // 8000= allow index, 01= prerecorded, 02= live 42= presentation with script commands
        fprintf(stderr, "File Time Point?: %lld double size: %d double value: %lf\n", total_file_length_in_seconds, (int) sizeof(double), duration);
        fprintf(stderr, "Total in Seconds: %d\n", total_length_in_seconds);
        fprintf(stderr, "Packet length: %d\n", packet_length);
        fprintf(stderr, "Total Packet Count: %d\n", total_packet_count);
        fprintf(stderr, "Highest Bit Rate: %d\n", highest_bit_rate);
        fprintf(stderr, "Header Size: %d\n", header_size);
        fprintf(stderr, "---- Done ----\n");
#endif

        /* Disable seeking in live broadcasts for now */
        if(! (broadcast_flags & 0x0200))
            mms->seekable = 1;
    }
}

static void handle_packet_stream_changing_type(MMSContext *mms)
{
    ByteIOContext pkt;
#if (MMS_DEBUG_LEVEL>0)
    fprintf(stderr, "Stream changing!\n");
#endif

    // read these from the incoming buffer.. (40 is the packet header size, without the prefixes)
    init_put_byte(&pkt, mms->incoming_buffer+40, mms->incoming_buffer_length-40, 0, NULL, NULL, NULL, NULL);
    get_le32(&pkt); // prefix 1
    mms->header_packet_id= (get_le32(&pkt) & 0xff); // prefix 2

    fprintf(stderr, "Changed header prefix to 0x%x", mms->header_packet_id);
    // mms->asf_header_length= 0;

    ff_mms_set_state(mms, AWAITING_ASF_HEADER); // this is going to hork our avstreams.
}

/** Send the initial handshake. */
static int send_startup_packet(MMSContext *mms)
{
    int err;
    char data_string[256];

    snprintf(data_string, sizeof(data_string), "NSPlayer/7.0.0.1956; {%s}; Host: %s",
            mms->local_guid, mms->host);

    start_command_packet(mms, CS_PACKET_INITIAL_TYPE);
    insert_command_prefixes(mms, 0, 0x0004000b);
    put_le32(&mms->outgoing_packet_data, 0x0003001c);
    put_le_utf16(&mms->outgoing_packet_data, data_string);
    put_le16(&mms->outgoing_packet_data, 0); // double unicode ended string...

    err = send_command_packet(mms);
    ff_mms_set_state(mms, AWAITING_SC_PACKET_CLIENT_ACCEPTED);
    return err;
}

static int send_pause_packet(MMSContext *mms)
{
    int err;
    start_command_packet(mms, CS_PACKET_STREAM_PAUSE_TYPE);
    insert_command_prefixes(mms, 1, 0xffff0100);

    err = send_command_packet(mms);
    ff_mms_set_state(mms, AWAITING_PAUSE_ACKNOWLEDGE);
    return err;
}

static int send_close_packet(MMSContext *mms)
{
    int err;
    start_command_packet(mms, CS_PACKET_STREAM_CLOSE_TYPE);
    insert_command_prefixes(mms, 1, 1);

    err = send_command_packet(mms);
    ff_mms_set_state(mms, AWAITING_PAUSE_ACKNOWLEDGE);
    return err;
}

/** Handling pf TCP-specific packets.
 * @param packet_type incoming packet type to process.
 * @return  0 if the packet_type wasn't handled by this function.
 *          1 if it expected and handled.
 *         -1 if it the packet was unexpected in the current state.
 */
static int tcp_packet_state_machine(MMSContext *mms, MMSSCPacketType packet_type)
{
    switch(packet_type) {
    case SC_PACKET_CLIENT_ACCEPTED:
#if 0
        if(mms->state==AWAITING_SC_PACKET_CLIENT_ACCEPTED) {
#if (MMS_DEBUG_LEVEL>0)
            fprintf(stderr, "Transitioning from AWAITING_SC_PACKET_CLIENT_ACCEPTED to AWAITING_SC_PACKET_TIMING_TEST_REPLY_TYPE\n");
#endif
            // send the timing request packet...
            start_command_packet(mms, CS_PACKET_TIMING_DATA_REQUEST_TYPE);
            insert_command_prefixes(mms, 0xf0f0f0f1, 0x0004000b);
            send_command_packet(mms);

            ff_mms_set_state(mms, AWAITING_SC_PACKET_TIMING_TEST_REPLY_TYPE);
        } else {
            return -1;
        }
        break;
#endif

    case SC_PACKET_TIMING_TEST_REPLY_TYPE: // we may, or may not have timing tests.
        if(mms->state==AWAITING_SC_PACKET_TIMING_TEST_REPLY_TYPE || mms->state==AWAITING_SC_PACKET_CLIENT_ACCEPTED) {
            send_protocol_select(mms);
        } else {
            return -1;
        }
        break;

    case SC_PACKET_PROTOCOL_ACCEPTED_TYPE:
        if(mms->state==AWAITING_CS_PACKET_PROTOCOL_ACCEPTANCE) {
            send_media_file_request(mms);
        } else {
            return -1;
        }
        break;

    case SC_PACKET_PROTOCOL_FAILED_TYPE:
        if(mms->state==AWAITING_CS_PACKET_PROTOCOL_ACCEPTANCE) {
            // abort;
#if (MMS_DEBUG_LEVEL>0)
            fprintf(stderr, "Protocol failed\n");
#endif
            ff_mms_set_state(mms, STATE_ERROR);
        } else {
            return -1;
        }
        break;

    case SC_PACKET_PASSWORD_REQUIRED_TYPE:
        if(mms->state==AWAITING_PASSWORD_QUERY_OR_MEDIA_FILE) {
            // we don't support this right now.
#if (MMS_DEBUG_LEVEL>0)
            fprintf(stderr, "Password required\n");
#endif
            ff_mms_set_state(mms, STATE_ERROR);
        } else {
            return -1;
        }
        break;

    case SC_PACKET_MEDIA_FILE_DETAILS_TYPE:
        if(mms->state==AWAITING_PASSWORD_QUERY_OR_MEDIA_FILE) {
            handle_packet_media_file_details(mms);
            if(mms->state != STATE_ERROR)
                send_media_header_request(mms);
        } else {
            return -1;
        }
        break;

    case SC_PACKET_HEADER_REQUEST_ACCEPTED_TYPE:
        if(mms->state==AWAITING_PACKET_HEADER_REQUEST_ACCEPTED_TYPE) {
            // reset (in case we are doing this more than once)
//                mms->asf_header_length= 0;

            // wait for the header (follows immediately)
            ff_mms_set_state(mms, AWAITING_ASF_HEADER);
        } else {
            return -1;
        }
        break;

    case SC_PACKET_STREAM_CHANGING_TYPE:
        if(mms->state==STREAMING) {
            handle_packet_stream_changing_type(mms);
        } else {
            return -1;
        }
        break;

    case SC_PACKET_STREAM_ID_ACCEPTED_TYPE:
        if(mms->state==AWAITING_STREAM_ID_ACCEPTANCE) {
#if (MMS_DEBUG_LEVEL>0)
            fprintf(stderr, "Stream ID's accepted!\n");
#endif
            ff_mms_set_state(mms, STREAM_PAUSED); // only way to get out of this is to play...
        } else {
            return -1;
        }
        break;

    case SC_PACKET_MEDIA_PACKET_FOLLOWS_TYPE:
        if(mms->state==AWAITING_STREAM_START_PACKET) {
            // get the stream packets...
            ff_mms_set_state(mms, STREAMING);
        } else {
            return -1;
        }
        break;

    case SC_PACKET_KEEPALIVE_TYPE:
        if(mms->state==STREAMING || mms->state==STREAM_PAUSED) {
#if (MMS_DEBUG_LEVEL>0)
            fprintf(stderr, "Got a Keepalive!\n");
#endif
            send_keepalive_packet(mms);
        } else {
            return -1;
        }
        break;

    default:
        return 0; // Not handled here
    }

    return 1; // Handled here
}

/** Request a MMST stream from a timestamp or packet offset.
 * @param rate Play rate. (Only 1 supported as for now)
 * @param byte_offset Byte position to seek to. Set to -1 when seeking by timestamp.
 *  The position is from the start of the media stream, i.e. not counting header size.
 * @param timestamp Time point in ms. Set to 0 when seeking from packet offsets.
 */
static int request_streaming_from(MMSContext *mms,
        int rate, int64_t byte_offset, int64_t timestamp)
{
    int32_t packet = -1;
    int result;

    if(byte_offset > 0)
        packet = byte_offset / mms->asf_context.packet_size;

    /* Send a stream selection request if this is the first call to play */
    if(mms->state == ASF_HEADER_DONE) {
        result = send_stream_selection_request(mms);

        if(result==0) {
            while(mms->state != STREAM_PAUSED && mms->state != STATE_ERROR && mms->state != STREAM_DONE) {
                ff_mms_packet_state_machine(mms);
            }
        }
    }

    if(mms->state==STREAM_PAUSED || mms->state == ASF_HEADER_DONE) {
        timestamp= av_dbl2int((double)timestamp/1000.0); // is this needed?

        start_command_packet(mms, CS_PACKET_START_FROM_PACKET_ID_TYPE);
        insert_command_prefixes(mms, 1, mms->packet_id);
        put_le64(&mms->outgoing_packet_data, timestamp); // seek timestamp
        put_le32(&mms->outgoing_packet_data, 0xffffffff);  // unknown
        put_le32(&mms->outgoing_packet_data, packet);    // packet offset
        put_byte(&mms->outgoing_packet_data, 0xff); // max stream time limit
        put_byte(&mms->outgoing_packet_data, 0xff); // max stream time limit
        put_byte(&mms->outgoing_packet_data, 0xff); // max stream time limit
        put_byte(&mms->outgoing_packet_data, 0x00); // stream time limit flag

        mms->packet_id++; // new packet_id so we can separate new data from old data
        put_le32(&mms->outgoing_packet_data, mms->packet_id);
        send_command_packet(mms);

        ff_mms_set_state(mms, AWAITING_STREAM_START_PACKET);
        return 0;
    } else {
#if (MMS_DEBUG_LEVEL>0)
//        fprintf(stderr, "Tried a read_play when the state was not stream paused (%s)\n", state_names[mms->state]);
#endif
        return -1;
    }
}

MMSProtocol mmst_mmsprotocol = {
    .default_port = DEFAULT_MMS_PORT,
    .packet_state_machine = tcp_packet_state_machine,
    .send_startup_message = send_startup_packet,
    .send_shutdown_message = send_close_packet,
    .pause = send_pause_packet,
    .play_from = request_streaming_from,
    .get_server_response = get_tcp_server_response,
};
