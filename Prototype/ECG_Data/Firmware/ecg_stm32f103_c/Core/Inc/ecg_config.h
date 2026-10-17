#ifndef ECG_CONFIG_H
#define ECG_CONFIG_H

#define ECG_SAMPLE_RATE_HZ       250U
#define ECG_PACKET_SAMPLES       5U
#define ECG_FRAME_BYTES          14U
#define ECG_QUEUE_SLOTS          8U
#ifndef ECG_OUTPUT_ADC_BITS
#define ECG_OUTPUT_ADC_BITS      10U
#endif
#ifndef ECG_CONFIGURE_HM10
#define ECG_CONFIGURE_HM10       1
#endif
#define ECG_HM10_BAUD            9600U
#define ECG_DEBUG_BAUD           115200U
#define ECG_UART_TIMEOUT_MS      100U

#if ECG_OUTPUT_ADC_BITS != 10U && ECG_OUTPUT_ADC_BITS != 12U
#error "ECG_OUTPUT_ADC_BITS must be 10 or 12"
#endif
#if ECG_SAMPLE_RATE_HZ == 0U || ECG_SAMPLE_RATE_HZ > 10000U
#error "Invalid ECG sampling rate"
#elif (10000U % ECG_SAMPLE_RATE_HZ) != 0U
#error "ECG_SAMPLE_RATE_HZ must divide the 10 kHz timer counter clock"
#endif

_Static_assert(ECG_FRAME_BYTES == 4U + 2U * ECG_PACKET_SAMPLES,
               "Frame size must match the sample count");
_Static_assert(ECG_QUEUE_SLOTS >= 2U && ECG_QUEUE_SLOTS <= 255U,
               "Frame queue size must fit its indices");

#endif
