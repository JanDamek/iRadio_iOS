/*
 * WMA 9/3/PRO compatible decoder
 * Copyright (c) 2007 Baptiste Coudurier, Benjamin Larsson, Ulion
 * Copyright (c) 2008 - 2009 Sascha Sommer, Benjamin Larsson
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
 * @file  libavcodec/wma3dec.c
 * @brief wmapro decoder implementation
 * Wmapro is an MDCT based codec comparable to wma standard or AAC.
 * The decoding therefore consist of the following steps:
 * - bitstream decoding
 * - reconstruction of per channel data
 * - rescaling and requantization
 * - IMDCT
 * - windowing and overlapp-add
 *
 * The compressed wmapro bitstream is split into individual packets.
 * Every such packet contains one or more wma frames.
 * The compressed frames may have a variable length and frames may
 * cross packet boundaries.
 * Common to all wmapro frames is the number of samples that are stored in
 * a frame.
 * The number of samples and a few other decode flags are stored
 * as extradata that has to be passed to the decoder.
 *
 * The wmapro frames themselves are again split into a variable number of
 * subframes. Every subframe contains the data for 2^N time domain samples
 * where N varies between 7 and 12.
 *
 * The frame layouts for the individual channels of a wma frame does not need
 * to be the same.
 * However, if the offsets and lengths of several subframes of a frame are the
 * same, the subframes of the channels can be grouped.
 * Every group may then use special coding techniques like M/S stereo coding
 * to improve the compression ratio. These channel transformations do not
 * need to be applied to a whole subframe. Instead, they can also work on
 * individual scale factor bands (see below).
 * The coefficients that cary the audio signal in the frequency domain
 * are transmitted as huffman coded vectors with 4, 2 and 1 elements.
 * In addition to that, the encoder can switch to a run level coding scheme
 * by transmitting subframen_length / 128 zero coefficients.
 *
 * Before the audio signal can be converted to the time domain, the
 * coefficients have to be rescaled and inverse quantized.
 * A subframe is therefore split into several scale factor bands that get
 * scaled individually.
 * Scale factors are submitted for every frame but they might be shared
 * between the subframes of a channel. Scale factors are initially DPCM coded.
 * Once scale factors are shared, the differences are transmitted as run
 * level codes.
 * Every subframe length and offset combination in the frame layout shares a
 * common quantization factor that can be adjusted for every channel by a
 * modifier.
 * After the inverse quantization, the coefficients get processed by an IMDCT.
 * The resulting values are then windowed with a sine window and the first half
 * of the values are added to the second half of the output from the previous
 * subframe in order to reconstruct the output samples.
 */

#include "avcodec.h"
#include "internal.h"
 #include "avformat.h"
#include "bitstream.h"
//#include "get_bits.h"
#include "wma3data.h"
#include "dsputil.h"
#include "wma.h"

/** current decoder limitations */
#define WMAPRO_MAX_CHANNELS    8                             ///< max number of handled channels
#define MAX_SUBFRAMES  32                                    ///< max number of subframes per channel
#define MAX_BANDS      29                                    ///< max number of scale factor bands
#define MAX_FRAMESIZE  16384                                 ///< maximum compressed frame size

#define WMAPRO_BLOCK_MAX_BITS 12                                           ///< log2 of max block size
#define WMAPRO_BLOCK_MAX_SIZE (1 << WMAPRO_BLOCK_MAX_BITS)                 ///< maximum block size
#define WMAPRO_BLOCK_SIZES    (WMAPRO_BLOCK_MAX_BITS - BLOCK_MIN_BITS + 1) ///< possible block sizes


#define VLCBITS            9
#define SCALEVLCBITS       8
#define VEC4MAXDEPTH    ((HUFF_VEC4_MAXBITS+VLCBITS-1)/VLCBITS)
#define VEC2MAXDEPTH    ((HUFF_VEC2_MAXBITS+VLCBITS-1)/VLCBITS)
#define VEC1MAXDEPTH    ((HUFF_VEC1_MAXBITS+VLCBITS-1)/VLCBITS)
#define SCALEMAXDEPTH   ((HUFF_SCALE_MAXBITS+SCALEVLCBITS-1)/SCALEVLCBITS)
#define SCALERLMAXDEPTH ((HUFF_SCALE_RL_MAXBITS+VLCBITS-1)/VLCBITS)

static VLC              sf_vlc;           ///< scale factor dpcm vlc
static VLC              sf_rl_vlc;        ///< scale factor run length vlc
static VLC              vec4_vlc;         ///< 4 coefficients per symbol
static VLC              vec2_vlc;         ///< 2 coefficients per symbol
static VLC              vec1_vlc;         ///< 1 coefficient per symbol
static VLC              coef_vlc[2];      ///< coefficient run length vlc codes
static float            sin64[33];        ///< sinus table for decorrelation

/**
 * @brief decoder context for a single channel
 */
typedef struct {
    int16_t  prev_block_len;                          ///< length of the previous block
    uint8_t  transmit_coefs;                          ///< transmit coefficients
    uint8_t  num_subframes;                           ///< number of subframes
    uint16_t subframe_len[MAX_SUBFRAMES];             ///< subframe length in samples
    uint16_t subframe_offset[MAX_SUBFRAMES];          ///< subframe position
    uint8_t  cur_subframe;                            ///< subframe index
    uint16_t channel_len;                             ///< channel length in samples
    uint16_t decoded_samples;                         ///< already processed samples
    uint8_t  grouped;                                 ///< channel is part of a group
    int      quant_step;                              ///< quantization step
    int8_t   transmit_sf;                             ///< transmit scale factors
    int8_t   reuse_sf;                                ///< share scale factors between subframes
    int8_t   scale_factor_step;                       ///< scaling step
    int      max_scale_factor;                        ///< maximum scale factor
    int      scale_factors[MAX_BANDS];                ///< scale factor values
    int      resampled_scale_factors[MAX_BANDS];      ///< scale factors from a previous block
    int16_t  scale_factor_block_len;                  ///< scale factor reference block length
    float*   coeffs;                                  ///< pointer to the decode buffer
    DECLARE_ALIGNED_16(float, out[2*WMAPRO_BLOCK_MAX_SIZE]); ///< output buffer
} WMA3ChannelCtx;

/**
 * @brief channel group for channel transformations
 */
typedef struct {
    uint8_t num_channels;                                     ///< number of channels in the group
    int8_t  transform;                                        ///< controls the type of the transform
    int8_t  transform_band[MAX_BANDS];                        ///< controls if the transform is enabled for a certain band
    float   decorrelation_matrix[WMAPRO_MAX_CHANNELS*WMAPRO_MAX_CHANNELS];
    float*  channel_data[WMAPRO_MAX_CHANNELS];                ///< transformation coefficients
} WMA3ChannelGroup;

/**
 * @brief main decoder context
 */
