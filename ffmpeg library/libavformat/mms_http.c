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

/** @file mms_http.c
 * MMS protocol over HTTP.
 * @author Ryan Martell
 * @author Björn Axelsson
 */
#include "mms_private.h"

/* #define USERAGENT "User-Agent: NSPlayer/7.1.0.3055\r\n" */
#define USERAGENT "User-Agent: NSPlayer/4.1.0.3856\r\n"

#define CHUNK_HEADER_LENGTH         4
#define CHUNK_TYPE_ASF_HEADER  0x4824
#define CHUNK_TYPE_DATA        0x4424

#define CHUNK_TYPE_END         0x4524
#define CHUNK_TYPE_RESET       0x4324

#define CHUNK_TYPE_HTTP_HEADER_START      0x5448 // TH
#define CHUNK_TYPE_HTTP_HEADER_SECOND     0x5054 // PT

#define CHUNK_SIZE              65536  /* max chunk size */


static int request_streaming_from(MMSContext *mms, int rate, int64_t stream_offset, int64_t time_offset);

/** Read a single HTTP response line. */
static int get_http_line(ByteIOContext *context, char *buffer, int max_size)
{
    char *dst = buffer;
    int ch;
    int done = 0;

    while(!done) {
        ch = get_byte(context);
        if(ch == '\n') {
            done = 1;
        } else if(ch == '\r') {
            // eat it.
        } else if(dst-buffer < max_size-1) {
            *dst++ = ch;
        }
    }
    *dst='\0';

    return buffer-dst;
}

/** Try to match match_str as a prefix of src.
 * @param src String to search in.
 * @param match_str String to search for.
 * @return NULL if no match, pointer to the end of the prefix in src if match.
 */
static const char *matches(const char *src, const char *match_str)
{
    int match_length = strlen(match_str);

    if(strncmp(src, match_str, match_length) == 0)
        return src + match_length;
    else
        return NULL;
}

/** Try to match match_str as a substring of src.
 * @param src String to search in.
 * @param match_str String to search for.
 * @return NULL if no match, pointer to the end of the substring in src if match.
 */
static const char *matches_anywhere(const char *src, const char *match_str)
{
    const char *match = NULL;

    if((match = strstr(src, match_str)) != 0)
        match += strlen(match_str);

    return match;
}

