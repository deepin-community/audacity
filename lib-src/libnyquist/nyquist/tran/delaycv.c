#include "stdio.h"
#ifndef mips
#include "stdlib.h"
#endif
#include "xlisp.h"
#include "sound.h"

#include "falloc.h"
#include "cext.h"
#include "delaycv.h"

void delaycv_free(snd_susp_type a_susp);


typedef struct delaycv_susp_struct {
    snd_susp_node susp;
    boolean started;
    int64_t terminate_cnt;
    sound_type input;
    int input_cnt;
    sample_block_values_type input_ptr;

    /* support for interpolation of input */
    sample_type input_x1_sample;
    double input_pHaSe;
    double input_pHaSe_iNcR;

    /* support for ramp between samples of input */
    double output_per_input;
    int64_t input_n;
    sound_type feedback;
    int feedback_cnt;
    sample_block_values_type feedback_ptr;

    /* support for interpolation of feedback */
    sample_type feedback_x1_sample;
    double feedback_pHaSe;
    double feedback_pHaSe_iNcR;

    /* support for ramp between samples of feedback */
    double output_per_feedback;
    int64_t feedback_n;

    long delaylen;
    sample_type *delaybuf;
    sample_type *delayptr;
    sample_type *endptr;
} delaycv_susp_node, *delaycv_susp_type;


void delaycv_nn_fetch(snd_susp_type a_susp, snd_list_type snd_list)
{
    delaycv_susp_type susp = (delaycv_susp_type) a_susp;
    int cnt = 0; /* how many samples computed */
    int togo;
    int n;
    sample_block_type out;
    register sample_block_values_type out_ptr;

    register sample_block_values_type out_ptr_reg;

    register sample_type * delayptr_reg;
    register sample_type * endptr_reg;
    register sample_block_values_type feedback_ptr_reg;
    register sample_block_values_type input_ptr_reg;
    falloc_sample_block(out, "delaycv_nn_fetch");
    out_ptr = out->samples;
    snd_list->block = out;

    while (cnt < max_sample_block_len) { /* outer loop */
        /* first compute how many samples to generate in inner loop: */
        /* don't overflow the output sample block: */
        togo = max_sample_block_len - cnt;

        /* don't run past the input input sample block: */
        susp_check_term_samples(input, input_ptr, input_cnt);
        togo = min(togo, susp->input_cnt);

        /* don't run past the feedback input sample block: */
        susp_check_samples(feedback, feedback_ptr, feedback_cnt);
        togo = min(togo, susp->feedback_cnt);

        /* don't run past terminate time */
        if (susp->terminate_cnt != UNKNOWN &&
            susp->terminate_cnt <= susp->susp.current + cnt + togo) {
            togo = (int) (susp->terminate_cnt - (susp->susp.current + cnt));
            if (togo < 0) togo = 0;  /* avoids rounding errros */
            if (togo == 0) break;
        }

        n = togo;
        delayptr_reg = susp->delayptr;
        endptr_reg = susp->endptr;
        feedback_ptr_reg = susp->feedback_ptr;
        input_ptr_reg = susp->input_ptr;
        out_ptr_reg = out_ptr;
        if (n) do { /* the inner sample computation loop */
            *out_ptr_reg++ = *delayptr_reg;
            *delayptr_reg = *delayptr_reg * (sample_type) *feedback_ptr_reg++ +
                        (sample_type) *input_ptr_reg++;
            if (++delayptr_reg >= endptr_reg) delayptr_reg = susp->delaybuf;
        } while (--n); /* inner loop */

        susp->delayptr = delayptr_reg;
        susp->endptr = endptr_reg;
        /* using feedback_ptr_reg is a bad idea on RS/6000: */
        susp->feedback_ptr += togo;
        /* using input_ptr_reg is a bad idea on RS/6000: */
        susp->input_ptr += togo;
        out_ptr += togo;
        susp_took(input_cnt, togo);
        susp_took(feedback_cnt, togo);
        cnt += togo;
    } /* outer loop */

    /* test for termination */
    if (togo == 0 && cnt == 0) {
        snd_list_terminate(snd_list);
    } else {
        snd_list->block_len = cnt;
        susp->susp.current += cnt;
    }
} /* delaycv_nn_fetch */


