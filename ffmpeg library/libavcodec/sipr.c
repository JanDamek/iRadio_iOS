#include <inttypes.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>
#include <string.h>

#include "avcodec.h"
#include "libavutil/lfg.h"
#define ALT_BITSTREAM_READER_LE
#include "avformat.h"
#include "bitstream.h"
//#include "get_bits.h"

#define FRAC_BITS 15
#include "mathops.h"

#include "lsp.h"
#include "celp_math.h"
#include "acelp_vectors.h"
#include "acelp_pitch_delay.h"
#include "acelp_filters.h"
#include "celp_filters.h"
#include "dsputil.h"


#define LP_FILTER_ORDER   16
#define L_INTERPOL        11
#define L_SUBFR_16k0      80
#define PITCH_MIN         30
#define PITCH_MAX         281

#define LSFQ_DIFF_MIN   161             ///< 50.0 * PI / 8000 in (2.13)
#define LSFQ_MIN        LSFQ_DIFF_MIN
#define LSFQ_MAX        25736           ///< PI in (2.13)

/* 8k5 */
#define LP_FILTER_ORDER_8k5 10
#define L_SUBFR_8k5         48
#define PITCH_MIN_8k5       20
#define PITCH_MAX_8k5       143

#include "siprdata.h"

typedef enum {
    MODE_16k=0,
    MODE_8k5,
    MODE_6k5,
    MODE_5k0,
    MODE_COUNT
} Sipr_Mode;

typedef struct {
    uint16_t bits_per_frame;
    uint8_t subframe_size;
    uint8_t subframe_count;
    uint8_t frames_per_packet;
    uint16_t treshpit;

    /* bitstream parameters */
    uint8_t number_of_fc_indexes;
    uint8_t ma_predictor_bits;  ///< size in bits of the switched MA predictor

    /** size in bits of the i-th stage vector of quantizer */
    uint8_t vq_indexes_bits[5];

    /** size in bits of the adaptive-codebook index for every subframe */
    uint8_t pitch_delay_bits[5];

    uint8_t gp_index_bits;
    uint8_t fc_index_bits; ///< size in bits of the fixed codebook indexes
    uint8_t gc_index_bits; ///< size in bits of the gain codebook indexes
} Sipr_Mode_Param;

static const Sipr_Mode_Param modes[MODE_COUNT] ={
    // 16k
    {
        .bits_per_frame    = 160,
        .subframe_size     = L_SUBFR_16k0,
        .subframe_count    = 2,
        .frames_per_packet = 1,
        .treshpit          = 0.00,

        .number_of_fc_indexes = 5,
        .ma_predictor_bits    = 1,
        .vq_indexes_bits      = {7, 8, 7, 7, 7},
        .pitch_delay_bits     = {9, 6},
        .gp_index_bits        = 4,
        .fc_index_bits        = 9,
        .gc_index_bits        = 5
    },{
    // 8k5
        .bits_per_frame    = 152,
        .subframe_size     = L_SUBFR_8k5,
        .subframe_count    = 3,
        .frames_per_packet = 1,
        .treshpit          = 26214,

        .number_of_fc_indexes = 3,
        .ma_predictor_bits    = 0,
        .vq_indexes_bits      = {6, 7, 7, 7, 5},
        .pitch_delay_bits     = {8, 5, 5},
        .gp_index_bits        = 0,
        .fc_index_bits        = 9,
        .gc_index_bits        = 7
    },{
    // 6k5
        .bits_per_frame    = 232,
        .subframe_size     = L_SUBFR_8k5,
        .subframe_count    = 3,
        .frames_per_packet = 2,
        .treshpit          = 26214,

        .number_of_fc_indexes = 3,
        .ma_predictor_bits    = 0,
        .vq_indexes_bits      = {6, 7, 7, 7, 5},
        .pitch_delay_bits     = {8, 5, 5},
        .gp_index_bits        = 0,
        .fc_index_bits        = 5,
        .gc_index_bits        = 7
    },{
    // 5k0
        .bits_per_frame    = 296,
        .subframe_size     = L_SUBFR_8k5,
        .subframe_count    = 5,
        .frames_per_packet = 2,
        .treshpit          = 27852,

        .number_of_fc_indexes = 1,
        .ma_predictor_bits    = 0,
        .vq_indexes_bits      = {6, 7, 7, 7, 5},
        .pitch_delay_bits     = {8, 5, 8, 5, 5},
        .gp_index_bits        = 0,
        .fc_index_bits        = 10,
        .gc_index_bits        = 7
    }
};