typedef struct WMA3DecodeContext {
    /** generic decoder variables */
    AVCodecContext*  avctx;                         ///< codec context for av_log
    DSPContext       dsp;                           ///< accelerated dsp functions
    uint8_t          frame_data[MAX_FRAMESIZE +
                      FF_INPUT_BUFFER_PADDING_SIZE];///< compressed frame data
    MDCTContext      mdct_ctx[WMAPRO_BLOCK_SIZES];  ///< MDCT context per block size
    DECLARE_ALIGNED_16(float, tmp[WMAPRO_BLOCK_MAX_SIZE]); ///< imdct output buffer
    float*           windows[WMAPRO_BLOCK_SIZES];   ///< window per block size
    int              coef_max[2];                   ///< max length of vlc codes

    /** frame size dependent frame information (set during initialization) */
    uint8_t          lossless;                      ///< lossless mode
    uint32_t         decode_flags;                  ///< used compression features
    uint8_t          len_prefix;                    ///< frame is prefixed with its length
    uint8_t          dynamic_range_compression;     ///< frame contains DRC data
    uint8_t          sample_bit_depth;              ///< bits per sample
    uint16_t         samples_per_frame;             ///< number of samples to output
    uint16_t         log2_frame_size;               ///< frame size
    int8_t           num_channels;                  ///< number of channels
    int8_t           lfe_channel;                   ///< lfe channel index
    uint8_t          max_num_subframes;             ///< maximum number of subframes
    int8_t           num_possible_block_sizes;      ///< nb of supported block sizes
    uint16_t         min_samples_per_subframe;      ///< minimum samples per subframe
    int8_t*          num_sfb;                       ///< scale factor bands per block size
    int16_t*         sfb_offsets;                   ///< scale factor band offsets
    int16_t*         sf_offsets;                    ///< scale factor resample matrix
    int16_t*         subwoofer_cutoffs;             ///< subwoofer cutoff values

    /** packet decode state */
    uint8_t          packet_sequence_number;        ///< current packet number
    int              num_saved_bits;                ///< saved number of bits
    int              frame_offset;                  ///< frame offset in the bit reservoir
    int              subframe_offset;               ///< subframe offset in the bit reservoir
    uint8_t          packet_loss;                   ///< set in case of bitstream error

    /** frame decode state */
    uint32_t         frame_num;                     ///< current frame number
    GetBitContext    gb;                            ///< bitstream reader context
    int              buf_bit_size;                  ///< buffer size in bits
    int16_t*         samples;                       ///< current samplebuffer pointer
    int16_t*         samples_end;                   ///< maximum samplebuffer pointer
    uint8_t          drc_gain;                      ///< gain for the DRC tool
    int8_t           skip_frame;                    ///< skip output step
    int8_t           parsed_all_subframes;          ///< all subframes decoded?

    /** subframe/block decode state */
    int16_t          subframe_len;                  ///< current subframe length
    int8_t           channels_for_cur_subframe;     ///< number of channels that contain the subframe
    int8_t           channel_indexes_for_cur_subframe[WMAPRO_MAX_CHANNELS];
    int16_t          cur_subwoofer_cutoff;          ///< subwoofer cutoff value
    int8_t           num_bands;                     ///< number of scale factor bands
    int16_t*         cur_sfb_offsets;               ///< sfb offsets for the current block
    int8_t           esc_len;                       ///< length of escaped coefficients

    uint8_t          num_chgroups;                  ///< number of channel groups
    WMA3ChannelGroup chgroup[WMAPRO_MAX_CHANNELS];  ///< channel group information

    WMA3ChannelCtx   channel[WMAPRO_MAX_CHANNELS];  ///< per channel data
} WMA3DecodeContext;

/**
 * init MDCT or IMDCT computation.
 */
static av_cold int AJS_ff_mdct_init(MDCTContext *s, int nbits, int inverse, double scale)
{
    int n, n4, i;
    double alpha, theta;
	
    memset(s, 0, sizeof(*s));
    n = 1 << nbits;
    s->nbits = nbits;
    s->n = n;
    n4 = n >> 2;
    s->tcos = av_malloc(n4 * sizeof(FFTSample));
    if (!s->tcos)
        goto fail;
    s->tsin = av_malloc(n4 * sizeof(FFTSample));
    if (!s->tsin)
        goto fail;
	
    theta = 1.0 / 8.0 + (scale < 0 ? n4 : 0);
    scale = sqrt(fabs(scale));
    for(i=0;i<n4;i++) {
        alpha = 2 * M_PI * (i + theta) / n;
        s->tcos[i] = -cos(alpha) * scale;
        s->tsin[i] = -sin(alpha) * scale;
    }
    if (ff_fft_init(&s->fft, s->nbits - 2, inverse) < 0)
        goto fail;
    return 0;
fail:
    av_freep(&s->tcos);
    av_freep(&s->tsin);
    return -1;
}

/**
 *@brief helper function to print the most important members of the context
 *@param s context
 */
static void av_cold dump_context(WMA3DecodeContext *s)
{
#define PRINT(a,b) av_log(s->avctx,AV_LOG_DEBUG," %s = %d\n", a, b);
#define PRINT_HEX(a,b) av_log(s->avctx,AV_LOG_DEBUG," %s = %x\n", a, b);

    PRINT("ed sample bit depth",s->sample_bit_depth);
    PRINT_HEX("ed decode flags",s->decode_flags);
    PRINT("samples per frame",s->samples_per_frame);
    PRINT("log2 frame size",s->log2_frame_size);
    PRINT("max num subframes",s->max_num_subframes);
    PRINT("len prefix",s->len_prefix);
    PRINT("num channels",s->num_channels);
    PRINT("lossless",s->lossless);
}

/**
 *@brief Uninitialize the decoder and free all resources.
 *@param avctx codec context
 *@return 0 on success, < 0 otherwise
 */
static av_cold int decode_end(AVCodecContext *avctx)
{
    WMA3DecodeContext *s = avctx->priv_data;
    int i;

    av_freep(&s->num_sfb);
    av_freep(&s->sfb_offsets);
    av_freep(&s->subwoofer_cutoffs);
    av_freep(&s->sf_offsets);

    for (i=0 ; i<WMAPRO_BLOCK_SIZES ; i++)
        ff_mdct_end(&s->mdct_ctx[i]);

    return 0;
}

/**
 *@brief Initialize the decoder.
 *@param avctx codec context
 *@return 0 on success, -1 otherwise
 */
