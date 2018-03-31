#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "dma_driver.h"

#define MEM_DUMP_MAX_BYTES  256

static void memdump(void* buf_ptr, int byte_count) 
{
    char *p = buf_ptr;
    int offset = 0;

    if (byte_count > MEM_DUMP_MAX_BYTES)
    {
        printf("Buffer size is %d bytes. Only show the last %d bytes...\n", byte_count, MEM_DUMP_MAX_BYTES);
        offset = byte_count - MEM_DUMP_MAX_BYTES;
    }    

    for (; offset < byte_count; offset++) 
    {
        printf("%02x", p[offset]);
        if (offset % 4 == 3)  
            printf(" ");
        if (offset % 32 == 31)
            printf("\n"); 
    }
    printf("\n");
}

int main(int argc, char *argv[])
{
    if (FAILURE == dma_init())
        exit(1);
    
    for (int i = 0; i < 96; i++)
    {
        psrc[i] = i;
    }
    printf("Test values: \n");
    memdump(psrc, 96);

    if (FAILURE == dma_start(96))
        exit(1);

    if (FAILURE == dma_sync())
        exit(1);

    printf("Result:\n");
    memdump(pdest, 96);

    dma_clean_up();
    return 0;
}
