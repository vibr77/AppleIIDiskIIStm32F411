//
// Created by naoki on 19/03/09.
//

#include "main.h"
#include "dma_ring.h"
#include "dma_scanf.h"

struct dma_scanf_info dsi;

void dma_scanf_init(UART_HandleTypeDef *scanf_huart){
    dsi.huart = scanf_huart;
    dma_ring_init(&dsi.rx_ring);
    HAL_UART_Receive_DMA(dsi.huart, dsi.rx_ring.buf, dsi.rx_ring.buf_size);
}

int dma_scanf_getc_blocking(){
    while(dma_ring_available(&dsi.rx_ring) == 0){
        dma_ring_set_w_ptr(&dsi.rx_ring, (uint16_t)((dsi.rx_ring.buf_size - dsi.huart->hdmarx->Instance->NDTR)&0xFFFF));
    }
    char c;
    dma_ring_getc(&dsi.rx_ring, &c);
    return c;
}