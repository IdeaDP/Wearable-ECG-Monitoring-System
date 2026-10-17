#ifndef ECG_SIGNAL_H
#define ECG_SIGNAL_H

#include <stdint.h>
#include "ecg_config.h"

typedef struct {
    int32_t baseline_q12;
    int32_t remainder;
    uint8_t initialized;
} ECG_Baseline;

void ECG_Baseline_Reset(ECG_Baseline *state);
int16_t ECG_Baseline_Apply(ECG_Baseline *state, uint16_t adc12);
void ECG_EncodeFrame(uint8_t frame[ECG_FRAME_BYTES], uint8_t sequence,
                     const int16_t samples[ECG_PACKET_SAMPLES]);

#endif
