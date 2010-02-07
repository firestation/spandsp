/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v29rx.h - ITU V.29 modem receive part
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
 * $Id: v29rx.h,v 1.5 2004/03/28 12:46:42 steveu Exp $
 */

/*! \file */

#if !defined(_V29RX_H_)
#define _V29RX_H_

#include "fsk.h"

/*! \page V29rx_page The V.29 receiver
\section V29rx_page_sec_1 What does it do
The V.29 receiver implements the receive side of a V.29 modem. This can operate
at data rates of 9600, 7200 and 4800 bits/s. The audio input is a stream of 16
bit samples, at 8000 samples/second. The transmit and receive side of V.29
modems operate independantly. V.29 is mostly used for FAX transmission, where it
provides the standard 9600 and 7200 bits/s rates (the 4800 bits/s mode is not
used for FAX). 

\section V29rx_page_sec_2 Theory of operation
V.29 use QAM modulation. It specifies a training sequence at the start of
transmission, which makes the design of a V.29 receiver relatively
straightforward. The first stage of the training sequence consists of a 128
symbols, alternating between two constellation positions. The receiver monitors
the signal power, to sense the possible presence of a valid carrier. When the
alternating signal begins, the power rising above a minimum threshold (-26dBm0)
causes the main receiver computation to begin. The initial measured power is
used to quickly set the gain of the receiver. After this initial setting, the
front end gain is locked, and the adaptive equalizer tracks any subsequent
signal level variation. The signal is multiplied by a complex carrier, generated
by a DDS, at 8000 samples/second. It is then fed at 24000 samples/second (i.e.
signal, zero, zero, signal, zero, zero, ...) to a root raised cosine pulse
shaping filter. This interpolates the samples, and pulse shapes at the same
time. Every fifth sample is taken from the output of the filter, and fed to an
adaptive equalizer. This means the adaptive equalizer receives samples at 4800
samples/second, so it is a T/2 equalizer. The Gardner algorithm is used to tune
the sampling, so the samples fed to the equalizer are close to the mid point and
edges of each symbol. Initially the algorithm is very lightly damped, to ensure
the symbol alignment pulls in quickly. Because the sampling rate will not be
precisely the same as the transmitter's (the spec. says the symbol timing should
be within 0.01%), the receiver constantly evaluates and corrects this sampling
throughout its operation. During the symbol timing maintainence phase, the
algorithm uses a heavily damped Gardner plus integrate and dump approach to
updates. This heavy damping achieves several things - the Gardner algorithm is
statistically based, so the statistics must be smoothed; a number of samples
must be fed to the equalizer buffer before the equalizer output actually
responds to a step change in the sampling; we need to prevent rapid fluctuations
in the sampling position, due to the optimal position being close to a boundary.

The carrier is specified as 1700Hz +- 1Hz at the transmitter, and 1700 +-7Hz at
the receiver. The receive carrier would only be this inaccurate if the link
includes FDM sections. These are being phased out, but the design must still
allow for the worst case. Using an initial 1700Hz signal for demodulation gives
a worst case rotation rate for the constellation of about one degree per symbol.
Once the Gardner algorithm has been given time to lock to the symbol timing of
the initial alternating pattern, the phase of the demodulated signal is recorded
on two successive symbols - once for each of the constellation positions. The
receiver then tracks the symbol alternations, until a large phase jump occurs.
This signifies the start of the next phase of the training sequence. At this
point the total phase shift between the original recorded symbol phase, and the
symbol phase just before the phase jump occurred is used to provide a coarse
estimation of the rotation rate of the constellation, and it current absolute
angle of rotation. These are used to update the current carrier phase and phase
update rate in the carrier DDS. The working data already in the pulse shaping
filter and equalizer buffers is given a similar step rotation to pull it all
into line. From this point on, a heavily damped integrate and dump approach,
based on the angular difference between each received constellation position and
its expected position, is sufficient to track the carrier, and maintain phase
alignment. A fast rough approximator for the arc-tangent function is adequate
for the estimation of the angular error. 

The next phase of the training sequence is a scrambled sequence of two
particular symbols. We train the T/2 adaptive equalizer using this sequence. The
scrambling makes the signal sufficiently diverse to ensure the equalizer
converges to the proper generalised solution. At the end of this sequence, the
equalizer should be sufficiently well adapted that is can correctly resolve the
full QAM constellation. However, the equalizer continues to adapt throughout
operation of the modem, fine tuning on the more complex data patterns of the
full QAM constellation. 

In the last phase of the training sequence, the modem enters normal data
operation, with a short defined period of all ones as data. As in most high
speed modems, data in a V.29 modem passes through a scrambler, to whiten the
spectrum of the signal. The transmitter should initialise its data scrambler,
and pass the ones through it. At the end of the ones, real data begins to pass
through the scrambler, and the transmit modem is in normal operation. The
receiver tests that ones are really received, in order to verify the modem
trained correctly. If all is well, the data following the ones is fed to the
application, and the receive modem is up and running. Unfortunately, some
transmit side of some real V.29 modems fail to initialise their scrambler before
sending the ones. This means the first 23 received bits (the length of the
scrambler register) cannot be trusted for the test. The receive modem,
therefore, only tests that bits starting at bit 24 are really ones. 
*/

