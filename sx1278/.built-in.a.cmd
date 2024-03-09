cmd_drivers/sx1278/built-in.a := rm -f drivers/sx1278/built-in.a;  printf "drivers/sx1278/%s " sx1278.o | xargs ar cDPrST drivers/sx1278/built-in.a
