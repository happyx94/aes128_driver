#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

#include "dma_driver.h"


#define TEST_KEY_HH    0x00010203
#define TEST_KEY_HL    0x04050607
#define TEST_KEY_LH    0x08090A0B
#define TEST_KEY_LH    0x0C0D0E0F

#define TEST_VALUE     0x00
#define TEST_LENGTH    (16 * 5)
#define TEST_ROUNDS    1

int main(int argc, char *argv[])
{
    u32 key[4];


    if (FAILURE == dma_init())
        exit(1);

    key[0] = TEST_KEY_LH;
    key[1] = TEST_KEY_LH;
    key[2] = TEST_KEY_HL;
    key[3] = TEST_KEY_HH;
 
    if (FAILURE == aes_set_key(key))
        exit(1);

    printf("AES Key: \n");
    memdump(key, 16);

    for (int i = 0; i < TEST_LENGTH; i++)
    {
        psrc[i] = TEST_VALUE + i;
    }
    printf("Test values: \n");
    memdump(psrc, TEST_LENGTH);

    for (int round = 0; round < TEST_ROUNDS; round++)
    {
        printf("------- Round %d -------\n", round + 1);

        if (FAILURE == dma_start(TEST_LENGTH))
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
