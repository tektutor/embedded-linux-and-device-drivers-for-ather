# Day 2

## Lab - Flash latest trixie Debian Embedded Linux with SD Card and boot your BeagleBone Black Board
```
# On your laptop
# Download the latest IOT (non-gui) debian image from www.beagleboard.org/distros
cd ~/Downloads
wget https://files.beagle.cc/file/beagleboard-public-2021/images/am335x-debian-13.7-base-v6.18-armhf-2026-09-15-4gb.img.xz
ls -l ls am335x-debian-*.img.xz

# On your laptop
# Insert your microSD card in your laptop, you may have to use full-size SD Card adapter, make sure the write protection is OFF
lsblk # Find your card (a disk like mmcblk0 or sdb); read the RO column ( 1 - indicates it is write protected, ideally value is 0 )

# On your laptop
xzcat am335x-debian-13.7-base-v6.18-armhf-2026-09-15-4gb.img.xz \
| sudo dd of=/dev/mmcblk0 bs=4M status=progress conv=fsync
sync
```



## Lab - Build and flash U-Boot
```

```
