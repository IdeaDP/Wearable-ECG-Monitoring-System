#include "main.h"

static void Check(HAL_StatusTypeDef result)
{
    if (result != HAL_OK) Error_Handler();
}

void HAL_MspInit(void)
{
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
}

void HAL_ADC_MspInit(ADC_HandleTypeDef *adc)
{
    if (adc->Instance != ADC1) return;
    __HAL_RCC_ADC1_CLK_ENABLE();
    hdma_adc1.Instance = DMA1_Channel1;
    hdma_adc1.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_adc1.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_adc1.Init.MemInc = DMA_MINC_ENABLE;
    hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_adc1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_adc1.Init.Mode = DMA_CIRCULAR;
    hdma_adc1.Init.Priority = DMA_PRIORITY_HIGH;
    Check(HAL_DMA_Init(&hdma_adc1));
    __HAL_LINKDMA(adc, DMA_Handle, hdma_adc1);
}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *timer)
{
    if (timer->Instance == TIM3) __HAL_RCC_TIM3_CLK_ENABLE();
}

void HAL_UART_MspInit(UART_HandleTypeDef *uart)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Mode = GPIO_MODE_AF_PP;
    if (uart->Instance == USART1) {
        __HAL_RCC_USART1_CLK_ENABLE();
        gpio.Pin = GPIO_PIN_9;
        HAL_GPIO_Init(GPIOA, &gpio);
        gpio.Pin = GPIO_PIN_10;
        gpio.Mode = GPIO_MODE_INPUT;
        gpio.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &gpio);
        hdma_usart1_tx.Instance = DMA1_Channel4;
        hdma_usart1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
        hdma_usart1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_usart1_tx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_usart1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_usart1_tx.Init.Mode = DMA_NORMAL;
        hdma_usart1_tx.Init.Priority = DMA_PRIORITY_LOW;
        Check(HAL_DMA_Init(&hdma_usart1_tx));
        __HAL_LINKDMA(uart, hdmatx, hdma_usart1_tx);
        HAL_NVIC_SetPriority(USART1_IRQn, 3U, 0U);
        HAL_NVIC_EnableIRQ(USART1_IRQn);
    } else if (uart->Instance == USART2) {
        __HAL_RCC_USART2_CLK_ENABLE();
        gpio.Pin = GPIO_PIN_2;
        HAL_GPIO_Init(GPIOA, &gpio);
        gpio.Pin = GPIO_PIN_3;
        gpio.Mode = GPIO_MODE_INPUT;
        gpio.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &gpio);
    }
}