/** Parse the header of a MMSH chunk. */
static int get_http_chunk_header(MMSContext *mms, int *chunk_type, int *chunk_length)
{
    int unknown0, unknown1, chunk_seq_number, chunk_data_len2;
    int ext_length;

    /* read chunk header */
    *chunk_type= get_le16(&mms->incoming_io_buffer);
    *chunk_length= get_le16(&mms->incoming_io_buffer);
    switch (*chunk_type) {
    case CHUNK_TYPE_DATA:
        mms->incoming_packet_seq = get_le32(&mms->incoming_io_buffer);
        unknown0 = get_byte(&mms->incoming_io_buffer); // unknown;
        unknown1 = get_byte(&mms->incoming_io_buffer); //
        chunk_data_len2 = get_le16(&mms->incoming_io_buffer);
        ext_length = 8;
        break;
    case CHUNK_TYPE_END:
        chunk_seq_number = get_le32(&mms->incoming_io_buffer);
        ext_length = 4;
        break;
    case CHUNK_TYPE_ASF_HEADER:
        mms->incoming_packet_seq = get_le32(&mms->incoming_io_buffer);
        unknown0 = get_byte(&mms->incoming_io_buffer); // unknown;
        unknown1 = get_byte(&mms->incoming_io_buffer); //
        chunk_data_len2 = get_le16(&mms->incoming_io_buffer);
        ext_length = 8;
        break;
    case CHUNK_TYPE_RESET:
        get_le32(&mms->incoming_io_buffer); // unknown
        ext_length = 4;
        break;

    case CHUNK_TYPE_HTTP_HEADER_START:
        ext_length = 0;
        break;

    default:
        ext_length = 0;
        break;
    }

    /* display debug infos */
#if (MMS_DEBUG_LEVEL>2)
    switch (*chunk_type) {
    case CHUNK_TYPE_DATA:
        fprintf(stderr, "chunk type:       CHUNK_TYPE_DATA\n");
        fprintf(stderr, "chunk length:     %d\n", *chunk_length);
        fprintf(stderr, "packet sequence:  %d\n", mms->incoming_packet_seq);
        fprintf(stderr, "unknown0:         %d\n", unknown0);
        fprintf(stderr, "unknown1:         %d\n", unknown1);
        fprintf(stderr, "len2:             %d\n", chunk_data_len2);
        break;
    case CHUNK_TYPE_END:
        fprintf(stderr, "chunk type:       CHUNK_TYPE_END\n");
        fprintf(stderr, "continue: %d\n",  chunk_seq_number);
        break;
    case CHUNK_TYPE_ASF_HEADER:
        fprintf(stderr, "chunk type:       CHUNK_TYPE_ASF_HEADER\n");
        fprintf(stderr, "chunk length:     %d\n", *chunk_length);
        fprintf(stderr, "packet sequence:  %d\n", mms->incoming_packet_seq);
//            fprintf(stderr, "unknown:          %2X %2X %2X %2X %2X %2X\n",
//                     ext_header[0], ext_header[1], ext_header[2], ext_header[3],
//                     ext_header[4], ext_header[5]);
        fprintf(stderr, "len2:             %d\n", chunk_data_len2);
        break;
    case CHUNK_TYPE_RESET:
        fprintf(stderr, "chunk type:       CHUNK_TYPE_RESET\n");
        fprintf(stderr, "chunk seq:        %d\n", chunk_seq_number);
//            fprintf(stderr, "unknown:          %2X %2X %2X %2X\n",
//                     ext_header[0], ext_header[1], ext_header[2], ext_header[3]);
        break;

    case CHUNK_TYPE_HTTP_HEADER_START:
        fprintf(stderr, "chunk type:    CHUNK_TYPE_HTTP_HEADER_START\n");
        fprintf(stderr, "length:      %d\n", *chunk_length);
        break;

    default:
        fprintf(stderr, "unk. chunk:    %4X\n", *chunk_type);
        fprintf(stderr, "unk. len:      %4X\n", *chunk_length);
    }
#endif
    *chunk_length -= ext_length;

    return 1;
}

/** Parse MMSH server response. Normally a media or header chunk when streaming, and/or
 * a HTTP response header after a command or (re)connection.
 */