typedef struct {
    AVCodecContext *avctx;
    Sipr_Mode_Param m;
    Sipr_Mode mode;

    AVLFG random_state;         ///< random number generator state
    DSPContext dsp;

    int pitch_delay_int_prev;

    int16_t A_past[LP_FILTER_ORDER+1];   ///< FIXME: I don't know what is this
    int16_t B_past[LP_FILTER_ORDER+1];   ///< FIXME: I don't know what is this

    int16_t* exc;

    int16_t past_gain_pit;
    int16_t past_gain_code;
    int16_t past_lsf_q[LP_FILTER_ORDER];
    int16_t past_lsf[LP_FILTER_ORDER];
    int16_t past_qua_en[2];
    int16_t past_q_target[LP_FILTER_ORDER];
    int ma_predictor_prev;

    int16_t old_exc[L_INTERPOL + PITCH_MAX + 2 * L_SUBFR_16k0];

    int16_t mem_preemph[LP_FILTER_ORDER];
    int16_t mem_syn[LP_FILTER_ORDER + 2 * L_SUBFR_16k0];

    /* 8k5 */
    float past_gain_pit_8k5;
    float past_gain_code_8k5;
    float gain_har;
    float old_pitch;
    float dec_past_q_ener[4];
    int16_t hpf_z[2];
    int hpf_f[2];
    float old_exc_8k5[PITCH_MAX_8k5 + L_INTERPOL];
    DECLARE_ALIGNED_16(int16_t, code_fp[L_SUBFR_16k0]);
    DECLARE_ALIGNED_16(int16_t, fc_v[L_SUBFR_16k0]);
} Sipr_Context;

typedef struct
{
    int bfi;              ///< bad frame indicator
    int ma_pred_switch;   ///< switched MA predictor
    int vq_indexes[5];
    int pitch_delay[5];   ///< pitch delay
    int gp_index[5];      ///< adaptive-codebook gain index
    int fc_indexes[5][5]; ///< fixed-codebook index
    int gc_index[5];      ///< fixed-codebook gain index
} Sipr_Parameters;

static void lsf_restore_from_previous(int16_t* lsf, int16_t *past_q_target,
                                      int16_t* past_lsf_q, int k_prev)
{
    int i;

    memcpy(lsf, past_lsf_q, LP_FILTER_ORDER * sizeof(int16_t));

    for (i=0; i<LP_FILTER_ORDER; i++) {
        int tmp  = lsf[i] - ((ma_predictor[k_prev] * past_q_target[i]) >> 15);
        past_q_target[i] = (tmp * ma_predictor_sum_inv[k_prev]) >> 13;
    }
}

static void lsf_decode(int16_t* past_q_target, int16_t* lsfq,
                       int ma_pred_switch, const int* vq_indexes)
{
    int i;
    int16_t lspq[LP_FILTER_ORDER];

    lspq[0] = cb_lsp_1st[vq_indexes[0]][0];
    lspq[1] = cb_lsp_1st[vq_indexes[0]][1];
    lspq[2] = cb_lsp_1st[vq_indexes[0]][2];

    lspq[3] = cb_lsp_2nd[vq_indexes[1]][0];
    lspq[4] = cb_lsp_2nd[vq_indexes[1]][1];
    lspq[5] = cb_lsp_2nd[vq_indexes[1]][2];

    lspq[6] = cb_lsp_3rd[vq_indexes[2]][0];
    lspq[7] = cb_lsp_3rd[vq_indexes[2]][1];
    lspq[8] = cb_lsp_3rd[vq_indexes[2]][2];

    lspq[9]  = cb_lsp_4th[vq_indexes[3]][0];
    lspq[10] = cb_lsp_4th[vq_indexes[3]][1];
    lspq[11] = cb_lsp_4th[vq_indexes[3]][2];

    lspq[12] = cb_lsp_5th[vq_indexes[4]][0];
    lspq[13] = cb_lsp_5th[vq_indexes[4]][1];
    lspq[14] = cb_lsp_5th[vq_indexes[4]][2];
    lspq[15] = cb_lsp_5th[vq_indexes[4]][3];

    for (i=0; i<LP_FILTER_ORDER; i++) {
        lsfq[i] = (lspq[i] * ma_predictor_sum[ma_pred_switch] +
                past_q_target[i] * ma_predictor[ma_pred_switch]) >> 15;
    }
    memcpy(past_q_target, lspq, LP_FILTER_ORDER * sizeof(int16_t));
}

