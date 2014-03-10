/*
 * Native MMS client
 * Copyright (c) 2006,2007 Ryan Martell
 * Copyright (c) 2007 Bjˆrn Axelsson
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

/** @file mms_protocol.c
 * MMS protocol client.
 * @author Ryan Martell
 * @author Bjˆrn Axelsson
 */

#include "avformat.h"

#include "network.h"
#include "asf.h"
#include "random.h"
#include "avstring.h"
#include "mms_private.h"

#if (MMS_DEBUG_LEVEL>0)
static const char *state_names[]= {
    "AWAITING_SC_PACKET_CLIENT_ACCEPTED",
    "AWAITING_SC_PACKET_TIMING_TEST_REPLY_TYPE",
    "AWAITING_CS_PACKET_PROTOCOL_ACCEPTANCE",
    "AWAITING_PASSWORD_QUERY_OR_MEDIA_FILE",
    "AWAITING_PACKET_HEADER_REQUEST_ACCEPTED_TYPE",
    "AWAITING_STREAM_ID_ACCEPTANCE",
    "AWAITING_STREAM_START_PACKET",
    "AWAITING_ASF_HEADER",
    "ASF_HEADER_DONE",
    "AWAITING_PAUSE_ACKNOWLEDGE",
    "AWAITING_HTTP_PAUSE_CONTROL_ACKNOWLEDGE",
    "STREAMING",
    "STREAM_DONE",
    "STATE_ERROR",
    "STREAM_PAUSED",
    "USER_CANCELLED"
};
#endif

/* TODOs and known issues:
  - standardized extended URLProtocol & ByteIOContext API
  - finalize stream selection interface
    (set/get_demuxer(), set/get_streams() ?)
  - reimplement print_command() to facilitate mmst debugging
  - cosmetics: indentation, trailing whitespace, doxygen
  - debug, error and status messages: use av_log(), remove fprintf
  - pausing and seeking works differently for live stream vs. a prerecorded stream
  - better way to handle paused streams in ffplay (don't try to read from it)
  - mmst: seeking when paused gives broken packet messages from demuxer
  - review for endianness problems, buffer overflows etc
  - http and tcp authentication
  - it would be very nice to be able to hook the mmsh:// access onto http:// to
    get free auth, redirect and proxy handling...
  - test cases:
    * different server versions
    * different protocols
    * different packet sizes
    * live vs prerecorded streams
    * pause, seek, (ffwd ?)
    * byte-based seeking, seeking into odd (non-packet-aligned) offsets
  - use cases:
    * ffplay, ffmpeg (through asf_demuxer)
    * directly as a url (e.g. stream archiver)
    * non-avformat demuxer (mplayer)
*/

extern AVInputFormat asf_demuxer;

static int mms_close(URLContext *h);
int ff_mms_play(URLContext *h);
int ff_mms_seek(URLContext *h, int stream_index, int64_t timestamp, int flags);

/** Clear all buffers of partial and old packets after a seek or other discontinuity */
void clear_stream_buffers(MMSContext *mms)
{
    mms->incoming_io_buffer.buf_ptr = mms->incoming_io_buffer.buf_end;
    mms->media_packet_buffer_length = 0;
    mms->media_packet_read_ptr = mms->media_packet_incoming_buffer;
}

/** Pad media packets smaller than max_packet_size and/or adjust read position
 * after a seek. */
static void pad_media_packet(MMSContext *mms)
{
    if(mms->media_packet_buffer_length<mms->asf_context.packet_size) {
        int padding_size = mms->asf_context.packet_size - mms->media_packet_buffer_length;
        //  fprintf(stderr, "Incoming packet smaller than the asf packet size stated (%d<%d) Padding.\n", mms->media_packet_buffer_length, mms->asf_context.packet_size);
        memset(mms->media_packet_incoming_buffer+mms->media_packet_buffer_length, 0, padding_size);
        mms->media_packet_buffer_length += padding_size;
    }

    if(mms->media_packet_seek_offset) {
        mms->media_packet_buffer_length -= mms->media_packet_seek_offset;
        mms->media_packet_read_ptr += mms->media_packet_seek_offset;
        mms->media_packet_seek_offset = 0;
    }
}

