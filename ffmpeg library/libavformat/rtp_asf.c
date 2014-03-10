/*
 * Microsoft RTP/ASF support.
 * Copyright (c) 2008 Ronald S. Bultje
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

/**
 * @file rtp-asf.c
 * @brief Microsoft RTP/ASF support
 * @author Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 */

#include "libavutil/base64.h"
#include "libavutil/avstring.h"
//#include "libavcodec/internal.h"
#include "rtp.h"
#include "rtp_internal.h"
#include "rtp_asf.h"
#include "rtsp.h"
#include "asf.h"

void
ff_wms_parse_sdp_a_line (AVFormatContext *s, const char *p)
{
    if (av_strstart(p, "pgmpu:data:application/vnd.ms.wms-hdr.asfv1;base64,", &p)) {
        ByteIOContext gb;
        RTSPState *rt = s->priv_data;
        int len = strlen(p) * 6 / 8;
        char *buf = av_mallocz(len);
        av_base64_decode(buf, p, len);

        init_put_byte(&gb, buf, len, 0, NULL, NULL, NULL, NULL);
        if (rt->asf_ctx) {
            av_close_input_stream(rt->asf_ctx);
            rt->asf_ctx = NULL;
        }
        av_open_input_stream(&rt->asf_ctx, &gb, "", &asf_demuxer, NULL);
        rt->asf_gb_pos = url_ftell(&gb);
        av_free(buf);
    }
}

static int
asfrtp_parse_sdp_line (AVFormatContext *s, int stream_index,
                       PayloadContext *asf, const char *line)
{
    if (av_strstart(line, "stream:", &line)) {
        RTSPState *rt = s->priv_data;

        s->streams[stream_index]->id = strtol(line, NULL, 10);

        if (rt->asf_ctx) {
            int i;

            for (i = 0; i < rt->asf_ctx->nb_streams; i++) {
                if (s->streams[stream_index]->id == rt->asf_ctx->streams[i]->id) {
                    *s->streams[stream_index]->codec =
                        *rt->asf_ctx->streams[i]->codec;
                    s->streams[stream_index]->codec->extradata =
                        av_malloc(s->streams[stream_index]->codec->extradata_size);
                    memcpy(s->streams[stream_index]->codec->extradata,
                           rt->asf_ctx->streams[i]->codec->extradata,
                           s->streams[stream_index]->codec->extradata_size);
                    av_set_pts_info(s->streams[stream_index], 32, 1, 1000);
                }
           }
        }
    }

    return 0;
}

struct PayloadContext {
    ByteIOContext gb;
    char *buffer;
};

/**< return 0 on packet, no more left, 1 on packet, 1 on partial packet... */
static int
asfrtp_parse_packet (PayloadContext *asf, AVStream *st,
                     AVPacket *pkt, uint32_t *timestamp,
                     const uint8_t *buf, int len, int flags)
{
    ByteIOContext *gb = &asf->gb;
    int res, mflags, len_off;
    RTSPStream *rtsp_st = st->priv_data;
    AVFormatContext *s = rtsp_st->tx_ctx->ic;
    RTSPState *rt = s->priv_data;

    if (!rt->asf_ctx)
        return -1;

    if (len > 0) {
        if (len < 4)
            return -1;

        av_freep(&asf->buffer);
        asf->buffer = av_malloc(len);
        memcpy(asf->buffer, buf, len);

        init_put_byte(gb, asf->buffer, len, 0, NULL, NULL, NULL, NULL);
        mflags = get_byte(gb);
        if (mflags & 0x80)
            flags |= PKT_FLAG_KEY;
        len_off = (get_byte(gb)<<16)|(get_byte(gb)<<8)|get_byte(gb);
        if (mflags & 0x20) /* relative timestamp */
            url_fskip(gb, 4);
        if (mflags & 0x10) /* has duration */
            url_fskip(gb, 4);
        if (mflags & 0x8) /* has location ID */
            url_fskip(gb, 4);

        if (!(mflags & 0x40) || len_off != len) {
            ff_log_missing_feature(s,
                "RTSP-MS packet concatenation and splitting", 1);
            return -1;
        }

        gb->pos -= url_ftell(gb);
        gb->pos += rt->asf_gb_pos;
        res = ff_asf_get_packet(rt->asf_ctx, gb);
        rt->asf_gb_pos = url_ftell(gb);
        if (res < 0)
            return res;
    }

    for (;;) {
        int i;

        res = ff_asf_parse_packet(rt->asf_ctx, gb, pkt);
        rt->asf_gb_pos = url_ftell(gb);
        if (res != 0)
            break;
        for (i = 0; i < s->nb_streams; i++) {
            if (s->streams[i]->id == rt->asf_ctx->streams[pkt->stream_index]->id) {
                pkt->stream_index = i;
                return 1; // FIXME: return 0 if last packet
            }
        }
        av_free_packet(pkt);
    }

    return res == 1 ? -1 : res;
}

static PayloadContext *
asfrtp_new_extradata (void)
{
    return av_mallocz(sizeof(PayloadContext));
}

static void
asfrtp_free_extradata (PayloadContext *asf)
{
    av_freep(&asf->buffer);
    av_free(asf);
}

#define RTP_ASF_HANDLER(n, s, t) \
RTPDynamicProtocolHandler ff_ms_rtp_ ## n ## _handler = { \
    s, \
    t, \
    CODEC_ID_NONE, \
    asfrtp_parse_sdp_line, \
    asfrtp_new_extradata,  \
    asfrtp_free_extradata, \
    asfrtp_parse_packet,   \
};

RTP_ASF_HANDLER(asf_pfv, "x-asf-pf",  CODEC_TYPE_VIDEO);
RTP_ASF_HANDLER(asf_pfa, "x-asf-pf",  CODEC_TYPE_AUDIO);
RTP_ASF_HANDLER(asf_pfs, "x-asf-pf",  CODEC_TYPE_SUBTITLE); /* AJS ADDED FOR SUBTITLES */
RTP_ASF_HANDLER(wms_rtx, "x-wms-rtx", CODEC_TYPE_DATA);
