# aes128_driver

## Build Instructions

---
Prerequisites
---
1. Have a petalinux project targeting at a Zynq SoC.
2. Include axis_aes128 and axilite_aes_cntl in your PL.
3. The PS controls axis_aes128 through AXI-DMA.
4. Know the base address of the DMA and aes_cntl registers.
5. **DISABLED** the kernel AXI-DMA driver in Petalinux kernel settings.

---
Add rsvmem (kernel memory allocator)
---
First, create a custom module. To do so, inside the petalinux project, type
```
$ petalinux-create -t modules --name rsvmem --enable
```
And then replace <Your-Project-Root>/project-spec/meta-user/recipes-modules/rsvmem/files/rsvmem.c
with /rsvmem/rsvmem.c 
(Optional: change the RESERVED_SIZE macro in the source file if needed)


(Optional) Add the test program for rsvmem. Create a custom app by typing
```
$ petalinux-create -t apps --name rsvmemtest --enable
```
Replace <Your-Project-Root>/project-spec/meta-user/recipes-apps/rsvmemtest/files/rsvmemtest.c
with /rsvmem/rsvmemtest.c (Change the RESERVED_SIZE macro in the source file if needed)

*Note*: if the auto-generate Makefile under the same directory doesn't contain the *clean:* target. 
Append the following code to the Makefile:
```
clean:
	rm -f *.o rsvmemtest
```

---
Add aes128
---
Create a custom app by typing
```
$ petalinux-create -t apps --name aes128 --enable
```
Copy *all* files under aes128/ to <Your-Project-Root>/project-spec/meta-user/recipes-apps/aes128/files.

Replace all. If you have modified the size of rsvmem, modify RSV_BUF_LEN in dma_driver.h accordingly.
 
\***Important** Modify the *AES_KEY_ADDR* and *DMA_BASE_ADDR* macros in dma_driver.c based on your IP address mapping.

---
Build
---
```
$ petalinux-build
```

## Run
### Initialzation
Copy scripts/dma_setup.sh to your system. In the same folder, issue
```
./dma_setup.sh
``` 
You should see a message saying "rsvmem is successfully inserted." 

### Encrypt a file using the hardware AES accelerator
To encrypt a file named *infile* with *keyfile* and write the encrypted file to *outfile*, issue
``` 
aes128 -i infile -k keyfile -o outfile
```
Note that by default the program performs busy-polling on the DMA. Add *-p INTERVAL* option to avoid wasting processing power.
 
### Encrypt a stream file
To encrypt the input from *STDIN* in chunk size of *16384 bytes* with *keyfile* with *50 us* polling interval and write the output to *STDOUT*, issue
``` 
aes128 -k keyfile -f 16384 -p 50
```

### Help
```
aes128 -h
```


