#ifndef u32
  #define u32 unsigned int
#endif

#ifndef u8
  #define u8 unsigned char
#endif

#define SUCCESS 0
#define FAILURE -1

#define RSV_BUF_LEN         (1024 * 1024)
#define psrc		    ((char *)pbuf)
#define pdest               (((char *)pbuf)+RSV_BUF_LEN/2)
extern void *pbuf;



extern int dma_sync();

extern void dma_clean_up();

extern int dma_reset();

extern int dma_start(u32 len);

extern int dma_init();



