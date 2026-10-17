#include "main.h"
#include "ecg_signal.h"
#include <string.h>

ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;
DMA_HandleTypeDef hdma_usart1_tx;
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
TIM_HandleTypeDef htim3;
ECG_Diagnostics ecg_diagnostics;

static uint16_t adc_dma[ECG_PACKET_SAMPLES * 2U] __attribute__((aligned(4)));
static uint8_t frames[ECG_QUEUE_SLOTS][ECG_FRAME_BYTES];
static volatile uint8_t frame_head;
static volatile uint8_t frame_tail;
static volatile uint8_t tx_busy;
static volatile uint8_t acquisition_fault;
static volatile uint32_t last_adc_tick;
static uint8_t tx_frame[ECG_FRAME_BYTES];
static uint8_t sequence;
static uint32_t tx_started;
static ECG_Baseline baseline;
static uint32_t led_started;

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);

static void Check(HAL_StatusTypeDef status)
{
    if (status != HAL_OK) Error_Handler();
}

static void LED_Process(void)
{
    static uint8_t done;
    static GPIO_PinState previous = GPIO_PIN_RESET;
    if (done) return;
    uint32_t elapsed = HAL_GetTick() - led_started;
    GPIO_PinState value;
    if (elapsed < 10000U)
        value = ((elapsed / 250U) & 1U) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    else if (elapsed < 20000U)
        value = GPIO_PIN_SET;
    else {
        value = GPIO_PIN_RESET;
        done = 1U;
    }
    if (value != previous) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, value);
        previous = value;
    }
}

#if ECG_CONFIGURE_HM10
static void HM10_Wait(uint32_t duration)
{
    uint32_t start = HAL_GetTick();
    while ((uint32_t)(HAL_GetTick() - start) < duration) {
        uint8_t byte;
        if (HAL_UART_Receive(&huart1, &byte, 1U, 1U) == HAL_OK)
            (void)HAL_UART_Transmit(&huart2, &byte, 1U, 2U);
        LED_Process();
    }
}

static void HM10_Command(const char *command, uint32_t wait_ms)
{
    Check(HAL_UART_Transmit(&huart1, (uint8_t *)command,
                           (uint16_t)strlen(command), 100U));
    HM10_Wait(wait_ms);
}

static void HM10_Configure(void)
{
    HM10_Wait(500U);
    HM10_Command("AT\r\n", 400U);
    HM10_Command("AT+NAME=Intelliwave\r\n", 400U);
    HM10_Command("AT+PASS=123456\r\n", 400U);
    HM10_Command("AT+PIO10\r\n", 400U);
    HM10_Command("AT+RESET\r\n", 700U);
}
#endif

static void ADC_ProcessBlock(const uint16_t *raw)
{
    int16_t samples[ECG_PACKET_SAMPLES];
    for (uint32_t i = 0U; i < ECG_PACKET_SAMPLES; ++i) {
        if (raw[i] == 0U || raw[i] == 4095U)
            ++ecg_diagnostics.adc_rail_samples;
        samples[i] = ECG_Baseline_Apply(&baseline, raw[i]);
    }
    ecg_diagnostics.samples += ECG_PACKET_SAMPLES;
    ++ecg_diagnostics.packets_created;
    last_adc_tick = HAL_GetTick();
    uint8_t next = (uint8_t)((frame_head + 1U) % ECG_QUEUE_SLOTS);
    if (next == frame_tail) {
        ++ecg_diagnostics.packets_dropped;
    } else {
        ECG_EncodeFrame(frames[frame_head], sequence, samples);
        __DMB();
        frame_head = next;
    }
    ++sequence;
}