void delaycv_ns_fetch(snd_susp_type a_susp, snd_list_type snd_list)
{
    delaycv_susp_type susp = (delaycv_susp_type) a_susp;
    int cnt = 0; /* how many samples computed */
    int togo;
    int n;
    sample_block_type out;
    register sample_block_values_type out_ptr;

    register sample_block_values_type out_ptr_reg;

    register sample_type * delayptr_reg;
    register sample_type * endptr_reg;
    register sample_type feedback_scale_reg = susp->feedback->scale;
    register sample_block_values_type feedback_ptr_reg;
    register sample_block_values_type input_ptr_reg;
    falloc_sample_block(out, "delaycv_ns_fetch");
    out_ptr = out->samples;
    snd_list->block = out;

    while (cnt < max_sample_block_len) { /* outer loop */
        /* first compute how many samples to generate in inner loop: */
        /* don't overflow the output sample block: */
        togo = max_sample_block_len - cnt;

        /* don't run past the input input sample block: */
        susp_check_term_samples(input, input_ptr, input_cnt);
        togo = min(togo, susp->input_cnt);

        /* don't run past the feedback input sample block: */
        susp_check_samples(feedback, feedback_ptr, feedback_cnt);
        togo = min(togo, susp->feedback_cnt);

        /* don't run past terminate time */
        if (susp->terminate_cnt != UNKNOWN &&
            susp->terminate_cnt <= susp->susp.current + cnt + togo) {
            togo = (int) (susp->terminate_cnt - (susp->susp.current + cnt));
            if (togo < 0) togo = 0;  /* avoids rounding errros */
            if (togo == 0) break;
        }

        n = togo;
        delayptr_reg = susp->delayptr;
        endptr_reg = susp->endptr;
        feedback_ptr_reg = susp->feedback_ptr;
        input_ptr_reg = susp->input_ptr;
        out_ptr_reg = out_ptr;
        if (n) do { /* the inner sample computation loop */
            *out_ptr_reg++ = *delayptr_reg;
            *delayptr_reg = *delayptr_reg * (sample_type) (feedback_scale_reg * *feedback_ptr_reg++) +
                        (sample_type) *input_ptr_reg++;
            if (++delayptr_reg >= endptr_reg) delayptr_reg = susp->delaybuf;
        } while (--n); /* inner loop */

        susp->delayptr = delayptr_reg;
        susp->endptr = endptr_reg;
        /* using feedback_ptr_reg is a bad idea on RS/6000: */
        susp->feedback_ptr += togo;
        /* using input_ptr_reg is a bad idea on RS/6000: */
        susp->input_ptr += togo;
        out_ptr += togo;
        susp_took(input_cnt, togo);
        susp_took(feedback_cnt, togo);
        cnt += togo;
    } /* outer loop */

    /* test for termination */
    if (togo == 0 && cnt == 0) {
        snd_list_terminate(snd_list);
    } else {
        snd_list->block_len = cnt;
        susp->susp.current += cnt;
    }
} /* delaycv_ns_fetch */


void delaycv_ni_fetch(snd_susp_type a_susp, snd_list_type snd_list)
{
    delaycv_susp_type susp = (delaycv_susp_type) a_susp;
    int cnt = 0; /* how many samples computed */
    sample_type feedback_x2_sample;
    int togo;
    int n;
    sample_block_type out;
    register sample_block_values_type out_ptr;

    register sample_block_values_type out_ptr_reg;

    register sample_type * delayptr_reg;
    register sample_type * endptr_reg;
    register double feedback_pHaSe_iNcR_rEg = susp->feedback_pHaSe_iNcR;
    register double feedback_pHaSe_ReG;
    register sample_type feedback_x1_sample_reg;
    register sample_block_values_type input_ptr_reg;
    falloc_sample_block(out, "delaycv_ni_fetch");
    out_ptr = out->samples;
    snd_list->block = out;

    /* make sure sounds are primed with first values */
    if (!susp->started) {
        susp->started = true;
        susp_check_samples(feedback, feedback_ptr, feedback_cnt);
        susp->feedback_x1_sample = susp_fetch_sample(feedback, feedback_ptr, feedback_cnt);
    }

    susp_check_samples(feedback, feedback_ptr, feedback_cnt);
    feedback_x2_sample = susp_current_sample(feedback, feedback_ptr);

    while (cnt < max_sample_block_len) { /* outer loop */
        /* first compute how many samples to generate in inner loop: */
        /* don't overflow the output sample block: */
        togo = max_sample_block_len - cnt;

        /* don't run past the input input sample block: */
        susp_check_term_samples(input, input_ptr, input_cnt);
        togo = min(togo, susp->input_cnt);

        /* don't run past terminate time */
        if (susp->terminate_cnt != UNKNOWN &&
            susp->terminate_cnt <= susp->susp.current + cnt + togo) {
            togo = (int) (susp->terminate_cnt - (susp->susp.current + cnt));
            if (togo < 0) togo = 0;  /* avoids rounding errros */
            if (togo == 0) break;
        }

        n = togo;
        delayptr_reg = susp->delayptr;
        endptr_reg = susp->endptr;
        feedback_pHaSe_ReG = susp->feedback_pHaSe;
        feedback_x1_sample_reg = susp->feedback_x1_sample;
        input_ptr_reg = susp->input_ptr;
        out_ptr_reg = out_ptr;
        if (n) do { /* the inner sample computation loop */
            if (feedback_pHaSe_ReG >= 1.0) {
                feedback_x1_sample_reg = feedback_x2_sample;
                /* pick up next sample as feedback_x2_sample: */
                susp->feedback_ptr++;
                susp_took(feedback_cnt, 1);
                feedback_pHaSe_ReG -= 1.0;
                susp_check_samples_break(feedback, feedback_ptr, feedback_cnt, feedback_x2_sample);
            }
            *out_ptr_reg++ = *delayptr_reg;
            *delayptr_reg = *delayptr_reg * (sample_type) 
                (feedback_x1_sample_reg * (1 - feedback_pHaSe_ReG) + feedback_x2_sample * feedback_pHaSe_ReG) +
                        (sample_type) *input_ptr_reg++;
            if (++delayptr_reg >= endptr_reg) delayptr_reg = susp->delaybuf;
            feedback_pHaSe_ReG += feedback_pHaSe_iNcR_rEg;
        } while (--n); /* inner loop */

        togo -= n;
        susp->delayptr = delayptr_reg;
        susp->endptr = endptr_reg;
        susp->feedback_pHaSe = feedback_pHaSe_ReG;
        susp->feedback_x1_sample = feedback_x1_sample_reg;
        /* using input_ptr_reg is a bad idea on RS/6000: */
        susp->input_ptr += togo;
        out_ptr += togo;
        susp_took(input_cnt, togo);
        cnt += togo;
    } /* outer loop */

    /* test for termination */
    if (togo == 0 && cnt == 0) {
        snd_list_terminate(snd_list);
    } else {
        snd_list->block_len = cnt;
        susp->susp.current += cnt;
    }
} /* delaycv_ni_fetch */


