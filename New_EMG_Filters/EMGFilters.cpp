/*
 * Copyright 2017, OYMotion Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 */

#include "EMGFilters.h"

// coefficients of transfer function of LPF
// coef[sampleFreqInd][order]
// updated for 2000hz
static float lpf_numerator_coef[3][3] = {{0.3913, 0.7827, 0.3913},
                                         {0.1311, 0.2622, 0.1311},
                                         {0.2483, 0.4967, 0.2483}};
static float lpf_denominator_coef[3][3] = {{1.0000, 0.3695, 0.1958},
                                           {1.0000, -0.7478, 0.2722},
                                           {1.0000, -0.1842, 0.1776}};


// coefficients of transfer function of HPF
static float hpf_numerator_coef[3][3] = {{0.8371, -1.6742, 0.8371},
                                         {0.9150, -1.8299, 0.9150},
                                         {0.9565, -1.9131, 0.9565}};
static float hpf_denominator_coef[3][3] = {{1.0000, -1.6475, 0.7009},
                                           {1.0000, -1.8227, 0.8372},
                                           {1.0000, -1.9112, 0.9150}};


// coefficients of transfer function of anti-hum filter
// coef[sampleFreqInd][order] for 50Hz
// updated ofr 2000hz
static float ahf_numerator_coef_50Hz[3][6] = {
    {0.9522, -1.5407, 0.9522, 0.8158, -0.8045, 0.0855},
    {0.5869, -1.1146, 0.5869, 1.0499, -2.0000, 1.0499},
    {0.9969, -1.9692, 0.9969, 0.9969, -1.9692, 0.9969}}; // 2000Hz coeffs

static float ahf_denominator_coef_50Hz[3][6] = {
    {1.0000, -1.5395, 0.9056, 1.0000, - 1.1187, 0.3129},
    {1.0000, -1.8844, 0.9893, 1.0000, -1.8991, 0.9892},
    {1.0000, -1.9692, 0.9937, 1.0000, -1.9692, 0.9937}}; // 2000Hz coeffs

static float ahf_output_gain_coef_50Hz[3] = {1.3422, 1.4399, 1.0000};



// coef[sampleFreqInd][order] for 60Hz
static float ahf_numerator_coef_60Hz[3][6] = {
    {0.9528, -1.3891, 0.9528, 0.8272, -0.7225, 0.0264},
    {0.5824, -1.0810, 0.5824, 1.0736, -2.0000, 1.0736},
    {0.9969, -1.9584, 0.9969, 0.9969, -1.9584, 0.9969}};
static float ahf_denominator_coef_60Hz[3][6] = {
    {1.0000, -1.3880, 0.9066, 1.0000, -0.9739, 0.2371},
    {1.0000, -1.8407, 0.9894, 1.0000, -1.8584, 0.9891},
    {1.0000, -1.9584, 0.9937, 1.0000, -1.9584, 0.9937}};
static float ahf_output_gain_coef_60Hz[3] = {1.3430, 1.4206, 1.0000};


void FILTER_2nd::init(FILTER_TYPE ftype, int sampleFreq) {
    states[0] = 0;
    states[1] = 0;
    
    // Map frequency to array index
    int idx = -1;
    if (sampleFreq == SAMPLE_FREQ_500HZ) idx = 0;
    else if (sampleFreq == SAMPLE_FREQ_1000HZ) idx = 1;
    else if (sampleFreq == SAMPLE_FREQ_2000HZ) idx = 2;

    if (idx != -1) {
        if (ftype == FILTER_TYPE_LOWPASS) {
            for (int i = 0; i < 3; i++) {
                num[i] = lpf_numerator_coef[idx][i];
                den[i] = lpf_denominator_coef[idx][i];
            }
        } else if (ftype == FILTER_TYPE_HIGHPASS) {
            for (int i = 0; i < 3; i++) {
                num[i] = hpf_numerator_coef[idx][i];
                den[i] = hpf_denominator_coef[idx][i];
            }
        }
    }
}

