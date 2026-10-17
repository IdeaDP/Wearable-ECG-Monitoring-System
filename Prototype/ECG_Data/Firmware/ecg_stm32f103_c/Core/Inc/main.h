#ifndef MAIN_H
#define MAIN_H

#include "stm32f1xx_hal.h"
#include "ecg_config.h"

extern ADC_HandleTypeDef hadc1;
extern DMA_HandleTypeDef hdma_adc1;
extern DMA_HandleTypeDef hdma_usart1_tx;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern TIM_HandleTypeDef htim3;

typedef struct {
    volatile uint32_t samples;
    volatile uint32_t packets_created;
    volatile uint32_t packets_dropped;
    volatile uint32_t packets_completed;
    volatile uint32_t adc_errors;
    volatile uint32_t uart_start_errors;
    volatile uint32_t uart_errors;
    volatile uint32_t uart_timeouts;
    volatile uint32_t adc_timeouts;
    volatile uint32_t adc_rail_samples;
} ECG_Diagnostics;

extern ECG_Diagnostics ecg_diagnostics;
void Error_Handler(void);

#endif
