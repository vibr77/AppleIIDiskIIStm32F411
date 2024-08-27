//
// Created by naoki on 19/03/09.
//

#include "main.h"
#include "dma_printf.h"
#include "dma_ring.h"
#include <stdbool.h>

struct dma_printf_info dmi;

void dma_printf_init(UART_HandleTypeDef *printf_huart){
    dmi.huart = printf_huart;
    dma_ring_init(&dmi.tx_ring);
    dmi.sending = false;
    dmi.previous_send_len = 0;
}

void dma_printf_putc(uint8_t c){
    dma_ring_putc(&dmi.tx_ring, c);
    if(!dmi.sending){
        uint16_t len = (uint16_t)dma_ring_available_linear(&dmi.tx_ring);
        dmi.previous_send_len = len;
        dmi.sending = true;
        HAL_UART_Transmit_DMA(dmi.huart, dmi.tx_ring.buf+dma_ring_get_r_ptr(&dmi.tx_ring), len);
    }
}

void dma_printf_send_it(UART_HandleTypeDef *printf_huart) {
    if(dmi.huart != printf_huart) return;

    dma_ring_forward_r_ptr(&dmi.tx_ring, dmi.previous_send_len);
    uint16_t len = (uint16_t)dma_ring_available_linear(&dmi.tx_ring);
    if (len > 0 ) {
        dmi.previous_send_len = len;
        dmi.sending = true;
        HAL_UART_Transmit_DMA(dmi.huart, dmi.tx_ring.buf+dma_ring_get_r_ptr(&dmi.tx_ring), len);
    }else{
        dmi.sending = false;
    }
}