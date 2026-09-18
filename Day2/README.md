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


## Lab - Build and flash U-Boot
```

```