#define V29_EQUALIZER_LEN   7  /* this much to the left and this much to the right */
#define V29_EQUALIZER_MASK  15 /* one less than a power of 2 >= (2*V29_EQUALIZER_LEN + 1) */

#define V29RX_FILTER_STEPS  27

typedef void (qam_report_handler_t)(void *user_data, complex_t *constel, int symbol);

/*!
    V.29 modem receive side descriptor. This defines the working state for a
    single instance of a V.29 modem receiver.
*/
typedef struct
{
    /*! \brief The bit rate of the modem. Valid values are 4800, 7200 and 9600. */
    int bit_rate;
    /*! \brief The callback function used to put each bit received. */
    put_bit_func_t put_bit;
    /*! \brief A user specified opaque pointer passed to the callback function. */
    void *user_data;
    /*! \brief A callback function which may be enabled to report every symbol's
               constellation position. */
    qam_report_handler_t *qam_report;
    /*! \brief A user specified opaque pointer passed to the qam_report callback
               function. */
    void *qam_user_data;

    /*! \brief The route raised cosine (RRC) pulse shaping filter buffer. */
    complex_t rrc_filter[2*V29RX_FILTER_STEPS];
    /*! \brief Current offset into the RRC pulse shaping filter buffer. */
    int rrc_filter_step;

    /*! \brief The register for the data scrambler. */
    unsigned int scramble_reg;
    /*! \brief The register for the training scrambler. */
    uint8_t training_scramble_reg;
    int in_training;
    int training_cd;
    int training_count;
    float training_error;
    int carrier_present;

    /*! \brief The current phase of the carrier (i.e. the DDS parameter). */
    uint32_t carrier_phase;
    /*! \brief The update rate for the phase of the carrier (i.e. the DDS increment). */
    int32_t carrier_phase_rate;

    power_meter_t power;
    int32_t carrier_on_power;
    int32_t carrier_off_power;
    float agc_scaling;
    
    int constellation_state;

    float eq_delta;
    /*! \brief The adaptive equalizer coefficients */
    complex_t eq_coeff[2*V29_EQUALIZER_LEN + 1];
    complex_t eq_buf[V29_EQUALIZER_MASK + 1];
    /*! \brief Current offset into equalizer buffer. */
    int eq_step;
    int eq_put_step;

    /*! \brief Integration variable for damping the Gardner algorithm tests. */
    int gardner_integrate;
    int gardner_step;
    /*! \brief The current fractional phase of the baud timing. */
    int baud_phase;
    /*! \brief The integrated lead or lag of the carrier phase against its expected
               value. This is used in fine carrier tracking. */
    int lead_lag;
    /*! \brief The number of bauds over which lead_lag has been gathered. */
    int lead_lag_time;

    /*! \brief A starting phase angle for the coarse carrier aquisition step. */
    int32_t start_angle_a;
    /*! \brief A starting phase angle for the coarse carrier aquisition step. */
    int32_t start_angle_b;
    /*! \brief History list of phase angles for the coarse carrier aquisition step. */
    int32_t angles[16];
} v29_rx_state_t;

extern const complex_t v29_constellation[16];

/*! Initialise a V.29 modem receive context.
    \brief Initialise a V.29 modem receive context.
    \param s The modem context.
    \param rate The bit rate of the modem. Valid values are 4800, 7200 and 9600.
    \param put_bit The callback routine used to put the received data.
    \param user_data An opaque pointer. */
void v29_rx_init(v29_rx_state_t *s, int bit_rate, put_bit_func_t put_bit, void *user_data);

/*! Reinitialise an existing V.29 modem receive context.
    \brief Reinitialise an existing V.29 modem receive context.
    \param s The modem context.
    \param rate The bit rate of the modem. Valid values are 4800, 7200 and 9600. */
void v29_rx_restart(v29_rx_state_t *s, int bit_rate);

/*! Process a block of received V.29 modem audio samples.
    \brief Process a block of received V.29 modem audio samples.
    \param s The modem context.
    \param amp The audio sample buffer.
    \param len The number of samples in the buffer.
*/
void v29_rx(v29_rx_state_t *s, const int16_t *amp, int len);

/*! Get a snapshot of the current equalizer coefficients.
    \brief Get a snapshot of the current equalizer coefficients.
    \param coeffs The vector of complex coefficients.
    \return The number of coefficients in the vector. */
int v29_rx_equalizer_state(v29_rx_state_t *s, complex_t **coeffs);

void v29_rx_set_qam_report_handler(v29_rx_state_t *s, qam_report_handler_t *handler, void *user_data);
#endif
/*- End of file ------------------------------------------------------------*/