float FILTER_2nd::update(float input) {
    // Direct Form II (Safe for Float)
    float tmp = (input - den[1] * states[0] - den[2] * states[1]) / den[0];
    float output = num[0] * tmp + num[1] * states[0] + num[2] * states[1];
    
    // Shift states
    states[1] = states[0];
    states[0] = tmp;
    return output;
}

void FILTER_4th::init(int sampleFreq, int humFreq) {
    gain = 0;
    for (int i = 0; i < 4; i++) {
        states[i] = 0;
    }

    // Map frequency to array index
    int idx = -1;
    if (sampleFreq == SAMPLE_FREQ_500HZ) idx = 0;
    else if (sampleFreq == SAMPLE_FREQ_1000HZ) idx = 1;
    else if (sampleFreq == SAMPLE_FREQ_2000HZ) idx = 2;

    if (idx != -1) {
        if (humFreq == NOTCH_FREQ_50HZ) { 
            for (int i = 0; i < 6; i++) {
                num[i] = ahf_numerator_coef_50Hz[idx][i];
                den[i] = ahf_denominator_coef_50Hz[idx][i];
            }
            gain = ahf_output_gain_coef_50Hz[idx];
        }
        else if (humFreq == NOTCH_FREQ_60HZ) {
            for (int i = 0; i < 6; i++) {
                num[i] = ahf_numerator_coef_60Hz[idx][i];
                den[i] = ahf_denominator_coef_60Hz[idx][i];
            }
            gain = ahf_output_gain_coef_60Hz[idx];
        }
    }
}

float FILTER_4th::update(float input) {
    float output;
    float stageIn;
    float stageOut;

    // Stage 1 (First Biquad)
    stageOut  = num[0] * input + states[0];
    states[0] = (num[1] * input + states[1]) - den[1] * stageOut;
    states[1] = num[2] * input - den[2] * stageOut;

    // Stage 2 (Second Biquad)
    stageIn   = stageOut;
    stageOut  = num[3] * stageOut + states[2];
    states[2] = (num[4] * stageIn + states[3]) - den[4] * stageOut;
    states[3] = num[5] * stageIn - den[5] * stageOut;

    output = gain * stageOut;
    return output;
}

void EMGFilters::init(SAMPLE_FREQUENCY sampleFreq,
                     NOTCH_FREQUENCY  notchFreq,
                     bool             enableNotchFilter,
                     bool             enableLowpassFilter,
                     bool             enableHighpassFilter) {
    m_sampleFreq   = sampleFreq;
    m_notchFreq    = notchFreq;
    m_bypassEnabled = true;

    // Validate configuration including 2000Hz
    if (((sampleFreq == SAMPLE_FREQ_500HZ) || 
         (sampleFreq == SAMPLE_FREQ_1000HZ) || 
         (sampleFreq == SAMPLE_FREQ_2000HZ)) &&
        ((notchFreq == NOTCH_FREQ_50HZ) || (notchFreq == NOTCH_FREQ_60HZ))) {
            m_bypassEnabled = false;
    }

    // Initialize member objects
    LPF.init(FILTER_TYPE_LOWPASS, m_sampleFreq);
    HPF.init(FILTER_TYPE_HIGHPASS, m_sampleFreq);
    AHF.init(m_sampleFreq, m_notchFreq);

    m_notchFilterEnabled    = enableNotchFilter;
    m_lowpassFilterEnabled  = enableLowpassFilter;
    m_highpassFilterEnabled = enableHighpassFilter;
}

float EMGFilters::update(float inputValue) {
    float output = 0;
    if (m_bypassEnabled) {
        return inputValue;
    }

    // Signal Path: Float precision
    output = inputValue;

    // 1. Notch Filter
    if (m_notchFilterEnabled) {
        output = AHF.update(output);
    }

    // 2. Low Pass Filter
    if (m_lowpassFilterEnabled) {
        output = LPF.update(output);
    }

    // 3. High Pass Filter
    if (m_highpassFilterEnabled) {
        output = HPF.update(output);
    }

    return output;
}
