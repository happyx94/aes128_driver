/*
 * File name: dma_driver.c
 * Version: 1.0
 * Author: Hsiang-Ju Lai
 * Description:
 *  A simple DMA User space that talks to the AXIS_AES128 crypto-accelerator.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>

#include "dma_driver.h"

/* AES-related macros */
#define AES_KEY_ADDR            0x43C10000
#define AES_KEY_REGS_MAP_LEN    4096

/* DMA-related macros */
#define DMA_BASE_ADDR       0x40400000
#define DMA_MMAP_LEN        4096

#define MM2S_CNTL_REG       0x00
#define MM2S_STATUS_REG     0x04
#define MM2S_SRC_ADDR_REG   0x18
#define MM2S_LEN_REG        0x28

#define S2MM_CNTL_REG       0x30
#define S2MM_STATUS_REG     0x34
#define S2MM_DEST_ADDR_REG  0x48
#define S2MM_LEN_REG        0x58

#define DMA_HALT            0
#define DMA_RESET           4

/* Macro functions */
#define set_dma_reg(offset,value) ((u32 *)pdma)[(offset)>>2] = value
#define dma_reg(offset) (((u32 *)pdma)[(offset)>>2])

#define dma_s2mm_status() (dma_status(S2MM_STATUS_REG))
#define dma_mm2s_status() (dma_status(MM2S_STATUS_REG))

#define dma_s2mm_poll() ((dma_reg(S2MM_STATUS_REG) & (1<<1 | 1<<12)) == (1<<1 | 1<<12) ? SUCCESS : FAILURE)
#define dma_mm2s_poll() ((dma_reg(MM2S_STATUS_REG) & (1<<1 | 1<<12)) == (1<<1 | 1<<12) ? SUCCESS : FAILURE)

#define DMA_SOURCE_ADDR       buf_phy_addr
#define DMA_DESTINATION_ADDR  (buf_phy_addr + RSV_BUF_LEN / 2)

#define REVERSE_32(n) ((((n)>>24)&0xff) | (((n)<<8)&0xff0000) | (((n)>>8)&0xff00) | (((n)<<24)&0xff000000))

void *pbuf;
int mem_fd;
int polling_interval;
static void *pdma;
static u32 buf_phy_addr;

char* dma_status(u8 offset) 
{
    int count;
    u32 status = dma_reg(offset);
    static char status_buf[128];
	

    if (status & 0x00000001) 
        count = snprintf(status_buf, 128, "Halted ( ");
    else 
        count = snprintf(status_buf, 128, "Running ( ");

    if (status & 0x00000002) 
        count += snprintf(status_buf+count, 128 - count, "Idle ");
    if (status & 0x00000008) 
        count += snprintf(status_buf+count, 128 - count, "SGIncld ");
    if (status & 0x00000010) 
        count += snprintf(status_buf+count, 128 - count, "DMAIntErr ");
    if (status & 0x00000020) 
        count += snprintf(status_buf+count, 128 - count, "DMASlvErr ");
    if (status & 0x00000040) 
        count += snprintf(status_buf+count, 128 - count, "DMADecErr ");
    if (status & 0x00000100) 
        count += snprintf(status_buf+count, 128 - count, "SGIntErr ");
    if (status & 0x00000200) 
        count += snprintf(status_buf+count, 128 - count, "SGSlvErr ");
    if (status & 0x00000400) 
        count += snprintf(status_buf+count, 128 - count, "SGDecErr ");
    if (status & 0x00001000) 
        count += snprintf(status_buf+count, 128 - count, "IOC_Irq ");
    if (status & 0x00002000) 
        count += snprintf(status_buf+count, 128 - count, "Dly_Irq ");
    if (status & 0x00004000)
        count += snprintf(status_buf+count, 128 - count, "Err_Irq ");
    
    snprintf(status_buf+count, 128 - count, ")");

    return status_buf;
}

