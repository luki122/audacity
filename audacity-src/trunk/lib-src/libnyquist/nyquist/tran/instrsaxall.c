#include "stdio.h"
#ifndef mips
#include "stdlib.h"
#endif
#include "xlisp.h"
#include "sound.h"

#include "falloc.h"
#include "cext.h"
#include "instrsaxall.h"

void sax_all_free();


typedef struct sax_all_susp_struct {
    snd_susp_node susp;
    long terminate_cnt;
    sound_type breath_env;
    long breath_env_cnt;
    sample_block_values_type breath_env_ptr;
    sound_type freq_env;
    long freq_env_cnt;
    sample_block_values_type freq_env_ptr;
    sound_type reed_stiffness;
    long reed_stiffness_cnt;
    sample_block_values_type reed_stiffness_ptr;
    sound_type noise;
    long noise_cnt;
    sample_block_values_type noise_ptr;
    sound_type blow_pos;
    long blow_pos_cnt;
    sample_block_values_type blow_pos_ptr;
    sound_type reed_table_offset;
    long reed_table_offset_cnt;
    sample_block_values_type reed_table_offset_ptr;

    struct instr *sax;
    double frequency;
} sax_all_susp_node, *sax_all_susp_type;


	    #include "instr.h"


void sax_all_ssssss_fetch(register sax_all_susp_type susp, snd_list_type snd_list)
{
    int cnt = 0; /* how many samples computed */
    int togo;
    int n;
    sample_block_type out;
    register sample_block_values_type out_ptr;

    register sample_block_values_type out_ptr_reg;

    register struct instr * sax_reg;
    register double frequency_reg;
    register sample_type reed_table_offset_scale_reg = susp->reed_table_offset->scale;
    register sample_block_values_type reed_table_offset_ptr_reg;
    register sample_type blow_pos_scale_reg = susp->blow_pos->scale;
    register sample_block_values_type blow_pos_ptr_reg;
    register sample_type noise_scale_reg = susp->noise->scale;
    register sample_block_values_type noise_ptr_reg;
    register sample_type reed_stiffness_scale_reg = susp->reed_stiffness->scale;
    register sample_block_values_type reed_stiffness_ptr_reg;
    register sample_type freq_env_scale_reg = susp->freq_env->scale;
    register sample_block_values_type freq_env_ptr_reg;
    register sample_type breath_env_scale_reg = susp->breath_env->scale;
    register sample_block_values_type breath_env_ptr_reg;
    falloc_sample_block(out, "sax_all_ssssss_fetch");
    out_ptr = out->samples;
    snd_list->block = out;

    while (cnt < max_sample_block_len) { /* outer loop */
	/* first compute how many samples to generate in inner loop: */
	/* don't overflow the output sample block: */
	togo = max_sample_block_len - cnt;

	/* don't run past the breath_env input sample block: */
	susp_check_term_samples(breath_env, breath_env_ptr, breath_env_cnt);
	togo = min(togo, susp->breath_env_cnt);

	/* don't run past the freq_env input sample block: */
	susp_check_samples(freq_env, freq_env_ptr, freq_env_cnt);
	togo = min(togo, susp->freq_env_cnt);

	/* don't run past the reed_stiffness input sample block: */
	susp_check_samples(reed_stiffness, reed_stiffness_ptr, reed_stiffness_cnt);
	togo = min(togo, susp->reed_stiffness_cnt);

	/* don't run past the noise input sample block: */
	susp_check_samples(noise, noise_ptr, noise_cnt);
	togo = min(togo, susp->noise_cnt);

	/* don't run past the blow_pos input sample block: */
	susp_check_samples(blow_pos, blow_pos_ptr, blow_pos_cnt);
	togo = min(togo, susp->blow_pos_cnt);

	/* don't run past the reed_table_offset input sample block: */
	susp_check_samples(reed_table_offset, reed_table_offset_ptr, reed_table_offset_cnt);
	togo = min(togo, susp->reed_table_offset_cnt);

	/* don't run past terminate time */
	if (susp->terminate_cnt != UNKNOWN &&
	    susp->terminate_cnt <= susp->susp.current + cnt + togo) {
	    togo = susp->terminate_cnt - (susp->susp.current + cnt);
	    if (togo == 0) break;
	}

	n = togo;
	sax_reg = susp->sax;
	frequency_reg = susp->frequency;
	reed_table_offset_ptr_reg = susp->reed_table_offset_ptr;
	blow_pos_ptr_reg = susp->blow_pos_ptr;
	noise_ptr_reg = susp->noise_ptr;
	reed_stiffness_ptr_reg = susp->reed_stiffness_ptr;
	freq_env_ptr_reg = susp->freq_env_ptr;
	breath_env_ptr_reg = susp->breath_env_ptr;
	out_ptr_reg = out_ptr;
	if (n) do { /* the inner sample computation loop */

	    controlChange(sax_reg, 128, SAX_CONTROL_CHANGE_CONST * (breath_env_scale_reg * *breath_env_ptr_reg++));
	    controlChange(sax_reg, 2, SAX_CONTROL_CHANGE_CONST * (reed_stiffness_scale_reg * *reed_stiffness_ptr_reg++));
	    controlChange(sax_reg, 4, SAX_CONTROL_CHANGE_CONST * (noise_scale_reg * *noise_ptr_reg++));
	    controlChange(sax_reg, 11, SAX_CONTROL_CHANGE_CONST * (blow_pos_scale_reg * *blow_pos_ptr_reg++));
	    controlChange(sax_reg, 26, SAX_CONTROL_CHANGE_CONST * (reed_table_offset_scale_reg * *reed_table_offset_ptr_reg++));
	    setFrequency(sax_reg, frequency_reg + (freq_env_scale_reg * *freq_env_ptr_reg++));
	    *out_ptr_reg++ = (sample_type) tick(sax_reg);
	} while (--n); /* inner loop */

	susp->sax = sax_reg;
	/* using reed_table_offset_ptr_reg is a bad idea on RS/6000: */
	susp->reed_table_offset_ptr += togo;
	/* using blow_pos_ptr_reg is a bad idea on RS/6000: */
	susp->blow_pos_ptr += togo;
	/* using noise_ptr_reg is a bad idea on RS/6000: */
	susp->noise_ptr += togo;
	/* using reed_stiffness_ptr_reg is a bad idea on RS/6000: */
	susp->reed_stiffness_ptr += togo;
	/* using freq_env_ptr_reg is a bad idea on RS/6000: */
	susp->freq_env_ptr += togo;
	/* using breath_env_ptr_reg is a bad idea on RS/6000: */
	susp->breath_env_ptr += togo;
	out_ptr += togo;
	susp_took(breath_env_cnt, togo);
	susp_took(freq_env_cnt, togo);
	susp_took(reed_stiffness_cnt, togo);
	susp_took(noise_cnt, togo);
	susp_took(blow_pos_cnt, togo);
	susp_took(reed_table_offset_cnt, togo);
	cnt += togo;
    } /* outer loop */

    /* test for termination */
    if (togo == 0 && cnt == 0) {
	snd_list_terminate(snd_list);
    } else {
	snd_list->block_len = cnt;
	susp->susp.current += cnt;
    }
} /* sax_all_ssssss_fetch */


