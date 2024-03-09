.PHONY: all clean
#KDIR = /lib/modules/`uname -r`/build
#obj-m += sx1278.o
Src:=`pwd`/Src/
Inc:=`pwd`/Inc/

all:
	@#make -C ${KDIR} M=`pwd` modules
	@gcc -o gateway main.c ${Src}firebase.c ${Src}lora.c -I ${Inc} -lpthread -lcurl -lrt
	@echo "make completed"
clean:
	@#make -C ${KDIR} M=`pwd` clean
	@rm -rf gateway
	@echo "clean"