static void UART_Process(void)
{
    if (tx_busy) {
        if ((uint32_t)(HAL_GetTick() - tx_started) < ECG_UART_TIMEOUT_MS)
            return;
        Check(HAL_UART_AbortTransmit(&huart1));
        tx_busy = 0U;
        ++ecg_diagnostics.uart_timeouts;
        ++ecg_diagnostics.packets_dropped;
    }
    if (frame_tail == frame_head) return;
    __DMB();
    memcpy(tx_frame, frames[frame_tail], ECG_FRAME_BYTES);
    tx_started = HAL_GetTick();
    tx_busy = 1U;
    if (HAL_UART_Transmit_DMA(&huart1, tx_frame, ECG_FRAME_BYTES) != HAL_OK) {
        tx_busy = 0U;
        ++ecg_diagnostics.uart_start_errors;
        Check(HAL_UART_AbortTransmit(&huart1));
        return;
    }
    __DMB();
    frame_tail = (uint8_t)((frame_tail + 1U) % ECG_QUEUE_SLOTS);
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();
    MX_ADC1_Init();
    MX_TIM3_Init();

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
    HAL_Delay(100U);
    led_started = HAL_GetTick();
#if ECG_CONFIGURE_HM10
    HM10_Configure();
#endif
    ECG_Baseline_Reset(&baseline);
    Check(HAL_ADCEx_Calibration_Start(&hadc1));
    Check(HAL_ADC_Start_DMA(&hadc1, (uint32_t *)(void *)adc_dma,
                           ECG_PACKET_SAMPLES * 2U));
    last_adc_tick = HAL_GetTick();
    Check(HAL_TIM_Base_Start(&htim3));

    while (1) {
        if (acquisition_fault) Error_Handler();
        if ((uint32_t)(HAL_GetTick() - last_adc_tick) >= 100U) {
            ++ecg_diagnostics.adc_timeouts;
            Error_Handler();
        }
        UART_Process();
        LED_Process();
        __WFI();
    }
}

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clock = {0};
    RCC_PeriphCLKInitTypeDef peripheral = {0};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL = RCC_PLL_MUL9;
    Check(HAL_RCC_OscConfig(&osc));
    clock.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                      RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clock.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clock.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clock.APB1CLKDivider = RCC_HCLK_DIV2;
    clock.APB2CLKDivider = RCC_HCLK_DIV1;
    Check(HAL_RCC_ClockConfig(&clock, FLASH_LATENCY_2));
    peripheral.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    peripheral.AdcClockSelection = RCC_ADCPCLK2_DIV6;
    Check(HAL_RCCEx_PeriphCLKConfig(&peripheral));
}

static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_RESET);
    GPIO_InitTypeDef gpio = {0};
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Pin = GPIO_PIN_4;
    HAL_GPIO_Init(GPIOA, &gpio);
    gpio.Pin = GPIO_PIN_11;
    HAL_GPIO_Init(GPIOB, &gpio);
    gpio.Pin = GPIO_PIN_7;
    gpio.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(GPIOA, &gpio);
}

static void MX_DMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();
    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 1U, 0U);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 3U, 0U);
    HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);
}

static void MX_ADC1_Init(void)
{
    ADC_ChannelConfTypeDef channel = {0};
    hadc1.Instance = ADC1;
    hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T3_TRGO;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1U;
    Check(HAL_ADC_Init(&hadc1));
    channel.Channel = ADC_CHANNEL_7;
    channel.Rank = ADC_REGULAR_RANK_1;
    channel.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;
    Check(HAL_ADC_ConfigChannel(&hadc1, &channel));
}

static void MX_TIM3_Init(void)
{
    TIM_MasterConfigTypeDef master = {0};
    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 7199U;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 10000U / ECG_SAMPLE_RATE_HZ - 1U;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    Check(HAL_TIM_Base_Init(&htim3));
    master.MasterOutputTrigger = TIM_TRGO_UPDATE;
    master.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    Check(HAL_TIMEx_MasterConfigSynchronization(&htim3, &master));
}

static void UART_Init(UART_HandleTypeDef *uart, USART_TypeDef *instance, uint32_t baud)
{
    uart->Instance = instance;
    uart->Init.BaudRate = baud;
    uart->Init.WordLength = UART_WORDLENGTH_8B;
    uart->Init.StopBits = UART_STOPBITS_1;
    uart->Init.Parity = UART_PARITY_NONE;
    uart->Init.Mode = UART_MODE_TX_RX;
    uart->Init.HwFlowCtl = UART_HWCONTROL_NONE;
    uart->Init.OverSampling = UART_OVERSAMPLING_16;
    Check(HAL_UART_Init(uart));
}

static void MX_USART1_UART_Init(void)
{
    UART_Init(&huart1, USART1, ECG_HM10_BAUD);
}

static void MX_USART2_UART_Init(void)
{
    UART_Init(&huart2, USART2, ECG_DEBUG_BAUD);
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *adc)
{
    if (adc->Instance == ADC1) ADC_ProcessBlock(&adc_dma[0]);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *adc)
{
    if (adc->Instance == ADC1) ADC_ProcessBlock(&adc_dma[ECG_PACKET_SAMPLES]);
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *adc)
{
    if (adc->Instance == ADC1) {
        ++ecg_diagnostics.adc_errors;
        acquisition_fault = 1U;
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uart)
{
    if (uart->Instance == USART1) {
        ++ecg_diagnostics.packets_completed;
        tx_busy = 0U;
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
    if (uart->Instance == USART1) ++ecg_diagnostics.uart_errors;
}

void Error_Handler(void)
{
    __disable_irq();
    __HAL_RCC_TIM3_CLK_DISABLE();
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_SET);
    while (1) __WFI();
}
