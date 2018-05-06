/*
 * File name: aestest.c
 * Program name: aestest
 * Version: 1.0
 * Author: Hsiang-Ju Lai
 * Description:
 *  This program is used to test the DMA/AES driver for axis_aes128.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <strings.h>

#include "dma_driver.h"


#define TEST_VALUE     0x00
#define TEST_LENGTH    (16 * 5)
#define TEST_ROUNDS    1

int main(int argc, char *argv[])
{
    char key[16];


    if (FAILURE == dma_init())
        exit(1);

    for (int i = 15; i >= 0; i--)
    {
        key[i] = i;
    }

    if (FAILURE == aes_set_iv(key))
        exit(1);
 
    printf("AES IV: \n");
    memdump(key, 16);

    for (int i = 0; i < 16; i++)
    {
        key[i] = i;
    }
 
    if (FAILURE == aes_set_key(key))
        exit(1);

    printf("AES Key: \n");
    memdump(key, 16);

    for (int i = 0; i < TEST_LENGTH; i++)
    {
        psrc[i] = TEST_VALUE + i;
    }

    printf("Plaintext: \n");
    memdump(psrc, TEST_LENGTH);

    for (int round = 0; round < TEST_ROUNDS; round++)
    {
        printf("------- Round %d -------\n", round + 1);

        if (FAILURE == dma_start(TEST_LENGTH))
            exit(1);
        
        if (FAILURE == dma_sync())
            exit(1);

        printf("Ciphertext:\n");
        memdump(pdest, 16);
        printf("\n");
    }

    dma_clean_up();
    return 0;
}