void sax_all_toss_fetch(susp, snd_list)
  register sax_all_susp_type susp;
  snd_list_type snd_list;
{
    long final_count = susp->susp.toss_cnt;
    time_type final_time = susp->susp.t0;
    long n;

    /* fetch samples from breath_env up to final_time for this block of zeros */
    while ((round((final_time - susp->breath_env->t0) * susp->breath_env->sr)) >=
	   susp->breath_env->current)
	susp_get_samples(breath_env, breath_env_ptr, breath_env_cnt);
    /* fetch samples from freq_env up to final_time for this block of zeros */
    while ((round((final_time - susp->freq_env->t0) * susp->freq_env->sr)) >=
	   susp->freq_env->current)
	susp_get_samples(freq_env, freq_env_ptr, freq_env_cnt);
    /* fetch samples from reed_stiffness up to final_time for this block of zeros */
    while ((round((final_time - susp->reed_stiffness->t0) * susp->reed_stiffness->sr)) >=
	   susp->reed_stiffness->current)
	susp_get_samples(reed_stiffness, reed_stiffness_ptr, reed_stiffness_cnt);
    /* fetch samples from noise up to final_time for this block of zeros */
    while ((round((final_time - susp->noise->t0) * susp->noise->sr)) >=
	   susp->noise->current)
	susp_get_samples(noise, noise_ptr, noise_cnt);
    /* fetch samples from blow_pos up to final_time for this block of zeros */
    while ((round((final_time - susp->blow_pos->t0) * susp->blow_pos->sr)) >=
	   susp->blow_pos->current)
	susp_get_samples(blow_pos, blow_pos_ptr, blow_pos_cnt);
    /* fetch samples from reed_table_offset up to final_time for this block of zeros */
    while ((round((final_time - susp->reed_table_offset->t0) * susp->reed_table_offset->sr)) >=
	   susp->reed_table_offset->current)
	susp_get_samples(reed_table_offset, reed_table_offset_ptr, reed_table_offset_cnt);
    /* convert to normal processing when we hit final_count */
    /* we want each signal positioned at final_time */
    n = round((final_time - susp->breath_env->t0) * susp->breath_env->sr -
         (susp->breath_env->current - susp->breath_env_cnt));
    susp->breath_env_ptr += n;
    susp_took(breath_env_cnt, n);
    n = round((final_time - susp->freq_env->t0) * susp->freq_env->sr -
         (susp->freq_env->current - susp->freq_env_cnt));
    susp->freq_env_ptr += n;
    susp_took(freq_env_cnt, n);
    n = round((final_time - susp->reed_stiffness->t0) * susp->reed_stiffness->sr -
         (susp->reed_stiffness->current - susp->reed_stiffness_cnt));
    susp->reed_stiffness_ptr += n;
    susp_took(reed_stiffness_cnt, n);
    n = round((final_time - susp->noise->t0) * susp->noise->sr -
         (susp->noise->current - susp->noise_cnt));
    susp->noise_ptr += n;
    susp_took(noise_cnt, n);
    n = round((final_time - susp->blow_pos->t0) * susp->blow_pos->sr -
         (susp->blow_pos->current - susp->blow_pos_cnt));
    susp->blow_pos_ptr += n;
    susp_took(blow_pos_cnt, n);
    n = round((final_time - susp->reed_table_offset->t0) * susp->reed_table_offset->sr -
         (susp->reed_table_offset->current - susp->reed_table_offset_cnt));
    susp->reed_table_offset_ptr += n;
    susp_took(reed_table_offset_cnt, n);
    susp->susp.fetch = susp->susp.keep_fetch;
    (*(susp->susp.fetch))(susp, snd_list);
}