/** Convert from AVDISCARD_* values to MMS stream selection code for the stream. */
int ff_mms_stream_selection_code(AVStream *st)
{
    switch(st->discard) {
    case AVDISCARD_NONE:
    case AVDISCARD_DEFAULT:
    default:
        return 0; // 00 = stream at full frame rate;

    case AVDISCARD_NONREF:
    case AVDISCARD_BIDIR:
    case AVDISCARD_NONKEY:
        return 1; // 01 = only stream key frames (doesn't work that well)

    case AVDISCARD_ALL:
        return 2; // 02 = no stream, switch it off.
    }
}

/** Generate a "good-enough unique ID" for use as session(?) ID. */
static void generate_guid(char *path, char *dst)
{
    AVRandomState random_state;
    char digit[]= "0123456789ABCDEF";
    int ii;

    // set the seed; contrary to popular belief, this does matter.
    // if i request a different asset during the same running process, the server will send me the previous requested
    // VOD file.  This does fix it.  Not sure if this is a protocol bug, or some weird way to queue files for future playback, or what, but
    // "These are not the behaviors you're looking for.  Move along."
    int seed= 0;
    char *src= path;
    while(*src) {
        seed += *src;
        src++;
    }

    av_init_random(seed, &random_state);
    for(ii= 0; ii<36; ii++) {
        switch(ii) {
        case 8:
        case 13:
        case 18:
        case 23:
            *dst++= '-';
            break;

        default:
            *dst++= digit[av_random(&random_state)%16];
            break;
        }
    }
    *dst= '\0';
}

/** Perform state transition. */
void ff_mms_set_state(MMSContext *mms, int new_state)
{
    /* Can't exit error state */
    if(mms->state==STATE_ERROR) {
#if (MMS_DEBUG_LEVEL>0)
        fprintf(stderr, "Trying to set state to %s from %s!\n", state_names[new_state], state_names[mms->state]);
#endif
        return;
    }

#if (MMS_DEBUG_LEVEL>0)
    fprintf(stderr, "Set state to %s (%d) from %s (%d)!\n", state_names[new_state], new_state, state_names[mms->state], mms->state);
#endif
    if(mms->state==new_state && new_state==USER_CANCELLED) {
        ff_mms_set_state(mms, STATE_ERROR);
        return;
    }

    mms->state= new_state;
}

/** Log unexpected incoming packet */
void log_packet_in_wrong_state(MMSContext *mms, MMSSCPacketType packet_type)
{
#if (MMS_DEBUG_LEVEL>0)
    if(packet_type>=0) {
        fprintf(stderr, "Got a packet 0x%02x in the wrong state: %s (%d)!\n", packet_type, state_names[mms->state], mms->state);
    } else {
        fprintf(stderr, "Got a pseudo-packet %d in the wrong state: %s (%d)!\n", packet_type, state_names[mms->state], mms->state);
    }
#endif
}

/** Close the remote connection. */
static void close_connection(MMSContext *mms)
{
    if(mms->mms_hd) {
        if(mms->incoming_io_buffer.buffer) {
            av_free(mms->incoming_io_buffer.buffer);
            mms->incoming_io_buffer.buffer = NULL;
        }

        url_close(mms->mms_hd);
        mms->mms_hd = NULL;
    }
}

/** Open or reopen (http) the remote connection. */
int ff_mms_open_connection(MMSContext *mms)
{
    char tcpname[256];
    int flags, err;

    close_connection(mms);

    snprintf(tcpname, sizeof(tcpname), "tcp://%s:%d", mms->host, mms->port);
    err = url_open(&mms->mms_hd, tcpname, URL_RDWR);
    if(err == 0) {
        /* open the incoming and outgoing connections; you can't open a single one with read/write, because it only has one buffer, not two. */
        /* you can't use url_fdopen if the flags of the mms_hd have a WR component, because it will screw up (returning data that is uninitialized) */
        flags = mms->mms_hd->flags;
        mms->mms_hd->flags = URL_RDONLY;
        err = url_fdopen(&mms->incoming_io_buffer, mms->mms_hd);
        mms->mms_hd->flags = flags;
        if(err) {
            // should have a url_fdclose()
            url_close(mms->mms_hd);
        }
    }

    return err;
}