static av_cold int decode_init(AVCodecContext *avctx)
{
    WMA3DecodeContext *s = avctx->priv_data;
    uint8_t *edata_ptr = avctx->extradata;
    int16_t* sfb_offsets;
    unsigned int channel_mask;
    int i;
    int log2_num_subframes;

    s->avctx = avctx;
    dsputil_init(&s->dsp, avctx);

    avctx->sample_fmt = SAMPLE_FMT_S16;

    /** FIXME: is this really the right thing to do for 24 bits? */
    s->sample_bit_depth = 16; // avctx->bits_per_sample;
    if (avctx->extradata_size >= 18) {
        s->decode_flags     = AV_RL16(edata_ptr+14);
        channel_mask    = AV_RL32(edata_ptr+2);
//        s->sample_bit_depth = AV_RL16(edata_ptr);
#ifdef DEBUG
        /** dump the extradata */
        for (i=0 ; i<avctx->extradata_size ; i++)
            av_log(avctx, AV_LOG_DEBUG, "[%x] ",avctx->extradata[i]);
        av_log(avctx, AV_LOG_DEBUG, "\n");
#endif

    } else {
        ff_log_ask_for_sample(avctx, "Unknown extradata size\n");
        return -1;
    }

    /** generic init */
    s->log2_frame_size = av_log2(avctx->block_align*8)+1;

    /** frame info */
    s->skip_frame = 1; /** skip first frame */
    s->packet_loss = 1;
    s->len_prefix = (s->decode_flags & 0x40) >> 6;

    if (!s->len_prefix) {
         ff_log_ask_for_sample(avctx, "no length prefix\n");
         return -1;
    }

    /** get frame len */
    s->samples_per_frame = 1 << ff_wma_get_frame_len_bits(avctx->sample_rate,
                                                          3, s->decode_flags);

    /** init previous block len */
    for (i=0;i<avctx->channels;i++)
        s->channel[i].prev_block_len = s->samples_per_frame;

    /** subframe info */
    log2_num_subframes = ((s->decode_flags & 0x38) >> 3);
    s->max_num_subframes = 1 << log2_num_subframes;
    s->num_possible_block_sizes = log2_num_subframes + 1;
    s->min_samples_per_subframe = s->samples_per_frame / s->max_num_subframes;
    s->dynamic_range_compression = (s->decode_flags & 0x80) >> 7;

    if (s->max_num_subframes > MAX_SUBFRAMES) {
        av_log(avctx, AV_LOG_ERROR, "invalid number of subframes %i\n",
                      s->max_num_subframes);
        return -1;
    }

    s->num_channels = avctx->channels;

    /** extract lfe channel position */
    s->lfe_channel = -1;

    if (channel_mask & 8) {
        unsigned int mask;
        for (mask=1;mask < 16;mask <<= 1) {
            if (channel_mask & mask)
                ++s->lfe_channel;
        }
    }

    if (s->num_channels < 0 || s->num_channels > WMAPRO_MAX_CHANNELS) {
        ff_log_ask_for_sample(avctx, "invalid number of channels\n");
        return -1;
    }

    INIT_VLC_STATIC(&sf_vlc, SCALEVLCBITS, HUFF_SCALE_SIZE,
                 scale_huffbits, 1, 1,
                 scale_huffcodes, 2, 2, 616);

    INIT_VLC_STATIC(&sf_rl_vlc, VLCBITS, HUFF_SCALE_RL_SIZE,
                 scale_rl_huffbits, 1, 1,
                 scale_rl_huffcodes, 4, 4, 1406);

    INIT_VLC_STATIC(&coef_vlc[0], VLCBITS, HUFF_COEF0_SIZE,
                 coef0_huffbits, 1, 1,
                 coef0_huffcodes, 4, 4, 2108);

    s->coef_max[0] = ((HUFF_COEF0_MAXBITS+VLCBITS-1)/VLCBITS);

    INIT_VLC_STATIC(&coef_vlc[1], VLCBITS, HUFF_COEF1_SIZE,
                 coef1_huffbits, 1, 1,
                 coef1_huffcodes, 4, 4, 3912);

    s->coef_max[1] = ((HUFF_COEF1_MAXBITS+VLCBITS-1)/VLCBITS);

    INIT_VLC_STATIC(&vec4_vlc, VLCBITS, HUFF_VEC4_SIZE,
                 vec4_huffbits, 1, 1,
                 vec4_huffcodes, 2, 2, 604);

    INIT_VLC_STATIC(&vec2_vlc, VLCBITS, HUFF_VEC2_SIZE,
                 vec2_huffbits, 1, 1,
                 vec2_huffcodes, 2, 2, 562);

    INIT_VLC_STATIC(&vec1_vlc, VLCBITS, HUFF_VEC1_SIZE,
                 vec1_huffbits, 1, 1,
                 vec1_huffcodes, 2, 2, 562);

    s->num_sfb = av_mallocz(sizeof(int8_t)*s->num_possible_block_sizes);
    s->sfb_offsets = av_mallocz(MAX_BANDS *
                                sizeof(int16_t) * s->num_possible_block_sizes);
    s->subwoofer_cutoffs = av_mallocz(sizeof(int16_t) *
                                      s->num_possible_block_sizes);
    s->sf_offsets = av_mallocz(MAX_BANDS * s->num_possible_block_sizes *
                               s->num_possible_block_sizes * sizeof(int16_t));

    if (!s->num_sfb ||
       !s->sfb_offsets || !s->subwoofer_cutoffs || !s->sf_offsets) {
        av_log(avctx, AV_LOG_ERROR,
                      "failed to allocate scale factor offset tables\n");
        decode_end(avctx);
        return -1;
    }

    /** calculate number of scale factor bands and their offsets
        for every possible block size */
    sfb_offsets = s->sfb_offsets;

    for (i=0;i<s->num_possible_block_sizes;i++) {
        int subframe_len = s->samples_per_frame >> i;
        int x;
        int band = 1;

        sfb_offsets[0] = 0;

        for (x=0;x < MAX_BANDS-1 && sfb_offsets[band-1] < subframe_len;x++) {
            int offset = (subframe_len * 2 * critical_freq[x])
                          / s->avctx->sample_rate + 2;
            offset -= offset % 4;
            if ( offset > sfb_offsets[band - 1] ) {
                sfb_offsets[band] = offset;
                ++band;
            }
        }
        sfb_offsets[band - 1] = subframe_len;
        s->num_sfb[i] = band - 1;
        sfb_offsets += MAX_BANDS;
    }


    /** Scale factors can be shared between blocks of different size
        as every block has a different scale factor band layout.
        The matrix sf_offsets is needed to find the correct scale factor.
     */

    for (i=0;i<s->num_possible_block_sizes;i++) {
        int b;
        for (b=0;b< s->num_sfb[i];b++) {
            int x;
            int offset = ((s->sfb_offsets[MAX_BANDS * i + b]
                         + s->sfb_offsets[MAX_BANDS * i + b + 1] - 1)<<i) >> 1;
            for (x=0;x<s->num_possible_block_sizes;x++) {
                int v = 0;
                while (s->sfb_offsets[MAX_BANDS * x + v +1] << x < offset)
                    ++v;
                s->sf_offsets[  i * s->num_possible_block_sizes * MAX_BANDS
                              + x * MAX_BANDS + b] = v;
            }
        }
    }

    /** init MDCT, FIXME: only init needed sizes */
    for (i = 0; i < WMAPRO_BLOCK_SIZES; i++)
        AJS_ff_mdct_init(&s->mdct_ctx[i],
                     BLOCK_MIN_BITS+1+i, 1, 1.0 / (1<<(BLOCK_MIN_BITS+i-1)));

    /** init MDCT windows: simple sinus window */
    for (i=0 ; i<WMAPRO_BLOCK_SIZES ; i++) {
        const int n = 1 << (WMAPRO_BLOCK_MAX_BITS - i);
        const int win_idx = WMAPRO_BLOCK_MAX_BITS - i - 7;
        ff_sine_window_init(ff_sine_windows[win_idx], n);
        s->windows[WMAPRO_BLOCK_SIZES-i-1] = ff_sine_windows[win_idx];
    }

    /** calculate subwoofer cutoff values */

    for (i=0;i< s->num_possible_block_sizes;i++) {
        int block_size = s->samples_per_frame / (1 << i);
        int cutoff = ceil(block_size * 440.0
                          / (double)s->avctx->sample_rate + 0.5);
        s->subwoofer_cutoffs[i] = av_clip(cutoff,4,block_size);
    }

    /** calculate sine values for the decorrelation matrix */
    for (i=0;i<33;i++)
        sin64[i] = sin(i*M_PI / 64.0);

    dump_context(s);
    avctx->channel_layout = channel_mask;
    return 0;
}

/**
 *@brief Decode how the data in the frame is split into subframes.
 *       Every WMA frame contains the encoded data for a fixed number of
 *       samples per channel. The data for every channel might be split
 *       into several subframes. This function will reconstruct the list of
 *       subframes for every channel.
 *
 *       If the subframes are not evenly split, the algorithm estimates the
 *       channels with the lowest number of total samples.
 *       Afterwards, for each of these channels a bit is read from the
 *       bitstream that indicates if the channel contains a frame with the
 *       next subframe size that is going to be read from the bitstream or not.
 *       If a channel contains such a subframe, the subframe size gets added to
 *       the channel's subframe list.
 *       The algorithm repeats these steps until the frame is properly divided
 *       between the individual channels.
 *
 *@param s context
 *@return 0 on success, < 0 in case of an error
 */
