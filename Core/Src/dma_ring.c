#include "dma_ring.h"
#include "main.h"

void dma_ring_init(struct dma_ring_buf *ring){
  ring->buf_size = RING_BUF_SIZE;
  ring->w_ptr = 0;
  ring->r_ptr = 0;
  ring->overwrite_cnt = 0;
}

int dma_ring_getc(struct dma_ring_buf *ring, uint8_t *c){
  if(ring->r_ptr == ring->w_ptr) return RING_FAIL;
  uint16_t next_ptr = ring->r_ptr+1;
  if(next_ptr >= ring->buf_size) next_ptr = 0;

  *c = ring->buf[ring->r_ptr];
  ring->r_ptr = next_ptr;
  return RING_SUCCESS;
}

int dma_ring_putc(struct dma_ring_buf *ring, uint8_t c){
  uint16_t next_ptr = ring->w_ptr+1;
  if(next_ptr >= ring->buf_size) next_ptr = 0;

  if(next_ptr == ring->r_ptr){
    ring->overwrite_cnt++;
    return RING_FAIL;
  }
  ring->buf[ring->w_ptr] = c;
  ring->w_ptr = next_ptr;
  return RING_SUCCESS;
}

int dma_ring_available(struct dma_ring_buf *ring){
  if(ring->w_ptr >= ring->r_ptr){
    return ring->w_ptr - ring->r_ptr;
  }else{
    return ring->buf_size + ring->w_ptr - ring->r_ptr;
  }
}
int dma_ring_available_linear(struct dma_ring_buf *ring){
  if(ring->w_ptr >= ring->r_ptr){
    return ring->w_ptr - ring->r_ptr;
  }else{
    return ring->buf_size - ring->r_ptr;
  }
}

uint16_t dma_ring_get_w_ptr(struct dma_ring_buf *ring){
  return ring->w_ptr;
}

uint16_t dma_ring_get_r_ptr(struct dma_ring_buf *ring){
  return ring->r_ptr;
}

void dma_ring_forward_r_ptr(struct dma_ring_buf *ring, int len){
  while(len > 0){
    if(ring->r_ptr+1 >= ring->buf_size){
      ring->r_ptr = 0;
    }else{
      ring->r_ptr += 1;
    }
    len--;
  }
}

void dma_ring_set_w_ptr(struct dma_ring_buf *ring, uint16_t w_ptr){
    ring->w_ptr = w_ptr;
}
void dma_ring_debug(struct dma_ring_buf *ring){
  printf("\n====Ring Debug information====\n");
    printf("Buffer Size: %d\n", ring->buf_size);
    printf("Write Pointer: %d\n", ring->w_ptr);
    printf("Read Pointer: %d\n", ring->r_ptr);
    printf("OverWrite Count: %d\n\n", ring->overwrite_cnt);
}