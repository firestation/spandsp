/*
 * SpanDSP - a series of DSP components for telephony
 *
 * dtmf_tests.c - Test the DTMF detector against the spec., whatever the spec.
 *                may be :)
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001 Steve Underwood
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
 * $Id: dtmf_tests.c,v 1.4 2004/03/19 19:12:46 steveu Exp $
 */

/*
 * These tests include conversion to and from A-law. I assume the
 * distortion this produces is comparable to u-law, so it should be
 * a fair test.
 *
 * These tests mirror those on the CM7291 test tape from Mitel.
 * Many of these tests are highly questionable, but they are a
 * well accepted industry standard.
 *
 * However standard these tests might be, Mitel appears to have stopped
 * selling copies of their tape.
 *
 * For the talk-off test the Bellcore tapes may be used. However, they are
 * copyright material, so the test data files produced from the Bellcore
 * tapes cannot be distributed as a part of this package.
 *
 */

/*! \page DTMF_rx_tests_page DTMF receiver tests
\section DTMF_rx_tests_page_sec_1 What does it do

The DTMF detection test suite performs similar tests to the Mitel test tape,
traditionally used for testing DTMF receivers. Mitel seem to have discontinued
this product, but all it not lost. 

The first side of the Mitel tape consists of a number of tone and tone+noise
based tests. The test suite synthesizes equivalent test data. Being digitally
generated, this data is rather more predictable than the test data on the nasty
old stretchy cassette tapes which Mitel sold. 

The second side of the Mitel tape contains fragments of real speech from real
phone calls captured from the North American telephone network. These are
considered troublesome for DTMF detectors. A good detector is expected to
achieve a reasonably low number of false detections on this data. Fresh clean
copies of this seem to be unobtainable. However, Bellcore produce a much more
aggressive set of three cassette tapes. All six side (about 30 minutes each) are
filled with much tougher fragments of real speech from the North American
telephone network. If you can do well in this test, nobody cares about your
results against the Mitel test tape. 

A fresh set of tapes was purchased for these tests, and digitised, producing 6
wave files of 16 bit signed PCM data, sampled at 8kHz. They were transcribed
using a speed adjustable cassette player. The test tone at the start of the
tapes is pretty accurate, and the new tapes should not have had much opportunity
to stretch. It is believed these transcriptions are about as good as the source
material permits. 

PLEASE NOTE

These transcriptions may be freely used by anyone who has a legitimate copy of
the original tapes. However, if you don't have a legitimate copy of those tapes,
you also have no right to use this data. The original tapes are the copyright
material of BellCore, and they charge over US$200 for a set. I doubt they sell
enough copies to consider this much of a business. However, it is their data,
and it is their right to do as they wish with it. Currently I see no indication
they wish to give it away for free. 
*/

#define _ISOC9X_SOURCE  1
#define _ISOC99_SOURCE  1

#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <audiofile.h>
#include <tiffio.h>

#include "spandsp.h"

/* Basic DTMF specs:
 *
 * Minimum tone on = 40ms
 * Minimum tone off = 50ms
 * Maximum digit rate = 10 per second
 * Normal twist <= 8dB accepted
 * Reverse twist <= 4dB accepted
 * S/N >= 15dB will detect OK
 * Attenuation <= 26dB will detect OK
 * Frequency tolerance +- 1.5% will detect, +-3.5% will reject
 */

#define DTMF_DURATION               380
#define DTMF_PAUSE                  400
#define DTMF_CYCLE                  (DTMF_DURATION + DTMF_PAUSE)

#define BELLCORE_DIR	"/home/steveu/bellcore/"

char *bellcore_files[] =
{
    BELLCORE_DIR "tr-tsy-00763-1.wav",
    BELLCORE_DIR "tr-tsy-00763-2.wav",
    BELLCORE_DIR "tr-tsy-00763-3.wav",
    BELLCORE_DIR "tr-tsy-00763-4.wav",
    BELLCORE_DIR "tr-tsy-00763-5.wav",
    BELLCORE_DIR "tr-tsy-00763-6.wav",
    ""
};

static tone_gen_descriptor_t my_dtmf_digit_tones[16];

float dtmf_row[] =
{
     697.0,  770.0,  852.0,  941.0
};
float dtmf_col[] =
{
    1209.0, 1336.0, 1477.0, 1633.0
};

