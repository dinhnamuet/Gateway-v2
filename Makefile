.PHONY: all clean
KDIR = /lib/modules/`uname -r`/build
obj-m += sx1278.o
all:
	make -C ${KDIR} M=`pwd` modules
	@gcc -o gateway main.c firebase.c lora.c -I. -lpthread -lcurl -lrt
	@#rm /dev/shm/*
clean:
	make -C ${KDIR} M=`pwd` clean
	@rm -rf gateway
