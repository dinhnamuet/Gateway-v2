.PHONY: all clean
Src:=`pwd`/Src/
Inc:=`pwd`/Inc/

all:
	gcc -o gateway main.c ${Src}firebase.c ${Src}lora.c -I ${Inc} -lpthread -lcurl -lrt
clean:
	@rm -rf gateway
	@echo "clean"