static void d_isf_ma_8k5(int16_t *isp, int16_t *past_q_target,
                         int16_t *past_lsf_q, const Sipr_Parameters *parm,
                         int bfi)
{
    int i;
    int16_t isf_tmp[10];
    int16_t isfnew[10];

    if (bfi == 2) {
        memset(isf_tmp, 0, 10 * sizeof(float));

        for (i=0; i<10; i++)
            isfnew[i] = past_lsf_q[i] * 0.9 + mean_isf_8k5[i] * 0.1;
    } else {
        isf_tmp[0] = dico1_isf_8k5[parm->vq_indexes[0]][0];
        isf_tmp[1] = dico1_isf_8k5[parm->vq_indexes[0]][1];
        isf_tmp[2] = dico2_isf_8k5[parm->vq_indexes[1]][0];
        isf_tmp[3] = dico2_isf_8k5[parm->vq_indexes[1]][1];
        isf_tmp[4] = dico3_isf_8k5[parm->vq_indexes[2]][0];
        isf_tmp[5] = dico3_isf_8k5[parm->vq_indexes[2]][1];
        isf_tmp[6] = dico4_isf_8k5[parm->vq_indexes[3]][0];
        isf_tmp[7] = dico4_isf_8k5[parm->vq_indexes[3]][1];
        isf_tmp[8] = dico5_isf_8k5[parm->vq_indexes[4]][0];
        isf_tmp[9] = dico5_isf_8k5[parm->vq_indexes[4]][1];

        if (!bfi) {
            for (i=0; i < 10; i++)
                isfnew[i] = (past_q_target[i] * 0.33 + isf_tmp[i] +
                             mean_isf_8k5[i] * 4) * 0.25;
        } else {
            for (i=0; i<10; i++) {
                isfnew[i] = mean_isf_8k5[i] +
                    (4 * past_lsf_q[i] - 4 * mean_isf_8k5[i] + isf_tmp[i] -
                     past_q_target[i] * 0.1089) * (0.5988024 * 0.25);
                isf_tmp[i] = isfnew[i] - past_q_target[i] * (0.33/4)  -
                    mean_isf_8k5[i];
            }
        }

        // Ugh, SIPR implement ff_acelp_reorder_lsf() in a buggy way
        ff_acelp_reorder_lsf(isfnew, 50 * (1<<3), 50 * (1<<3), INT16_MAX, 9);
        isfnew[9] = FFMIN(isfnew[9], 520 * (1<<3));
    }

    memcpy(past_q_target, isf_tmp, 10*sizeof(*isf_tmp));
    memcpy(past_lsf_q, isfnew, 10*sizeof(*isfnew));

    for (i=0; i < 9; i++)
        isp[i] = ff_cos((isfnew[i]*16777) >> 15);
    isp[9] = (isfnew[9] * 206488) >> 15;
}

static int dec_delay3_1st(int index)
{
    if (index < 390)
        return index + 88;
    else
        return 3 * index - 690;
}

static int dec_delay3_2nd(int index, int pit_min, int pit_max,
                          int pitch_delay_int_prev)
{
    int pitch_delay_min =
        FFMIN(FFMAX(pitch_delay_int_prev - 10, pit_min), pit_max - 19) ;

    if (index < 62)
        return 3 * pitch_delay_min + index - 2;
    else
        return 3 * pitch_delay_int_prev;
}

/**
 * main decoding routine
 */
