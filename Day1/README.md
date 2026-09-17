# Day 1 

## Lab - Flashing Zephyr on STM32 F446RE Nucleo ( Complementary exercise - not in our Training agenda )

Install Zephyr SDK and west tools
```
sudo apt update
sudo apt install --no-install-recommends git cmake ninja-build gperf \
  ccache dfu-util device-tree-compiler wget \
  python3-dev python3-pip python3-setuptools python3-tk python3-wheel \
  xz-utils file make gcc gcc-multilib g++-multilib libsdl2-dev libmagic1

python3 -m venv ~/zephyr-venv
source ~/zephyr-venv/bin/activate
pip install west

echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc

west --version
```
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/88072b23-256c-4101-ac78-f411cbb4b45b" />

<pre> 
Prerequisites
- Install the Zephyr SDK and west tool first
- west update will clones around 70+ Git repositories
- Total west update might require about 2~3 GB 
- Make sure you have atleast 5GB+ free space 
</pre>
```
west init ~/zephyrproject
cd ~/zephyrproject
west update
pip install -r ~/zephyrproject/zephyr/scripts/requirements.txt
west zephyr-export
```
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/ee363511-94cd-4a09-8fe2-dfa0622e89fe" />
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/8b1f68b8-186b-400c-aa0b-8da38cb3c4bb" />
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/3ad54253-04e1-4ce6-95c5-253e1f314196" />
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/38ab75fc-cdd9-444e-96a3-d9cdfd75aa54" />
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/22697063-171b-42c6-a29e-dc240b46f63c" />
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/2ae3d608-5cd2-440c-9313-185e44b5c091" />
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/86286d34-a437-4b1f-adf2-a1355babdc28" />

Download and install the Zephyr SDK for your platform. It bundles the ARM toolchain and host tools you need.
```
cd ~/zephyrproject
source zephyr/zephyr-env.sh
```

Install Zephyr SDK
```
cd ~
rm -rf zephyrproject
source ~/zephyr-venv/bin/activate
west init -m https://github.com/zephyrproject-rtos/zephyr --mr v4.3.0 ~/zephyrproject
cd ~/zephyrproject
west update
west zephyr-export
pip install -r zephyr/scripts/requirements.txt
```
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/a48b1a4b-b1a7-486e-a013-7207be680839" />
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/82b1d19e-ff90-49c3-8833-42c913b5bbf8" />



Build a Sample App
- The Nucleo F446RE board identifier in Zephyr is nucleo_f446re
```
cd ~/zephyrproject/zephyr
west build -p always -b nucleo_f446re samples/basic/blinky
```
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/17b658c3-b9b2-46b5-ad6c-de9ce74c9659" />
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/d8f19a5c-e7c7-44ef-9f65-4fb23d715cb3" />


Flash the Board
Connect the Nucleo board via its onboard ST-Link USB port, then run
```
west flash  --runner openocd
```
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/de478100-0052-4594-9177-21868ba79459" />


Troubleshooting wesh flash failed issue
Check USB permissions (Linux). Add a udev rule so your user can access the ST-Link without sudo
```
sudo cp ~/zephyrproject/zephyr/scripts/openocd.udev /etc/udev/rules.d/60-openocd.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```
Monitor Serial Output
```
minicom -D /dev/ttyACM0 -b 115200
```
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/2610e38d-b774-4cf2-9c7a-a76e14d8ac0b" />


or use screen
```
screen /dev/ttyACM0 115200
```

Note
<pre>
- On macOS the device path is typically /dev/tty.usbmodem*
- On Windows, check Device Manager for the COM port number and use PuTTY or a similar terminal
</pre>


Under the ~/zephyrproject/zephyr, the samples/ directory has dozens of examples covering threads, timers, GPIO, UART, I2C, SPI, and more. Run ls samples/ to browse what's available.
<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/61379ca5-e9d9-4711-bbb3-e1db4e3cf698" />

---

## Info - BeagleBone Black Rev D
<pre>
- The BeagleBone Black (BBB) Rev D is a low-cost, open-source single-board computer 
  made by BeagleBoard.org Foundation