static int decode_tilehdr(WMA3DecodeContext *s)
{
    int c;
    int missing_samples = s->num_channels * s->samples_per_frame;

    /* should never consume more than 3073 bits (256 iterations for the
     * while loop when always the minimum amount of 128 samples is substracted
     * from missing samples in the 8 channel case)
     * 1 + BLOCK_MAX_SIZE * MAX_CHANNELS / BLOCK_MIN_SIZE * (MAX_CHANNELS  + 4)
     */

    /** reset tiling information */
    for (c=0;c<s->num_channels;c++) {
        s->channel[c].num_subframes = 0;
        s->channel[c].channel_len = 0;
    }

    /** handle the easy case with one constant-sized subframe per channel */
    if (s->max_num_subframes == 1) {
        for (c=0;c<s->num_channels;c++) {
            s->channel[c].num_subframes = 1;
            s->channel[c].subframe_len[0] = s->samples_per_frame;
            s->channel[c].channel_len = 0;
        }
    } else { /** subframe length and number of subframes is not constant */
        /** bits needed for the subframe length */
        int subframe_len_bits = 0;
        /** first bit indicates if length is zero */
        int subframe_len_zero_bit = 0;
        /** all channels have the same subframe layout */
        int fixed_channel_layout;

        fixed_channel_layout = get_bits1(&s->gb);

        /** calculate subframe len bits */
        if (s->lossless) {
            subframe_len_bits = av_log2(s->max_num_subframes - 1) + 1;
        } else if (s->max_num_subframes == 16) {
            subframe_len_zero_bit = 1;
            subframe_len_bits = 3;
        } else
            subframe_len_bits = av_log2(av_log2(s->max_num_subframes)) + 1;

        /** loop until the frame data is split between the subframes */
        while (missing_samples > 0) {
            unsigned int channel_mask = 0;
            int min_channel_len = s->samples_per_frame;
            int read_channel_mask = 1;
            int channels_for_cur_subframe = 0;
            int subframe_len;
            /** minimum number of samples that need to be read */
            int min_samples = s->min_samples_per_subframe;

            if (fixed_channel_layout) {
                read_channel_mask = 0;
                channels_for_cur_subframe = s->num_channels;
                min_samples *= channels_for_cur_subframe;
                min_channel_len = s->channel[0].channel_len;
            } else {
                /** find channels with the smallest overall length */
                for (c=0;c<s->num_channels;c++) {
                    if (s->channel[c].channel_len <= min_channel_len) {
                        if (s->channel[c].channel_len < min_channel_len) {
                            channels_for_cur_subframe = 0;
                            min_channel_len = s->channel[c].channel_len;
                        }
                        ++channels_for_cur_subframe;
                    }
                }
                min_samples *= channels_for_cur_subframe;

                if (channels_for_cur_subframe == 1 ||
                   min_samples == missing_samples)
                    read_channel_mask = 0;
            }

            /** For every channel with the minimum length, 1 bit
                might be transmitted that informs us if the channel
                contains a subframe with the next subframe_len. */
            if (read_channel_mask) {
                channel_mask = get_bits(&s->gb,channels_for_cur_subframe);
                if (!channel_mask) {
                    av_log(s->avctx, AV_LOG_ERROR,
                        "broken frame: zero frames for subframe_len\n");
                    return -1;
                }
            }

            /** if we have the choice get next subframe length from the
                bitstream */
            if (min_samples != missing_samples) {
                int log2_subframe_len = 0;
                /* 1 bit indicates if the subframe length is zero */
                if (subframe_len_zero_bit) {
                    if (get_bits1(&s->gb)) {
                        log2_subframe_len =
                            get_bits(&s->gb,subframe_len_bits-1);
                        ++log2_subframe_len;
                    }
                } else
                    log2_subframe_len = get_bits(&s->gb,subframe_len_bits);

                if (s->lossless) {
                    subframe_len =
                        s->samples_per_frame / s->max_num_subframes;
                    subframe_len *= log2_subframe_len + 1;
                } else {
                    subframe_len =
                        s->samples_per_frame / (1 << log2_subframe_len);
                }

                /** sanity check the length */
                if (subframe_len < s->min_samples_per_subframe
                    || subframe_len > s->samples_per_frame) {
                    av_log(s->avctx, AV_LOG_ERROR,
                        "broken frame: subframe_len %i\n", subframe_len);
                    return -1;
                }
            } else
                subframe_len = s->min_samples_per_subframe;

            for (c=0; c<s->num_channels;c++) {
                WMA3ChannelCtx* chan = &s->channel[c];

                /** add subframes to the individual channels */
                if (min_channel_len == chan->channel_len) {
                    --channels_for_cur_subframe;
                    if (!read_channel_mask ||
                       channel_mask & (1<<channels_for_cur_subframe)) {
                        if (chan->num_subframes >= MAX_SUBFRAMES) {
                            av_log(s->avctx, AV_LOG_ERROR,
                                    "broken frame: num subframes > 31\n");
                            return -1;
                        }
                        chan->subframe_len[chan->num_subframes] = subframe_len;
                        chan->channel_len += subframe_len;
                        missing_samples -= subframe_len;
                        ++chan->num_subframes;
                        if (missing_samples < 0
                            || chan->channel_len > s->samples_per_frame) {
                            av_log(s->avctx, AV_LOG_ERROR,"broken frame: "
                                    "channel len > samples_per_frame\n");
                            return -1;
                        }
                    }
                }
            }
        }
    }

    for (c=0;c<s->num_channels;c++) {
        int i;
        int offset = 0;
        for (i=0;i<s->channel[c].num_subframes;i++) {
            dprintf(s->avctx, "frame[%i] channel[%i] subframe[%i]"
                   " len %i\n",s->frame_num,c,i,s->channel[c].subframe_len[i]);
            s->channel[c].subframe_offset[i] = offset;
            offset += s->channel[c].subframe_len[i];
        }
    }

    return 0;
}

/**
 *@brief Calculate a decorrelation matrix from the bitstream parameters.
 *@param s codec context
 *@param chgroup channel group for which the matrix needs to be calculated
 */
static void decode_decorrelation_matrix(WMA3DecodeContext* s,
                                            WMA3ChannelGroup* chgroup)
{
    int i;
    int offset = 0;
    char rotation_offset[WMAPRO_MAX_CHANNELS * WMAPRO_MAX_CHANNELS];
    memset(chgroup->decorrelation_matrix,0,
           sizeof(float) *s->num_channels * s->num_channels);

    for (i=0;i<chgroup->num_channels  * (chgroup->num_channels - 1) >> 1;i++)
        rotation_offset[i] = get_bits(&s->gb,6);

    for (i=0;i<chgroup->num_channels;i++)
        chgroup->decorrelation_matrix[chgroup->num_channels * i + i] =
                                                get_bits1(&s->gb) ? 1.0 : -1.0;

    for (i=1;i<chgroup->num_channels;i++) {
        int x;
        for (x=0;x<i;x++) {
            int y;
            float tmp1[WMAPRO_MAX_CHANNELS];
            float tmp2[WMAPRO_MAX_CHANNELS];
            memcpy(tmp1,
                   &chgroup->decorrelation_matrix[x * chgroup->num_channels],
                   sizeof(float) * (i + 1));
            memcpy(tmp2,
                   &chgroup->decorrelation_matrix[i * chgroup->num_channels],
                   sizeof(float) * (i + 1));
            for (y=0;y < i + 1 ; y++) {
                float v1 = tmp1[y];
                float v2 = tmp2[y];
                int n = rotation_offset[offset + x];
                float sinv;
                float cosv;

                if (n<32) {
                    sinv = sin64[n];
                    cosv = sin64[32-n];
                } else {
                    sinv = sin64[64-n];
                    cosv = -sin64[n-32];
                }

                chgroup->decorrelation_matrix[y + x * chgroup->num_channels] =
                                               (v1 * sinv) - (v2 * cosv);
                chgroup->decorrelation_matrix[y + i * chgroup->num_channels] =
                                               (v1 * cosv) + (v2 * sinv);
            }
        }
        offset += i;
    }
}

/**
 *@brief Decode channel transformation parameters
 *@param s codec context
 *@return 0 in case of bitstream errors, 1 on success
 */
