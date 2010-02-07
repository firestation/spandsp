/*
 * SpanDSP - a series of DSP components for telephony
 *
 * super_tone_tx.c - Flexible telephony supervisory tone generation.
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
 * $Id: super_tone_tx.c,v 1.5 2005/08/31 19:27:52 steveu Exp $
 */

/*! \file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include <inttypes.h>
#include <tgmath.h>

#include "spandsp/telephony.h"
#include "spandsp/complex.h"
#include "spandsp/dds.h"
#include "spandsp/super_tone_tx.h"

super_tone_tx_step_t *super_tone_tx_make_step(super_tone_tx_step_t *s,
                                              float f1,
                                              float l1,
                                              float f2,
                                              float l2,
                                              int length,
                                              int cycles)
{
    float gain;

    if (s == NULL)
    {
        s = (super_tone_tx_step_t *) malloc(sizeof(super_tone_tx_step_t));
        if (s == NULL)
            return NULL;
    }
    if (f1 >= 1.0)
    {    
        s->phase_rate[0] = dds_phase_stepf(f1);
        s->gain[0] = dds_scalingf(l1);
    }
    else
    {
        s->phase_rate[0] = 0;
        s->gain[0] = 0;
    }
    if (f2 >= 1.0)
    {
        s->phase_rate[1] = dds_phase_stepf(f2);
        s->gain[1] = dds_scalingf(l2);
    }
    else
    {
        s->phase_rate[1] = 0;
        s->gain[1] = 0;
    }
    s->tone = (f1 > 0.0);
    s->length = length*8;
    s->cycles = cycles;
    s->next = NULL;
    s->nest = NULL;
    return  s;
}
/*- End of function --------------------------------------------------------*/

void super_tone_tx_free(super_tone_tx_step_t *s)
{
    super_tone_tx_step_t *t;

    while (s)
    {
        /* Follow nesting... */
        if (s->nest)
            super_tone_tx_free(s->nest);
        t = s;
        s = s->next;
        free(t);
    }
}
/*- End of function --------------------------------------------------------*/

void super_tone_tx_init(super_tone_tx_state_t *s, super_tone_tx_step_t *tree)
{
    s->level = 0;
    s->levels[0] = tree;
    s->cycles[0] = tree->cycles;

    s->current_position = 0;
}
/*- End of function --------------------------------------------------------*/

int super_tone_tx(super_tone_tx_state_t *s, int16_t amp[], int max_samples)
{
    int samples;
    int limit;
    int len;
    float xamp;
    super_tone_tx_step_t *tree;

    if (s->level < 0  ||  s->level > 3)
        return  0;
    samples = 0;
    tree = s->levels[s->level];
    while (tree  &&  samples < max_samples)
    {
        if (tree->tone)
        {
            /* A period of tone. A length of zero means infinite
               length. */
            if (s->current_position == 0)
            {
                /* New step - prepare the tone generator */
                s->phase_rate[0] = tree->phase_rate[0];
                s->gain[0] = tree->gain[0];
                s->phase_rate[1] = tree->phase_rate[1];
                s->gain[1] = tree->gain[1];
            }
            len = tree->length - s->current_position;
            if (tree->length == 0)
            {
                len = max_samples - samples;
                /* We just need to make current position non-zero */
                s->current_position = 1;
            }
            else if (len > max_samples - samples)
            {
                len = max_samples - samples;
                s->current_position += len;
            }
            else
            {
                s->current_position = 0;
            }
            for (limit = len + samples;  samples < limit;  samples++)
            {
                xamp = 0.0;
                if (s->phase_rate[0])
                    xamp += dds_modf(&(s->phase[0]), s->phase_rate[0], s->gain[0], 0);
                if (s->phase_rate[1])
                    xamp += dds_modf(&(s->phase[1]), s->phase_rate[1], s->gain[1], 0);
                amp[samples] = xamp;
            }
            if (s->current_position)
                return samples;
        }
        else if (tree->length)
        {
            /* A period of silence. The length must always
               be explicitly stated. A length of zero does
               not give infinite silence. */
            len = tree->length - s->current_position;
            if (len > max_samples - samples)
            {
                len = max_samples - samples;
                s->current_position += len;
            }
            else
            {
                s->current_position = 0;
            }
            memset(amp + samples, 0, sizeof(uint16_t)*len);
            samples += len;
            if (s->current_position)
                return samples;
        }
        /* Nesting has priority... */
        if (tree->nest)
        {
            tree = tree->nest;
            s->levels[++s->level] = tree;
            s->cycles[s->level] = tree->cycles;
        }
        else
        {
            /* ...Next comes repeating, and finally moving forward a step. */
            /* When repeating, note that zero cycles really means endless cycles. */
            while (tree->cycles  &&  --s->cycles[s->level] <= 0)
            {
                tree = tree->next;
                if (tree)
                {
                    /* A fresh new step. */
                    s->levels[s->level] = tree;
                    s->cycles[s->level] = tree->cycles;
                    break;
                }
                /* If we are nested we need to pop, otherwise this is the end. */
                if (s->level <= 0)
                {
                    /* Mark the tone as completed */
                    s->levels[0] = NULL;
                    break;
                }
                tree = s->levels[--s->level];
            }
        }
        
    }
    return  samples;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
