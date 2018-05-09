/**
 *  Author: Hsiang-Ju Lai <happyx94@gmail.com>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#define RESERVED_SIZE (1024*1024)

int main()
{
    int mem, rsv;
    unsigned long offset;
    char* buf;

    mem = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem == -1)
    {
        perror("Failed to open /dev/mem. Make sure you have root privilege.\n");
        exit(1);
    }

    rsv = open("/dev/rsvmem", O_RDWR);
    if (rsv == -1)
    {
        perror("Failed to open /dev/rsvmem.\n");
        exit(1);
    }

    if (read(rsv, (void *)&offset, sizeof(unsigned long)) <= 0)
    {
        perror("Failed to read from /dev/rsvmem.\n");
        exit(1);
    }

    printf("Phys. address of the reserved memory is at %08lx\n\n", offset);
    buf = (char *)mmap(NULL, RESERVED_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem, (off_t)offset);
    if (NULL == buf)
    {
        perror("mmap() failed.\n");
        exit(1);
    }

    if (write(rsv, "G", 1U) != 1)
    {
        printf("Error: Failed to write 1 byte to /dev/rsvmem.\n");
        exit(1);
    }

    for (int i = 0; i < RESERVED_SIZE; i++)
    {
        if (buf[i] != 'G')
        {
            printf("Error: buf[%d]'s content is not right.\n", i);
            break;
        }
    }

    if (write(rsv, "<Start of the Reserved Memory Region>", 38U) != 38)
    {
        printf("Error: Failed to write a sentence to /dev/rsvmem.\n");
        exit(1);
    }

    printf("Memory content: %s\n", buf);
    
    printf("Writing to the memory region directly...\n");
    for (int i = 0; i < RESERVED_SIZE; i++)
    {
        buf[i] = i % 256;
    }

    printf("The ending 256 bytes are...\n");
    for (int i = RESERVED_SIZE - 256; i < RESERVED_SIZE; i++)
    {
        printf("%02x", (unsigned int)buf[i]);
        if (i % 4 == 3) printf(" ");
        if (i % 32 == 31) printf("\n");
    }
    printf("\n\n");

    printf("---- All Tests Passed ----\n\n");
    return 0;
}