void sax_all_mark(sax_all_susp_type susp)
{
    sound_xlmark(susp->breath_env);
    sound_xlmark(susp->freq_env);
    sound_xlmark(susp->reed_stiffness);
    sound_xlmark(susp->noise);
    sound_xlmark(susp->blow_pos);
    sound_xlmark(susp->reed_table_offset);
}


void sax_all_free(sax_all_susp_type susp)
{

	    deleteInstrument(susp->sax);
    sound_unref(susp->breath_env);
    sound_unref(susp->freq_env);
    sound_unref(susp->reed_stiffness);
    sound_unref(susp->noise);
    sound_unref(susp->blow_pos);
    sound_unref(susp->reed_table_offset);
    ffree_generic(susp, sizeof(sax_all_susp_node), "sax_all_free");
}


void sax_all_print_tree(sax_all_susp_type susp, int n)
{
    indent(n);
    stdputstr("breath_env:");
    sound_print_tree_1(susp->breath_env, n);

    indent(n);
    stdputstr("freq_env:");
    sound_print_tree_1(susp->freq_env, n);

    indent(n);
    stdputstr("reed_stiffness:");
    sound_print_tree_1(susp->reed_stiffness, n);

    indent(n);
    stdputstr("noise:");
    sound_print_tree_1(susp->noise, n);

    indent(n);
    stdputstr("blow_pos:");
    sound_print_tree_1(susp->blow_pos, n);

    indent(n);
    stdputstr("reed_table_offset:");
    sound_print_tree_1(susp->reed_table_offset, n);
}


