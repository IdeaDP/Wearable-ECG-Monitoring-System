#include "ecg_signal.h"

void ECG_Baseline_Reset(ECG_Baseline *state)
{
    state->baseline_q12 = 0;
    state->remainder = 0;
    state->initialized = 0U;
}

int16_t ECG_Baseline_Apply(ECG_Baseline *state, uint16_t adc12)
{
    uint32_t raw = ((uint32_t)adc12 & 0xFFFU) >> (12U - ECG_OUTPUT_ADC_BITS);
    int32_t target = (int32_t)raw * 4096;
    if (!state->initialized) {
        state->baseline_q12 = target;
        state->initialized = 1U;
    } else {
        /* alpha = 0.0063, with fractional error carried into the next sample. */
        int32_t delta = (target - state->baseline_q12) * 63 + state->remainder;
        int32_t step = delta / 10000;
        state->remainder = delta - step * 10000;
        state->baseline_q12 += step;
    }
    return (int16_t)((int32_t)raw - state->baseline_q12 / 4096);
}

void ECG_EncodeFrame(uint8_t frame[ECG_FRAME_BYTES], uint8_t sequence,
                     const int16_t samples[ECG_PACKET_SAMPLES])
{
    frame[0] = 0xAAU;
    frame[1] = 0x55U;
    frame[2] = sequence;
    uint8_t checksum = sequence;
    for (uint32_t i = 0U; i < ECG_PACKET_SAMPLES; ++i) {
        uint16_t value = (uint16_t)samples[i];
        uint8_t lo = (uint8_t)value;
        uint8_t hi = (uint8_t)(value >> 8);
        frame[3U + i * 2U] = lo;
        frame[4U + i * 2U] = hi;
        checksum = (uint8_t)(checksum + lo + hi);
    }
    frame[ECG_FRAME_BYTES - 1U] = checksum;
}
