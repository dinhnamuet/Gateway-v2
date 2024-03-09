There are the steps to built-in driver for lora ra-02 sx1278
step 1: download and compile linux kernel by tutorial at: https://www.raspberrypi.com/documentation/computers/linux_kernel.html
step 2: in folder ~/linux/drivers, create folder sx1278, go to folder sx1278, copy sx1278.c and sx1278.h to sx1278 folder, create Makefile and Kconfig
step 3: in folder ~linux/drivers, add obj-$(CONFIG_SX1278_LORA) += sx1278/ into Makefile, and add path to Kconfig of folder sx1278 to file Kconfig
step 4: make menuconfig -> Device Driver -> sx1278 device driver -> [*]sx1278 driver -> save
step 5: make -j4 Image.gz modules dtbs
step 6: sudo make modules_install
step 7: copy device tree and kernel to /boot folder and reboot!

When rasperry pi reboot done, you can check by command: dmesg |grep sx1278