static int decod_frame_16k(Sipr_Context * ctx, Sipr_Parameters *params, int16_t* out_data)
{
    int16_t *fc_v = ctx->fc_v;
    int16_t gain_pitch, gain_code;
    int16_t lp_filter_coeffs[2][LP_FILTER_ORDER + 1];
    int16_t lsp[LP_FILTER_ORDER];
    int16_t lsf[LP_FILTER_ORDER];
    int16_t synth[LP_FILTER_ORDER + 2 * ctx->m.subframe_size];

    int i, n, i_subfr;
    int pitch_delay_3x, pitch_delay_int;
    int16_t B[LP_FILTER_ORDER];           // FIXME: I don't know what is this
    int16_t var348[LP_FILTER_ORDER + 30]; // FIXME: I don't know what is this

    if (params->bfi) {
        for (i=0; i<5; i++) {
            params->fc_indexes[0][i] = av_lfg_get(&ctx->random_state) & 0x1ff;
            params->fc_indexes[1][i] = av_lfg_get(&ctx->random_state) & 0x1ff;
        }
        lsf_restore_from_previous(lsf, ctx->past_q_target, ctx->past_lsf_q,
                                  ctx->ma_predictor_prev);
    } else {
        lsf_decode(ctx->past_q_target, lsf, params->ma_pred_switch,
                   params->vq_indexes);
        ff_acelp_reorder_lsf(lsf, LSFQ_DIFF_MIN, LSFQ_MIN, LSFQ_MAX,
                             LP_FILTER_ORDER);

        memcpy(ctx->past_lsf_q, lsf, LP_FILTER_ORDER * sizeof(int16_t));
        ctx->ma_predictor_prev = params->ma_pred_switch;
    }
    ff_acelp_lsf2lsp(lsp, lsf, LP_FILTER_ORDER);

    ff_acelp_lp_decode(lp_filter_coeffs[0], lp_filter_coeffs[1], lsp,
                       ctx->past_lsf, LP_FILTER_ORDER);
    memcpy(ctx->past_lsf, lsp, LP_FILTER_ORDER * sizeof(int16_t));

    memcpy(synth, ctx->mem_syn, LP_FILTER_ORDER * sizeof(int16_t));
    for (i=0, i_subfr=0; i < ctx->m.subframe_count; i++, i_subfr += ctx->m.subframe_size) {
        if (params->bfi) {
            pitch_delay_3x = 3 * (ctx->pitch_delay_int_prev + 1);
        } else {
            if (!i)
                pitch_delay_3x = dec_delay3_1st(params->pitch_delay[i]);
            else
                pitch_delay_3x = dec_delay3_2nd(params->pitch_delay[i], PITCH_MIN, PITCH_MAX,  ctx->pitch_delay_int_prev);
        }
        pitch_delay_int = (pitch_delay_3x+1)/3;

        if (params->bfi)
            gain_pitch = (ctx->past_gain_pit * 15565) >> 14; // 0.95 in (1.14)
        else
            gain_pitch = qua_gain_pitch[params->gp_index[i]];
        ctx->past_gain_pit = FFMIN(gain_pitch, 1<<14);

        ff_acelp_interpolate(ctx->exc + i_subfr,
                             ctx->exc + i_subfr - pitch_delay_3x/3,
                             ff_acelp_interp_filter, 6, (pitch_delay_3x%3)<<1,
                             10, ctx->m.subframe_size);


        memset(fc_v, 0, ctx->m.subframe_size * sizeof(int16_t));
        ff_acelp_two_pulses_per_track(fc_v, ff_fc_4pulses_8bits_tracks_13,
                                      ff_fc_4pulses_8bits_tracks_13,
                                      params->fc_indexes[i], 5, 4);

        // enhance harmonics
        ff_acelp_weighted_vector_sum(fc_v + pitch_delay_int,
                                     fc_v + pitch_delay_int,
                                     fc_v, 1 << 14, ctx->past_gain_pit, 0, 14,
                                     ctx->m.subframe_size - pitch_delay_int);

        if (params->bfi) {
            ff_acelp_update_past_gain(ctx->past_qua_en, 0, 1, 1);
            gain_code = (ctx->past_gain_code * 6554 ) >> 13; // 0.8 in Q13
        } else {
            int gain_corr_factor = qua_gain_code[params->gc_index[i]];
            gain_code = ff_acelp_decode_gain_code(&ctx->dsp, gain_corr_factor,
                                                  fc_v, 0xe8985,
                                                  ctx->past_qua_en,
                                                  gain_prediction_coeff,
                                                  ctx->m.subframe_size, 2);
            ff_acelp_update_past_gain(ctx->past_qua_en, gain_corr_factor, 1, 0);
        }
        ctx->past_gain_code = gain_code;

        ff_acelp_weighted_vector_sum(&ctx->exc[i_subfr], &ctx->exc[i_subfr],
                                     fc_v, gain_pitch, gain_code, 0x2000, 14,
                                     ctx->m.subframe_size);

        ff_celp_lp_synthesis_filter(&synth[i_subfr+16],
                                    &lp_filter_coeffs[i_subfr?1:0][1],
                                    &ctx->exc[i_subfr], ctx->m.subframe_size,
                                    LP_FILTER_ORDER, 0, 0x800);

        ctx->pitch_delay_int_prev = pitch_delay_int;
    }
    memcpy(ctx->mem_syn, synth + 2*ctx->m.subframe_size,
           LP_FILTER_ORDER * sizeof(int16_t));
    memmove(ctx->old_exc, ctx->old_exc + 2 * ctx->m.subframe_size,
            (L_INTERPOL+PITCH_MAX) * sizeof(int16_t));


    /* postfilter start */
    memcpy(synth, ctx->mem_preemph, LP_FILTER_ORDER * sizeof(int16_t));
    memcpy(var348, synth, LP_FILTER_ORDER * sizeof(int16_t));

    for (n=0; n<LP_FILTER_ORDER + 1; n++)
        B[n] = (ctx->A_past[n] * weight_pow[n] + 0x4000) >> 15;

    ff_celp_lp_synthesis_filter(var348 + LP_FILTER_ORDER, ctx->B_past+1,
                                synth  + LP_FILTER_ORDER, 30, LP_FILTER_ORDER,
                                0, 0x800);
    ff_celp_lp_synthesis_filter(synth + LP_FILTER_ORDER, B+1,
                                synth + LP_FILTER_ORDER,
                                2 * ctx->m.subframe_size, LP_FILTER_ORDER,
                                0, 0x800);

    for (i=0; i<30; i++)
        synth[i + LP_FILTER_ORDER] =
                (div_30[30-i] * var348[i + LP_FILTER_ORDER] +
                 div_30[   i] *  synth[i + LP_FILTER_ORDER]) >> 15;

    memcpy(ctx->B_past, B, LP_FILTER_ORDER * sizeof(int16_t));
    memcpy(ctx->A_past, &lp_filter_coeffs[1][0],
           (LP_FILTER_ORDER + 1) * sizeof(int16_t));
    memcpy(ctx->mem_preemph, synth + 2 * ctx->m.subframe_size,
           LP_FILTER_ORDER * sizeof(int16_t));
    /* postfilter end */


    for (i=0; i < ctx->m.subframe_size * ctx->m.subframe_count; i++)
        out_data[i] = av_clip_int16(synth[i + LP_FILTER_ORDER]);

    return ctx->m.subframe_size * ctx->m.subframe_count;
}

/**
 * Extract decoding parameters from the input bitstream.
 * @param parms          parameters structure
 * @param pgb            pointer to initialized GetbitContext structure
 */
static void decode_parameters(Sipr_Parameters* parms, GetBitContext *pgb,
                              const Sipr_Mode_Param *p)
{
    int i, j;

    parms->bfi              = 0;
#ifdef TEST_BFI
    parms->bfi = (1.0 * av_lfg_get(&ctx->random_state) / 0xffffffffUL) > TEST_BFI;
#endif

    parms->ma_pred_switch   = get_bits(pgb, p->ma_predictor_bits);

    for(i=0; i<5; i++)
            parms->vq_indexes[i]    = get_bits(pgb, p->vq_indexes_bits[i]);

    for(i=0; i < p->subframe_count; i++) {
        parms->pitch_delay[i]   = get_bits(pgb, p->pitch_delay_bits[i]);
        parms->gp_index[i]      = get_bits(pgb, p->gp_index_bits);
        for(j=0; j < p->number_of_fc_indexes; j++)
            parms->fc_indexes[i][j] = get_bits(pgb, p->fc_index_bits);
        parms->gc_index[i]      = get_bits(pgb, p->gc_index_bits);
    }
}