static int decode_channel_transform(WMA3DecodeContext* s)
{
    int i;
    /* should never consume more than 1921 bits for the 8 channel case
     * 1 + MAX_CHANNELS * ( MAX_CHANNELS + 2 + 3 * MAX_CHANNELS * MAX_CHANNELS
     * + MAX_CHANNELS + MAX_BANDS + 1)
     */

    /** in the one channel case channel transforms are pointless */
    s->num_chgroups = 0;
    if (s->num_channels > 1) {
        int remaining_channels = s->channels_for_cur_subframe;

        if (get_bits1(&s->gb)) {
            ff_log_ask_for_sample(s->avctx,
                   "unsupported channel transform bit\n");
            return 0;
        }

        for (s->num_chgroups = 0; remaining_channels &&
            s->num_chgroups < s->channels_for_cur_subframe;s->num_chgroups++) {
            WMA3ChannelGroup* chgroup = &s->chgroup[s->num_chgroups];
            float** channel_data = chgroup->channel_data;
            chgroup->num_channels = 0;
            chgroup->transform = 0;

            /** decode channel mask */
            if (remaining_channels > 2) {
                for (i=0;i<s->channels_for_cur_subframe;i++) {
                    int channel_idx = s->channel_indexes_for_cur_subframe[i];
                    if (!s->channel[channel_idx].grouped
                       && get_bits1(&s->gb)) {
                        ++chgroup->num_channels;
                        s->channel[channel_idx].grouped = 1;
                        *channel_data++ = s->channel[channel_idx].coeffs;
                    }
                }
            } else {
                chgroup->num_channels = remaining_channels;
                for (i=0;i<s->channels_for_cur_subframe ;i++) {
                    int channel_idx = s->channel_indexes_for_cur_subframe[i];
                    if (!s->channel[channel_idx].grouped)
                        *channel_data++ = s->channel[channel_idx].coeffs;
                    s->channel[channel_idx].grouped = 1;
                }
            }

            /** decode transform type */
            if (chgroup->num_channels == 2) {
                if (get_bits1(&s->gb)) {
                    if (get_bits1(&s->gb)) {
                        ff_log_ask_for_sample(s->avctx,
                               "unsupported channel transform type\n");
                    }
                } else {
                    if (s->num_channels == 2) {
                        chgroup->transform = 1;
                    } else {
                        chgroup->transform = 2;
                        /** cos(pi/4) */
                        chgroup->decorrelation_matrix[0] = 0.70703125;
                        chgroup->decorrelation_matrix[1] = -0.70703125;
                        chgroup->decorrelation_matrix[2] = 0.70703125;
                        chgroup->decorrelation_matrix[3] = 0.70703125;
                    }
                }
            } else if (chgroup->num_channels > 2) {
                if (get_bits1(&s->gb)) {
                    chgroup->transform = 2;
                    if (get_bits1(&s->gb)) {
                        decode_decorrelation_matrix(s, chgroup);
                    } else {
                        /** FIXME: more than 6 coupled channels not supported */
                        if (chgroup->num_channels > 6) {
                            ff_log_ask_for_sample(s->avctx,
                                   "coupled channels > 6\n");
                        } else {
                            memcpy(chgroup->decorrelation_matrix,
                              default_decorrelation[chgroup->num_channels],
                              sizeof(float) * chgroup->num_channels *
                              chgroup->num_channels);
                        }
                    }
                }
            }

            /** decode transform on / off */
            if (chgroup->transform) {
                if (!get_bits1(&s->gb)) {
                    int i;
                    /** transform can be enabled for individual bands */
                    for (i=0;i< s->num_bands;i++) {
                        chgroup->transform_band[i] = get_bits1(&s->gb);
                    }
                } else {
                    memset(chgroup->transform_band,1,s->num_bands);
                }
            }
            remaining_channels -= chgroup->num_channels;
        }
    }
    return 1;
}

/**
 *@brief Extract the coefficients from the bitstream.
 *@param s codec context
 *@param c current channel number
 *@return 0 in case of bitstream errors, 1 on success
 */
static int decode_coeffs(WMA3DecodeContext *s, int c)
{
    int vlctable;
    VLC* vlc;
    int vlcmax;
    WMA3ChannelCtx* ci = &s->channel[c];
    int rl_mode = 0;
    int cur_coeff = 0;
    int num_zeros = 0;
    const uint16_t* run;
    const uint16_t* level;
    int zero_init = 0;
    int rl_switchmask = (s->subframe_len>>8);

    dprintf(s->avctx, "decode coefficients for channel %i\n",c);

    vlctable = get_bits1(&s->gb);
    vlc = &coef_vlc[vlctable];
    vlcmax = s->coef_max[vlctable];

    if (vlctable) {
        run = coef1_run;
        level = coef1_level;
    } else {
        run = coef0_run;
        level = coef0_level;
    }

    /** for subframe_len 128 the first zero coefficient will switch to
        the run level mode */
    if (s->subframe_len == 128) {
        zero_init = num_zeros = 1;
        rl_switchmask = 1;
    }

    /** decode vector coefficients (consumes up to 167 bits per iteration for
      4 vector coded large values) */
    while (!rl_mode && cur_coeff + 3 < s->subframe_len) {
        int vals[4];
        int i;
        unsigned int idx;

        idx = get_vlc2(&s->gb, vec4_vlc.table, VLCBITS, VEC4MAXDEPTH);

        if ( idx == HUFF_VEC4_SIZE - 1 ) {
            for (i=0 ; i < 4 ; i+= 2) {
                idx = get_vlc2(&s->gb, vec2_vlc.table, VLCBITS, VEC2MAXDEPTH);
                if ( idx == HUFF_VEC2_SIZE - 1 ) {
                    vals[i] = get_vlc2(&s->gb, vec1_vlc.table, VLCBITS, VEC1MAXDEPTH);
                    if (vals[i] == HUFF_VEC1_SIZE - 1)
                        vals[i] += ff_wma_get_large_val(&s->gb);
                    vals[i+1] = get_vlc2(&s->gb, vec1_vlc.table, VLCBITS, VEC1MAXDEPTH);
                    if (vals[i+1] == HUFF_VEC1_SIZE - 1)
                        vals[i+1] += ff_wma_get_large_val(&s->gb);
                } else {
                    vals[i] = (symbol_to_vec2[idx] >> 4) & 0xF;
                    vals[i+1] = symbol_to_vec2[idx] & 0xF;
                }
            }
        } else {
             vals[0] = (symbol_to_vec4[idx] >> 8) >> 4;
             vals[1] = (symbol_to_vec4[idx] >> 8) & 0xF;
             vals[2] = (symbol_to_vec4[idx] >> 4) & 0xF;
             vals[3] = symbol_to_vec4[idx] & 0xF;
        }

        /** decode sign */
        for (i=0;i<4;i++) {
            if (vals[i]) {
                int sign = get_bits1(&s->gb) - 1;
                ci->coeffs[cur_coeff] = (vals[i]^sign) - sign;
                num_zeros = zero_init;
            } else {
                /** switch to run level mode when subframe_len / 128 zeros
                   were found in a row */
                rl_mode |= (num_zeros & rl_switchmask);
                ++num_zeros;
            }
            ++cur_coeff;
        }
    }

    /** decode run level coded coefficients */
    if (rl_mode) {
        if(ff_wma_run_level_decode(s->avctx, &s->gb, vlc,
                             level, run, 1, ci->coeffs,
                             cur_coeff, s->subframe_len, s->subframe_len,
                             s->esc_len, 0))
            return -1;
    }

    return 1;
}

/**
 *@brief Extract scale factors from the bitstream.
 *@param s codec context
 *@return 0 in case of bitstream errors, 1 on success
 */
static int decode_scale_factors(WMA3DecodeContext* s)
{
    int i;

    /** should never consume more than 5344 bits
     *  MAX_CHANNELS * (1 +  MAX_BANDS * 23)
     */

    for (i=0;i<s->channels_for_cur_subframe;i++) {
        int c = s->channel_indexes_for_cur_subframe[i];
        int* sf;
        int* sf_end = s->channel[c].scale_factors + s->num_bands;

        /** resample scale factors for the new block size */
        if (s->channel[c].reuse_sf) {
            const int blocks_per_frame = s->samples_per_frame/s->subframe_len;
            const int res_blocks_per_frame = s->samples_per_frame /
                                          s->channel[c].scale_factor_block_len;
            const int idx0 = av_log2(blocks_per_frame);
            const int idx1 = av_log2(res_blocks_per_frame);
            const int16_t* sf_offsets =
                               &s->sf_offsets[s->num_possible_block_sizes *
                               MAX_BANDS  * idx0 + MAX_BANDS * idx1];
            int b;
            for (b=0;b<s->num_bands;b++)
                s->channel[c].resampled_scale_factors[b] =
                                   s->channel[c].scale_factors[*sf_offsets++];

            s->channel[c].max_scale_factor =
                                   s->channel[c].resampled_scale_factors[0];
            sf = s->channel[c].resampled_scale_factors + 1;
            while (sf < s->channel[c].resampled_scale_factors + s->num_bands) {
                if (*sf > s->channel[c].max_scale_factor)
                    s->channel[c].max_scale_factor = *sf;
                ++sf;
            }
        }

        if (s->channel[c].cur_subframe > 0) {
            s->channel[c].transmit_sf = get_bits1(&s->gb);
        } else
            s->channel[c].transmit_sf = 1;

        if (s->channel[c].transmit_sf) {

            if (!s->channel[c].reuse_sf) {
                int val;
                /** decode DPCM coded scale factors */
                s->channel[c].scale_factor_step = get_bits(&s->gb,2) + 1;
                val = 45 / s->channel[c].scale_factor_step;
                for (sf = s->channel[c].scale_factors; sf < sf_end; sf++) {
                    val += get_vlc2(&s->gb, sf_vlc.table, SCALEVLCBITS, SCALEMAXDEPTH) - 60;
                    *sf = val;
                }
            } else {
                int i;
                /** run level decode differences to the resampled factors */

                memcpy(s->channel[c].scale_factors,
                       s->channel[c].resampled_scale_factors,
                       sizeof(int) * s->num_bands);

                for (i=0;i<s->num_bands;i++) {
                    int idx;
                    int skip;
                    int val;
                    int sign;

                    idx = get_vlc2(&s->gb, sf_rl_vlc.table, VLCBITS, SCALERLMAXDEPTH);

                    if ( !idx ) {
                        uint32_t code = get_bits(&s->gb,14);
                        val = code >> 6;
                        sign = (code & 1) - 1;
                        skip = (code & 0x3f)>>1;
                    } else if (idx == 1) {
                        break;
                    } else {
                        skip = scale_rl_run[idx];
                        val = scale_rl_level[idx];
                        sign = get_bits1(&s->gb)-1;
                    }

                    i += skip;
                    if (i >= s->num_bands) {
                        av_log(s->avctx,AV_LOG_ERROR,
                               "invalid scale factor coding\n");
                        return 0;
                    } else
                        s->channel[c].scale_factors[i] += (val ^ sign) - sign;
                }
            }

            s->channel[c].reuse_sf = 1;
            s->channel[c].max_scale_factor = s->channel[c].scale_factors[0];
            for (sf=s->channel[c].scale_factors + 1; sf < sf_end; sf++) {
                if (s->channel[c].max_scale_factor < *sf)
                    s->channel[c].max_scale_factor = *sf;
            }
            s->channel[c].scale_factor_block_len = s->subframe_len;
        }
    }
    return 1;
}