void delaycv_nr_fetch(snd_susp_type a_susp, snd_list_type snd_list)
{
    delaycv_susp_type susp = (delaycv_susp_type) a_susp;
    int cnt = 0; /* how many samples computed */
    sample_type feedback_DeLtA;
    sample_type feedback_val;
    sample_type feedback_x2_sample;
    int togo;
    int n;
    sample_block_type out;
    register sample_block_values_type out_ptr;

    register sample_block_values_type out_ptr_reg;

    register sample_type * delayptr_reg;
    register sample_type * endptr_reg;
    register sample_block_values_type input_ptr_reg;
    falloc_sample_block(out, "delaycv_nr_fetch");
    out_ptr = out->samples;
    snd_list->block = out;

    /* make sure sounds are primed with first values */
    if (!susp->started) {
        susp->started = true;
        susp->feedback_pHaSe = 1.0;
    }

    susp_check_samples(feedback, feedback_ptr, feedback_cnt);
    feedback_x2_sample = susp_current_sample(feedback, feedback_ptr);

    while (cnt < max_sample_block_len) { /* outer loop */
        /* first compute how many samples to generate in inner loop: */
        /* don't overflow the output sample block: */
        togo = max_sample_block_len - cnt;

        /* don't run past the input input sample block: */
        susp_check_term_samples(input, input_ptr, input_cnt);
        togo = min(togo, susp->input_cnt);

        /* grab next feedback_x2_sample when phase goes past 1.0; */
        /* we use feedback_n (computed below) to avoid roundoff errors: */
        if (susp->feedback_n <= 0) {
            susp->feedback_x1_sample = feedback_x2_sample;
            susp->feedback_ptr++;
            susp_took(feedback_cnt, 1);
            susp->feedback_pHaSe -= 1.0;
            susp_check_samples(feedback, feedback_ptr, feedback_cnt);
            feedback_x2_sample = susp_current_sample(feedback, feedback_ptr);
            /* feedback_n gets number of samples before phase exceeds 1.0: */
            susp->feedback_n = (int64_t) ((1.0 - susp->feedback_pHaSe) *
                                        susp->output_per_feedback);
        }
        togo = (int) min(togo, susp->feedback_n);
        feedback_DeLtA = (sample_type) ((feedback_x2_sample - susp->feedback_x1_sample) * susp->feedback_pHaSe_iNcR);
        feedback_val = (sample_type) (susp->feedback_x1_sample * (1.0 - susp->feedback_pHaSe) +
                 feedback_x2_sample * susp->feedback_pHaSe);

        /* don't run past terminate time */
        if (susp->terminate_cnt != UNKNOWN &&
            susp->terminate_cnt <= susp->susp.current + cnt + togo) {
            togo = (int) (susp->terminate_cnt - (susp->susp.current + cnt));
            if (togo < 0) togo = 0;  /* avoids rounding errros */
            if (togo == 0) break;
        }

        n = togo;
        delayptr_reg = susp->delayptr;
        endptr_reg = susp->endptr;
        input_ptr_reg = susp->input_ptr;
        out_ptr_reg = out_ptr;
        if (n) do { /* the inner sample computation loop */
            *out_ptr_reg++ = *delayptr_reg;
            *delayptr_reg = *delayptr_reg * (sample_type) feedback_val +
                        (sample_type) *input_ptr_reg++;
            if (++delayptr_reg >= endptr_reg) delayptr_reg = susp->delaybuf;
            feedback_val += feedback_DeLtA;
        } while (--n); /* inner loop */

        susp->delayptr = delayptr_reg;
        susp->endptr = endptr_reg;
        /* using input_ptr_reg is a bad idea on RS/6000: */
        susp->input_ptr += togo;
        out_ptr += togo;
        susp_took(input_cnt, togo);
        susp->feedback_pHaSe += togo * susp->feedback_pHaSe_iNcR;
        susp->feedback_n -= togo;
        cnt += togo;
    } /* outer loop */

    /* test for termination */
    if (togo == 0 && cnt == 0) {
        snd_list_terminate(snd_list);
    } else {
        snd_list->block_len = cnt;
        susp->susp.current += cnt;
    }
} /* delaycv_nr_fetch */