/**
 * Start streaming from the specified byte offset.
 * If offset if sufficiently low, the ASF header will be seen again by URLProtocol::read()
 * @param offset byte position to stream from. This includes the header length.
 */
static int mms_stream_play_from_offset(MMSContext *mms, int64_t offset)
{
    int result = 0;
#if (MMS_DEBUG_LEVEL>0)
    fprintf(stderr, "mms_stream_play: pos %lld\n", offset);
#endif

    /* Enable header resending if seeking backwards very far */
    mms->asf_header_read_pos = FFMIN(mms->asf_header_read_pos, offset);

    /* Handle seeking into packets */
    offset = FFMAX(0, offset - mms->asf_header_size);
    mms->media_packet_seek_offset = offset % mms->asf_context.packet_size;
    offset -= mms->media_packet_seek_offset; /* Align to packet size */

    result = mms->protocol->play_from(mms, 1, offset, 0);

    if(result == 0)
        mms->incoming_packet_seq = offset / mms->asf_context.packet_size;

    return result;
}

/**
 * Start streaming from the specified timestamp.
 * After a seek to a timestamp, the ASF header will not be returned by URLProtocol::read().
 * @param timestamp approximate target stream time in ms.
 */
static int mms_stream_play_from_timestamp(MMSContext *mms, int64_t timestamp)
{
    int err= 0;

    /* Normally the server will start streaming from a video keyframe after
     * seeking to a time index. But in the case of multiple video streams
     * one or more may resume on a non-keyframe or even in the middle of a media
     * object (frame), which doesn't make the asf demuxer happy.
     * This is possibly also the case when seeking in a live stream
     */

#if (MMS_DEBUG_LEVEL>0)
    fprintf(stderr, "mms_stream_play: %lld TS: %lf\n", timestamp, timestamp/1000.0);
#endif
    err = mms->protocol->play_from(mms, 1, -1, FFMAX(0, timestamp));

    if(err == 0 && mms->media_packet_buffer_length == 0) {
        /* Read a single media packet to update our current stream position. */
        MMSSCPacketType packet_type = -1;
        while((mms->state == AWAITING_STREAM_START_PACKET || mms->state == STREAMING) && packet_type != SC_PACKET_ASF_MEDIA_TYPE)
            packet_type = ff_mms_packet_state_machine(mms);
    }

    /* Never return the header after a time-based seek. Even if timestamp == 0 */
    mms->asf_header_read_pos = mms->asf_header_size;

    return err;
}

/** Single-step the packet-pumping state machine.
 * @return The type of the last packet from the server.
 */