static MMSSCPacketType get_http_server_response(MMSContext *mms)
{
    MMSSCPacketType packet_type = -1;

/*
     HTTP/1.0 200 OK
     Content-Type: application/vnd.ms.wms-hdr.asfv1
     Server: Cougar/9.01.01.3814
     Content-Length: 5580
     Date: Thu, 01 Mar 2007 03:22:31 GMT
     Pragma: no-cache, client-id=551232529, xResetStrm=1, features="broadcast"
     Cache-Control: no-cache, x-wms-stream-type="broadcast"
     Last-Modified: Sat, 30 Dec 1899 00:00:00 GMT
     Supported: com.microsoft.wm.srvppair, com.microsoft.wm.sswitch, com.microsoft.wm.predstrm, com.microsoft.wm.fastcache, com.microsoft.wm.startupprofile


*/
//    fprintf(stderr, "Get http server response!\n");
    if(mms->state==AWAITING_ASF_HEADER || mms->state==AWAITING_STREAM_ID_ACCEPTANCE || mms->state==AWAITING_PAUSE_ACKNOWLEDGE) {
        char line[512];
        int content_length = 0;
        char content_type[128] = "";
        int line_number = 0;

        /* Parse HTTP response header */
        while(get_http_line(&mms->incoming_io_buffer, line, sizeof(line))) {
            const char *match;

            if(mms->state==AWAITING_PAUSE_ACKNOWLEDGE && line_number==0) {
                // the chunking system (below) read the HTTP, so we reinsert it here.
                memmove(line+4, line, strlen(line));
                line[0]= 'H'; line[1]= 'T'; line[2]= 'T'; line[3]= 'P';
            }

#if (MMS_DEBUG_LEVEL>0)
            fprintf(stderr, "%d-> \"%s\"\n", line_number, line);
#endif

            if((match= matches(line, "HTTP/")) != 0) {
                char http_status[50];
                int main_version, minor_version;
                int http_code;

                if (sscanf(match, "%d.%d %d %50s", &main_version, &minor_version,
                           &http_code, http_status)==4) {
                    if(http_code>=200 && http_code<300) {
                        // success!
                        if(mms->state==AWAITING_PAUSE_ACKNOWLEDGE) {
                            // we got what we needed, let the state machine handle this packet for us.
                            packet_type= SC_PACKET_STREAM_STOPPED_TYPE;
                        }
                    } else if(http_code>=300 && http_code<400) {
                        //
                        fprintf(stderr, "mmsh: 3xx redirection not implemented: >%d %s<\n", http_code, http_status);
                        ff_mms_set_state(mms, STATE_ERROR);
                        break;
                    }
                } else {
                    ff_mms_set_state(mms, STATE_ERROR);
                    break;
                }
            } else if((match= matches(line, "Content-Type: ")) != 0) {
                strncpy(content_type, match, sizeof(content_type));
//                fprintf(stderr, "Content Type: \"%s\"\n", content_type);
            } else if((match= matches(line, "Content-Length: ")) != 0) {
                content_length= atoi(match);
//                fprintf(stderr, "Content Length: %d\n", content_length);
            } else if((match= matches(line, "Pragma: ")) != 0) {
                const char *features;

                if((features= matches_anywhere(match, "features=")) != 0) {
                    const char *seekable, *broadcast;

                    seekable= matches_anywhere(features, "seekable");
                    broadcast= matches_anywhere(features, "broadcast");

                    if(seekable && !broadcast) {
//                        fprintf(stderr, "Seekable stream!");
                        mms->seekable= 1;
                    }
//                    if(broadcast) fprintf(stderr, "Broadcast stream!");
                }

                if((features= matches_anywhere(match, "client-id=")) != 0) {
                    char id[64];
                    const char *src= features;
                    char *dst= id;

                    while(*src && *src !=',' && (src-features<sizeof(id)-1)) *dst++= *src++;
                    *dst='\0';

                    mms->http_client_id= atoi(id);
                }
            }

            line_number++;
        }

        /* Handle content information */
        if(strcmp(content_type, "application/x-mms-framed") == 0 ||
                strcmp(content_type, "application/octet-stream") == 0 ||
                strcmp(content_type, "application/vnd.ms.wms-hdr.asfv1") == 0) {
            int length_read, chunk_type, chunk_length;

            while(get_http_chunk_header(mms, &chunk_type, &chunk_length)) {
                if(chunk_type==CHUNK_TYPE_ASF_HEADER || chunk_type==CHUNK_TYPE_DATA) {
                    if(chunk_length<sizeof(mms->media_packet_incoming_buffer)) {
                        length_read = get_buffer(&mms->incoming_io_buffer, mms->media_packet_incoming_buffer, chunk_length);
                        if(length_read == chunk_length) {
                            mms->media_packet_read_ptr = mms->media_packet_incoming_buffer;
                            mms->media_packet_buffer_length = length_read;

                            //fprintf(stderr, "First Chunks (not streaming yet): media_packet_buffer_length set to %d (Type: 0x%x)\n", length_read, chunk_type);
                            // Handle header or media data
                            if(chunk_type == CHUNK_TYPE_ASF_HEADER) {
                                packet_type = SC_PACKET_ASF_HEADER_TYPE;
                                mms->incoming_flags = 0X08; // force the bit set saying we got the entire header.

                                if(!mms->header_parsed) {
                                    // Store the asf header
                                    mms->asf_header = av_realloc(mms->asf_header, mms->media_packet_buffer_length);
                                    memcpy(mms->asf_header, mms->media_packet_read_ptr, mms->media_packet_buffer_length);
                                    mms->asf_header_size = mms->media_packet_buffer_length;
                                }

                                // Break out after the header on the first request.
                                if(mms->state == AWAITING_ASF_HEADER)
                                {
                                    if(strcmp(content_type, "application/vnd.ms.wms-hdr.asfv1") == 0 ||
                                            strcmp(content_type, "application/octet-stream") == 0)
                                        break;
                                }
                            } else {
                                packet_type = SC_PACKET_ASF_MEDIA_TYPE;

                                // get the stream packets...
                                ff_mms_set_state(mms, STREAMING);
                                break;
                            }
                        }
                    } else {
                        ff_mms_set_state(mms, STATE_ERROR);
                        break;
                    }
                } else {
//                    fprintf(stderr, "Unknown chunk type1: %d\n", chunk_type);
                }
            }
        } else {
            if(content_length > 0)
            {
                fprintf(stderr, "Unknown Content-Type: %s\n", content_type);
                ff_mms_set_state(mms, STATE_ERROR);
            }
        }
    } else if(mms->state==STREAMING || mms->state==AWAITING_HTTP_PAUSE_CONTROL_ACKNOWLEDGE) {
        int length_read, chunk_type, chunk_length;

        while(get_http_chunk_header(mms, &chunk_type, &chunk_length)) {
            if(chunk_type == CHUNK_TYPE_ASF_HEADER || chunk_type == CHUNK_TYPE_DATA) {
                // read it to ignore it....
                if(chunk_length<sizeof(mms->media_packet_incoming_buffer)) {
                    length_read = get_buffer(&mms->incoming_io_buffer, mms->media_packet_incoming_buffer, chunk_length);

                    if(length_read == chunk_length) {
//fprintf(stderr, "Secondary Chunks: media_packet_buffer_length set to %d (Type: 0x%x)\n", length_read, chunk_type);
                        mms->media_packet_read_ptr = mms->media_packet_incoming_buffer;
                        mms->media_packet_buffer_length = length_read;
                        // we don't break out of here, because we don't want the header this time
                        if(chunk_type == CHUNK_TYPE_DATA) {
                            packet_type = SC_PACKET_ASF_MEDIA_TYPE;
                            break;
                        }
                    }
                } else {
                    ff_mms_set_state(mms, STATE_ERROR);
                    break;
                }
            } else if(chunk_type==CHUNK_TYPE_HTTP_HEADER_START && chunk_length==CHUNK_TYPE_HTTP_HEADER_SECOND) {
                if(mms->state==AWAITING_HTTP_PAUSE_CONTROL_ACKNOWLEDGE) {
                    // we got the start of the header. break out and change state...
                    packet_type= SC_PACKET_HTTP_CONTROL_ACKNOWLEDGE;
                } else {
                    ff_mms_set_state(mms, STATE_ERROR);
                }
                break;
            } else if (chunk_type==CHUNK_TYPE_END) {
                packet_type = SC_PACKET_STREAM_STOPPED_TYPE;
                break;
            } else {
                fprintf(stderr, "Unknown chunk type: %d Length: %d\n", chunk_type, chunk_length);
                packet_type= -1; // the connection may have closed.
                break;
            }
        }
    } else {
#if (MMS_DEBUG_LEVEL>0)
//        fprintf(stderr, "Asking for http packet in state %s\n", state_names[mms->state]);
#endif
        packet_type= -1;
    }
//    fprintf(stderr, "Returning %d from  http server response!\n", packet_type);

    return packet_type;
}