static void sipr_decode_lp(int16_t *lp_filter_coeff, const int16_t *isfnew, const int16_t *past_lsf, int num_subfr, int filter_order)
{
    int i,j,t;
    int16_t isfint[filter_order];
    int t0 = 32767 / num_subfr;
    int16_t *pAz = lp_filter_coeff;

    t = t0 >> 1;
    for (i=0; i<num_subfr; i++) {
        for (j=0;j<filter_order; j++)
            isfint[j] = (past_lsf[j] * (32767 - t) + t * isfnew[j]) >> 15;

        ff_acelp_isp2lpc(pAz, isfint, filter_order >> 1);
        pAz += filter_order + 1;
        t += t0;
    }
}

static void dec_lag3_8k5(int index, int i_subfr, int third_as_first, int bfi, int *old_pitch, int *t0_first, int* T0, int* T0_frac)
{
    int i;

    if (((i_subfr==0)||(i_subfr==2 && third_as_first)) && (bfi==0)) {
        if (index < 197) {
           *T0 = (index+2)/3 + 19;
           *T0_frac = index - *T0*3 + 58;
        } else {
          *T0 = index - 112;
          *T0_frac = 0;
        }
        *t0_first = *T0;
    } else if ((i_subfr!=0) && (bfi==0)) {
        i = (index + 2) / 3 - 1;
        *T0 = i + FFMIN(FFMAX(*t0_first - 5, PITCH_MIN_8k5), PITCH_MAX_8k5 - 9);
        *T0_frac = index - 3*i - 2;
    } else if ((i_subfr==0) && (bfi==2)) {
        *T0 = *old_pitch / 3 + 1;
        if (*T0 > PITCH_MAX_8k5)
            *T0 = PITCH_MAX_8k5;
        *T0_frac = 0;
        *t0_first = *T0;
    } else if ((i_subfr!=0) && (bfi==2)) {
        *T0 = *old_pitch / 3 + 1;
        if (*T0 > PITCH_MAX_8k5)
            *T0 = PITCH_MAX_8k5;
        *T0_frac = 0;
    }
    *old_pitch = 3 * *T0 + *T0_frac;
}

static void interpol_8k5(float *in, float *out, float *filter, int filter_len,
                         int frac, int length)
{
    float *C1,*X0;
    int step,i,j;
    int filter_len_2x = filter_len << 1;
    if (frac < 0) {
        C1 = &filter[2*filter_len_2x-1];
        X0 = &in[-filter_len];
        step = -2;
   } else {
        C1 = &filter[frac];
        X0 = &in[-filter_len+1];
        step = 2;
    }

    for (i=0; i<length; i++) {
        float *ptr = C1;
        float s = 0;
        for (j=0; j<filter_len_2x; j++, ptr+=step)
            s += X0[j] * ptr[0];

        out[i] = s;
        X0++;
    }
}

static void find_F_fp(int16_t *Az_fp, int pitch_delay_int, int16_t *freq_fp,
                      int16_t treshpit, int length)
{
    int i;
    float fac1 = 0.55;
    float fac2 = 0.70;
    int16_t tmp[LP_FILTER_ORDER_8k5+1];

    memset(freq_fp, 0, LP_FILTER_ORDER_8k5 * sizeof(int16_t));
    memset(freq_fp + 2*LP_FILTER_ORDER_8k5, 0, (length-LP_FILTER_ORDER_8k5) * sizeof(int16_t));

    freq_fp[LP_FILTER_ORDER_8k5] = tmp[0] = Az_fp[0];
    for (i=1; i< LP_FILTER_ORDER_8k5+1; i++) {
        tmp[i] = fac2 * Az_fp[i];
        freq_fp[LP_FILTER_ORDER_8k5+i] = fac1 * Az_fp[i];
        fac1 *= 0.55;
        fac2 *= 0.70;
    }
    ff_celp_lp_synthesis_filter(freq_fp + LP_FILTER_ORDER_8k5, tmp+1,
                                freq_fp + LP_FILTER_ORDER_8k5, length,
                                LP_FILTER_ORDER_8k5, 0, 0);
    /* Code Above looks like: A'(z) := A(z/gamma3)/A(z/gamma4) */

    ff_acelp_weighted_vector_sum(
        freq_fp+LP_FILTER_ORDER_8k5+pitch_delay_int,
        freq_fp+LP_FILTER_ORDER_8k5+pitch_delay_int,
        freq_fp+LP_FILTER_ORDER_8k5,
        32767,
        treshpit,
        1<<14,
        15,
        length-pitch_delay_int);
}