void delaycv_in_fetch(snd_susp_type a_susp, snd_list_type snd_list)
{
    delaycv_susp_type susp = (delaycv_susp_type) a_susp;
    int cnt = 0; /* how many samples computed */
    sample_type input_x2_sample;
    int togo;
    int n;
    sample_block_type out;
    register sample_block_values_type out_ptr;

    register sample_block_values_type out_ptr_reg;

    register sample_type * delayptr_reg;
    register sample_type * endptr_reg;
    register sample_block_values_type feedback_ptr_reg;
    register double input_pHaSe_iNcR_rEg = susp->input_pHaSe_iNcR;
    register double input_pHaSe_ReG;
    register sample_type input_x1_sample_reg;
    falloc_sample_block(out, "delaycv_in_fetch");
    out_ptr = out->samples;
    snd_list->block = out;

    /* make sure sounds are primed with first values */
    if (!susp->started) {
        susp->started = true;
        susp_check_term_samples(input, input_ptr, input_cnt);
        susp->input_x1_sample = susp_fetch_sample(input, input_ptr, input_cnt);
    }

    susp_check_term_samples(input, input_ptr, input_cnt);
    input_x2_sample = susp_current_sample(input, input_ptr);

    while (cnt < max_sample_block_len) { /* outer loop */
        /* first compute how many samples to generate in inner loop: */
        /* don't overflow the output sample block: */
        togo = max_sample_block_len - cnt;

        /* don't run past the feedback input sample block: */
        susp_check_samples(feedback, feedback_ptr, feedback_cnt);
        togo = min(togo, susp->feedback_cnt);

        /* don't run past terminate time */
        if (susp->terminate_cnt != UNKNOWN &&
            susp->terminate_cnt <= susp->susp.current + cnt + togo) {
            togo = (int) (susp->terminate_cnt - (susp->susp.current + cnt));
            if (togo < 0) togo = 0;  /* avoids rounding errros */
            if (togo == 0) break;
        }

        n = togo;
        delayptr_reg = susp->delayptr;
        endptr_reg = susp->endptr;
        feedback_ptr_reg = susp->feedback_ptr;
        input_pHaSe_ReG = susp->input_pHaSe;
        input_x1_sample_reg = susp->input_x1_sample;
        out_ptr_reg = out_ptr;
        if (n) do { /* the inner sample computation loop */
            if (input_pHaSe_ReG >= 1.0) {
                input_x1_sample_reg = input_x2_sample;
                /* pick up next sample as input_x2_sample: */
                susp->input_ptr++;
                susp_took(input_cnt, 1);
                input_pHaSe_ReG -= 1.0;
                susp_check_term_samples_break(input, input_ptr, input_cnt, input_x2_sample);
            }
            *out_ptr_reg++ = *delayptr_reg;
            *delayptr_reg = *delayptr_reg * (sample_type) *feedback_ptr_reg++ +
                        (sample_type) 
                (input_x1_sample_reg * (1 - input_pHaSe_ReG) + input_x2_sample * input_pHaSe_ReG);
            if (++delayptr_reg >= endptr_reg) delayptr_reg = susp->delaybuf;
            input_pHaSe_ReG += input_pHaSe_iNcR_rEg;
        } while (--n); /* inner loop */

        togo -= n;
        susp->delayptr = delayptr_reg;
        susp->endptr = endptr_reg;
        /* using feedback_ptr_reg is a bad idea on RS/6000: */
        susp->feedback_ptr += togo;
        susp->input_pHaSe = input_pHaSe_ReG;
        susp->input_x1_sample = input_x1_sample_reg;
        out_ptr += togo;
        susp_took(feedback_cnt, togo);
        cnt += togo;
    } /* outer loop */

    /* test for termination */
    if (togo == 0 && cnt == 0) {
        snd_list_terminate(snd_list);
    } else {
        snd_list->block_len = cnt;
        susp->susp.current += cnt;
    }
} /* delaycv_in_fetch */


