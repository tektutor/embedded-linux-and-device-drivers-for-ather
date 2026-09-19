# Day 2

## Lab - Flash latest trixie Debian Embedded Linux with SD Card and boot your BeagleBone Black Board
**Note - Do not ignore this**
<pre>
dd is unforgiving: of=/dev/mmcblk0 writes the whole card, and pointing it at the wrong device 
(your laptop's disk, often /dev/nvme0n1 or /dev/sda) will destroy that disk. Run lsblk, 
and confirm the device name and size match your SD card, not your system disk. 
Write to the whole card (mmcblk0), not a partition (mmcblk0p1)  
</pre>
```
# On your laptop
# Download the latest IOT (non-gui - avoid xfce) debian image from www.beagleboard.org/distros
cd ~/Downloads
wget https://files.beagle.cc/file/beagleboard-public-2021/images/am335x-debian-13.7-base-v6.18-armhf-2026-09-15-4gb.img.xz
ls -l ls am335x-debian-*.img.xz

# On your laptop
# Insert your microSD card in your laptop, you may have to use full-size SD Card adapter, make sure the write protection is OFF
# nvme is your laptop SSD - leave that alone
lsblk # Find your card (a disk like mmcblk0 or sdb); read the RO column ( 1 - indicates it is write protected, ideally value is 0 )

# On your laptop terminal
xzcat am335x-debian-13.7-base-v6.18-armhf-2026-09-15-4gb.img.xz \
| sudo dd of=/dev/mmcblk0 bs=4M status=progress conv=fsync
sync

# On your laptop, mount the SD Card partition
sudo mkdir -p /mnt/bbb-boot
ls -la /mnt/bbb-boot/
sudo mount /dev/mmcblk0p1 /mnt/bbb-boot
ls -la /mnt/bbb-boot/

# In case your SD Card image comes with corrupted or emtpy sysconf.txt
cd ~/Downloads
ls -l am335x-debian-13.7-base-v6.18-armhf-2026-09-15-4gb.img.xz

# decompress a working copy of the image
xz -dkc am335x-debian-13.7-base-v6.18-armhf-2026-09-15-4gb.img.xz > /tmp/bbb.img

# attach it as a loop device and auto-scan its partitions
sudo losetup -Pf --show /tmp/bbb.img

# mount the boot (FAT) partition of the image
sudo mkdir -p /mnt/img-boot

# Make sure to update your loop device number not my /dev/loop35p1
ls -l /dev/loop35*
sudo mount /dev/loop35p1 /mnt/img-boot
# Now, edit the file at /mnt/img-boot/sysconf.txt with your login credentials, refer screenshot below

# After uncommenting and update your login credentials, including the root password
sudo cp /mnt/img-boot/sysconf.txt /mnt/bbb-boot/sysconf.txt
sync

# Make sure your SD card reflects your login credentials
sudo cat /mnt/bbb-boot/sysconf.txt


# We are done with the SD card flashing
sudo umount /mnt/bbb-boot
sudo umount /mnt/img-boot
sudo losetup -d /dev/loop35
rm /tmp/bbb.img
```
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/2edce19a-396d-47de-8232-7e0afc0d97ee" />
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/7ea784be-a521-467e-bfed-2bae7aa63bf4" />
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/1c833906-d8bb-46cd-8258-14a9ca6d1e56" />
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/efd9d404-3933-4664-a956-8fdd2a11419f" />
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/fdc83585-5f68-4fb7-91a8-67a1a592b803" />
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/08b2ce7c-7724-40fb-b29a-5a1c17a109ad" />

Power off your BBB board, insert your microSD card in your BBB board.  On your laptop start
```
sudo ls -l /dev/ttyACM*
sudo minicom -D /dev/ttyACM0 -b 115200
```

Troubleshooting, Device /dev/ttyACM0 is locked issue ( Generally, happens if another session is already open or closed abruptly )
```
# check which application or user has opened it
sudo lsof /dev/ttyACM0
sudo fuser -v /dev/ttyACM0

# minicom / uucp lock files live here
ls -l /var/lock/LCK..ttyACM0 /run/lock/LCK..ttyACM0 2>/dev/null
sudo rm -f /var/lock/LCK..ttyACM0 /run/lock/LCK..ttyACM0

# If a real process, holds it
sudo pkill -f minicom          # kill leftover minicom sessions

# Now, this should work
sudo minicom -D /dev/ttyACM0 -b 115200
```

<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/a9859a74-48a5-4377-8934-504aedbf0e7a" />
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/1005bae5-ab64-4659-a960-1aaafa5a8eda" />

You can run the LED Blink, on your newly flashed Trixie 13.7 IOT - SD Card image
```
gpioset -t 500ms P9_12=1
```

## Lab - Build and flash U-Boot
Install this on your laptop
```
sudo apt update
sudo apt install -y build-essential git bison flex libssl-dev \
        device-tree-compiler swig python3-dev python3-setuptools \
        gcc-arm-linux-gnueabihf u-boot-tools libgnutls28-dev

arm-linux-gnueabihf-gcc --version
```
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/e082f132-e03b-4f16-8ac1-a5d77764a317" />
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/99480b95-c582-4e00-88ec-e32faf32633f" />

Download the U-Boot source code to build it yourself on your laptop
```
cd ~
git clone https://source.denx.de/u-boot/u-boot.git
cd u-boot

# pin a known release so the build is repeatable
git checkout v2026.07

# point the build at the cross compiler
export CROSS_COMPILE=arm-linux-gnueabihf-

# select the BeagleBone Black configuration
ls configs/ | grep -iE 'am335x|bone|beagle'
make am335x_evm_defconfig

# build
make -j"$(nproc)"

ls -l MLO u-boot.img

```

<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/970dc4db-5b37-4df8-9565-9cc10782fb70" />
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/824c737d-14a4-4e4b-81f3-8d120e8d0a6c" />
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/3176c1c2-2534-4ab9-8f14-70177ab60d72" />
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/229739e6-3d9d-4925-8d18-4d77e069ba6e" />
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/1317ae2a-df0f-4e0a-9b40-88408464159b" />
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/97057b53-ce7f-4898-922b-41ba23f5e5a4" />

If you wish to make some custom changes like add your custom name
```
cd ~/u-boot
grep -n "board_init\|board_late_init" board/ti/am335x/board.c
# Edit board/ti/am335x/board.c and update the beginning of the board_late_init(void) function as shown in screenshot and save
vim  board/ti/am335x/board.c
cat board/ti/am335x/board.c | grep Jegan

export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-
make -j"$(nproc)"
grep -n "TekTutor" board/ti/am335x/board.c   # confirm your lines are there
ls -l MLO u-boot.img                          # confirm rebuilt 
```
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/79eccad3-f823-4dcf-87c1-018528133ede" />
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/69bb836d-a104-4b57-8ed8-073bd97b4eb5" />
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/f5e9d539-7974-424b-a75c-687d2d314de0" />
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/349e95ce-95b2-4b45-8182-88a6243d7ad6" />
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/cddd3483-9a2d-4fca-83fb-55dba567af6b" />
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/cc99a234-db70-49fe-83c6-f52e10561cf0" />
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/9d3868ad-6d85-46d9-8e94-09aae4b605e9" />
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/b93581fc-6b93-4bc3-8766-1075d74575fd" />

Reflash the Uboot(bootloader) in SDCard and flash
```
sudo mkdir -p /mnt/sdboot
cd ~/u-boot
sudo mount /dev/mmcblk0p1 /mnt/sdboot
sudo cp MLO /mnt/sdboot/ && sync
sudo cp u-boot.img /mnt/sdboot/ && sync
sudo umount /mnt/sdboot
```
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/d45b3cc8-92ea-4d8e-a9e1-0477428f64d9" />

Now, boot your BBB board with FTDI to watch your custom u-boot bootloader banner.

## Lab - Booting OS from SDCard using your custom u-boot boo tloader in SDCard
Assumption is, you have already flashed your SD-Card with Trixie OS and copied your custom u-boot bootloader on your SD-Card.

Now, hold the S2 button on your BBB Board and power it on, wait until you get the u-boot prompt ==>
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/8b1e714b-d538-43fb-9d17-3512730b9d06" />

You need to type this command on the u-boot promt ==>
```
setenv bootcmd 'load mmc 0:3 0x82000000 /boot/vmlinuz-6.18.52-bone54; load mmc 0:3 0x88000000 /boot/dtbs/6.18.52-bone54/am335x-boneblack.dtb; load mmc 0:3 0x88080000 /boot/initrd.img-6.18.52-bone54; setenv irdsize ${filesize}; setenv bootargs console=ttyS0,115200n8 root=/dev/mmcblk0p3 rw rootfstype=ext4 rootwait fsck.repair=yes coherent_pool=1M net.ifnames=0 rng_core.default_quality=100; bootz 0x82000000 0x88080000:${irdsize} 0x88000000'
```
Understanding the above command, basically we are configuring our custom u-boot to load the OS to find the kernel and load it
<pre>
- load mmc 0:3 0x82000000 /boot/vmlinuz-6.18.52-bone54  
- Loads the kernel, load reads a file from storage into RAM
- mmc 0:3 means mmc device 0 (the SD card), partition 3 (the root filesystem)
- On this board, mmc 0 = SD, mmc 1 = eMMC, you need to find yours from your u-boot prompt by typing 
- 0x82000000 is the RAM address to load the kernel
- /boot/vmlinuz-6.18.52-bone54 is the kernel file on your SD-Card

- load mmc 0:3 0x88000000 /boot/dtbs/6.18.52-bone54/am335x-boneblack.dtb
- loads the device tree blob into RAM at 0x88000000

- load mmc 0:3 0x88080000 /boot/initrd.img-6.18.52-bone54
  setenv irdsize ${filesize}
- Loads the initial RAM disk (a small early-boot filesystem) into RAM at 0x88080000
- ${filesize} is a variable U-Boot automatically sets to the size of the last file loaded

- setenv bootargs console=ttyS0,115200n8 root=/dev/mmcblk0p3 rw rootfstype=ext4 rootwait fsck.repair=yes coherent_pool=1M net.ifnames=0 rng_core.default_quality=100
- bootargs is the text passed to the kernel as its command line. Piece by piece
  - console=ttyS0,115200n8, send kernel messages to the serial console at 115200 baud
  - root=/dev/mmcblk0p3, the root filesystem is on the SD card, partition 3. Critical: this must point at the real rootfs partition
  - rw, mount root read-write
  - rootfstype=ext4, the filesystem type
  - rootwait, wait for the storage to appear before mounting (SD/eMMC can be slow to enumerate).
  - The rest (fsck.repair=yes coherent_pool=1M net.ifnames=0 rng_core.default_quality=100) are settings copied from the image's own uEnv.txt so the OS behaves as the vendor intended (auto-repair the filesystem, a memory pool size, predictable network naming, RNG quality).

- bootz 0x82000000 0x88080000:${irdsize} 0x88000000
- bootz boots a zImage-format ARM kernel. Its three arguments are: kernel address, initrd address:size, device tree address.
  - 0x82000000, the kernel we loaded
  - 0x88080000:${irdsize}, the initrd address, followed by its size (that's what we saved earlier).
  - 0x88000000, the device tree
  - The order is always: kernel, initrd, dtb
</pre>

Then type in the prompt ==>
```
saveenv
```
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/a342e15c-56ea-4cfc-81db-651634d1f874" />

Test without rebooting
```
boot
```
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/f87c5393-caf2-4bbb-8f94-8bc2153abe76" />
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/d5e7afbb-215b-4198-8baa-5a17872cb0af" />

Shutdown your BBB board, hold the S2 button and Power ON to see your custom u-boot loading the OS on SD-Card automatically
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/4302d3b6-4870-4e0e-8a75-bd1e9fadb4e4" />
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/15fc0b2b-f59f-4290-b5bb-f7ba610f8a41" />
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/296b1625-d655-4a77-ac23-9b5efe92daa4" />
