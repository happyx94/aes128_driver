#!/bin/sh                                                                       
                                                                                
insmod /lib/modules/`uname -r`/extra/rsvmem.ko                        
mknod /dev/rsvmem c 60 0
