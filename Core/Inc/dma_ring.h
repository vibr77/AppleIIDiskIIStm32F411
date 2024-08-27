#ifndef RING_H
#define RING_H

#include "main.h"

#define RING_SUCCESS 0
#define RING_FAIL 1
#define RING_BUF_SIZE 1024
struct dma_ring_buf {
  uint8_t buf[RING_BUF_SIZE];
  uint16_t buf_size;
  uint16_t w_ptr, r_ptr;
  uint16_t overwrite_cnt;
};

void dma_ring_init(struct dma_ring_buf *ring);
int dma_ring_getc(struct dma_ring_buf *ring, uint8_t *c);
int dma_ring_putc(struct dma_ring_buf *ring, uint8_t c);
int dma_ring_available(struct dma_ring_buf *ring);

int dma_ring_available_linear(struct dma_ring_buf *ring);
uint16_t dma_ring_get_w_ptr(struct dma_ring_buf *ring);
uint16_t dma_ring_get_r_ptr(struct dma_ring_buf *ring);

void dma_ring_forward_r_ptr(struct dma_ring_buf *ring, int len);

void dma_ring_set_w_ptr(struct dma_ring_buf *ring, uint16_t w_ptr);

void dma_ring_debug(struct dma_ring_buf *ring);

#endif