void delaycv_is_fetch(snd_susp_type a_susp, snd_list_type snd_list)
{
    delaycv_susp_type susp = (delaycv_susp_type) a_susp;
    int cnt = 0; /* how many samples computed */
    sample_type input_x2_sample;
    int togo;
    int n;
    sample_block_type out;
    register sample_block_values_type out_ptr;

    register sample_block_values_type out_ptr_reg;

    register sample_type * delayptr_reg;
    register sample_type * endptr_reg;
    register sample_type feedback_scale_reg = susp->feedback->scale;
    register sample_block_values_type feedback_ptr_reg;
    register double input_pHaSe_iNcR_rEg = susp->input_pHaSe_iNcR;
    register double input_pHaSe_ReG;
    register sample_type input_x1_sample_reg;
    falloc_sample_block(out, "delaycv_is_fetch");
    out_ptr = out->samples;
    snd_list->block = out;

    /* make sure sounds are primed with first values */
    if (!susp->started) {
        susp->started = true;
        susp_check_term_samples(input, input_ptr, input_cnt);
        susp->input_x1_sample = susp_fetch_sample(input, input_ptr, input_cnt);
    }

    susp_check_term_samples(input, input_ptr, input_cnt);
    input_x2_sample = susp_current_sample(input, input_ptr);

    while (cnt < max_sample_block_len) { /* outer loop */
        /* first compute how many samples to generate in inner loop: */
        /* don't overflow the output sample block: */
        togo = max_sample_block_len - cnt;

        /* don't run past the feedback input sample block: */
        susp_check_samples(feedback, feedback_ptr, feedback_cnt);
        togo = min(togo, susp->feedback_cnt);

        /* don't run past terminate time */
        if (susp->terminate_cnt != UNKNOWN &&
            susp->terminate_cnt <= susp->susp.current + cnt + togo) {
            togo = (int) (susp->terminate_cnt - (susp->susp.current + cnt));
            if (togo < 0) togo = 0;  /* avoids rounding errros */
            if (togo == 0) break;
        }

        n = togo;
        delayptr_reg = susp->delayptr;
        endptr_reg = susp->endptr;
        feedback_ptr_reg = susp->feedback_ptr;
        input_pHaSe_ReG = susp->input_pHaSe;
        input_x1_sample_reg = susp->input_x1_sample;
        out_ptr_reg = out_ptr;
        if (n) do { /* the inner sample computation loop */
            if (input_pHaSe_ReG >= 1.0) {
                input_x1_sample_reg = input_x2_sample;
                /* pick up next sample as input_x2_sample: */
                susp->input_ptr++;
                susp_took(input_cnt, 1);
                input_pHaSe_ReG -= 1.0;
                susp_check_term_samples_break(input, input_ptr, input_cnt, input_x2_sample);
            }
            *out_ptr_reg++ = *delayptr_reg;
            *delayptr_reg = *delayptr_reg * (sample_type) (feedback_scale_reg * *feedback_ptr_reg++) +
                        (sample_type) 
                (input_x1_sample_reg * (1 - input_pHaSe_ReG) + input_x2_sample * input_pHaSe_ReG);
            if (++delayptr_reg >= endptr_reg) delayptr_reg = susp->delaybuf;
            input_pHaSe_ReG += input_pHaSe_iNcR_rEg;
        } while (--n); /* inner loop */

        togo -= n;
        susp->delayptr = delayptr_reg;
        susp->endptr = endptr_reg;
        /* using feedback_ptr_reg is a bad idea on RS/6000: */
        susp->feedback_ptr += togo;
        susp->input_pHaSe = input_pHaSe_ReG;
        susp->input_x1_sample = input_x1_sample_reg;
        out_ptr += togo;
        susp_took(feedback_cnt, togo);
        cnt += togo;
    } /* outer loop */

    /* test for termination */
    if (togo == 0 && cnt == 0) {
        snd_list_terminate(snd_list);
    } else {
        snd_list->block_len = cnt;
        susp->susp.current += cnt;
    }
} /* delaycv_is_fetch */