/**
 * Build a http GET request for streaming.
 * For mms over http, the byte offset seem to include the header.
 * @param rate -5, 1, 5, for rewind, play, ffwd (seekable stream).
 * @param stream_offset Seek byte offset (valid on pause/play, rewind/ffwd/seek use time). Overrides seek_offset. Use -1 to disable.
 * @param seek_offset Seek time in ms (valid on rewind, ffwd, seek).
 */
static int build_http_stream_request(MMSContext *mms,
                                     char *outgoing_data,
                                     int max_length,
                                     int rate,
                                     int64_t stream_offset,
                                     int64_t seek_offset)
{
    char *dst= outgoing_data;
    int ii;
    static const char* header_part =
        "GET %s HTTP/1.0\r\n"
        "Accept: */*\r\n"
        USERAGENT
        "Host: %s\r\n";

    snprintf(dst, max_length-(dst-outgoing_data), header_part, mms->path, mms->host);
    dst+= strlen(dst);
    if(mms->seekable) {
        int time_offset= (int) seek_offset; // should be okay.
        static const char* seekable_part =
            "Pragma: no-cache,rate=%d.000000,stream-time=%u,stream-offset=%u:%u,request-context=%u,max-duration=%u\r\n"
            "Pragma: xClientGUID={%s}\r\n"
            "Pragma: xPlayStrm=1\r\n";
        snprintf(dst, max_length-(dst-outgoing_data), seekable_part,
                 rate, time_offset, (int) (stream_offset>>32), (int)(stream_offset&0xffffffff), 2, 0, mms->local_guid);
        dst += strlen(dst);
    } else {
        static const char* live_part =
            "Pragma: no-cache,rate=1.000000,request-context=%u\r\n"
            "Pragma: xPlayStrm=1\r\n"
            "Pragma: xClientGUID={%s}\r\n";

        snprintf(dst, max_length-(dst-outgoing_data), live_part, 2, mms->local_guid);
        dst+= strlen(dst);
    }

    snprintf(dst, max_length-(dst-outgoing_data),
            "Pragma: client-id=%d\r\n"
            "Pragma: stream-switch-count=%d\r\n"
            "Pragma: stream-switch-entry=", mms->http_client_id, mms->av_format_ctx->nb_streams);
    dst+= strlen(dst);

    for(ii= 0; ii<mms->av_format_ctx->nb_streams; ii++) {
        AVStream *st= mms->av_format_ctx->streams[ii];
        snprintf(dst, max_length-(dst-outgoing_data), "ffff:%d:%d ", st->id, ff_mms_stream_selection_code(st));
        dst+= strlen(dst);
    }

    snprintf(dst, max_length-(dst-outgoing_data), "\r\nConnection: Close\r\n\r\n");
    dst+= strlen(dst);

#if (MMS_DEBUG_LEVEL>0)
    fprintf(stderr, "Stream setup request:\n%s", outgoing_data);
#endif

    return strlen(outgoing_data);
}