/**
 *@brief Reconstruct the individual channel data.
 *@param s codec context
 */
static void inverse_channel_transform(WMA3DecodeContext *s)
{
    int i;

    for (i=0;i<s->num_chgroups;i++) {

        if (s->chgroup[i].transform == 1) {
            /** M/S stereo decoding */
            int16_t* sfb_offsets = s->cur_sfb_offsets;
            float* ch0 = *sfb_offsets + s->channel[0].coeffs;
            float* ch1 = *sfb_offsets++ + s->channel[1].coeffs;
            const char* tb = s->chgroup[i].transform_band;
            const char* tb_end = tb + s->num_bands;

            while (tb < tb_end) {
                const float* ch0_end = s->channel[0].coeffs +
                                       FFMIN(*sfb_offsets,s->subframe_len);
                if (*tb++ == 1) {
                    while (ch0 < ch0_end) {
                        const float v1 = *ch0;
                        const float v2 = *ch1;
                        *ch0++ = v1 - v2;
                        *ch1++ = v1 + v2;
                    }
                } else {
                    while (ch0 < ch0_end) {
                        *ch0++ *= 181.0 / 128;
                        *ch1++ *= 181.0 / 128;
                    }
                }
                ++sfb_offsets;
            }
        } else if (s->chgroup[i].transform) {
            float data[WMAPRO_MAX_CHANNELS];
            const int num_channels = s->chgroup[i].num_channels;
            float** ch_data = s->chgroup[i].channel_data;
            float** ch_end = ch_data + num_channels;
            const int8_t* tb = s->chgroup[i].transform_band;
            int16_t* sfb;

            /** multichannel decorrelation */
            for (sfb = s->cur_sfb_offsets ;
                sfb < s->cur_sfb_offsets + s->num_bands;sfb++) {
                if (*tb++ == 1) {
                    int y;
                    /** multiply values with the decorrelation_matrix */
                    for (y=sfb[0];y<FFMIN(sfb[1], s->subframe_len);y++) {
                        const float* mat = s->chgroup[i].decorrelation_matrix;
                        const float* data_end= data + num_channels;
                        float* data_ptr= data;
                        float** ch;

                        for (ch = ch_data;ch < ch_end; ch++)
                           *data_ptr++ = (*ch)[y];

                        for (ch = ch_data; ch < ch_end; ch++) {
                            float sum = 0;
                            data_ptr = data;
                            while (data_ptr < data_end)
                                sum += *data_ptr++ * *mat++;

                            (*ch)[y] = sum;
                        }
                    }
                }
            }
        }
    }
}

/**
 *@brief Apply sine window and reconstruct the output buffer.
 *@param s codec context
 */
static void window(WMA3DecodeContext *s)
{
    int i;
    for (i=0;i<s->channels_for_cur_subframe;i++) {
        int c = s->channel_indexes_for_cur_subframe[i];
        float* start;
        float* window;
        int winlen = s->channel[c].prev_block_len;
        start = s->channel[c].coeffs - (winlen >> 1);

        if (s->subframe_len < winlen) {
            start += (winlen - s->subframe_len)>>1;
            winlen = s->subframe_len;
        }

        window = s->windows[av_log2(winlen)-BLOCK_MIN_BITS];

        winlen >>= 1;

        s->dsp.vector_fmul_window(start, start, start + winlen,
                                  window, 0, winlen);

        s->channel[c].prev_block_len = s->subframe_len;
    }
}

/**
 *@brief Decode a single subframe (block).
 *@param s codec context
 *@return 0 if decoding failed, 1 on success
 */
