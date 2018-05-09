obj-m+=rsvmem.o

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
test:
	gcc -o rsvmemtest rsvmemtest.c
clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
	rm -f rsvmemtest
