/*
 * File name: aes128.c
 * Program name: aes128
 * Version: 1.0
 * Author: Hsiang-Ju Lai
 * Description:
 *  This program enc/decrypts a file and produces a new file with the result.
 *  Proper command line options and arguments must be provided:
 *      Usage: ./cipher [-devh] [-p PASSWD] infile outfile
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sysexits.h>
#include <sys/time.h>

#include "dma_driver.h"
#include "sw_aes.h"

#define USAGE_LINE "Usage: aes128 [-vhtsnd] [-f nbytes] [-k keyfile] [-i infile] [-o outfile] \n"

#define OPTIONS "vhtsndf:k:i:o:" /* Options for getopt(3) */
#define VERSION "aes128 version 1.0 by Hsiang-Ju Lai\n"

/* Key = 0x000102030405060708090A0B0C0D0E0F */
#define TEST_KEY_HH    0x00010203
#define TEST_KEY_HL    0x04050607
#define TEST_KEY_LH    0x08090A0B
#define TEST_KEY_LL    0x0C0D0E0F


int aes_init(u32* iv)
{
    if (FAILURE == dma_init())
        return FAILURE;

    if (FAILURE == aes_set_iv(iv))
        return FAILURE;

    return SUCCESS;
}

/* This method encrypts the file indicating by fdin and writes
 * to the file indicating by fdout.
 * Parameters: fdin, the file descriptor of the infile
 *             fdout, the file descriptor of the outfile
 *             key, pointer to the key
 * Pre-condition: fdin and fdout are opened and are read/writable.
 * Return: SUCCESS or FAILURE
 */
int encrypt_file(int fdin, int fdout, u32 *key, int forced_buffer_len, int timing)
{
    u32 cnt; /* size of page, number of bytes read, blowfish variable */
    struct timeval tv;
    double begin_t = 0, end_t;
    clock_t start = 0, end;
    double cpu_time_used;
    size_t read_len = (size_t) (forced_buffer_len > 0 ? forced_buffer_len : MAX_SRC_LEN);

    if (FAILURE == aes_set_key(key))
        return FAILURE;

    /* Read from infile to buffer, enc/decrypt buffer, and outputs to outfile */
    while((cnt = (u32)read(fdin, psrc, read_len)) > 0)
    {
        if (forced_buffer_len > 0)
        {
            size_t n_left = forced_buffer_len - cnt;
            while(n_left > 0)
            {
                ssize_t n = read(fdin, psrc + cnt, n_left);
                n_left -= n;
                cnt += n;
            }
        }
        else
        {
            if (cnt % 16 != 0)
            {
                for (; cnt % 16 != 0; cnt++)
                {
                    psrc[cnt] = 0;
                }
            }
        }


        if (timing)
        {
            gettimeofday(&tv, NULL);
            begin_t = (tv.tv_sec) * 1000.0f + (tv.tv_usec) / 1000.0f ;
            start = clock();
        }


        /* encryption happens here */
        if (FAILURE == dma_start(cnt))
            return FAILURE;

        if (timing)
        {
            end = clock();
            cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
            printf("[TIMING] It takes %lf seconds to start the DMA transfer.\n", cpu_time_used);
        }

        if (FAILURE == dma_sync())
            return FAILURE;

        if (timing)
        {
            gettimeofday(&tv, NULL);
            end_t = (tv.tv_sec) * 1000.0f + (tv.tv_usec) / 1000.0f ;
            end = clock();
            cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
            printf("[TIMING] CPU Time: %lf seconds to complete the encryption of %d bytes.\n", cpu_time_used, (int) cnt);
            printf("[TIMING] Total Execution Time: %lf ms.\n", end_t - begin_t);
        }

        if(write(fdout, pdest, cnt) != cnt) //write exactly how many it reads
        {
            perror("outfile");
            return FAILURE;
        }
    }

    dma_clean_up();
    return SUCCESS;
}


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
    exit(EX_USAGE);
}

/*
 * This is the main method of Cipher. Proper command line options
 * and arguments must be provided.
 * Precondition: the program must have permission to access in/outfile.
 * Post-condition: outfile is created and stores the enc/decrypted data
 */