- It runs Linux and targets embedded systems developers, hobbyists, and students
- Core specifications
  - Processor
    - Texas Instruments AM3358 Sitara
    - Single ARM Cortex A8 at 1 GHz
    - Has got 2 Programmable Runtime Unit (PRU) coprocessors 
      - gives you deterministic, real-time I/O without an RTOS
  - Memory: 512 MB DDR3L RAM
  - Storage: 4 GB onboard eMMC flash (Rev C had 4 GB too, but Rev D ships with Debian pre-installed on it)
  - You can also boot from a microSD card slot
  - Video: micro-HDMI output, up to 1280×1024 resolution
    - There's also an LCD header for connecting custom display
  - Networking: 10/100 Ethernet (RJ45)
  - USB: One USB 2.0 host port (Type-A) and one mini-USB client port (used for power, serial debug, and network-over-USB)
  - Power: 5V DC via the barrel jack (recommended) or the mini-USB port. Draws roughly 210-460 mA depending on load
  - GPIO and Expansion
    - The board breaks out two 46-pin expansion headers (P8 and P9), giving you access to:
      - 65 digital GPIO pins
      - 7 analog inputs (1.8V max, 12-bit ADC)
      - 4 hardware timers
      - 4 UARTs
      - 2 SPI buses
      - 2 I2C buses
      - 8 PWM outputs
      - 2 CAN bus interfaces
  - PRU Coprocessors
    - This is what sets the BBB apart from a Raspberry Pi
    - The two PRU-ICSS cores run independently of the main ARM core and Linux kernel
    - Each PRU cycle is 5 ns (200 MHz), and they have direct access to GPIO pins
    - You can bit-bang protocols, drive stepper motors, or handle precise timing tasks that Linux 
      alone cannot guarantee. 
    - You program them in C (using TI's PRU compiler) or PRU assembly
  - What Rev D Changed from Rev C
    - Rev D is a relatively minor revision. The main differences:
      - Updated power management circuitry for better stability
      - Debian Linux pre-loaded on the eMMC (Rev C initially shipped with Angstrom, later switched to Debian)
      - Minor PCB layout tweaks
      - In practice, Rev C and Rev D are functionally identical for most use cases
- Software
  - The BBB runs mainline Linux with good upstream kernel support 
  - The default OS is Debian. You can also run Ubuntu, Buildroot, Yocto, Android, or FreeRTOS (on the PRUs) 
    Device tree overlays control pin muxing and cape configuration
  - The board supports Cloud9 IDE out of the box: connect via USB, open a browser to 192.168.7.2, 
    and you get a web-based IDE with BoneScript (a Node.js library that wraps GPIO access in an Arduino-like API)
- Common Use Cases
  - Embedded Linux development and prototyping
  - Industrial automation (CAN bus + PRU make it a strong fit)
  - Robotics (real-time motor control via PRUs)
  - IoT gateways
  - Teaching embedded systems and device driver development
  - Home automation
- Practical Considerations
  - Advantages over Raspberry Pi
    - PRU coprocessors, more GPIO pins, built-in ADC, CAN bus support, better 
      suited for real-time and industrial tasks
  - Disadvantages: 
    - Slower CPU than modern Pi boards, no built-in Wi-Fi/Bluetooth, smaller community 
      and fewer ready-made tutorials, 512 MB RAM limits what you can run comfortably
- How the ARM Core and PRU Communicate
  - They share a portion of RAM (12 KB shared + access to main DDR3). 
    The typical pattern is:
    - The ARM core (Linux) loads firmware onto the PRU using the remoteproc framework
    - The PRU runs its task, reading/writing shared memory or raising interrupts
    - The ARM core reads results from shared memory, or gets notified via interrupt
    - Linux provides the /dev/rpmsg_pruX character device for message passing between the ARM core and PRUs
    - You can also map shared memory directly from a Linux userspace application
</pre>


## Lab 1 — Board bring-up
Objective: reach the serial console, log in, and read the board's identity from /proc and /sys.

```
# on the HOST: connect FTDI (GND, TX->board RX, RX<-board TX), then open the console
ls /dev/ttyUSB*
picocom -b 115200 /dev/ttyUSB0
# exit with Ctrl-A Ctrl-X
# on the BOARD, after login:
uname -a
cat /proc/cpuinfo
cat /proc/iomem | head
ls /sys/class/gpio /sys/class/leds
```
Expected: a login prompt at 115200 8N1; cpuinfo shows AM33xx; iomem lists the SoC regions.

## Lab 2 — Toggle an LED two ways
Objective: see the difference between an abstraction and the bare hardware.

<img width="960" height="600" alt="image" src="https://github.com/user-attachments/assets/36b67573-1f36-4c45-9d64-21f36e3ff912" />
Safety: always a series resistor; the GPIO is 3.3 V. LED long leg (anode) to P9_12, short leg (cathode)
toward the resistor and GND.

Through sysfs, using an on-board user LED:
```
ls /sys/class/leds/
LED=/sys/class/leds/beaglebone:green:usr0
echo none | sudo tee $LED/trigger
# take manual control
echo 1
| sudo tee $LED/brightness
# on
echo 0
| sudo tee $LED/brightness
# off
```

Through the GPIO / register level, driving an external LED on P9_12 (with a series resistor to P9_1 GND):
```
# confirm which chip/line P9_12 is on YOUR board:
gpioinfo | grep -n P9_12
# e.g. gpiochip0 line 28
gpioset --by-name P9_12=1
# LED on
gpioset --by-name P9_12=0
# LED off
# register-level alternative (bare hardware): devmem2 on the GPIO data register
```