void delaycv_rn_fetch(snd_susp_type a_susp, snd_list_type snd_list)
{
    delaycv_susp_type susp = (delaycv_susp_type) a_susp;
    int cnt = 0; /* how many samples computed */
    sample_type input_DeLtA;
    sample_type input_val;
    sample_type input_x2_sample;
    int togo;
    int n;
    sample_block_type out;
    register sample_block_values_type out_ptr;

    register sample_block_values_type out_ptr_reg;

    register sample_type * delayptr_reg;
    register sample_type * endptr_reg;
    register sample_block_values_type feedback_ptr_reg;
    falloc_sample_block(out, "delaycv_rn_fetch");
    out_ptr = out->samples;
    snd_list->block = out;

    /* make sure sounds are primed with first values */
    if (!susp->started) {
        susp->started = true;
        susp->input_pHaSe = 1.0;
    }

    susp_check_term_samples(input, input_ptr, input_cnt);
    input_x2_sample = susp_current_sample(input, input_ptr);

    while (cnt < max_sample_block_len) { /* outer loop */
        /* first compute how many samples to generate in inner loop: */
        /* don't overflow the output sample block: */
        togo = max_sample_block_len - cnt;

        /* grab next input_x2_sample when phase goes past 1.0; */
        /* we use input_n (computed below) to avoid roundoff errors: */
        if (susp->input_n <= 0) {
            susp->input_x1_sample = input_x2_sample;
            susp->input_ptr++;
            susp_took(input_cnt, 1);
            susp->input_pHaSe -= 1.0;
            susp_check_term_samples(input, input_ptr, input_cnt);
            input_x2_sample = susp_current_sample(input, input_ptr);
            /* input_n gets number of samples before phase exceeds 1.0: */
            susp->input_n = (int64_t) ((1.0 - susp->input_pHaSe) *
                                        susp->output_per_input);
        }
        togo = (int) min(togo, susp->input_n);
        input_DeLtA = (sample_type) ((input_x2_sample - susp->input_x1_sample) * susp->input_pHaSe_iNcR);
        input_val = (sample_type) (susp->input_x1_sample * (1.0 - susp->input_pHaSe) +
                 input_x2_sample * susp->input_pHaSe);

        /* don't run past the feedback input sample block: */
        susp_check_samples(feedback, feedback_ptr, feedback_cnt);
        togo = min(togo, susp->feedback_cnt);

        /* don't run past terminate time */
        if (susp->terminate_cnt != UNKNOWN &&
            susp->terminate_cnt <= susp->susp.current + cnt + togo) {
            togo = (int) (susp->terminate_cnt - (susp->susp.current + cnt));
            if (togo < 0) togo = 0;  /* avoids rounding errros */
            if (togo == 0) break;
        }

        n = togo;
        delayptr_reg = susp->delayptr;
        endptr_reg = susp->endptr;
        feedback_ptr_reg = susp->feedback_ptr;
        out_ptr_reg = out_ptr;
        if (n) do { /* the inner sample computation loop */
            *out_ptr_reg++ = *delayptr_reg;
            *delayptr_reg = *delayptr_reg * (sample_type) *feedback_ptr_reg++ +
                        (sample_type) input_val;
            if (++delayptr_reg >= endptr_reg) delayptr_reg = susp->delaybuf;
            input_val += input_DeLtA;
        } while (--n); /* inner loop */

        susp->delayptr = delayptr_reg;
        susp->endptr = endptr_reg;
        /* using feedback_ptr_reg is a bad idea on RS/6000: */
        susp->feedback_ptr += togo;
        out_ptr += togo;
        susp->input_pHaSe += togo * susp->input_pHaSe_iNcR;
        susp->input_n -= togo;
        susp_took(feedback_cnt, togo);
        cnt += togo;
    } /* outer loop */

    /* test for termination */
    if (togo == 0 && cnt == 0) {
        snd_list_terminate(snd_list);
    } else {
        snd_list->block_len = cnt;
        susp->susp.current += cnt;
    }
} /* delaycv_rn_fetch */


