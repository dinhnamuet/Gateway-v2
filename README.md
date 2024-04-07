** There are the steps to built-in driver for lora gateway

step 1: download and compile linux kernel by tutorial at: https://www.raspberrypi.com/documentation/computers/linux_kernel.html

step 2: Copy all file of folder Driver to folder ~/linux/drivers/gateway

step 3: in folder ~linux/drivers, add obj-$(CONFIG_GATEWAY) += gateway/ into Makefile, and add path to Kconfig of folder gateway to file Kconfig

step 4: make menuconfig -> Device Driver -> Gateway LORA -> [*]LoRa gateway driver -> save

step 5: make -j4 Image.gz modules dtbs

step 6: sudo make modules_install

step 7: copy device tree and kernel to /boot folder and reboot!

When rasperry pi reboot done, you can check by command: dmesg |grep sx1278 and dmesg |grep Oled

------------------------------------------------------------------------------------------------

** Write init script to run application at boot time

Step 1: copy file gatewayLora to /etc/init.d

Step 2: sudo chmod +x /etc/init.d/gatewayLora

Step 3: sudo update-rc.d gatewayLora defaults
