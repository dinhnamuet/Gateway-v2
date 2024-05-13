savedcmd_drivers/Lora/built-in.a := rm -f drivers/Lora/built-in.a;  printf "drivers/Lora/%s " sx1278.o ssd1306.o | xargs aarch64-linux-gnu-ar cDPrST drivers/Lora/built-in.a