/** Send a MMSH stream request. */
static int request_streaming_from(MMSContext *mms, int rate, int64_t stream_offset, int64_t time_offset)
{
    int err= 0;
    char outgoing_data[1024];
    int exact_length, write_result;

    /* the http protocol allows for byte-precision seeking, but current servers
     * (Cougar/9.01.01.3814) throw an error if seeking into the header, and
     * will silently align offsets to max_packet_size */
    if(stream_offset >= 0)
        stream_offset += mms->asf_header_size;

    exact_length= build_http_stream_request(mms, outgoing_data, sizeof(outgoing_data), rate, stream_offset, time_offset);

    // reopen the connection...
    // TODO: can't we use HTTP 1.1 keepalive?
    if((err= ff_mms_open_connection(mms))==0) {
        write_result= url_write(mms->mms_hd, outgoing_data, exact_length);
        if(write_result != exact_length) {
#if (MMS_DEBUG_LEVEL>0)
            fprintf(stderr, "url_write returned: %d != %d\n", write_result, exact_length);
#endif
        } else {
            // remember our rate.
            mms->http_play_rate= rate;
        }
    }

    if(err==0) {
        ff_mms_set_state(mms, AWAITING_STREAM_ID_ACCEPTANCE);

        while(mms->state != STREAMING && mms->state != STATE_ERROR && mms->state != STREAM_DONE) {
            ff_mms_packet_state_machine(mms);
        }
    } else {
        ff_mms_set_state(mms, STATE_ERROR);
    }

    if(mms->state==STATE_ERROR)
        err= AVERROR_IO;

    return err;
}