MMSSCPacketType ff_mms_packet_state_machine(MMSContext *mms)
{
    MMSSCPacketType packet_type = mms->protocol->get_server_response(mms);

    /* First, try protocol-specific packet handling */
    if(mms->protocol->packet_state_machine) {
        int ret = mms->protocol->packet_state_machine(mms, packet_type);
        if(ret != 0) {
            if(ret == -1)
                log_packet_in_wrong_state(mms, packet_type);
            return packet_type;
        }
    }

    /* Common packet handling */
    switch(packet_type) {
    case SC_PACKET_ASF_HEADER_TYPE:
        if(mms->state==AWAITING_ASF_HEADER) {
#if (MMS_DEBUG_LEVEL>0)
            fprintf(stderr, "Got a SC_PACKET_ASF_HEADER: %d\n", mms->media_packet_buffer_length);
#endif
            if((mms->incoming_flags == 0X08) || (mms->incoming_flags == 0X0C))
            {
#if (MMS_DEBUG_LEVEL>0)
                fprintf(stderr, "Got the full header!\n");
#endif
                ff_mms_set_state(mms, ASF_HEADER_DONE);
            }
        } else {
            log_packet_in_wrong_state(mms, packet_type);
        }
        break;

    case SC_PACKET_ASF_MEDIA_TYPE:
        if(mms->state==STREAMING || mms->state==AWAITING_PAUSE_ACKNOWLEDGE || mms->state==AWAITING_HTTP_PAUSE_CONTROL_ACKNOWLEDGE) {
//                fprintf(stderr, "Got a stream packet of length %d!\n", mms->incoming_buffer_length);
            pad_media_packet(mms);
        } else {
            log_packet_in_wrong_state(mms, packet_type);
        }
        break;

    case SC_PACKET_STREAM_STOPPED_TYPE:
        if(mms->state==AWAITING_PAUSE_ACKNOWLEDGE) {
#if (MMS_DEBUG_LEVEL>0)
            fprintf(stderr, "Server echoed stream pause\n");
#endif
            ff_mms_set_state(mms, STREAM_PAUSED);
        } else if(mms->state==STREAMING) {
            /*
            When echoing a start (from me):
             receive command 0x1e, 48 bytes
             start sequence 00000001
             command id     b00bface
             length               20
             protocol       20534d4d
             len8                  4
             sequence #     00000006
             len8  (II)            2
             dir | comm     0004001e
             prefix1        00000000
             prefix2        ffff0100

             When Ending on it's own:
             receive command 0x1e, 48 bytes
             start sequence 09000001
             command id     b00bface
             length               20
             protocol       20534d4d
             len8                  4
             sequence #     00000006
             len8  (II)            2
             dir | comm     0004001e
             prefix1        00000000
             prefix2        00000004
            */
#if (MMS_DEBUG_LEVEL>0)
            fprintf(stderr, "** Server hit end of stream (may be sending new header information)\n");
#endif
            // TODO: if this is a live stream, on the resumption of a pause, this happens, then it follows with a SC_PACKET_STREAM_CHANGING_TYPE
            // otherwise it means this stream is done.
            ff_mms_set_state(mms, STREAM_DONE);
        } else {
            log_packet_in_wrong_state(mms, packet_type);
        }
        break;

    case SC_PACKET_TYPE_CANCEL:
        fprintf(stderr, "Got a -1 packet type\n");
        // user cancelled; let us out so it gets closed down...
        ff_mms_set_state(mms, USER_CANCELLED);
        break;

    case SC_PACKET_TYPE_NO_DATA:
#if (MMS_DEBUG_LEVEL>0)
        fprintf(stderr, "Got no data (closed?)\n");
#endif
        ff_mms_set_state(mms, STREAM_DONE); //?
        break;

    case SC_PACKET_HTTP_CONTROL_ACKNOWLEDGE:
        ff_mms_set_state(mms, AWAITING_PAUSE_ACKNOWLEDGE);
        break;

    default:
        fprintf(stderr, "Unhandled packet type %d\n", packet_type);
        break;
    }

    return packet_type;
}