static void dec_ACB_27(int *fc_indexes, int16_t *xf, int16_t *code, int length)
{
    int i,n;

    for (n=0; n<3; n++) {
        int index1 = 3 * ((fc_indexes[n] >> 4) & 0xf) + n;
        int index2 = 3 * (fc_indexes[n] & 0xf) + n;
        float X = (fc_indexes[n] & 0x100) ? -1.0: 1.0;

        if (X < 0)
            for (i=index1; i < length; i++)
                code[i] -= xf[i-index1];
        else
            for (i=index1; i < length; i++)
                code[i] += xf[i-index1];

        if (index2 < index1)
            X = -X;

        if (X < 0)
            for (i=index2; i < length; i++)
                code[i] -= xf[i-index2];
        else
            for (i=index2; i < length; i++)
                code[i] += xf[i-index2];
    }
}

static void dec_ACB_15(int *fc_indexes, int16_t* xf, int16_t* code, int length)
{
    int i, j;
    int index;

    for (i=0; i<3; i++) {
        index = 3 * (fc_indexes[i] & 0xf) + i;

        if (fc_indexes[i] & 0x10) {
            for (j=index; j < length; j++)
                code[j] += xf[j-index];
        } else {
            for (j=index; j < length; j++)
                code[j] -= xf[j-index];
        }
    }
}

static void dec_acb_2p10b(int fc_indexes, int16_t *xf, int16_t *code,
                          int length)
{
    int index1, index2, offset, i;

    offset = (fc_indexes >> 8) & 1;

    index1 = ((fc_indexes >> 4) & 0xf) * 3 + offset;
    index2 = (fc_indexes & 0xf) * 3 + offset + 1;

    if (fc_indexes & 0x200)
        FFSWAP(int, index1, index2);

    for (i=index1; i<length; i++)
        code[i] += xf[i-index1];

    for (i=index2; i<length; i++)
        code[i] += xf[i-index2];
}

static void dec_acb_3p10b(int fc_indexes, int16_t *xf, int16_t *code,
                          int length)
{
    int offset, i, j;

    offset = (fc_indexes & 0x200) ? 2 : 0;

    for (i=2; i>=0; i--) {
        int index = (fc_indexes & 0x7) * 6 + i * 2;

        if ((offset + index) & 0x3)
            for (j=index; j<length; j++)
                code[j] -= xf[j-index];
        else
            for (j=index; j<length; j++)
                code[j] += xf[j-index];

        fc_indexes >>= 3;
    }
}

static float d_gain_8k5(DSPContext *dsp, int index, int subframe_len,
                        const int16_t *code,  const float *dec_past_q_ener)
{
    int i;

    float ener_code = 0.01;
    float pred_code = 34.0;

    ener_code = dsp->scalarproduct_int16(code, code, subframe_len, 8);
    ener_code = 0.01 + ener_code / (1<<16);
    ener_code = 10.0 * log10(ener_code / subframe_len);

    for (i=0; i<4; i++)
        pred_code += pred_8k5[i] * dec_past_q_ener[i];
    pred_code -= ener_code;

    return (float)pow(10, pred_code / 20) * t_qua_gain[index][1];
}