/** Send the initial MMST or MMSH handshake. */
static int send_startup_request(MMSContext *mms)
{
    static const char* mmsh_FirstRequest =
        "GET %s HTTP/1.0\r\n"
        "Accept: */*\r\n"
        USERAGENT
        "Host: %s\r\n"
        "Pragma: no-cache,rate=1.000000,stream-time=0,stream-offset=0:0,request-context=%u,max-duration=0\r\n"
        "Pragma: xClientGUID={%s}\r\n"
        "Connection: Close\r\n\r\n";
    char outgoing_buffer[512];
    int write_result, exact_length;

    snprintf(outgoing_buffer, sizeof(outgoing_buffer), mmsh_FirstRequest, mms->path,
            mms->host, 1, mms->local_guid);

#if (MMS_DEBUG_LEVEL>0)
    fprintf(stderr, "First request:\n%s", outgoing_buffer);
#endif

    exact_length= strlen(outgoing_buffer);
    write_result= url_write(mms->mms_hd, outgoing_buffer, exact_length);
    if(write_result != exact_length) {
#if (MMS_DEBUG_LEVEL>0)
        fprintf(stderr, "url_write returned: %d != %d\n", write_result, exact_length);
#endif
        ff_mms_set_state(mms, STATE_ERROR);
    } else
        ff_mms_set_state(mms, AWAITING_ASF_HEADER);
    return write_result;
}

static int send_pause_request(MMSContext *mms)
{
    //            X-Accept-Authentication: Negotiate, NTLM, Digest, Basic
    //Pragma: client-id=4083067693
    static const char* mmsh_PauseRequest =
        "POST %s HTTP/1.0\r\n"
        "Accept: */*\r\n"
        USERAGENT
        "Host: %s\r\n"
        "Pragma: xClientGUID={%s}\r\n"
        "Pragma: client-id=%d\r\n"
        "Pragma: xStopStrm=1\r\n"
        "Content-Length: 0\r\n\r\n";
    char outgoing_buffer[512];
    int write_result, exact_length;

    snprintf(outgoing_buffer, sizeof(outgoing_buffer), mmsh_PauseRequest, mms->path,
            mms->host, mms->local_guid, mms->http_client_id);

#if (MMS_DEBUG_LEVEL>0)
    fprintf(stderr, "Pause request:\n%s", outgoing_buffer);
#endif

    exact_length= strlen(outgoing_buffer);
    write_result= url_write(mms->mms_hd, outgoing_buffer, exact_length);
    if(write_result != exact_length) {
#if (MMS_DEBUG_LEVEL>0)
        fprintf(stderr, "url_write returned: %d != %d\n", write_result, exact_length);
#endif
        ff_mms_set_state(mms, STATE_ERROR);
    } else
        ff_mms_set_state(mms, AWAITING_HTTP_PAUSE_CONTROL_ACKNOWLEDGE);
    return write_result;
}

MMSProtocol mmsh_mmsprotocol = {
    .default_port = 80,
    .send_startup_message = send_startup_request,
    .play_from = request_streaming_from,
    .pause = send_pause_request,
    .get_server_response = get_http_server_response,
};