int main(int argc, char *argv[])
{
    int opt, rc; /* option and error return code holders */
    int fdin, fdout, fdkey; /* file descriptors of in/outfile */
    int forced_transfer_len = -1;
    char *keyfile = NULL; /* char pointer to the password */
    char *infile = NULL;
    char *outfile = NULL;
    char *fin_name, *fout_name; /* in/outfile paths */
    clock_t start = 0, end;
    double cpu_time_used;
    u32 key[4];
    u32 iv[4];
    struct {
        unsigned int k : 1;
        unsigned int i : 1;
        unsigned int o : 1;
        unsigned int v : 1;
        unsigned int h : 1;
        unsigned int t : 1;
        unsigned int s : 1;
        unsigned int n : 1;
        unsigned int d : 1;
        unsigned int f : 1;
    } flags; /* flags for command line options */
    memset(&flags, 0, sizeof(flags)); /* zero the flags */
    memset(iv, 0, sizeof(u32) * 4); /* zero the iv */

    /* parse arguments and set flags/arguments */
    while((opt = getopt(argc, argv, OPTIONS)) != -1)
    {
        switch(opt)
        {
            case 'v':
                flags.v = 1;
                break;
            case 'h':
                flags.h = 1;
                break;
            case 't':
                flags.t = 1;
                break;
            case 's':
                flags.s = 1;
                break;
            case 'n':
                flags.n = 1;
                break;
            case 'd':
                flags.d = 1;
                break;
            case 'k':
                if(flags.k == 0)  /* make sure -k hasn't been provided yet */
                {
                    keyfile = malloc(strlen(optarg) + 1);
                    if (keyfile == NULL)
                    {
                        perror("malloc");
                        exit(1);
                    }
                    strcpy(keyfile, optarg);
                    flags.k = 1;
                }
                else
                    args_error("[ERROR] Option -p should only be provided once.\n");
                break;
            case 'f':
                if(flags.f == 0)  /* make sure -k hasn't been provided yet */
                {
                    forced_transfer_len = atoi(optarg);
                    flags.f = 1;
                }
                else
                    args_error("[ERROR] Option -p should only be provided once.\n");
                break;
            case 'i':
                if(flags.i == 0)  /* make sure -p hasn't been provided yet */
                {
                    infile = malloc(strlen(optarg) + 1);
                    if (infile == NULL)
                    {
                        perror("malloc");
                        exit(1);
                    }
                    strcpy(infile, optarg);
                    flags.i = 1;
                }
                else
                    args_error("[ERROR] Option -i should only be provided once.\n");
                break;
            case 'o':
                if(flags.o == 0)  /* make sure -p hasn't been provided yet */
                {
                    outfile = malloc(strlen(optarg) + 1);
                    if (outfile == NULL)
                    {
                        perror("malloc");
                        exit(1);
                    }
                    strcpy(outfile, optarg);
                    flags.o = 1;
                }
                else
                    args_error("[ERROR] Option -o should only be provided once.\n");
                break;

            default: /* opt == '?' */
                args_error(NULL);
        }
    }

    /* Check if options and arguments are valid. */
    if(flags.v)
        printf(VERSION); //print version line

    /* Display the usage line and exit if -h is provided. */
    if(flags.h)
        args_error(NULL);


    // make sure two file paths for in/out file are provided
    if(argc - optind != 0)
        args_error("[ERROR] Extra arguments are provided.\n");


    /* -------- arguments checking is done by here --------- */

    if (flags.f)
        printf("[INFO] Forced transfer length to be exact %d bytes.\n", forced_transfer_len);

    /* If -p is provided, open the password file */
    if(flags.k)
    {
        if((fdkey = open(keyfile, O_RDONLY)) < 0)
        {
            perror(keyfile);
            exit(EX_NOINPUT);
        }

        char temp[9];
        temp[8] = '\0';
        for (int i = 0; i < 4; i++)
        {
            if (8 != read(fdkey, temp, 8))
            {
                perror("Failed to read key file");
                exit(EX_NOINPUT);
            }
            key[i] = (u32) strtol(temp, NULL, 16);
        }
        close(fdkey);
        free(keyfile);
        keyfile = NULL;
    }
    else
    {
        key[0] = TEST_KEY_HH;
        key[1] = TEST_KEY_HL;
        key[2] = TEST_KEY_LH;
        key[3] = TEST_KEY_LL;
    }
    printf("[INFO] Key = %08x%08x%08x%08x\n", key[0], key[1], key[2], key[3]);

    /* If -i is provided, open the infile */
    if(flags.i)
    {
        if((fdin = open(infile, O_RDONLY)) < 0)
        {
            perror(infile);
            exit(EX_NOINPUT);
        }
        printf("[INFO] Input is retrieved from %s\n", infile);
        free(infile);
        infile = NULL;
    }
    else
    {
        fdin = STDIN_FILENO;
        printf("[INFO] Input is retrieved from STDIN\n");
    }

    /* If -o is provided, open the outfile */
    if(flags.o)
    {
        if((fdout = open(outfile, O_RDWR|O_CREAT, 0666)) < 0)
        {
            perror(outfile);
            close(fdin);
            exit(EX_CANTCREAT);
        }
        printf("[INFO] Output is set to %s\n", outfile);
        free(outfile);
        outfile = NULL;
    }
    else
    {
        fdout = STDOUT_FILENO;
        printf("[INFO] Output is set to STDOUT\n");
    }

    if (flags.n)
    {
        printf("[INFO] Option -n is set. Nothing is done. Exiting with code 0...\n");
        close(fdin);
        close(fdout);
        exit(0);
    }

    if (FAILURE == aes_init(iv))
    {
        close(fdin);
        close(fdout);
        exit(1);
    }

    if (flags.t)
        start = clock();

    if (flags.s)
    {
        printf("[INFO] Option -s is set. Use software encryption.\n");
        if (0 != encrypt_file_sw(fdin, fdout, key, iv, psrc))
        {
            perror("encryption");
            close(fdin);
            close(fdout);
            exit(1);
        }
    }
    else if (flags.d)
    {
        printf("[INFO] Option -d is set. Use software decryption.\n");
        if (0 != decrypt_file_sw(fdin, fdout, key, iv, psrc))
        {
            perror("decryption");
            close(fdin);
            close(fdout);
            exit(1);
        }
    }
    else
    {
        if(FAILURE == encrypt_file(fdin, fdout, key, forced_transfer_len, flags.t))
        {
            close(fdin);
            close(fdout);
            exit(1);
        }
    }

    if (flags.t)
    {
        end = clock();
        cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
        printf("\n--------------------------\nTotal CPU Time: %lf\n--------------------------\n\n", cpu_time_used);
    }


    close(fdin);
    close(fdout);

    return 0; /* exit(0) */
}