/** Read at most one media packet (or a whole header). */
static int read_packet(MMSContext *mms, uint8_t *buf, int buf_size)
{
    int result = 0;
    MMSSCPacketType packet_type;
    int size_to_copy;

//    fprintf(stderr, "mms_read_packet()\n");

//    fprintf(stderr, "*** read packet %p needs %d bytes at %lld...\n", buf, buf_size, url_ftell(&mms->av_format_ctx->pb));
    if(mms->state != STREAM_DONE && mms->state != STREAM_PAUSED && mms->state != STATE_ERROR) {
        do {
            if(mms->asf_header_read_pos < mms->asf_header_size) {
                /* Read from ASF header buffer */
                size_to_copy= FFMIN(buf_size, mms->asf_header_size - mms->asf_header_read_pos);
                memcpy(buf, mms->asf_header + mms->asf_header_read_pos, size_to_copy);
                mms->asf_header_read_pos += size_to_copy;
                result += size_to_copy;
#if (MMS_DEBUG_LEVEL > 0)
                fprintf(stderr, "Copied %d bytes from stored header. left: %d\n", size_to_copy, mms->asf_header_size - mms->asf_header_read_pos);
#endif
            } else if(mms->media_packet_buffer_length) {
                /* Read from media packet buffer */
                size_to_copy = FFMIN(buf_size, mms->media_packet_buffer_length);
                memcpy(buf, mms->media_packet_read_ptr, size_to_copy);
                mms->media_packet_buffer_length -= size_to_copy;
                mms->media_packet_read_ptr+= size_to_copy;
                result += size_to_copy;
                //fprintf(stderr, "Copied %d bytes from media_packet read pointer! (result: %d, left: %d)\n", size_to_copy, result, mms->media_packet_buffer_length);
            } else {
                /* Read from network */
//               fprintf(stderr, "Calling state machine...\n");
                packet_type= ff_mms_packet_state_machine(mms);
//                fprintf(stderr, "Type: 0x%x\n", packet_type);
                switch (packet_type) {
                case SC_PACKET_ASF_MEDIA_TYPE:
                    if(mms->media_packet_buffer_length>mms->asf_context.packet_size) {
                        fprintf(stderr, "Incoming packet larger than the asf packet size stated (%d>%d)\n", mms->media_packet_buffer_length, mms->asf_context.packet_size);
                        result= AVERROR_IO;
                        break;
                    }

                    // copy the data to the packet buffer...
                    size_to_copy= FFMIN(buf_size, mms->media_packet_buffer_length);
                    memcpy(buf, mms->media_packet_read_ptr, size_to_copy);
                    mms->media_packet_buffer_length -= size_to_copy;
                    mms->media_packet_read_ptr += size_to_copy;
                    result += size_to_copy;
//fprintf(stderr, "Copied %d bytes (Media) from media_packet read pointer! (result: %d)\n", size_to_copy, result);
                    break;
                case SC_PACKET_ASF_HEADER_TYPE:
                    // copy the data to the packet buffer...
                    size_to_copy= FFMIN(buf_size, mms->media_packet_buffer_length);
                    memcpy(buf, mms->media_packet_read_ptr, size_to_copy);
                    mms->media_packet_buffer_length -= size_to_copy;
                    mms->media_packet_read_ptr+= size_to_copy;
                    result+= size_to_copy;
//fprintf(stderr, "Copied %d bytes (header) from media_packet read pointer! (result: %d)\n", size_to_copy, result);
                    break;
                default:
                    if(mms->state==STREAM_PAUSED) {
                        result= 0;
                    } else if(mms->state==STREAM_DONE || mms->state==USER_CANCELLED) {
                        result=-1;
                    } else if(mms->state==AWAITING_ASF_HEADER) {
                        // we have reset the header; spin though the loop..
//                        fprintf(stderr, "****-- Spinning the loop!\n");
                        while(mms->state != STREAMING && mms->state != STATE_ERROR) {
                            ff_mms_packet_state_machine(mms);
                        }
//                        fprintf(stderr, "****-- Done Spinning the loop!\n");
                    } else {
#if (MMS_DEBUG_LEVEL>0)
                        fprintf(stderr, "Got a packet in odd state: %s Packet Type: 0x%x\n", state_names[mms->state], packet_type);
#endif
                    }
                    break;
                }
            }
        } while(result==0 && mms->state!=STREAM_PAUSED); // only return one packet...
    } else {
        if(mms->state==STREAM_PAUSED) {
            result= 0;
        } else {
            result= -1;
        }
    }
//    fprintf(stderr, "read packet %p needs %d bytes.  getting %d\n", buf, buf_size, result);

    return result;
}

/** Read the whole mms header into a buffer of our own .*/
static int read_header(MMSContext *mms)
{
    if(mms->state != AWAITING_ASF_HEADER) {
        fprintf(stderr, "cannot read header this state\n");
        ff_mms_set_state(mms, STATE_ERROR);
        return -1;
    }

    /* TODO: add timeout */
    /* This will run until the header is stored */
    while(mms->state != ASF_HEADER_DONE && mms->state != STATE_ERROR)
        ff_mms_packet_state_machine(mms);

    if(mms->state == STATE_ERROR)
        return -1;

    /* Parse the header */
    init_put_byte(&mms->private_av_format_ctx.pb, mms->asf_header, mms->asf_header_size, 0, NULL, NULL, NULL, NULL);
    mms->private_av_format_ctx.priv_data = &mms->asf_context;

    if(asf_demuxer.read_header(&mms->private_av_format_ctx, NULL) < 0) {
        fprintf(stderr, "read_header failed\n");
        return -1;
    }

    mms->av_format_ctx = &mms->private_av_format_ctx; // Default
    mms->header_parsed = 1;

    return 0;
}