static int decod_frame_8k5(Sipr_Context * ctx, Sipr_Parameters *params,
                           int16_t* out_data)
{
    int i_subfr, i, j;
    int16_t Az_fp[(LP_FILTER_ORDER_8k5+1)*ctx->m.subframe_count];
    int16_t isp[LP_FILTER_ORDER_8k5];
    float exc_base[PITCH_MAX_8k5 + L_INTERPOL + ctx->m.subframe_size];
    int16_t *pAz_fp;
    float *exc;
    int16_t xf_buf[ctx->m.subframe_size+LP_FILTER_ORDER_8k5];//FIXME: what is this?
    int16_t *xf = xf_buf;
    int16_t *code_fp = ctx->code_fp;
    int T0,T0_frac,t0_first = 0;
    float gain_pit, gain_code, gain_pit_inc;
    int16_t synth_fp[ctx->m.subframe_size * ctx->m.subframe_count+LP_FILTER_ORDER_8k5];
    float q_ener;

    d_isf_ma_8k5(isp, ctx->past_q_target, ctx->past_lsf_q, params, 0);

    sipr_decode_lp(Az_fp, isp, ctx->past_lsf, ctx->m.subframe_count, LP_FILTER_ORDER_8k5);
    memcpy(ctx->past_lsf, isp, LP_FILTER_ORDER_8k5 * sizeof(int16_t));


    exc = &exc_base[PITCH_MAX_8k5 + L_INTERPOL];
    pAz_fp = Az_fp;

    memcpy(synth_fp, ctx->mem_syn, LP_FILTER_ORDER_8k5 * sizeof(int16_t));
    memmove(exc_base, ctx->old_exc_8k5, (PITCH_MAX_8k5 + L_INTERPOL) * sizeof(float));
    for (i_subfr=0, i=0; i < ctx->m.subframe_count; i_subfr+=ctx->m.subframe_size, i++) {
        dec_lag3_8k5(params->pitch_delay[i], i, ctx->mode == MODE_5k0, 0, &ctx->pitch_delay_int_prev, &t0_first, &T0, &T0_frac);

        /* The following is pred_lt(exc, T0, T0_frac, ctx->m.subframe_size) */
        interpol_8k5(exc-T0, exc, inter3_2_8k5, 10,-T0_frac, ctx->m.subframe_size);

        find_F_fp(pAz_fp, T0, xf, ctx->m.treshpit, ctx->m.subframe_size);

        xf += LP_FILTER_ORDER_8k5;

        if (params->bfi) {
            params->fc_indexes[i][0] = av_lfg_get(&ctx->random_state) & 0x1ff;
            params->fc_indexes[i][1] = av_lfg_get(&ctx->random_state) & 0x1ff;
            params->fc_indexes[i][2] = av_lfg_get(&ctx->random_state) & 0x1ff;
        }

        memset(code_fp, 0, ctx->m.subframe_size * sizeof(int16_t));
        switch (ctx->mode) {
        case MODE_6k5:
            dec_ACB_15(params->fc_indexes[i], xf, code_fp,
                       ctx->m.subframe_size);
            break;
        case MODE_8k5:
            dec_ACB_27(params->fc_indexes[i], xf, code_fp,
                       ctx->m.subframe_size);
            break;
        case MODE_5k0:
            if (ctx->past_gain_pit_8k5 < 0.8)
                dec_acb_3p10b(params->fc_indexes[i][0], xf, code_fp,
                              ctx->m.subframe_size);
            else
                dec_acb_2p10b(params->fc_indexes[i][0], xf, code_fp,
                              ctx->m.subframe_size);
            break;
        default:
            av_log(ctx->avctx, AV_LOG_ERROR, "Fatal error: Mode %d is not supported by fixed-codevector decode routine\n", ctx->mode);
            return 0;
        }

        if (params->bfi) {
            if (params->bfi==1)
                ctx->past_gain_pit_8k5 += gain_pit_inc;
            else
                ctx->past_gain_pit_8k5 =
                    FFMIN(ctx->past_gain_pit_8k5 * 0.9, 1.0);

            gain_pit = FFMIN(ctx->past_gain_pit_8k5, 1.0);

            ctx->past_gain_code_8k5 = gain_code = 0.8 * ctx->past_gain_code_8k5;

            q_ener = -14;
            for (j=0; j<4; j++)
                q_ener += ctx->dec_past_q_ener[j];
            q_ener = 0.2 * q_ener;
        } else {
            ctx->past_gain_pit_8k5 = gain_pit =
                t_qua_gain[params->gc_index[i]][0];

            ctx->past_gain_code_8k5 = gain_code =
                d_gain_8k5(&ctx->dsp, params->gc_index[i], ctx->m.subframe_size, code_fp, ctx->dec_past_q_ener);

            q_ener = log10(t_qua_gain[params->gc_index[i]][1]) * 20.0;
        }

        ctx->dec_past_q_ener[3] = ctx->dec_past_q_ener[2];
        ctx->dec_past_q_ener[2] = ctx->dec_past_q_ener[1];
        ctx->dec_past_q_ener[1] = ctx->dec_past_q_ener[0];
        ctx->dec_past_q_ener[0] = q_ener;

        for (j=0; j<ctx->m.subframe_size; j++)
            exc[j] = exc[j] * gain_pit + (code_fp[j] * gain_code) / (1<<12);

        gain_pit *= 0.5 * gain_pit;
        gain_pit = FFMIN(gain_pit, 0.4);

        ctx->gain_har = 0.7 * ctx->gain_har + 0.3 * gain_pit;
        ctx->gain_har = FFMIN(ctx->gain_har, gain_pit);
        gain_code *= ctx->gain_har;

        for (j=0; j < ctx->m.subframe_size; j++)
            code_fp[j] = exc[j] - (gain_code * code_fp[j]) / (1<<12);

        if (ff_celp_lp_synthesis_filter(&synth_fp[i_subfr+LP_FILTER_ORDER_8k5],
                                        pAz_fp+1, code_fp,
                                        ctx->m.subframe_size,
                                        LP_FILTER_ORDER_8k5, 1, 0x800)) {
            for (j=0; j<ctx->m.subframe_size; j++)
                code_fp[i] >>= 1;
            ff_celp_lp_synthesis_filter(&synth_fp[i_subfr+LP_FILTER_ORDER_8k5],
                                        pAz_fp+1, code_fp,
                                        ctx->m.subframe_size,
                                        LP_FILTER_ORDER_8k5, 0, 0x800);
        }

        memmove(exc_base, exc_base + ctx->m.subframe_size,
                (PITCH_MAX_8k5 + L_INTERPOL) * sizeof(float));
        pAz_fp += LP_FILTER_ORDER_8k5+1;
    }
    memmove(ctx->old_exc_8k5, exc_base,
            (PITCH_MAX_8k5 + L_INTERPOL) * sizeof(float));

    memcpy(ctx->mem_syn,
           &synth_fp[ctx->m.subframe_size * ctx->m.subframe_count],
           LP_FILTER_ORDER_8k5 * sizeof(int16_t));

    for (i=0; i < ctx->m.subframe_size * ctx->m.subframe_count; i++)
        synth_fp[i+LP_FILTER_ORDER_8k5] >>= 1;

    memcpy(synth_fp+LP_FILTER_ORDER_8k5 - 2, ctx->hpf_z, 2 * sizeof(int16_t));

    ff_acelp_high_pass_filter(out_data, ctx->hpf_f,
                              synth_fp+LP_FILTER_ORDER_8k5,
                              ctx->m.subframe_size * ctx->m.subframe_count);

    memcpy(ctx->hpf_z,
           synth_fp+LP_FILTER_ORDER_8k5+ctx->m.subframe_size * ctx->m.subframe_count - 2, 2 * sizeof(int16_t));

    return ctx->m.subframe_size * ctx->m.subframe_count;
}