int dma_quick_poll()
{
    if (dma_mm2s_poll() && dma_s2mm_poll())
        return SUCCESS;
    return FAILURE;
}

static int dma_s2mm_sync()
{
    int count = 0;

    fprintf(stderr, "[INFO] Waiting for s2mm to finish tranfering...\n");
    while(FAILURE == dma_s2mm_poll())
    {
        if (polling_interval > 0)
        {
            usleep((__useconds_t) polling_interval);
            if (count++ >= 10000)
                break;
        }
    }
    return (count < 2001 ? SUCCESS : FAILURE); 
}

static int dma_mm2s_sync()
{
    int count = 0;

    fprintf(stderr, "[INFO] Waiting for mm2s to finish tranfering...\n");
    while(FAILURE == dma_mm2s_poll())
    {
        if (polling_interval > 0)
        {
            usleep((__useconds_t) polling_interval);
            if (count++ >= 10000)
                break;
        }
    }
    return (count < 2001 ? SUCCESS : FAILURE); 
}

int dma_sync()
{
    if (FAILURE == dma_mm2s_sync())
        return FAILURE;
    return dma_s2mm_sync();
}

void dma_clean_up()
{
    if(NULL != pdma)  munmap(pdma, DMA_MMAP_LEN);
    if(NULL != pbuf)  munmap(pbuf, RSV_BUF_LEN);
    buf_phy_addr = 0;
    close(mem_fd);
    mem_fd = -1;
}

int dma_reset()
{
    if (NULL == pdma)
    {
        dma_clean_up();
        fprintf(stderr, "[ERROR] DMA driver hasn't been initialized\n");
        return FAILURE;
    }

    fprintf(stderr, "[INFO] Resetting the DMA...\n");
    set_dma_reg(S2MM_CNTL_REG, DMA_RESET);
    set_dma_reg(MM2S_CNTL_REG, DMA_RESET);


    return SUCCESS;
}

int dma_start(u32 len)
{
    if (len > RSV_BUF_LEN / 2)
    {
        fprintf(stderr, "[ERROR] Failed to start transfer. The maximum size is %dKB\n", RSV_BUF_LEN / 2048);
        return FAILURE;
    }

    fprintf(stderr, "[INFO] Halting the DMA...\n");
    set_dma_reg(S2MM_CNTL_REG, DMA_HALT);
    set_dma_reg(MM2S_CNTL_REG, DMA_HALT);
    fprintf(stderr, "[DEBUG] S2MM Cntl Reg Status: %s\n", dma_s2mm_status());
    fprintf(stderr,"[DEBUG] MM2S Cntl Reg Status: %s\n", dma_mm2s_status());

    fprintf(stderr,"[INFO] Setting DMA transfer address...\n");
    set_dma_reg(S2MM_DEST_ADDR_REG, DMA_DESTINATION_ADDR); // Write destination address
    set_dma_reg(MM2S_SRC_ADDR_REG, DMA_SOURCE_ADDR);

    fprintf(stderr,"[INFO] Starting the DMA channels...\n");	
    set_dma_reg(S2MM_CNTL_REG, 0xf001);
    set_dma_reg(MM2S_CNTL_REG, 0xf001);

    fprintf(stderr,"[INFO] Initiating the transfer by writing the length...\n");
    set_dma_reg(S2MM_LEN_REG, len);
    set_dma_reg(MM2S_LEN_REG, len);

    fprintf(stderr,"[DEBUG] S2MM Cntl Reg Status: %s\n", dma_s2mm_status());
    fprintf(stderr,"[DEBUG] MM2S Cntl Reg Status: %s\n", dma_mm2s_status());
    
    return SUCCESS;
}

