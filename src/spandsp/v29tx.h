/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v29tx.h - ITU V.29 modem transmit part
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
 * $Id: v29tx.h,v 1.2 2004/03/19 19:12:46 steveu Exp $
 */

/*! \file */

#if !defined(_V29TX_H_)
#define _V29TX_H_

#include "fsk.h"

/*! \page V29tx_page The V.29 transmitter
\section V29tx_page_sec_1 What does it do
The V.29 transmitter implements the transmit side of a V.29 modem. This can
operate at data rates of 9600, 7200 and 4800 bits/s. The audio output is a
stream of 16 bit samples, at 8000 samples/second. The transmit and receive side
of V.29 modems operate independantly. V.29 is mostly used for FAX transmission,
where it provides the standard 9600 and 7200 bits/s rates (the 4800 bits/s mode
is not used for FAX). 

\section V29tx_page_sec_2 Theory of Operation
V.29 uses QAM modulation. The standard method of producing a QAM modulated
signal is to use a sampling rate which is a multiple of the baud rate. The raw
signal is then a series of complex pulses, each an integer number of samples
long. These can be shaped, using a suitable complex filter, and multiplied by a
complex carrier signal to produce the final QAM signal for transmission. 

The sampling rate for our transmitter is defined by the channel - 8000 per
second. This is not a multiple of the baud rate (i.e. 2400 baud). The baud
interval is actually 10/3 sample periods. Generating at the lowest common
multiple of the baud rate and channel sample rate (i.e. 24000 samples/second),
and then decimating to 8000 samples/second, would give good results. However,
this would require considerable computation. A shortcut is to use slightly
shaped pulses, instead of simple square ones. We can achieve the effect of pulse
transitions at the 1/2 and 2/3 sample points by adjusting the first sample of
each new pulse. The adjustment is simple. We need the effect of being 60 degrees
or 120 degrees through a sine wave cycle at the Shannon rate at the sample
point. This simply means we need to step by 0.25 or 0.75 of the actual step size
on the first sample of those pulses which should start at the 1/3 or 2/3 sample
positions. The logic and computation needed for this is much less than the
computation needed for oversampling at 24000 samples/second. 

The pulse shaping filter is only vaguely defined by the V.29 spec. Some of the
other ITU modem specs. fully define the filter, typically specifying a root
raised cosine filter, with 50% excess bandwidth. This is a pity, since it
increases the variability of the received signal. However, the receiver's
adaptive equalizer will largely compensate for these differences. The current
design uses a root raised cosine filter with 50% excess bandwidth. 

The carrier is generated using the DDS method. Using 2 second order resonators,
started in quadrature, might be more efficient, as it would have less impact on
the processor cache than a table lookup approach. However, the DDS approach
suits the receiver better, so then same signal generator is also used for the
transmitter. 
*/

#define V29TX_FILTER_STEPS      27

/*!
    V.29 modem transmit side descriptor. This defines the working state for a
    single instance of a V.29 modem transmitter.
*/
typedef struct
{
    /*! \brief The bit rate of the modem. Valid values are 4800, 7200 and 9600. */
    int bit_rate;
    /*! \brief The callback function used to get the next bit to be transmitted. */
    get_bit_func_t get_bit;
    /*! \brief A user specified opaque pointer passed to the callback function. */
    void *user_data;

    /*! \brief The route raised cosine (RRC) pulse shaping filter buffer. */
    complex_t rrc_filter[2*V29TX_FILTER_STEPS];
    /*! \brief Current offset into the RRC pulse shaping filter buffer. */
    int rrc_filter_step;
    /*! \brief The current constellation position. */
    complex_t current_point;

    /*! \brief The register for the data scrambler. */
    unsigned int scramble_reg;
    /*! \brief The register for the training scrambler. */
    uint8_t training_scramble_reg;
    /*! \brief TRUE if transmitting the training sequence. FALSE if transmitting user data. */
    int in_training;
    /*! \brief A counter used to track progress through sending the training sequence. */
    int training_step;
    /*! \brief An offset value into the table of training parameters, used to match the
               training pattern to the bit rate. */
    int training_offset;

    /*! \brief The current phase of the carrier (i.e. the DDS parameter). */
    uint32_t carrier_phase;
    /*! \brief The update rate for the phase of the carrier (i.e. the DDS increment). */
    int32_t carrier_phase_rate;
    /*! \brief The current fractional phase of the baud timing. */
    int baud_phase;
    /*! \brief The code number for the current position in the constellation. */
    int constellation_state;
} v29_tx_state_t;

/*! Initialise a V.29 modem transmit context. This must be called before the first
    use of the context, to initialise its contents.
    \brief Initialise a V.29 modem transmit context.
    \param s The modem context.
    \param rate The bit rate of the modem. Valid values are 4800, 7200 and 9600.
    \param get_bit The callback routine used to get the data to be transmitted.
    \param user_data An opaque pointer. */
void v29_tx_init(v29_tx_state_t *s, int rate, get_bit_func_t get_bit, void *user_data);

/*! Reinitialise an existing V.29 modem transmit context, so it may be reused.
    \brief Reinitialise an existing V.29 modem transmit context.
    \param s The modem context.
    \param rate The bit rate of the modem. Valid values are 4800, 7200 and 9600. */
void v29_tx_restart(v29_tx_state_t *s, int rate);

/*! Generate a block of V.29 modem audio samples.
    \brief Generate a block of V.29 modem audio samples.
    \param s The modem context.
    \param amp The audio sample buffer.
    \param len The number of samples to be generated.
    \return The number of samples actually generated.
*/
int v29_tx(v29_tx_state_t *s, int16_t *amp, int len);

#endif
/*- End of file ------------------------------------------------------------*/