static av_cold void init_decod_16k(Sipr_Context * ctx)
{
    int i;

    ctx->past_qua_en[0] = -14336;
    ctx->past_qua_en[1] = -14336;

    ctx->exc = ctx->old_exc + L_INTERPOL + PITCH_MAX;

    ctx->B_past[0] = ctx->A_past[0] = 4096;

    for (i=0; i<LP_FILTER_ORDER; i++) {
        ctx->past_lsf_q[i] = (i+1) * LSFQ_MAX / (LP_FILTER_ORDER + 1);
        ctx->past_q_target[i] = (i+1) * LSFQ_MAX / (LP_FILTER_ORDER + 1);
        ctx->past_lsf[i] = ff_cos((i+1) * 16384 / (LP_FILTER_ORDER + 1));
    }

}
static av_cold void init_decod_8k5(Sipr_Context *ctx)
{
    int i;

    ctx->old_pitch = 180;

    for (i=0; i<LP_FILTER_ORDER_8k5; i++)
        ctx->past_lsf[i] = ff_cos((i+1) * 16384 / (LP_FILTER_ORDER_8k5 + 1));

    memcpy(ctx->past_lsf_q, mean_isf_8k5,
           LP_FILTER_ORDER_8k5 * sizeof(int16_t));

    ctx->dec_past_q_ener[0] = -14;
    ctx->dec_past_q_ener[1] = -14;
    ctx->dec_past_q_ener[2] = -14;
    ctx->dec_past_q_ener[3] = -14;
}

static av_cold int ff_sipr_decoder_init(AVCodecContext * avctx)
{
    Sipr_Context *ctx = avctx->priv_data;
    const char *m_name;

    if (avctx->bit_rate > 12200) {
        ctx->mode = MODE_16k;
        init_decod_16k(ctx);
        m_name = "16k";
    } else if (avctx->bit_rate > 7500) {
        ctx->mode = MODE_8k5;
        init_decod_8k5(ctx);
        m_name = "8k5";
    } else if (avctx->bit_rate > 5750) {
        ctx->mode = MODE_6k5;
        init_decod_8k5(ctx);
        m_name = "6k5";
    } else {
        av_log(avctx, AV_LOG_ERROR, "5k0 postfilter code not yet implemented, quality might suffer.\n");
        ctx->mode = MODE_5k0;
        init_decod_8k5(ctx);
        m_name = "5k0";
    }
    ctx->m = modes[ctx->mode];
    av_log(avctx, AV_LOG_ERROR, "Mode: %s\n", m_name);


    dsputil_init(&ctx->dsp, avctx);
    av_lfg_init(&ctx->random_state, 0x27425745);

    return 0;
}

static int ff_sipr_decode_frame(AVCodecContext *avctx,
                             void *datap, int *data_size,
                          /*   AVPacket *avpkt*/const uint8_t *buf, int buf_size)
{
    Sipr_Context *ctx = avctx->priv_data;
   // const uint8_t *buf=avpkt->data;
   // int buf_size = avpkt->size;
    Sipr_Parameters parm;
    GetBitContext       gb;
    int16_t *data = datap;
    int i;

    ctx->avctx = avctx;
    if (buf_size < (ctx->m.bits_per_frame>>3)) {
        av_log(avctx, AV_LOG_ERROR,
               "Error processing packet: packet size (%d) too small\n",
               buf_size);

        *data_size = 0;
        return -1;
    }
    if (*data_size < ctx->m.subframe_size * ctx->m.subframe_count * 2) {
        av_log(avctx, AV_LOG_ERROR,
               "Error processing packet: output buffer (%d) too small\n",
               *data_size);

        *data_size = 0;
        return -1;
    }

    init_get_bits(&gb, buf, ctx->m.bits_per_frame);

    *data_size = 0;
    for (i=0; i < ctx->m.frames_per_packet; i++) {
        int bytes_decoded;

        decode_parameters(&parm, &gb, &ctx->m);

        if (ctx->mode == MODE_16k)
            bytes_decoded = decod_frame_16k(ctx, &parm, data) * 2;
        else
            bytes_decoded = decod_frame_8k5(ctx, &parm, data) * 2;
        *data_size += bytes_decoded;
        data += bytes_decoded/2;
    }

    return (ctx->m.bits_per_frame>>3);
};

AVCodec sipr_decoder =
{
    "sipr",
    CODEC_TYPE_AUDIO,
    CODEC_ID_SIPR,
    sizeof(Sipr_Context),
    ff_sipr_decoder_init,
    NULL,
    NULL,
    ff_sipr_decode_frame,
    .long_name = NULL_IF_CONFIG_SMALL("RealAudio sipr"),
};