static int decode_subframe(WMA3DecodeContext *s)
{
    int offset = s->samples_per_frame;
    int subframe_len = s->samples_per_frame;
    int i;
    int total_samples = s->samples_per_frame * s->num_channels;
    int transmit_coeffs = 0;

    s->subframe_offset = get_bits_count(&s->gb);

    /** reset channel context and find the next block offset and size
        == the next block of the channel with the smallest number of
        decoded samples
    */
    for (i=0;i<s->num_channels;i++) {
        s->channel[i].grouped = 0;
        if (offset > s->channel[i].decoded_samples) {
            offset = s->channel[i].decoded_samples;
            subframe_len =
                s->channel[i].subframe_len[s->channel[i].cur_subframe];
        }
    }

    dprintf(s->avctx,
           "processing subframe with offset %i len %i\n",offset,subframe_len);

    /** get a list of all channels that contain the estimated block */
    s->channels_for_cur_subframe = 0;
    for (i=0;i<s->num_channels;i++) {
        const int cur_subframe = s->channel[i].cur_subframe;
        /** substract already processed samples */
        total_samples -= s->channel[i].decoded_samples;

        /** and count if there are multiple subframes that match our profile */
        if (offset == s->channel[i].decoded_samples &&
           subframe_len == s->channel[i].subframe_len[cur_subframe]) {
            total_samples -= s->channel[i].subframe_len[cur_subframe];
            s->channel[i].decoded_samples +=
                s->channel[i].subframe_len[cur_subframe];
            s->channel_indexes_for_cur_subframe[s->channels_for_cur_subframe] = i;
            ++s->channels_for_cur_subframe;
        }
    }

    /** check if the frame will be complete after processing the
        estimated block */
    if (!total_samples)
        s->parsed_all_subframes = 1;


    dprintf(s->avctx, "subframe is part of %i channels\n",
           s->channels_for_cur_subframe);

    /** calculate number of scale factor bands and their offsets */
    if (subframe_len == s->samples_per_frame) {
        s->num_bands = s->num_sfb[0];
        s->cur_sfb_offsets = s->sfb_offsets;
        s->cur_subwoofer_cutoff = s->subwoofer_cutoffs[0];
    } else {
        int frame_offset = av_log2(s->samples_per_frame/subframe_len);
        s->num_bands = s->num_sfb[frame_offset];
        s->cur_sfb_offsets = &s->sfb_offsets[MAX_BANDS * frame_offset];
        s->cur_subwoofer_cutoff = s->subwoofer_cutoffs[frame_offset];
    }

    /** configure the decoder for the current subframe */
    for (i=0;i<s->channels_for_cur_subframe;i++) {
        int c = s->channel_indexes_for_cur_subframe[i];

        s->channel[c].coeffs = &s->channel[c].out[(s->samples_per_frame>>1)
                                                  + offset];
        memset(s->channel[c].coeffs,0,sizeof(float) * subframe_len);

        /** init some things if this is the first subframe */
        if (!s->channel[c].cur_subframe) {
              s->channel[c].scale_factor_step = 1;
              s->channel[c].max_scale_factor = 0;
              memset(s->channel[c].scale_factors, 0,
                     sizeof(s->channel[c].scale_factors));
              memset(s->channel[c].resampled_scale_factors, 0,
                     sizeof(s->channel[c].resampled_scale_factors));
        }

    }

    s->subframe_len = subframe_len;
    s->esc_len = av_log2(s->subframe_len - 1) + 1;

    /** skip extended header if any */
    if (get_bits1(&s->gb)) {
        int num_fill_bits;
        if (!(num_fill_bits = get_bits(&s->gb,2))) {
            num_fill_bits = get_bits(&s->gb,4);
            num_fill_bits = get_bits(&s->gb,num_fill_bits) + 1;
        }

        if (num_fill_bits >= 0) {
            if (get_bits_count(&s->gb) + num_fill_bits > s->num_saved_bits) {
                av_log(s->avctx,AV_LOG_ERROR,"invalid number of fill bits\n");
                return 0;
            }

            skip_bits_long(&s->gb,num_fill_bits);
        }
    }

    /** no idea for what the following bit is used */
    if (get_bits1(&s->gb)) {
        ff_log_ask_for_sample(s->avctx, "reserved bit set\n");
        return 0;
    }


    if (!decode_channel_transform(s))
        return 0;


    for (i=0;i<s->channels_for_cur_subframe;i++) {
        int c = s->channel_indexes_for_cur_subframe[i];
        if ((s->channel[c].transmit_coefs = get_bits1(&s->gb)))
            transmit_coeffs = 1;
    }

    if (transmit_coeffs) {
        int step;
        int quant_step = 90 * s->sample_bit_depth >> 4;
        if ((get_bits1(&s->gb))) {
            /** FIXME: might change run level mode decision */
            ff_log_ask_for_sample(s->avctx, "unsupported quant step coding\n");
            return 0;
        }
        /** decode quantization step */
        step = get_sbits(&s->gb,6);
        quant_step += step;
        if (step == -32 || step == 31) {
            const int sign = (step == 31) - 1;
            int quant = 0;
            while (get_bits_count(&s->gb) + 5 < s->num_saved_bits &&
                   (step = get_bits(&s->gb,5)) == 31 ) {
                     quant += 31;
            }
            quant_step += ((quant + step) ^ sign) - sign;
        }
        if (quant_step < 0) {
            av_log(s->avctx,AV_LOG_DEBUG,"negative quant step\n");
        }

        /** decode quantization step modifiers for every channel */

        if (s->channels_for_cur_subframe == 1) {
            s->channel[s->channel_indexes_for_cur_subframe[0]].quant_step = quant_step;
        } else {
            int modifier_len = get_bits(&s->gb,3);
            for (i=0;i<s->channels_for_cur_subframe;i++) {
                int c = s->channel_indexes_for_cur_subframe[i];
                s->channel[c].quant_step = quant_step;
                if (get_bits1(&s->gb)) {
                    if (modifier_len) {
                        s->channel[c].quant_step +=
                                get_bits(&s->gb,modifier_len) + 1;
                    } else
                        ++s->channel[c].quant_step;
                }
            }
        }

        /** decode scale factors */
        if (!decode_scale_factors(s))
            return 0;
    }

    dprintf(s->avctx, "BITSTREAM: subframe header length was %i\n",
           get_bits_count(&s->gb) - s->subframe_offset);

    /** parse coefficients */
    for (i=0;i<s->channels_for_cur_subframe;i++) {
        int c = s->channel_indexes_for_cur_subframe[i];
        if (s->channel[c].transmit_coefs &&
           get_bits_count(&s->gb) < s->num_saved_bits)
                decode_coeffs(s,c);
    }

    dprintf(s->avctx, "BITSTREAM: subframe length was %i\n",
           get_bits_count(&s->gb) - s->subframe_offset);

    if (transmit_coeffs) {
        /** reconstruct the per channel data */
        inverse_channel_transform(s);
        for (i=0;i<s->channels_for_cur_subframe;i++) {
            int c = s->channel_indexes_for_cur_subframe[i];
            int* sf;
            int b;

            if (s->channel[c].transmit_sf) {
                sf = s->channel[c].scale_factors;
            } else
                sf = s->channel[c].resampled_scale_factors;

            if (c == s->lfe_channel)
                memset(&s->tmp[s->cur_subwoofer_cutoff],0,
                     sizeof(float) * (subframe_len - s->cur_subwoofer_cutoff));

            /** inverse quantization and rescaling */
            for (b=0;b<s->num_bands;b++) {
                const int end = FFMIN(s->cur_sfb_offsets[b+1],s->subframe_len);
                const int exp = s->channel[c].quant_step -
                            (s->channel[c].max_scale_factor - *sf++) *
                            s->channel[c].scale_factor_step;
                const float quant = pow(10.0,exp / 20.0);
                int start;

                for (start = s->cur_sfb_offsets[b]; start < end; start++)
                    s->tmp[start] = s->channel[c].coeffs[start] * quant;
            }

            /** apply imdct (ff_imdct_half == DCTIV with reverse) */
            ff_imdct_half(&s->mdct_ctx[av_log2(subframe_len)-BLOCK_MIN_BITS],
                          s->channel[c].coeffs, s->tmp);
        }
    }

    /** window and overlapp-add */
    window(s);

    /** handled one subframe */
    for (i=0;i<s->channels_for_cur_subframe;i++) {
        int c = s->channel_indexes_for_cur_subframe[i];
        if (s->channel[c].cur_subframe >= s->channel[c].num_subframes) {
            av_log(s->avctx,AV_LOG_ERROR,"broken subframe\n");
            return 0;
        }
        ++s->channel[c].cur_subframe;
    }

    return 1;
}

/**
 *@brief Decode one WMA frame.
 *@param s codec context
 *@return 0 if the trailer bit indicates that this is the last frame,
 *        1 if there are additional frames
 */
static int decode_frame(WMA3DecodeContext *s)
{
    GetBitContext* gb = &s->gb;
    int more_frames = 0;
    int len = 0;
    int i;

    /** check for potential output buffer overflow */
    if (s->samples + s->num_channels * s->samples_per_frame > s->samples_end) {
        av_log(s->avctx,AV_LOG_ERROR,
               "not enough space for the output samples\n");
        s->packet_loss = 1;
        return 0;
    }

    /** get frame length */
    if (s->len_prefix)
        len = get_bits(gb,s->log2_frame_size);

    dprintf(s->avctx, "decoding frame with length %x\n",len);

    /** decode tile information */
    if (decode_tilehdr(s)) {
        s->packet_loss = 1;
        return 0;
    }

    /** read postproc transform */
    if (s->num_channels > 1 && get_bits1(gb)) {
        ff_log_ask_for_sample(s->avctx, "Unsupported postproc transform found\n");
        s->packet_loss = 1;
        return 0;
    }

    /** read drc info */
    if (s->dynamic_range_compression) {
        s->drc_gain = get_bits(gb,8);
        dprintf(s->avctx, "drc_gain %i\n",s->drc_gain);
    }

    /** no idea what these are for, might be the number of samples
        that need to be skipped at the beginning or end of a stream */
    if (get_bits1(gb)) {
        int skip;

        /** usually true for the first frame */
        if (get_bits1(gb)) {
            skip = get_bits(gb,av_log2(s->samples_per_frame * 2));
            dprintf(s->avctx, "start skip: %i\n",skip);
        }

        /** sometimes true for the last frame */
        if (get_bits1(gb)) {
            skip = get_bits(gb,av_log2(s->samples_per_frame * 2));
            dprintf(s->avctx, "end skip: %i\n",skip);
        }

    }

    dprintf(s->avctx, "BITSTREAM: frame header length was %i\n",
           get_bits_count(gb) - s->frame_offset);

    /** reset subframe states */
    s->parsed_all_subframes = 0;
    for (i=0;i<s->num_channels;i++) {
        s->channel[i].decoded_samples = 0;
        s->channel[i].cur_subframe = 0;
        s->channel[i].reuse_sf = 0;
    }

    /** decode all subframes */
    while (!s->parsed_all_subframes) {
        if (!decode_subframe(s)) {
            s->packet_loss = 1;
            return 0;
        }
    }

    /** convert samples to short and write them to the output buffer */
    for (i = 0; i < s->num_channels; i++) {
        int16_t* ptr;
        int incr = s->num_channels;
        float* iptr = s->channel[i].out;
        int x;

        ptr = s->samples + i;

        for (x=0;x<s->samples_per_frame;x++) {
            *ptr = av_clip_int16(lrintf(*iptr++));
            ptr += incr;
        }

        /** reuse second half of the IMDCT output for the next frame */
        memmove(&s->channel[i].out[0],
                &s->channel[i].out[s->samples_per_frame],
                s->samples_per_frame * sizeof(float));
    }

    if (s->skip_frame) {
        s->skip_frame = 0;
    } else
        s->samples += s->num_channels * s->samples_per_frame;

    if (len != (get_bits_count(gb) - s->frame_offset) + 2) {
        /** FIXME: not sure if this is always an error */
        av_log(s->avctx,AV_LOG_ERROR,"frame[%i] would have to skip %i bits\n",
               s->frame_num,len - (get_bits_count(gb) - s->frame_offset) - 1);
        s->packet_loss = 1;
        return 0;
    }

    /** skip the rest of the frame data */
    skip_bits_long(gb,len - (get_bits_count(gb) - s->frame_offset) - 1);

    /** decode trailer bit */
    more_frames = get_bits1(gb);

    ++s->frame_num;
    return more_frames;
}

