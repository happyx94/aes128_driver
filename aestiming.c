/*
 * File name: aestiming.c
 * Program name: aestiming
 * Version: 1.0
 * Author: Hsiang-Ju Lai
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dma_driver.h"
#include "sw_aes.h"

#define USAGE_LINE "Usage: aestiming [-sw] [-l CHUNK_LEN] [-n N_CHUNKS] [-i INTERVAL]\n"
#define OPTIONS "wsl:n:i:" /* Options for getopt(3) */



/* This method prints the passed-in error message on stderr if not NULL.
 * It then prints the usage line and exit with code EX_USAGE.
 * Parameters: err_msg, a constant char pointer to the string to be printed.
 *             pass NULL if no error message to print
 * Post-condition: The program exit with proper error code.
 */
void args_error(const char* err_msg)
{
    if(err_msg != NULL) //print err_msg is not NULL
        fputs(err_msg, stderr);
    fprintf(stderr, USAGE_LINE);
    exit(1);
}


int main(int argc, char *argv[])
{
    int opt; /* option and error return code holders */
    int chunk_len = 8192;
    int n_chunks = 1;
    int interval = 10;
    char *buf = NULL;
    uint8_t key[16];
    uint8_t iv[16];

    struct {
        unsigned int s : 1;
        unsigned int l : 1;
        unsigned int n : 1;
        unsigned int i : 1;
        unsigned int w : 1;
    } flags; /* flags for command line options */
    memset(&flags, 0, sizeof(flags)); /* zero the flags */

    /* parse arguments and set flags/arguments */
    while((opt = getopt(argc, argv, OPTIONS)) != -1)
    {
        switch(opt)
        {
            case 's':
                flags.s = 1;
                break;
            case 'w':
                flags.w = 1;
                break;
            case 'l':
                if(flags.l == 0)  /* make sure -k hasn't been provided yet */
                {
                    chunk_len = atoi(optarg);
                    flags.l = 1;
                }
                else
                    args_error("[ERROR] Option -l should only be provided once.\n");
                break;
            case 'n':
                if(flags.n == 0)  /* make sure -k hasn't been provided yet */
                {
                    n_chunks = atoi(optarg);
                    flags.n = 1;
                }
                else
                    args_error("[ERROR] Option -n should only be provided once.\n");
                break;
            case 'i':
                if(flags.i == 0)  /* make sure -k hasn't been provided yet */
                {
                    interval = atoi(optarg);
                    flags.i = 1;
                }
                else
                    args_error("[ERROR] Option -i should only be provided once.\n");
                break;

            default: /* opt == '?' */
                args_error(NULL);
        }
    }

    // make sure two file paths for in/out file are provided
    if(argc - optind != 0)
        args_error("[ERROR] Extra arguments are provided.\n");

    /* -------- arguments checking is done by here --------- */

    fprintf(stderr, "chunk_len = %d bytes, n_chunks = %d, interval = %d us\n", chunk_len, n_chunks, interval);

    if (FAILURE == dma_init())
        return FAILURE;

    buf = psrc;

    polling_interval = interval;


    if (flags.s)
    {
        struct AES_ctx ctx;

        fprintf(stderr,"[INFO] Option -s is set. Use software encryption.\n");
        AES_init_ctx_iv(&ctx, key, iv);
        for (int i = 0; i < n_chunks; i++)
        {
            AES_CBC_encrypt_buffer(&ctx, (uint8_t *) buf, (uint32_t) chunk_len);

            if (flags.w)
            {
                if (write(STDOUT_FILENO, buf, (size_t) chunk_len) != chunk_len)
                {
                    perror("outfile");
                    exit(1);
                }
            }
        }
    }
    else
    {
        for (int i = 0; i < n_chunks; i++)
        {
            if (FAILURE == dma_start((u32) chunk_len))
                exit(1);
            if (FAILURE == dma_sync())
                exit(1);

            if (flags.w)
            {
                if (write(STDOUT_FILENO, pdest, (size_t) chunk_len) != chunk_len)
                {
                    perror("outfile");
                    exit(1);
                }
            }
        }
    }

    return 0; /* exit(0) */
}
