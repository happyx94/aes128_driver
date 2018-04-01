#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

#include "dma_driver.h"

#define MEM_DUMP_MAX_BYTES  256

#define AES_KEY_ADDR            0x43C10000
#define AES_KEY_REGS_MAP_LEN    4096

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

int set_key(void *pkey)
{
    u32 *pregs;
    u32 *key = pkey;

    if (mem_fd < 0)
        return FAILURE;

    pregs = mmap(NULL, AES_KEY_REGS_MAP_LEN, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, AES_KEY_ADDR);
    if (NULL == pregs)
    {
        perror("Failed to mmap the AES key registers");
        return FAILURE;
    }
    for (int i = 0; i < 4; i++)
    {
        pregs[i] = key[i];
    }
    printf("AES key has be set.\n");
    munmap(pregs, AES_KEY_REGS_MAP_LEN);

    return SUCCESS;
}

int main(int argc, char *argv[])
{
    unsigned char key[16];

    if (FAILURE == dma_init())
        exit(1);

    for (int i = 0; i < 16; i++)
    {
        key[i] = (unsigned char)0xAA;
    }

    set_key(key);
    printf("AES Key: \n");
    memdump(key, 16);

    for (int i = 0; i < 16; i++)
    {
        psrc[i] = 0;
    }

    printf("Test values: \n");
    memdump(psrc, 16);

    for (int round = 0; round < 5; round++)
    {
        printf("------- Round %d -------\n", round + 1);

        if (FAILURE == dma_start(16))
            exit(1);

        if (FAILURE == dma_sync())
            exit(1);

        printf("Result:\n");
        memdump(pdest, 16);
        printf("\n");
    }

    dma_clean_up();
    return 0;
}