/**
 *@brief Calculate remaining input buffer length.
 *@param s codec context
 *@param gb bitstream reader context
 *@return remaining size in bits
 */
static int remaining_bits(WMA3DecodeContext *s, GetBitContext* gb)
{
    return s->buf_bit_size - get_bits_count(gb);
}

/**
 *@brief Fill the bit reservoir with a (partial) frame.
 *@param s codec context
 *@param gb bitstream reader context
 *@param len length of the partial frame
 *@param append decides wether to reset the buffer or not
 */
static void save_bits(WMA3DecodeContext *s, GetBitContext* gb, int len,
                          int append)
{
    int buflen;
    int bit_offset;
    int pos;

    if (!append) {
        s->frame_offset = get_bits_count(gb) & 7;
        s->num_saved_bits = s->frame_offset;
    }

    buflen = (s->num_saved_bits + len + 8) >> 3;

    if (len <= 0 || buflen > MAX_FRAMESIZE) {
         ff_log_ask_for_sample(s->avctx, "input buffer too small\n");
         s->packet_loss = 1;
         return;
    }

    if (!append) {
        s->num_saved_bits += len;
        memcpy(s->frame_data, gb->buffer + (get_bits_count(gb) >> 3),
              (s->num_saved_bits  + 8)>> 3);
        skip_bits_long(gb, len);
    } else {
        bit_offset = s->num_saved_bits & 7;
        pos = (s->num_saved_bits - bit_offset) >> 3;

        s->num_saved_bits += len;

        /** byte align prev_frame buffer */
        if (bit_offset) {
            int missing = 8 - bit_offset;
            missing = FFMIN(len, missing);
            s->frame_data[pos++] |=
                get_bits(gb, missing) << (8 - bit_offset - missing);
            len -= missing;
        }

        /** copy full bytes */
        while (len > 7) {
            s->frame_data[pos++] = get_bits(gb,8);
            len -= 8;
        }

        /** copy remaining bits */
        if (len > 0)
            s->frame_data[pos++] = get_bits(gb,len) << (8 - len);

    }

    init_get_bits(&s->gb, s->frame_data,s->num_saved_bits);
    skip_bits(&s->gb, s->frame_offset);
}

/**
 *@brief Decode a single WMA packet.
 *@param avctx codec context
 *@param data the output buffer
 *@param data_size number of bytes that were written to the output buffer
 *@param avpkt input packet
 *@return number of bytes that were read from the input buffer
 */
static int decode_packet(AVCodecContext *avctx,
                             void *data, int *data_size, /*AVPacket* avpkt*/ const uint8_t* buf , int buf_size)
{
    GetBitContext gb;
    WMA3DecodeContext *s = avctx->priv_data;
//    const uint8_t* buf = avpkt->data;
 //   int buf_size = avpkt->size;
    int more_frames=1;
    int num_bits_prev_frame;
    int packet_sequence_number;

    s->samples = data;
    s->samples_end = (int16_t*)((int8_t*)data + *data_size);
    s->buf_bit_size = buf_size << 3;


    *data_size = 0;

    /** sanity check for the buffer length */
    if (buf_size < avctx->block_align)
        return 0;

    buf_size = avctx->block_align;

    /** parse packet header */
    init_get_bits(&gb, buf, s->buf_bit_size);
    packet_sequence_number    = get_bits(&gb, 4);
    skip_bits(&gb, 2);

    /** get number of bits that need to be added to the previous frame */
    num_bits_prev_frame = get_bits(&gb, s->log2_frame_size);
    dprintf(avctx, "packet[%d]: nbpf %x\n", avctx->frame_number,
                  num_bits_prev_frame);

    /** check for packet loss */
    if (!s->packet_loss &&
        ((s->packet_sequence_number + 1)&0xF) != packet_sequence_number) {
        s->packet_loss = 1;
        av_log(avctx, AV_LOG_ERROR, "Packet loss detected! seq %x vs %x\n",
                      s->packet_sequence_number,packet_sequence_number);
    }
    s->packet_sequence_number = packet_sequence_number;

    if (num_bits_prev_frame > 0) {
        /** append the previous frame data to the remaining data from the
            previous packet to create a full frame */
        save_bits(s, &gb, num_bits_prev_frame, 1);
        dprintf(avctx, "accumulated %x bits of frame data\n",
                      s->num_saved_bits - s->frame_offset);

        /** decode the cross packet frame if it is valid */
        if (!s->packet_loss)
            decode_frame(s);
    } else if (s->num_saved_bits - s->frame_offset) {
        dprintf(avctx, "ignoring %x previously saved bits\n",
                      s->num_saved_bits - s->frame_offset);
    }

    s->packet_loss = 0;
    /** decode the rest of the packet */
    while (!s->packet_loss && more_frames &&
          remaining_bits(s,&gb) > s->log2_frame_size) {
        int frame_size = show_bits(&gb, s->log2_frame_size);

        /** there is enough data for a full frame */
        if (remaining_bits(s,&gb) >= frame_size && frame_size > 0) {
            save_bits(s, &gb, frame_size, 0);

            /** decode the frame */
            more_frames = decode_frame(s);

            if (!more_frames) {
                dprintf(avctx, "no more frames\n");
            }
        } else
            more_frames = 0;
    }

    if (!s->packet_loss && remaining_bits(s,&gb) > 0) {
        /** save the rest of the data so that it can be decoded
            with the next packet */
        save_bits(s, &gb, remaining_bits(s,&gb), 0);
    }

    *data_size = (int8_t *)s->samples - (int8_t *)data;

    return avctx->block_align;
}

/**
 *@brief Clear decoder buffers (for seeking).
 *@param avctx codec context
 */
static void flush(AVCodecContext *avctx)
{
    WMA3DecodeContext *s = avctx->priv_data;
    int i;
    /** reset output buffer as a part of it is used during the windowing of a
        new frame */
    for (i=0;i<s->num_channels;i++)
        memset(s->channel[i].out, 0, s->samples_per_frame * sizeof(float));
    s->packet_loss = 1;
}


/**
 *@brief WMA9 decoder
 */
AVCodec wmapro_decoder =
{
    "wmapro",
    CODEC_TYPE_AUDIO,
    CODEC_ID_WMAPRO,
    sizeof(WMA3DecodeContext),
    decode_init,
    NULL,
    decode_end,
    decode_packet,
    .flush= flush,
    .long_name = NULL_IF_CONFIG_SMALL("Windows Media Audio 9 Professional"),
};