/**
 * Public API exposed through the URLProtocol
 */

/** Open a MMST/MMSH connection */
static int mms_open(URLContext *h, const char *uri, int flags)
{
    char protocol[16];
    char authorization[64];
    int err = AVERROR(EIO);

    MMSContext *mms;

    mms = av_malloc(sizeof(MMSContext));
    if (!mms)
        return AVERROR(ENOMEM);
    memset(mms, 0, sizeof(MMSContext));
    h->priv_data = mms;
    h->is_streamed = 1;

    url_split(protocol, sizeof(protocol), authorization, sizeof(authorization), mms->host, sizeof(mms->host),
              &mms->port, mms->path, sizeof(mms->path), uri);

    if(strcmp(protocol, "mmsh") == 0) {
        mms->protocol = &mmsh_mmsprotocol;
    } else if (strcmp(protocol, "mms") == 0 || strcmp(protocol, "mmst") == 0) {
        mms->protocol = &mmst_mmsprotocol;
    } else {
        goto fail;
    }

    if(mms->port<0)
        mms->port = mms->protocol->default_port;

#if (MMS_DEBUG_LEVEL>0)
    fprintf(stderr, "Opening %s\n Protocol: %s\n Auth: %s\n Host: %s\n Port: %d\n Path: %s\n", uri, protocol, authorization, mms->host, mms->port, mms->path);
#endif

    /* the outgoing packet buffer */
    init_put_byte(&mms->outgoing_packet_data, mms->outgoing_packet_buffer, sizeof(mms->outgoing_packet_buffer), 1, NULL, NULL, NULL, NULL);

    /* open the tcp connexion */
    if((err = ff_mms_open_connection(mms)))
        goto fail;

    // Fill in some parameters...
    mms->local_ip_address = 0xc0a80081; // This should be the local IP address; how do I get this from the url_ stuff?  (nothing is apparent)
    mms->local_port = 1037; // as above, this should be the port I am connected to; how do I get this frmo the url stuff? (Server doesn't really seem to care too much)
    mms->packet_id = 3; // default, initial value. (3 will be incremented to 4 before first use)
    mms->header_packet_id = 2; // default, initial value.

    generate_guid(mms->path, mms->local_guid);

    /* okay, now setup stuff.  working from unclear specifications is great! */
    mms->protocol->send_startup_message(mms);

    // TODO: add timeout here... (mms_watchdog_reset() ?)
    while(mms->state != AWAITING_ASF_HEADER &&
            mms->state != STATE_ERROR &&
            mms->state != STREAM_DONE) {
        ff_mms_packet_state_machine(mms);
    }

    /* We store the header internally */
    if(mms->state != STATE_ERROR)
        read_header(mms);

    if(mms->state == STATE_ERROR) {
        err = AVERROR(EIO);
        goto fail;
    }

#if (MMS_DEBUG_LEVEL>0)
    fprintf(stderr, "Leaving open (success)\n");
#endif

    return 0;

fail:
    mms_close(h);

#if (MMS_DEBUG_LEVEL>0)
    fprintf(stderr, "Leaving open (failure: %d)\n", err);
#endif

    return err;
}

/** Close the MMSH/MMST connection */
static int mms_close(URLContext *h)
{
    MMSContext *mms = (MMSContext *)h->priv_data;

#if (MMS_DEBUG_LEVEL>0)
    fprintf(stderr, "mms_close\n");
#endif
    if(mms->mms_hd) {
        // send the close packet if we should...
        if(mms->state != STATE_ERROR) {
            if(mms->protocol->send_shutdown_message)
                mms->protocol->send_shutdown_message(mms);
        }

        // need an url_fdclose()
        close_connection(mms);
    }
#if (MMS_DEBUG_LEVEL>0)
    fprintf(stderr, "done with mms_close\n");
#endif

    /* TODO: free all separately allocated pointers in mms */
    if(mms->asf_header)
        av_free(mms->asf_header);
    av_free(mms);
    h->priv_data = NULL;

    return 0;
}

