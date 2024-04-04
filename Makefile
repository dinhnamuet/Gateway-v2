.PHONY: all clean mkObj static shared install

CC:=gcc
Src:=`pwd`/Src
Inc:=`pwd`/Inc
Obj:=`pwd`/Obj
BIN:=`pwd`/Bin
LIB_STATIC:=`pwd`/libs/static
LIB_SHARED:=`pwd`/libs/shared

all: mkObj static shared install
	${CC} -Wall -o ${BIN}/gateway ${Obj}/main.o ${Src}/oled.c -I ${Inc} -L${LIB_STATIC} -lfirebase -llora -lpthread -lcurl -lrt

mkObj:
	${CC} -c main.c -o ${Obj}/main.o -I ${Inc}
	${CC} -c ${Src}/firebase.c -o ${Obj}/firebase.o -I ${Inc}
	${CC} -c ${Src}/lora.c -o ${Obj}/lora.o -I ${Inc}

static:
	ar -rcs ${LIB_STATIC}/libfirebase.a ${Obj}/firebase.o

shared:
	${CC} -shared -o ${LIB_SHARED}/liblora.so ${Obj}/lora.o

install:
	install ${LIB_SHARED}/liblora.so /usr/lib
clean:
	@rm -rf ${BIN}/* ${Obj}/*.o ${LIB_STATIC}/* ${LIB_SHARED}/*
	@rm /usr/lib/liblora.so

