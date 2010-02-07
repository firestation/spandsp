/*
 * SpanDSP - a series of DSP components for telephony
 *
 * makecss.c - Create the composite source signal (CSS) for G.168 testing.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: makecss.c,v 1.3 2004/03/16 13:44:48 steveu Exp $
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <audiofile.h>
#include <tiffio.h>

#include <fftw.h>

#define GEN_CONST
#include <math.h>

#include "spandsp.h"
#include "spandsp/g168models.h"

#if !defined(NULL)
#define NULL (void *) 0
#endif

#define FAST_SAMPLE_RATE    44100.0

static double scaling(double f, double start, double end, double start_gain, double end_gain)
{
    double scale;

    scale = start_gain + (f - start)*(end_gain - start_gain)/(end - start);
    return scale;
}

int main(int argc, char *argv[])
{
    fftw_complex in[8192];
    fftw_complex out[8192];
    fftw_plan p;
    int16_t voiced_sound[8192];
    int16_t noise_sound[8192];
    int16_t silence_sound[8192];
    int i;
    int j;
    int outframes;
    int voiced_length;
    float f;
    double peak;
    double ms;
    double scale;
    AFfilehandle filehandle;
    AFfilesetup filesetup;
    awgn_state_t noise_source;

    filesetup = afNewFileSetup();
    if (filesetup == AF_NULL_FILESETUP)
    {
        fprintf(stderr, "    Failed to create file setup\n");
        exit(2);
    }
    afInitSampleFormat(filesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
    afInitRate(filesetup, AF_DEFAULT_TRACK, FAST_SAMPLE_RATE);
    afInitFileFormat(filesetup, AF_FILE_WAVE);
    afInitChannels(filesetup, AF_DEFAULT_TRACK, 1);
    filehandle = afOpenFile("sound_c1.wav", "w", filesetup);
    if (filehandle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Failed to open result file\n");
        exit(2);
    }

    for (i = 0;  i < sizeof(css_c1)/sizeof(css_c1[0]);  i++)
        voiced_sound[i] = css_c1[i];
    voiced_length = i;

    ms = 0;
    for (i = 0;  i < voiced_length;  i++)
    {
        if (abs(voiced_sound[i]) > peak)
            peak = abs(voiced_sound[i]);
        ms += voiced_sound[i]*voiced_sound[i];
    }
    ms = 20.0*log10(sqrt(ms/voiced_length)/32767.0) + 3.14;
    printf("Voiced level = %.2fdB\n", ms);

    p = fftw_create_plan(8192, FFTW_BACKWARD, FFTW_ESTIMATE);
    for (i = 0;  i < 8192;  i++)
    {
        in[i].re = 0.0;
        in[i].im = 0.0;
    }
    for (i = 1;  i <= 3715;  i++)
    {
        f = FAST_SAMPLE_RATE*i/8192.0;

#if 1
        if (f < 50.0)
            scale = -60.0;
        else if (f < 100.0)
            scale = scaling(f, 50.0, 100.0, -25.8, -12.8);
        else if (f < 200.0)
            scale = scaling(f, 100.0, 200.0, -12.8, 17.4);
        else if (f < 215.0)
            scale = scaling(f, 200.0, 215.0, 17.4, 17.8);
        else if (f < 500.0)
            scale = scaling(f, 215.0, 500.0, 17.8, 12.2);
        else if (f < 1000.0)
            scale = scaling(f, 500.0, 1000.0, 12.2, 7.2);
        else if (f < 2850.0)
            scale = scaling(f, 1000.0, 2850.0, 7.2, 0.0);
        else if (f < 3600.0)
            scale = scaling(f, 2850.0, 3600.0, 0.0, -2.0);
        else if (f < 3660.0)
            scale = scaling(f, 3600.0, 3660.0, -2.0, -20.0);
        else if (f < 3680.0)
            scale = scaling(f, 3600.0, 3680.0, -20.0, -30.0);
        else
            scale = -60.0;
#else
        scale = 0.0;
#endif
        in[i].re = 0.0;
        in[i].im = (rand() & 0x1)  ?  1.0  :  -1.0;
        in[i].im *= pow(10.0, scale/20.0);
        in[i].im *= 35.0; //305360;
        //printf("%10d %15.5f %15.5f\n", i, in[i].re, in[i].im);
        in[8192 - i].re = 0.0;
        in[8192 - i].im = -in[i].im;
    }
    
    fftw_one(p, in, out);
    for (i = 0;  i < 8192;  i++)
    {
        //printf("%10d %15.5f %15.5f\n", i, out[i].re, out[i].im);
        noise_sound[i] = out[i].re;
    }

    peak = 0;
    ms = 0;
    for (i = 0;  i < 8192;  i++)
    {
        if (abs(noise_sound[i]) > peak)
            peak = abs(noise_sound[i]);
        ms += noise_sound[i]*noise_sound[i];
    }
    printf("Noise level = %.2fdB\n", 20.0*log10(sqrt(ms/8192.0)/32767.0) + 3.14);
    printf("Crest factor = %.2fdB\n", 20.0*log10(peak/sqrt(ms/8192.0)));
    
    for (i = 0;  i < 8192;  i++)
        silence_sound[i] = 0.0;

    for (j = 0;  j < 16;  j++)
    {
        outframes = afWriteFrames(filehandle,
                                  AF_DEFAULT_TRACK,
                                  voiced_sound,
                                  voiced_length);
    }
    printf("%d samples of voice\n", 16*voiced_length);
    outframes = afWriteFrames(filehandle,
                              AF_DEFAULT_TRACK,
                              noise_sound,
                              8192);
    outframes = afWriteFrames(filehandle,
                              AF_DEFAULT_TRACK,
                              noise_sound,
                              8820 - 8192);
    printf("%d samples of noise\n", 8820);
    outframes = afWriteFrames(filehandle,
                              AF_DEFAULT_TRACK,
                              silence_sound,
                              4471);
    printf("%d samples of silence\n", 4471);

    for (j = 0;  j < voiced_length;  j++)
        voiced_sound[j] = -voiced_sound[j];
    for (j = 0;  j < 8192;  j++)
        noise_sound[j] = -noise_sound[j];

    for (j = 0;  j < 16;  j++)
    {
        outframes = afWriteFrames(filehandle,
                                  AF_DEFAULT_TRACK,
                                  voiced_sound,
                                  voiced_length);
    }
    printf("%d samples of voice\n", 16*voiced_length);
    outframes = afWriteFrames(filehandle,
                              AF_DEFAULT_TRACK,
                              noise_sound,
                              8192);
    outframes = afWriteFrames(filehandle,
                              AF_DEFAULT_TRACK,
                              noise_sound,
                              8820 - 8192);
    printf("%d samples of noise\n", 8820);
    outframes = afWriteFrames(filehandle,
                              AF_DEFAULT_TRACK,
                              silence_sound,
                              4471);
    printf("%d samples of silence\n", 4471);

    if (afCloseFile(filehandle) != 0)
    {
        fprintf(stderr, "    Cannot close speech file '%s'\n", "sound_c1.wav");
        exit(2);
    }

    afInitSampleFormat(filesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
    afInitRate(filesetup, AF_DEFAULT_TRACK, FAST_SAMPLE_RATE);
    afInitFileFormat(filesetup, AF_FILE_WAVE);
    afInitChannels(filesetup, AF_DEFAULT_TRACK, 1);
    filehandle = afOpenFile("sound_c3.wav", "w", filesetup);
    if (filehandle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Failed to open result file\n");
        exit(2);
    }

    for (i = 0;  i < sizeof(css_c3)/sizeof(css_c3[0]);  i++)
        voiced_sound[i] = css_c3[i];
    voiced_length = i;

    ms = 0;
    for (i = 0;  i < voiced_length;  i++)
    {
        if (abs(voiced_sound[i]) > peak)
            peak = abs(voiced_sound[i]);
        ms += voiced_sound[i]*voiced_sound[i];
    }
    ms = 20.0*log10(sqrt(ms/voiced_length)/32767.0) + 3.14;
    printf("Voiced level = %.2fdB\n", ms);

    awgn_init(&noise_source, 7162534, (int) (ms + 0.5));
    for (i = 0;  i < 8192;  i++)
        noise_sound[i] = awgn(&noise_source);
    peak = 0;
    ms = 0;
    for (i = 0;  i < 8192;  i++)
    {
        if (abs(noise_sound[i]) > peak)
            peak = abs(noise_sound[i]);
        ms += noise_sound[i]*noise_sound[i];
    }
    printf("Noise level = %.2fdB\n", 20.0*log10(sqrt(ms/8192.0)/32767.0) + 3.14);
    printf("Crest factor = %.2fdB\n", 20.0*log10(peak/sqrt(ms/8192.0)));

    for (j = 0;  j < 14;  j++)
    {
        outframes = afWriteFrames(filehandle,
                                  AF_DEFAULT_TRACK,
                                  voiced_sound,
                                  voiced_length);
    }
    printf("%d samples of voice\n", 14*i);
    outframes = afWriteFrames(filehandle,
                              AF_DEFAULT_TRACK,
                              noise_sound,
                              8192);
    outframes = afWriteFrames(filehandle,
                              AF_DEFAULT_TRACK,
                              noise_sound,
                              8820 - 8192);
    printf("%d samples of noise\n", 8820);
    outframes = afWriteFrames(filehandle,
                              AF_DEFAULT_TRACK,
                              silence_sound,
                              5614);
    printf("%d samples of silence\n", 5614);

    for (j = 0;  j < voiced_length;  j++)
        voiced_sound[j] = -voiced_sound[j];
    for (j = 0;  j < 8192;  j++)
        noise_sound[j] = -noise_sound[j];

    for (j = 0;  j < 14;  j++)
    {
        outframes = afWriteFrames(filehandle,
                                  AF_DEFAULT_TRACK,
                                  voiced_sound,
                                  voiced_length);
    }
    printf("%d samples of voice\n", 14*i);
    outframes = afWriteFrames(filehandle,
                              AF_DEFAULT_TRACK,
                              noise_sound,
                              8192);
    outframes = afWriteFrames(filehandle,
                              AF_DEFAULT_TRACK,
                              noise_sound,
                              8820 - 8192);
    printf("%d samples of noise\n", 8820);
    outframes = afWriteFrames(filehandle,
                              AF_DEFAULT_TRACK,
                              silence_sound,
                              5614);
    printf("%d samples of silence\n", 5614);

    if (afCloseFile(filehandle) != 0)
    {
        fprintf(stderr, "    Cannot close speech file '%s'\n", "sound_c3.wav");
        exit(2);
    }

    fftw_destroy_plan(p);
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