/** Read ASF data through the protocol. */
static int mms_read(URLContext *h, uint8_t *buf, int size)
{
    /* TODO: see tcp.c:tcp_read() about a possible timeout scheme */
    MMSContext *mms = h->priv_data;
    int result = 0;

    /* Since we read the header at open(), this shouldn't be possible */
    assert(mms->header_parsed);

    /* Automatically start playing if the app wants to read before it has called play()
     * (helps with non-streaming aware apps) */
    if(mms->state == ASF_HEADER_DONE && mms->asf_header_read_pos >= mms->asf_header_size) {
        fprintf(stderr, "mms_read() before play(). Playing automatically.\n");
        result = ff_mms_play(h);
        if(result < 0)
            return result;
    }

    /* We won't get any packets from the server if paused. Nothing else to do than
     * to return. FIXME: return AVERROR(EAGAIN)? */
    if(mms->state == STREAM_PAUSED) {
        fprintf(stderr, "mms_read in STREAM_PAUSED\n");
        return 0;
    } else if(mms->state==STREAMING || mms->state==AWAITING_STREAM_START_PACKET || mms->state == ASF_HEADER_DONE) {
        result = read_packet(mms, buf, size);

        /* Note which packet we last returned. FIXME: doesn't handle partially read packets */
        mms->pause_resume_seq = mms->incoming_packet_seq;
    } else {
#if (MMS_DEBUG_LEVEL>0)
        fprintf(stderr, "mms_read: wrong state %s, returning AVERROR_IO!\n", state_names[mms->state]);
#endif
        result = AVERROR(EIO);
    }

    return result;
}

/** Seek in the ASF stream. */
static int64_t mms_seek(URLContext *h, int64_t offset, int whence)
{
    MMSContext *mms = h->priv_data;
    int64_t cur_pos;
	
    if(! mms->seekable)
        return -1;
	
    /* TODO: is backwards seeking possible in live broadcasts? */
	
    switch(whence) {
		case SEEK_END:
		case AVSEEK_SIZE:
			/* file_size is probably not valid after stream selection,
			 * so we just disable this */
			return -1;
		case SEEK_CUR:
			cur_pos = mms->incoming_packet_seq * mms->asf_context.packet_size +
            mms->asf_header_read_pos +
            (mms->media_packet_read_ptr - mms->media_packet_incoming_buffer);
			
			/* special case for no-op. returns current position in bytes */
			if(offset == 0)
				return cur_pos;
			
			offset += cur_pos;
			
			break;
		case SEEK_SET:
			break;
			
		default:
			return -1;
    }
	
    if(ff_mms_seek(h, -1, offset, AVSEEK_FLAG_BYTE))
        return -1;
    else
        return offset;
}

/*
 * Non-standard exposed API (currently only used by the ASF muxer)
 */

/** Set up reference to the current ASF demuxer
 * (for finding stream discard details). */
void ff_mms_set_stream_selection(URLContext *h, AVFormatContext *format)
{
    MMSContext *mms = h->priv_data;
    mms->av_format_ctx = format;
}

/** Like AVInputFormat::read_play().
 * @see AVInputFormat::read_play()
 */
int ff_mms_play(URLContext *h)
{
    MMSContext *mms = h->priv_data;
    int64_t stream_offset = 0;
	
    /* Early return if already playing. */
    if(mms->state == STREAMING || mms->state == AWAITING_STREAM_START_PACKET)
        return 0;
	
#if (MMS_DEBUG_LEVEL>0)
    fprintf(stderr, "MMS Play\n");
#endif
	
    /* If resuming from pause */
    if(mms->state==STREAM_PAUSED)
        stream_offset = (mms->pause_resume_seq+1) * mms->asf_context.packet_size;
	
    clear_stream_buffers(mms);
	
    return mms->protocol->play_from(mms, 1, stream_offset, 0);
}

/** Like AVInputFormat::read_pause().
 * @see AVInputFormat::read_pause()
 */