char dtmf_positions[] = "123A" "456B" "789C" "*0#D";

int callback_ok;
int callback_roll;

static void my_dtmf_gen_init(float low_fudge,
                             int low_level,
                             float high_fudge,
                             int high_level,
                             int duration,
                             int gap)
{
    int row;
    int col;

    for (row = 0;  row < 4;  row++)
    {
    	for (col = 0;  col < 4;  col++)
	{
    	    make_tone_gen_descriptor (&my_dtmf_digit_tones[row*4 + col],
	    	    	    	      dtmf_row[row]*(1.0 + low_fudge),
				      low_level,
				      dtmf_col[col]*(1.0 + high_fudge),
				      high_level,
				      duration,
				      gap,
				      0,
				      0,
				      FALSE);
	}
    }
}
/*- End of function --------------------------------------------------------*/

static int my_dtmf_generate(int16_t amp[], char *digits)
{
    int len;
    char *cp;
    tone_gen_state_t tone;

    len = 0;
    while (*digits)
    {
        cp = strchr(dtmf_positions, *digits);
        if (cp)
        {
            tone_gen_init (&tone, &my_dtmf_digit_tones[cp - dtmf_positions]);
            len += tone_gen(&tone, amp + len, 1000);
        }
        digits++;
    }
    return len;
}
/*- End of function --------------------------------------------------------*/

static void alaw_munge(int16_t amp[], int len)
{
    int i;
    uint8_t alaw;
    
    for (i = 0;  i < len;  i++)
    {
        alaw = linear_to_alaw (amp[i]);
        amp[i] = alaw_to_linear (alaw);
    }
}
/*- End of function --------------------------------------------------------*/

#define ALL_POSSIBLE_DIGITS     "123A456B789C*0#D"

