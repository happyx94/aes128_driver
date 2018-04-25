/*
 * File name: aes128.c
 * Program name: aes128
 * Version: 1.0
 * Author: Hsiang-Ju Lai
 * Description:
 *  This program enc/decrypts a file and produces a new file with the result.
 *  Proper command line options and arguments must be provided:
 *      Usage: ./aes128 [-vhtsndr] [-p interval] [-f nbytes] [-k keyfile] [-i infile] [-o outfile]
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

#define USAGE_LINE "Usage: aes128 [-vhtsndr] [-p interval] [-f nbytes] [-k keyfile] [-i infile] [-o outfile] \n"

#define HELP "AES128: Encrypt a FILE with AES128 in CBC mode. Options...\n\n\
\t-v: Print a version line. \n\n\
\t-h: Display the help (this) message. \n\n\
\t-t: Display timeing information. \n\n\
\t-s: Use software encryption. \n\n\
\t-n: No hardward encryptor. Don't try to init hardware AES. \n\n\
\t-d: Do software decryption. \n\n\
\t-r: Reverse the byte order of each 16 bytes block. \n\n\
\t-p num: Set the DMA polling interval to 'num' us. \n\n\
\t-f nbytes: Force encryption chunck size to 'nbytes'. Must be multiples of 16, \n\n\
\t-k keyfile: Specify the path to the key file. \n\n\
\t-i infile: Read the input from 'infile'. The default is STDIN. \n\n\
\t-o outfile: Write the output to 'outfile'. The defailt is STDOUT. \n\n"

#define OPTIONS "vhtsndrp:f:k:i:o:" /* Options for getopt(3) */
#define VERSION "aes128 version 1.2 by Hsiang-Ju Lai\n"

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