void delaycv_rs_fetch(snd_susp_type a_susp, snd_list_type snd_list)
{
    delaycv_susp_type susp = (delaycv_susp_type) a_susp;
    int cnt = 0; /* how many samples computed */
    sample_type input_DeLtA;
    sample_type input_val;
    sample_type input_x2_sample;
    int togo;
    int n;
    sample_block_type out;
    register sample_block_values_type out_ptr;

    register sample_block_values_type out_ptr_reg;

    register sample_type * delayptr_reg;
    register sample_type * endptr_reg;
    register sample_type feedback_scale_reg = susp->feedback->scale;
    register sample_block_values_type feedback_ptr_reg;
    falloc_sample_block(out, "delaycv_rs_fetch");
    out_ptr = out->samples;
    snd_list->block = out;

    /* make sure sounds are primed with first values */
    if (!susp->started) {
        susp->started = true;
        susp->input_pHaSe = 1.0;
    }

    susp_check_term_samples(input, input_ptr, input_cnt);
    input_x2_sample = susp_current_sample(input, input_ptr);

    while (cnt < max_sample_block_len) { /* outer loop */
        /* first compute how many samples to generate in inner loop: */
        /* don't overflow the output sample block: */
        togo = max_sample_block_len - cnt;

        /* grab next input_x2_sample when phase goes past 1.0; */
        /* we use input_n (computed below) to avoid roundoff errors: */
        if (susp->input_n <= 0) {
            susp->input_x1_sample = input_x2_sample;
            susp->input_ptr++;
            susp_took(input_cnt, 1);
            susp->input_pHaSe -= 1.0;
            susp_check_term_samples(input, input_ptr, input_cnt);
            input_x2_sample = susp_current_sample(input, input_ptr);
            /* input_n gets number of samples before phase exceeds 1.0: */
            susp->input_n = (int64_t) ((1.0 - susp->input_pHaSe) *
                                        susp->output_per_input);
        }
        togo = (int) min(togo, susp->input_n);
        input_DeLtA = (sample_type) ((input_x2_sample - susp->input_x1_sample) * susp->input_pHaSe_iNcR);
        input_val = (sample_type) (susp->input_x1_sample * (1.0 - susp->input_pHaSe) +
                 input_x2_sample * susp->input_pHaSe);

        /* don't run past the feedback input sample block: */
        susp_check_samples(feedback, feedback_ptr, feedback_cnt);
        togo = min(togo, susp->feedback_cnt);

        /* don't run past terminate time */
        if (susp->terminate_cnt != UNKNOWN &&
            susp->terminate_cnt <= susp->susp.current + cnt + togo) {
            togo = (int) (susp->terminate_cnt - (susp->susp.current + cnt));
            if (togo < 0) togo = 0;  /* avoids rounding errros */
            if (togo == 0) break;
        }

        n = togo;
        delayptr_reg = susp->delayptr;
        endptr_reg = susp->endptr;
        feedback_ptr_reg = susp->feedback_ptr;
        out_ptr_reg = out_ptr;
        if (n) do { /* the inner sample computation loop */
            *out_ptr_reg++ = *delayptr_reg;
            *delayptr_reg = *delayptr_reg * (sample_type) (feedback_scale_reg * *feedback_ptr_reg++) +
                        (sample_type) input_val;
            if (++delayptr_reg >= endptr_reg) delayptr_reg = susp->delaybuf;
            input_val += input_DeLtA;
        } while (--n); /* inner loop */

        susp->delayptr = delayptr_reg;
        susp->endptr = endptr_reg;
        /* using feedback_ptr_reg is a bad idea on RS/6000: */
        susp->feedback_ptr += togo;
        out_ptr += togo;
        susp->input_pHaSe += togo * susp->input_pHaSe_iNcR;
        susp->input_n -= togo;
        susp_took(feedback_cnt, togo);
        cnt += togo;
    } /* outer loop */

    /* test for termination */
    if (togo == 0 && cnt == 0) {
        snd_list_terminate(snd_list);
    } else {
        snd_list->block_len = cnt;
        susp->susp.current += cnt;
    }
} /* delaycv_rs_fetch */


void delaycv_toss_fetch(snd_susp_type a_susp, snd_list_type snd_list)
{
    delaycv_susp_type susp = (delaycv_susp_type) a_susp;
    time_type final_time = susp->susp.t0;
    int n;

    /* fetch samples from input up to final_time for this block of zeros */
    while ((ROUNDBIG((final_time - susp->input->t0) * susp->input->sr)) >=
           susp->input->current)
        susp_get_samples(input, input_ptr, input_cnt);
    /* fetch samples from feedback up to final_time for this block of zeros */
    while ((ROUNDBIG((final_time - susp->feedback->t0) * susp->feedback->sr)) >=
           susp->feedback->current)
        susp_get_samples(feedback, feedback_ptr, feedback_cnt);
    /* convert to normal processing when we hit final_count */
    /* we want each signal positioned at final_time */
    n = (int) ROUNDBIG((final_time - susp->input->t0) * susp->input->sr -
         (susp->input->current - susp->input_cnt));
    susp->input_ptr += n;
    susp_took(input_cnt, n);
    n = (int) ROUNDBIG((final_time - susp->feedback->t0) * susp->feedback->sr -
         (susp->feedback->current - susp->feedback_cnt));
    susp->feedback_ptr += n;
    susp_took(feedback_cnt, n);
    susp->susp.fetch = susp->susp.keep_fetch;
    (*(susp->susp.fetch))(a_susp, snd_list);
}