static void digit_delivery(void *data, char *digits, int len)
{
    int i;
    int seg;
    char *s = ALL_POSSIBLE_DIGITS;
    char *t;

    if (data == (void *) 0x12345678)
    {
        callback_ok = TRUE;
        t = s + callback_roll;
        seg = 16 - callback_roll;
        for (i = 0;  i < len;  i += seg, seg = 16)
        {
            if (i + seg > len)
                seg = len - i;
            if (memcmp(digits + i, t, seg))
            {
                callback_ok = FALSE;
                printf("Fail at %d %d\n", i, seg);
                break;
            }
            t = s;
            callback_roll = seg;
        }
    }
    else
    {
        callback_ok = FALSE;
    }
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int duration;
    int i;
    int j;
    int len;
    int sample;
    char *s;
    char digit[2];
    char buf[128 + 1];
    int actual;
    int nplus;
    int nminus;
    float rrb;
    float rcfo;
    int16_t amp[1000000];
    time_t now;
    dtmf_rx_state_t dtmf_state;
    awgn_state_t noise_source;
    int hits;
    int hit_types[256];
    AFfilehandle inhandle;
    int frames;
    float x;

    time(&now);
    dtmf_rx_init(&dtmf_state, NULL, NULL);

    /* Test 1: Mitel's test 1 isn't really a test. Its a calibration step,
       which has no meaning here. */
    printf ("Test 1: Calibration\n");
    printf ("    Passed\n");

    /* Test 2: Decode check
       This is a sanity check, that all digits are reliably detected
       under ideal conditions.  Each possible digit repeated 10 times,
       with 50ms bursts. The level of each tone is about 6dB down from clip */
    printf ("Test 2: Decode check\n");
    my_dtmf_gen_init(0.0, -3, 0.0, -3, 50, 50);
    s = ALL_POSSIBLE_DIGITS;
    digit[1] = '\0';
    while (*s)
    {
        digit[0] = *s++;
        for (i = 0;  i < 10;  i++)
        {
            len = my_dtmf_generate (amp, digit);
            alaw_munge(amp, len);
            dtmf_rx(&dtmf_state, amp, len);

            actual = dtmf_get(&dtmf_state, buf, 128);

            if (actual != 1  ||  buf[0] != digit[0])
            {
                printf ("    Sent     '%s'\n", digit);
                printf ("    Received '%s'\n", buf);
                printf ("    Failed\n");
                exit (2);
            }
        }
    }
    printf ("    Passed\n");

    /* Test 3: Recognition bandwidth and channel centre frequency check.
       Use only the diagonal pairs of tones (digits 1, 5, 9 and D). Each
       tone pair requires four test to complete the check, making 16
       sections overall. Each section contains 40 pulses of
       50ms duration, with an amplitude of -20dB from clip per
       frequency.

       Four sections covering the tests for one tone (1 digit) are:
       a. H frequency at 0% deviation from center, L frequency at +0.1%.
          L frequency is then increments in +01.% steps up to +4%. The
          number of tone bursts is noted and designated N+.
       b. H frequency at 0% deviation, L frequency at -0.1%. L frequency
          is then incremental in -0.1% steps, up to -4%. The number of
          tone bursts is noted and designated N-.
       c. The test in (a) is repeated with the L frequency at 0% and the
          H frequency varied up to +4%.
       d. The test in (b) is repeated with the L frequency and 0% and the
          H frequency varied to -4%.

       Receiver Recognition Bandwidth (RRB) is calculated as follows:
            RRB% = (N+ + N-)/10
       Receiver Center Frequency Offset (RCFO) is calculated as follows:
            RCFO% = X + (N+ - N-)/20
    	
       Note that this test doesn't test what it says it is testing at all,
       and the results are quite inaccurate, if not a downright lie! However,
       it follows the Mitel procedure, so how can it be bad? :)
    */
    printf ("Test 3: Recognition bandwidth and channel centre frequency check\n");
    s = "159D";
    digit[1] = '\0';
    while (*s)
    {
        digit[0] = *s++;
        for (nplus = 0, i = 1;  i <= 60;  i++)
        {
            my_dtmf_gen_init((float) i/1000.0, -17, 0.0, -17, 50, 50);
            len = my_dtmf_generate (amp, digit);
            alaw_munge(amp, len);
            dtmf_rx(&dtmf_state, amp, len);
            nplus += dtmf_get(&dtmf_state, buf, 128);
        }
        for (nminus = 0, i = -1;  i >= -60;  i--)
        {
            my_dtmf_gen_init((float) i/1000.0, -17, 0.0, -17, 50, 50);
            len = my_dtmf_generate (amp, digit);
            alaw_munge(amp, len);
            dtmf_rx(&dtmf_state, amp, len);
            nminus += dtmf_get(&dtmf_state, buf, 128);
        }
        rrb = (float) (nplus + nminus)/10.0;
        rcfo = (float) (nplus - nminus)/10.0;
        printf ("    %c (low)  rrb = %5.2f%%, rcfo = %5.2f%%, max -ve = %5.2f, max +ve = %5.2f\n",
		digit[0],
		rrb,
		rcfo,
		(float) nminus/10.0,
		(float) nplus/10.0);
        if (rrb < 3.0 + rcfo  ||  rrb >= 15.0 + rcfo)
        {
            printf ("    Failed\n");
            exit (2);
        }

        for (nplus = 0, i = 1;  i <= 60;  i++)
        {
            my_dtmf_gen_init(0.0, -17, (float) i/1000.0, -17, 50, 50);
            len = my_dtmf_generate (amp, digit);
            alaw_munge(amp, len);
            dtmf_rx(&dtmf_state, amp, len);
            nplus += dtmf_get(&dtmf_state, buf, 128);
        }
        for (nminus = 0, i = -1;  i >= -60;  i--)
        {
            my_dtmf_gen_init(0.0, -17, (float) i/1000.0, -17, 50, 50);
            len = my_dtmf_generate (amp, digit);
            alaw_munge(amp, len);
            dtmf_rx(&dtmf_state, amp, len);
            nminus += dtmf_get(&dtmf_state, buf, 128);
        }
        rrb = (float) (nplus + nminus)/10.0;
        rcfo = (float) (nplus - nminus)/10.0;
        printf ("    %c (high) rrb = %5.2f%%, rcfo = %5.2f%%, max -ve = %5.2f, max +ve = %5.2f\n",
		digit[0],
		rrb,
		rcfo,
		(float) nminus/10.0,
		(float) nplus/10.0);
        if (rrb < 3.0 + rcfo  ||  rrb >= 15.0 + rcfo)
        {
            printf ("    Failed\n");
            exit (2);
        }
    }
    printf ("    Passed\n");

    /* Test 4: Acceptable amplitude ratio (twist).
       Use only the diagonal pairs of tones (digits 1, 5, 9 and D). 
       There are eight sections to the test. Each section contains 200
       pulses with a 50ms duration for each pulse. Initially the amplitude
       of both tones is 6dB down from clip. The two sections to test one
       tone pair are:

       a. Standard Twist: H tone amplitude is maintained at -6dB from clip,
          L tone amplitude is attenuated gradually until the amplitude ratio
          L/H is -20dB. Note the number of responses from the receiver.
       b. Reverse Twist: L tone amplitude is maintained at -6dB from clip,
          H tone amplitude is attenuated gradually until the amplitude ratio
          is 20dB. Note the number of responses from the receiver.

       All tone bursts are of 50ms duration.

       The Acceptable Amplitude Ratio in dB is equal to the number of
       responses registered in (a) or (b), divided by 10.
       
       TODO: This is supposed to work in 1/10dB steps, but here I used 1dB
             steps, as the current tone generator has its amplitude set in
             1dB steps.
    */
    printf ("Test 4: Acceptable amplitude ratio (twist)\n");
    s = "159D";
    digit[1] = '\0';
    while (*s)
    {
        digit[0] = *s++;
        for (nplus = 0, i = -30;  i >= -230;  i--)
        {
            my_dtmf_gen_init(0.0, -3, 0.0, i/10, 50, 50);

            len = my_dtmf_generate (amp, digit);
            alaw_munge(amp, len);
            dtmf_rx(&dtmf_state, amp, len);
            nplus += dtmf_get(&dtmf_state, buf, 128);
        }
        printf ("    %c normal twist  = %.2fdB\n", digit[0], (float) nplus/10.0);
        if (nplus < 80)
        {
            printf ("    Failed\n");
            exit (2);
        }
        for (nminus = 0, i = -30;  i >= -230;  i--)
        {
            my_dtmf_gen_init(0.0, i/10, 0.0, -3, 50, 50);

            len = my_dtmf_generate (amp, digit);
            alaw_munge(amp, len);
            dtmf_rx(&dtmf_state, amp, len);
            nminus += dtmf_get(&dtmf_state, buf, 128);
        }
        printf ("    %c reverse twist = %.2fdB\n", digit[0], (float) nminus/10.0);
        if (nminus < 40)
        {
            printf ("    Failed\n");
            exit (2);
        }
    }
    printf ("    Passed\n");

    /* Test 5: Dynamic range
       This test utilizes tone pair L1 H1 (digit 1). Thirty-five tone pair
       pulses are transmitted, with both frequencies stating at -6dB from
       clip. The amplitude of each is gradually attenuated by -35dB at a
       rate of 1dB per pulse. The Dynamic Range in dB is equal to the
       number of responses from the receiver during the test.
       
       Well not really, but that is the Mitel test. Lets sweep a bit further,
       and see what the real range is */
    printf ("Test 5: Dynamic range\n");
    for (nplus = 0, i = +3;  i >= -50;  i--)
    {
        my_dtmf_gen_init(0.0, i, 0.0, i, 50, 50);

        len = my_dtmf_generate (amp, "1");
        alaw_munge(amp, len);
        dtmf_rx(&dtmf_state, amp, len);
        nplus += dtmf_get(&dtmf_state, buf, 128);
    }
    printf ("    Dynamic range = %ddB\n", nplus);
    printf ("    Passed\n");

    /* Test 6: Guard time
       This test utilizes tone pair L1 H1 (digit 1). Four hundred pulses
       are transmitted at an amplitude of -6dB from clip per frequency.
       Pulse duration starts at 49ms and is gradually reduced to 10ms.
       Guard time in ms is equal to (500 - number of responses)/10.
       
       That is the Mitel test, and we will follow it. Its totally bogus,
       though. Just what the heck is a pass or fail here? */

    printf ("Test 6: Guard time\n");
    for (nplus = 0, i = 490;  i >= 100;  i--)
    {
        my_dtmf_gen_init(0.0, -3, 0.0, -3, i/10, 50);

        len = my_dtmf_generate (amp, "1");
        alaw_munge(amp, len);
        dtmf_rx(&dtmf_state, amp, len);
        nplus += dtmf_get(&dtmf_state, buf, 128);
    }
    printf ("    Guard time = %dms\n", (500 - nplus)/10);
    printf ("    Passed\n");

    /* Test 7: Acceptable signal to noise ratio
       This test utilizes tone pair L1 H1, transmitted on a noise background.
       The test consists of three sections in which the tone pair is
       transmitted 1000 times at an amplitude -6dB from clip per frequency,
       but with a different white noise level for each section. The first
       level is -24dBV, the second -18dBV and the third -12dBV.. The
       acceptable signal to noise ratio is the lowest ratio of signal
       to noise in the test where the receiver responds to all 1000 pulses.
       
       Well, that is the Mitel test, but it doesn't tell you what the
       decoder can really do. Lets do a more comprehensive test */

    printf ("Test 7: Acceptable signal to noise ratio\n");
    my_dtmf_gen_init(0.0, -3, 0.0, -3, 50, 50);

    for (j = -13;  j > -50;  j--)
    {
        awgn_init (&noise_source, 1234567, j);
        for (i = 0;  i < 1000;  i++)
        {
            len = my_dtmf_generate (amp, "1");

            // TODO: Clip
            for (sample = 0;  sample < len;  sample++)
                amp[sample] = saturate (amp[sample] + awgn (&noise_source));
            
            alaw_munge(amp, len);
            dtmf_rx(&dtmf_state, amp, len);

            if (dtmf_get(&dtmf_state, buf, 128) != 1)
                break;
        }
        if (i == 1000)
            break;
    }
    printf ("    Acceptable S/N ratio is %ddB\n", -3 - j);
    if (-3 - j > 26)
    {
        printf ("    Failed\n");
        exit (2);
    }
    printf ("    Passed\n");

    /* The remainder of the Mitel tape is the talk-off test */
    /* Here we use the Bellcore test tapes (much tougher), in six
       wave files - 1 from each side of the original 3 cassette tapes */
    /* Bellcore say you should get no more than 470 false detections with
       a good receiver. Dialogic claim 20. Of course, we can do better than
       that, eh? */
    printf("Test 8: Talk-off test\n");
    memset(hit_types, '\0', sizeof(hit_types));
    for (j = 0;  bellcore_files[j][0];  j++)
    {
        inhandle = afOpenFile(bellcore_files[j], "r", 0);
    	if (inhandle == AF_NULL_FILEHANDLE)
    	{
    	    printf("    Cannot open speech file '%s'\n", bellcore_files[j]);
	    exit(2);
    	}
        x = afGetFrameSize(inhandle, AF_DEFAULT_TRACK, 1);
    	if (x != 2.0)
	{
    	    printf("    Unexpected frame size in speech file '%s'\n", bellcore_files[j]);
	    exit(2);
    	}
    	hits = 0;
        while ((frames = afReadFrames(inhandle, AF_DEFAULT_TRACK, amp, 8000)))
    	{
            dtmf_rx(&dtmf_state, amp, frames);
            len = dtmf_get(&dtmf_state, buf, 128);
            if (len > 0)
	    {
		for (i = 0;  i < len;  i++)
		    hit_types[(int) buf[i]]++;
	    	hits += len;
	    }
    	}
        if (afCloseFile(inhandle) != 0)
    	{
    	    printf("    Cannot close speech file '%s'\n", bellcore_files[j]);
	    exit(2);
    	}
	printf("    File %d gave %d false hits.\n", j + 1, hits);
    }
    for (i = 0, j = 0;  i < 256;  i++)
    {
    	if (hit_types[i])
	{
	    printf("    Digit %c had %d false hits\n", i, hit_types[i]);
	    j += hit_types[i];
	}
    }
    printf ("    %d hits in total\n", j);
    if (j > 470)
    {
        printf("    Failed\n");
     	exit (2);
    }
    printf ("    Passed\n");

    /* Test the callback mode for delivering detected digits */

    printf("Test: Callback digit delivery mode.\n");
    callback_ok = FALSE;
    callback_roll = 0;
    dtmf_rx_init(&dtmf_state, digit_delivery, (void *) 0x12345678);
    my_dtmf_gen_init(0.0, -10, 0.0, -10, 50, 50);
    for (i = 1;  i < 10;  i++)
    {
        len = 0;
        for (j = 0;  j < i;  j++)
            len += my_dtmf_generate(amp + len, ALL_POSSIBLE_DIGITS);
        dtmf_rx(&dtmf_state, amp, len);
        if (!callback_ok)
            break;
    }
    if (!callback_ok)
    {
        printf("    Failed\n");
     	exit (2);
    }
    printf ("    Passed\n");

    duration = time (NULL) - now;
    printf ("Tests completed in  %ds\n", duration);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