int ff_mms_pause(URLContext *h)
{
    MMSContext *mms = h->priv_data;
	
#if (MMS_DEBUG_LEVEL>0)
    fprintf(stderr, "MMS Pause\n");
#endif
	
    if(mms->state==STREAMING || mms->state==STREAM_DONE) {
        // send the closedown message...
#if (MMS_DEBUG_LEVEL>0)
        fprintf(stderr, "Requesting pause in state: %s\n", state_names[mms->state]);
#endif
        mms->protocol->pause(mms);
		
        // wait until the server acknowledges...
        while(mms->state != STREAM_PAUSED && mms->state != STATE_ERROR)
            ff_mms_packet_state_machine(mms);
		
        if(mms->state == STATE_ERROR)
            return AVERROR(EIO);
    } else {
#if (MMS_DEBUG_LEVEL>0)
        fprintf(stderr, "Tried a pause when the state was not streaming: %s (%d)\n", state_names[mms->state], mms->state);
#endif
    }
	
    return 0;
}

/** Like AVInputFormat::read_seek().
 * @see AVInputFormat::read_seek()
 */
int ff_mms_seek(URLContext *h,
				int stream_index, int64_t timestamp, int flags)
{
    MMSContext *mms = h->priv_data;
    int err= 0;
	
    /* a custom interface to enable trick play (ffwd/rewind) */
#if 0
    if(mms->protocol_type==PROTOCOL_TYPE_HTTP && timestamp==-1) {
        int rate;
		
        if(mms->http_play_rate != 1) {
			fprintf(stderr, "Turning off ffwd or rewind\n");
            rate= 1;
            flags &= ~AVSEEK_FLAG_BACKWARD;
        } else {
            rate= 5;
			fprintf(stderr, "Turning on ffwd or rewind\n");
        }
		
		//        err= mms_read_fast_forward_or_rewind(s, rate, timestamp, flags); FIXME
    } else {
#endif
        if(! mms->seekable)
            return AVERROR(EBADF);
		
#if (MMS_DEBUG_LEVEL>0)
        fprintf(stderr, "ff_mms_seek stream: %d Timestamp: %lld flags: %d (byte: %d)\n", stream_index, timestamp, flags, flags & AVSEEK_FLAG_BYTE);
#endif
        err = ff_mms_pause(h);
        if(err == 0) {
#if (MMS_DEBUG_LEVEL>0)
            fprintf(stderr, "paused!\n");
#endif
            clear_stream_buffers(mms); // Seeking implies flushing input buffers
			
            if(flags & AVSEEK_FLAG_BYTE)
                err = mms_stream_play_from_offset(mms, FFMAX(0, timestamp));
            else
                err = mms_stream_play_from_timestamp(mms, FFMAX(0, timestamp));
#if (MMS_DEBUG_LEVEL>0)
            fprintf(stderr, "Sent packets!\n");
#endif
        }
		
        /* TODO: correctly handle mmst seek during pause */
#if 0
    }
#endif
	
    return err;
}

#if 0
/** (Untested since we became a protocol) */
int ff_mms_read_fast_forward_or_rewind(URLContext *h,
									   int rate,
									   int64_t timestamp,
									   int flags)
{
    MMSContext *mms = h->priv_data;
    int result= 0;
	
    if(flags & AVSEEK_FLAG_BACKWARD) {
        rate= -rate;
    }
	
    ff_mms_pause(h);
    result= play_from(mms, rate, -1, timestamp);
	
    return result;
}
#endif

/** Generic MMS.
 * TODO: should try mmsu, mmst, mmsh and possibly rtsp in turn
 */
URLProtocol mms_protocol = {
"mms",
mms_open,
mms_read,
NULL, // write
mms_seek,
mms_close,
};

/** MMS over TCP. */
URLProtocol mmst_protocol = {
"mmst",
mms_open,
mms_read,
NULL, // write
mms_seek,
mms_close,
};

/** MMS over HTTP. */
URLProtocol mmsh_protocol = {
"mmsh",
mms_open,
mms_read,
NULL, // write
mms_seek,
mms_close,
};