int dma_init()
{
    int rsv_fd;

    polling_interval = 0;
    fprintf(stderr,"[INFO] Set the polling interval to 0 us (busy waiting).\n");

    fprintf(stderr,"[INFO] Initializing the DMA driver...\n");

    fprintf(stderr,"[INFO] Trying to mmap physical memory...\n");
    mem_fd = open("/dev/mem", O_RDWR | O_SYNC); 
    if (mem_fd == -1)
    {
        perror("Failed to open /dev/mem");
        return FAILURE;
    }
 
    fprintf(stderr,"[INFO] Getting the physical buffer address from /dev/rsvmem...\n");
    rsv_fd = open("/dev/rsvmem", O_RDONLY);
    if (rsv_fd == -1)
    {
        perror("Failed to open /dev/rsvmem");
        if (mem_fd >= 0) close(mem_fd);
        return FAILURE;
    }

    if (read(rsv_fd, (void *)&buf_phy_addr, sizeof(u32)) <= 0)
    {
        perror("Failed to read from /dev/rsvmem.\n");
        return FAILURE;
    }
    fprintf(stderr,"[DEBUG] The physical buffer address is at %08x\n", buf_phy_addr);

    pbuf = mmap(NULL, RSV_BUF_LEN, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, (off_t)buf_phy_addr); 
    if (NULL == pbuf)
    {
        perror("Failed to mmap the rsvmem buffer");        
        if (mem_fd >= 0) close(mem_fd);
        if (rsv_fd >= 0) close(rsv_fd);
        
        return FAILURE;
    }

    /* mmap DMA AXI Lite Register Block */
    pdma = mmap(NULL, DMA_MMAP_LEN, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, DMA_BASE_ADDR); 
    if (NULL == pdma)
    {
        dma_clean_up();
        perror("Failed to mmap DMA registers");        
        if (mem_fd >= 0) close(mem_fd);
        if (rsv_fd >= 0) close(rsv_fd);
        
        return FAILURE;
    }
    close(rsv_fd);
    return dma_reset();
}

int aes_set_key(void *pkey)
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
        pregs[3-i] = REVERSE_32(key[i]);
    }
    fprintf(stderr,"[INFO] AES key has be set.\n");
    munmap(pregs, AES_KEY_REGS_MAP_LEN);

    return SUCCESS;
}

int aes_set_iv(void *piv)
{
    u32 *pregs;
    u32 *iv = piv;
    u32 temp[4];

    if (mem_fd < 0)
        return FAILURE;

    pregs = mmap(NULL, AES_KEY_REGS_MAP_LEN, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, AES_KEY_ADDR);
    if (NULL == pregs)
    {
        perror("Failed to mmap the AES control registers");
        return FAILURE;
    }

    for (int i = 0; i < 4; i++)
    {
        temp[3-i] = pregs[3-i];
        pregs[3-i] = iv[i];
    }
    /* Set the set_IV flag */
    pregs[4] = 0xFFFFFFFF;
    fprintf(stderr,"[INFO] Setting the IV... \n");
    pregs[4] = 0;

    for (int i = 0; i < 4; i++)
        pregs[i] = temp[i];

    fprintf(stderr,"[INFO] AES IV has be set.\n");
    munmap(pregs, AES_KEY_REGS_MAP_LEN);

    return SUCCESS;
}

void memdump(void* buf_ptr, int byte_count) 
{
    char *p = buf_ptr;
    int offset = 0;

    if (byte_count > MEM_DUMP_MAX_BYTES)
    {
        fprintf(stderr,"[DEBUG] Buffer size is %d bytes. Only show the last %d bytes...\n", byte_count, MEM_DUMP_MAX_BYTES);
        offset = byte_count - MEM_DUMP_MAX_BYTES;
    }    

    for (; offset < byte_count; offset++) 
    {
        fprintf(stderr,"%02x", p[offset]);
        if (offset % 4 == 3)  
            fprintf(stderr," ");
        if (offset % 32 == 31)
            fprintf(stderr,"\n"); 
    }
    fprintf(stderr,"\n");
}