sound_type snd_make_sax_all(double freq, sound_type breath_env, sound_type freq_env, double vibrato_freq, double vibrato_gain, sound_type reed_stiffness, sound_type noise, sound_type blow_pos, sound_type reed_table_offset, rate_type sr)
{
    register sax_all_susp_type susp;
    /* sr specified as input parameter */
    time_type t0 = breath_env->t0;
    int interp_desc = 0;
    sample_type scale_factor = 1.0F;
    time_type t0_min = t0;
    falloc_generic(susp, sax_all_susp_node, "snd_make_sax_all");
    susp->sax = initInstrument(SAXOFONY, round(sr));
    noteOn(susp->sax, freq, 1.0);
    controlChange(susp->sax, 29, SAX_CONTROL_CHANGE_CONST * vibrato_freq);
    controlChange(susp->sax, 1, SAX_CONTROL_CHANGE_CONST * vibrato_gain);;
    susp->frequency = freq;
    susp->susp.fetch = sax_all_ssssss_fetch;
    susp->terminate_cnt = UNKNOWN;
    /* handle unequal start times, if any */
    if (t0 < breath_env->t0) sound_prepend_zeros(breath_env, t0);
    if (t0 < freq_env->t0) sound_prepend_zeros(freq_env, t0);
    if (t0 < reed_stiffness->t0) sound_prepend_zeros(reed_stiffness, t0);
    if (t0 < noise->t0) sound_prepend_zeros(noise, t0);
    if (t0 < blow_pos->t0) sound_prepend_zeros(blow_pos, t0);
    if (t0 < reed_table_offset->t0) sound_prepend_zeros(reed_table_offset, t0);
    /* minimum start time over all inputs: */
    t0_min = min(breath_env->t0, min(freq_env->t0, min(reed_stiffness->t0, min(noise->t0, min(blow_pos->t0, min(reed_table_offset->t0, t0))))));
    /* how many samples to toss before t0: */
    susp->susp.toss_cnt = (long) ((t0 - t0_min) * sr + 0.5);
    if (susp->susp.toss_cnt > 0) {
	susp->susp.keep_fetch = susp->susp.fetch;
	susp->susp.fetch = sax_all_toss_fetch;
    }

    /* initialize susp state */
    susp->susp.free = sax_all_free;
    susp->susp.sr = sr;
    susp->susp.t0 = t0;
    susp->susp.mark = sax_all_mark;
    susp->susp.print_tree = sax_all_print_tree;
    susp->susp.name = "sax_all";
    susp->susp.log_stop_cnt = UNKNOWN;
    susp->susp.current = 0;
    susp->breath_env = breath_env;
    susp->breath_env_cnt = 0;
    susp->freq_env = freq_env;
    susp->freq_env_cnt = 0;
    susp->reed_stiffness = reed_stiffness;
    susp->reed_stiffness_cnt = 0;
    susp->noise = noise;
    susp->noise_cnt = 0;
    susp->blow_pos = blow_pos;
    susp->blow_pos_cnt = 0;
    susp->reed_table_offset = reed_table_offset;
    susp->reed_table_offset_cnt = 0;
    return sound_create((snd_susp_type)susp, t0, sr, scale_factor);
}


sound_type snd_sax_all(double freq, sound_type breath_env, sound_type freq_env, double vibrato_freq, double vibrato_gain, sound_type reed_stiffness, sound_type noise, sound_type blow_pos, sound_type reed_table_offset, rate_type sr)
{
    sound_type breath_env_copy = sound_copy(breath_env);
    sound_type freq_env_copy = sound_copy(freq_env);
    sound_type reed_stiffness_copy = sound_copy(reed_stiffness);
    sound_type noise_copy = sound_copy(noise);
    sound_type blow_pos_copy = sound_copy(blow_pos);
    sound_type reed_table_offset_copy = sound_copy(reed_table_offset);
    return snd_make_sax_all(freq, breath_env_copy, freq_env_copy, vibrato_freq, vibrato_gain, reed_stiffness_copy, noise_copy, blow_pos_copy, reed_table_offset_copy, sr);
}