static inline uint64_t time_diff_in_us(struct timespec *start, struct timespec *end)
{
    return (uint64_t) ((end->tv_sec * 1000000 + (uint64_t) (end->tv_nsec / 1000))
                       - (start->tv_sec * 1000000 + (uint64_t)(start->tv_nsec / 1000)));
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
    struct timespec begin_t, end_t;
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
                usleep(10);
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
            clock_gettime(CLOCK_MONOTONIC_RAW, &begin_t);
            start = clock();
        }


        /* encryption happens here */
        if (FAILURE == dma_start(cnt))
            return FAILURE;

        if (timing)
        {
            end = clock();
            cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
            fprintf(stderr,"[TIMING] It takes %lf seconds to start the DMA transfer.\n", cpu_time_used);
        }

        if (FAILURE == dma_sync())
            return FAILURE;

        if (timing)
        {
            clock_gettime(CLOCK_MONOTONIC_RAW, &end_t);
            end = clock();
            cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
            fprintf(stderr,"[TIMING] CPU Time: %lf seconds to complete the encryption of %d bytes.\n", cpu_time_used, (int) cnt);
            fprintf(stderr,"[TIMING] Total Time: %ld micro-seconds.\n", time_diff_in_us(&begin_t, &end_t));
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
    int opt; /* option and error return code holders */
    int fdin, fdout, fdkey; /* file descriptors of in/outfile */
    int forced_transfer_len = -1;
    int interval = -1;
    char *keyfile = NULL; /* char pointer to the password */
    char *infile = NULL;
    char *outfile = NULL;
    char *buf = psrc;
    clock_t start = 0, end;
    double cpu_time_used;
    struct timespec begin_t, end_t;
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
        unsigned int p : 1;
        unsigned int r : 1;
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
            case 'r':
                flags.r = 1;
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
                    args_error("[ERROR] Option -k should only be provided once.\n");
                break;
            case 'f':
                if(flags.f == 0)  /* make sure -k hasn't been provided yet */
                {
                    forced_transfer_len = atoi(optarg);
                    flags.f = 1;
                }
                else
                    args_error("[ERROR] Option -f should only be provided once.\n");
                break;
            case 'p':
                if(flags.p == 0)  /* make sure -k hasn't been provided yet */
                {
                    interval = atoi(optarg);
                    flags.p = 1;
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
        fprintf(stderr,VERSION); //print version line

    /* Display the usage line and exit if -h is provided. */
    if(flags.h)
    {
        fprintf(stderr,HELP);
        args_error(NULL);
    }


    // make sure two file paths for in/out file are provided
    if(argc - optind != 0)
        args_error("[ERROR] Extra arguments are provided.\n");


    /* -------- arguments checking is done by here --------- */

    if (flags.f)
        fprintf(stderr,"[INFO] Forced transfer length to be exact %d bytes.\n", forced_transfer_len);

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
    fprintf(stderr,"[INFO] Key = %08x%08x%08x%08x\n", key[0], key[1], key[2], key[3]);

    /* If -i is provided, open the infile */
    if(flags.i)
    {
        if((fdin = open(infile, O_RDONLY)) < 0)
        {
            perror(infile);
            exit(EX_NOINPUT);
        }
        fprintf(stderr,"[INFO] Input is retrieved from %s\n", infile);
        free(infile);
        infile = NULL;
    }
    else
    {
        fdin = STDIN_FILENO;
        fprintf(stderr,"[INFO] Input is retrieved from STDIN\n");
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
        fprintf(stderr,"[INFO] Output is set to %s\n", outfile);
        free(outfile);
        outfile = NULL;
    }
    else
    {
        fdout = STDOUT_FILENO;
        fprintf(stderr,"[INFO] Output is set to STDOUT\n");
    }

    if (!flags.n)
    {
        if (FAILURE == aes_init(iv))
        {
            close(fdin);
            close(fdout);
            exit(1);
        }
    }

    if (flags.p)
    {
        polling_interval = interval;
        fprintf(stderr,"[INFO] Set the polling interval to %d us\n", interval);
    }

    if (flags.t)
    {
        clock_gettime(CLOCK_MONOTONIC_RAW, &begin_t);
        start = clock();
    }

    if (flags.s)
    {
        if (flags.n)
        {
            if (NULL == (buf = malloc(1024 * 1024)))
            {
                perror("malloc");
                close(fdin);
                close(fdout);
                exit(1);
            }
        }

        fprintf(stderr,"[INFO] Option -s is set. Use software encryption.\n");
        if (0 != encrypt_file_sw(fdin, fdout, key, iv, buf, flags.r, forced_transfer_len))
        {
            perror("encryption");
            close(fdin);
            close(fdout);
            exit(1);
        }
    }
    else if (flags.d)
    {
        if (flags.n)
        {
            if (NULL == (buf = malloc(1024 * 1024)))
            {
                perror("malloc");
                close(fdin);
                close(fdout);
                exit(1);
            }
        }

        fprintf(stderr,"[INFO] Option -d is set. Use software decryption.\n");
        if (0 != decrypt_file_sw(fdin, fdout, key, iv, buf, flags.r, forced_transfer_len))
        {
            perror("decryption");
            close(fdin);
            close(fdout);
            exit(1);
        }
    }
    else if (!flags.n)
    {
        if(FAILURE == encrypt_file(fdin, fdout, key, forced_transfer_len, flags.t))
        {
            close(fdin);
            close(fdout);
            exit(1);
        }
    }
    else /* flags.n */
    {
        fprintf(stderr,"[INFO] Option -n is set. No actions are performed.\n");
    }

    if (flags.t)
    {
        end = clock();
        clock_gettime(CLOCK_MONOTONIC_RAW, &end_t);
        cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
        fprintf(stderr,"\n\n--------------------- Timing Summary --------------------\n\n");
        fprintf(stderr,"\tTotal CPU Time: %lf seconds.\n\n", cpu_time_used);
        fprintf(stderr,"\tTotal Real Time: %ld micro-seconds(us).", time_diff_in_us(&begin_t, &end_t));
        fprintf(stderr,"\n\n---------------------------------------------------------\n\n");
    }


    close(fdin);
    close(fdout);

    return 0; /* exit(0) */
}