void delaycv_mark(snd_susp_type a_susp)
{
    delaycv_susp_type susp = (delaycv_susp_type) a_susp;
    sound_xlmark(susp->input);
    sound_xlmark(susp->feedback);
}


void delaycv_free(snd_susp_type a_susp)
{
    delaycv_susp_type susp = (delaycv_susp_type) a_susp;
free(susp->delaybuf);
    sound_unref(susp->input);
    sound_unref(susp->feedback);
    ffree_generic(susp, sizeof(delaycv_susp_node), "delaycv_free");
}


void delaycv_print_tree(snd_susp_type a_susp, int n)
{
    delaycv_susp_type susp = (delaycv_susp_type) a_susp;
    indent(n);
    stdputstr("input:");
    sound_print_tree_1(susp->input, n);

    indent(n);
    stdputstr("feedback:");
    sound_print_tree_1(susp->feedback, n);
}


sound_type snd_make_delaycv(sound_type input, time_type delay, sound_type feedback)
{
    register delaycv_susp_type susp;
    rate_type sr = max(input->sr, feedback->sr);
    time_type t0 = max(input->t0, feedback->t0);
    int interp_desc = 0;
    sample_type scale_factor = 1.0F;
    time_type t0_min = t0;
    /* combine scale factors of linear inputs (INPUT) */
    scale_factor *= input->scale;
    input->scale = 1.0F;

    /* try to push scale_factor back to a low sr input */
    if (input->sr < sr) { input->scale = scale_factor; scale_factor = 1.0F; }

    falloc_generic(susp, delaycv_susp_node, "snd_make_delaycv");
    susp->delaylen = ROUND32(input->sr * delay);
    susp->delaybuf = (sample_type *) calloc (sizeof(double), susp->delaylen);
    susp->delayptr = susp->delaybuf;
    susp->endptr = susp->delaybuf + susp->delaylen;

    /* select a susp fn based on sample rates */
    interp_desc = (interp_desc << 2) + interp_style(input, sr);
    interp_desc = (interp_desc << 2) + interp_style(feedback, sr);
    switch (interp_desc) {
      case INTERP_nn: susp->susp.fetch = delaycv_nn_fetch; break;
      case INTERP_ns: susp->susp.fetch = delaycv_ns_fetch; break;
      case INTERP_ni: susp->susp.fetch = delaycv_ni_fetch; break;
      case INTERP_nr: susp->susp.fetch = delaycv_nr_fetch; break;
      case INTERP_in: susp->susp.fetch = delaycv_in_fetch; break;
      case INTERP_is: susp->susp.fetch = delaycv_is_fetch; break;
      case INTERP_rn: susp->susp.fetch = delaycv_rn_fetch; break;
      case INTERP_rs: susp->susp.fetch = delaycv_rs_fetch; break;
      default: snd_badsr(); break;
    }

    susp->terminate_cnt = UNKNOWN;
    /* handle unequal start times, if any */
    if (t0 < input->t0) sound_prepend_zeros(input, t0);
    if (t0 < feedback->t0) sound_prepend_zeros(feedback, t0);
    /* minimum start time over all inputs: */
    t0_min = min(input->t0, min(feedback->t0, t0));
    /* how many samples to toss before t0: */
    susp->susp.toss_cnt = (long) ((t0 - t0_min) * sr + 0.5);
    if (susp->susp.toss_cnt > 0) {
        susp->susp.keep_fetch = susp->susp.fetch;
        susp->susp.fetch = delaycv_toss_fetch;
    }

    /* initialize susp state */
    susp->susp.free = delaycv_free;
    susp->susp.sr = sr;
    susp->susp.t0 = t0;
    susp->susp.mark = delaycv_mark;
    susp->susp.print_tree = delaycv_print_tree;
    susp->susp.name = "delaycv";
    susp->susp.log_stop_cnt = UNKNOWN;
    susp->started = false;
    susp->susp.current = 0;
    susp->input = input;
    susp->input_cnt = 0;
    susp->input_pHaSe = 0.0;
    susp->input_pHaSe_iNcR = input->sr / sr;
    susp->input_n = 0;
    susp->output_per_input = sr / input->sr;
    susp->feedback = feedback;
    susp->feedback_cnt = 0;
    susp->feedback_pHaSe = 0.0;
    susp->feedback_pHaSe_iNcR = feedback->sr / sr;
    susp->feedback_n = 0;
    susp->output_per_feedback = sr / feedback->sr;
    return sound_create((snd_susp_type)susp, t0, sr, scale_factor);
}


sound_type snd_delaycv(sound_type input, time_type delay, sound_type feedback)
{
    sound_type input_copy = sound_copy(input);
    sound_type feedback_copy = sound_copy(feedback);
    return snd_make_delaycv(input_copy, delay, feedback_copy);
}
