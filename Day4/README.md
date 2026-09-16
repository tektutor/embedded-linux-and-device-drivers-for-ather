# Day 4 — RTOS Concepts and Real-Time on the Board

> `# on the HOST` = your laptop (build/flash the Nucleo, build PRU firmware) · `# on the BOARD` = the BeagleBone (PRU, cyclictest).
> **Labs 13–14 use the STM32 Nucleo, not the BeagleBone.**

## Agenda for the day
**Morning (concepts)**
- Why an RTOS: determinism, bounded latency, why general-purpose Linux is not real time by default.
- Core mechanisms: tasks and priorities, preemptive scheduling, context-switch cost, semaphores, mutexes, queues, **priority inversion and inheritance**.
- Measuring real time: worst-case execution time, jitter, latency — measure, don't assume.

**Afternoon (labs)** — run tasks, break and fix priority inversion, and measure real time on the PRU and PREEMPT_RT.

## Labs at a glance
| Lab | About | Outcome |
|-----|-------|---------|
| **Lab 13 — Tasks and scheduling** | Two FreeRTOS tasks at different priorities share a queue. | You see preemptive scheduling in action. |
| **Lab 14 — Priority inversion** | Reproduce inversion, then fix it with priority inheritance. | You understand a classic real-time failure and its fix. |
| **Lab 15 — Real-time on the PRU** | A tight deterministic loop on the PRU vs a Linux thread. | You see hard determinism outside Linux. |
| **Lab 16 — PREEMPT_RT latency** | Measure scheduling latency under load with cyclictest. | You can measure latency, not guess it. |

**Day 4 outcome:** explain when an RTOS earns its place and measure latency rather than guess at it.

**The microcontroller for the FreeRTOS labs:** ![F446](figures/fig_f446.png)

---

## Morning — Full Session Content (~4 hours)
*Detailed teaching material. Timings in brackets. Cues: `[WB]` draw it, `[Q]` ask the room, `[DEMO]` show live. The five parts build one chain: **need → mechanism → hazard → proof.***

**Time map:** Framing 10 · Why an RTOS 50 · Tasks & scheduling 60 · Synchronization 55 · Priority inversion 45 · Measuring real time 40 · Wrap 10 (≈ 4 h with a break).

---

### 0. Framing (10 min)
The morning answers one question: **when is "usually fast" not good enough, and what do you use instead?**

Contrast two systems that both react to an event in software. A doorbell chime that plays 30 ms late is fine; nobody notices. An airbag that fires 30 ms late is a catastrophe. Same shape of problem, opposite tolerance for lateness. Embedded work is full of the second kind: a motor commutation that must switch on time or the motor stalls, a sensor that must be sampled every 1 ms or the control loop goes unstable, a safety cutoff that must act within a bounded window.

`[Q]` Go around the room: each person names one real deadline from their own work. Write them on the board and keep referring back, by the end of the morning they should be able to say, for each, whether it needs an RTOS, a PRU, or whether ordinary Linux is fine.

Lay out the map for the morning: **why an RTOS → how scheduling works → how tasks talk safely → the classic failure (priority inversion) → how you prove timing with measurement.** The afternoon labs map one-to-one onto the last three.

---

### 1. Why an RTOS — determinism and bounded latency (50 min)

**"Real time" does not mean "fast."** This is the single most important idea of the day and the most common misconception. Real time means *the system provably meets its deadline every time, including the worst case*. A slow system with a guaranteed ceiling is real time; a blazing-fast system that occasionally stalls is not.

`[WB]` Draw two response-time distributions. The first is centred low (fast average) but has a long tail stretching far to the right, once in a while it is very late. The second is centred higher (slower average) but stops abruptly at a hard ceiling, it is never later than X. For a deadline at X, the second system is correct and the first is broken, even though the first "feels" faster. **The tail, not the average, decides.**

**Hard, firm, and soft real time.** Classify by what a missed deadline costs:
- *Hard*: a miss is a system failure. Airbag, motor commutation, flight control. The deadline is a correctness requirement.
- *Firm*: a late result is useless but not catastrophic, you discard it. A video frame that arrives after its display slot.
- *Soft*: lateness degrades quality but the result still has value. A UI that stutters, a log that arrives late.
Most products mix all three. `[Q]` Have the room bucket a few of their section-0 deadlines into hard/firm/soft.

**Why general-purpose Linux is not real time by default.** Tie this straight back to Day 1 and the PRU. Linux is engineered for *throughput and fairness across many jobs*, not for the worst-case latency of any one job. Concretely, these introduce delays that are hard to bound on stock Linux:
- **The scheduler** (CFS) balances fairness and throughput; it does not guarantee that your important thread runs within a fixed time.
- **Interrupt handling**: a burst of interrupts (network, disk) can delay your thread.
- **Kernel critical sections**: when the kernel disables preemption or holds a lock, your high-priority thread waits, and on stock kernels those sections are not tightly bounded.
- **Memory**: the MMU, page faults, and TLB/cache misses add variable delay. A page fault can cost thousands of cycles.
- **DMA and bus contention**: other masters moving data can stall yours.

`[Q]` "Which of these exist on the PRU? On the Cortex-M4 in the Nucleo?" Walk it out: the PRU has no OS scheduler stealing time and no MMU; the Cortex-M has no MMU and runs your RTOS directly. That absence of unpredictable machinery is *exactly* why they give determinism. This is the reason the book splits work: Linux on the BeagleBone runs the connected application; the PRU or an RTOS on a microcontroller handles the hard-timing piece.

**What an RTOS actually gives you.** A small, analyzable scheduler where *you* assign priorities and the highest-priority ready task always runs; a bounded, known context-switch cost; and synchronization primitives whose timing you can reason about. You trade away the rich OS, no filesystem, no process model, no networking stack for free, in exchange for predictability. That trade is the whole point.

**Where an RTOS sits.** `[WB]` A spectrum: bare metal (smallest, most predictable, hardest to scale) → RTOS (tasks, priorities, bounded latency, still small) → full Linux (huge capability, weak timing guarantees). Day 4 lives in the middle and at the extreme-determinism end (the PRU).

**Check:** *"A system can be very fast on average and still not be real time, why?"* (Because a single worst-case cycle can miss the deadline; the guarantee is about the ceiling, not the mean.)

---

### 2. Tasks and the scheduler (60 min)

**A task is an independent function with its own stack that runs forever.** In FreeRTOS a task is a C function shaped like:
```c
void my_task(void *arg) {
    /* one-time setup */
    for (;;) {
        /* do work, then block (delay or wait on something) */
    }
}
```
It must never `return`, a returning task is a bug (you either loop forever or delete the task explicitly). Contrast this with the bare-metal **super-loop** (`while(1){ do_a(); do_b(); do_c(); }`). `[WB]` Put the super-loop next to three tasks. The super-loop is simple but every job's timing depends on every other job, add a slow `do_d()` and everything downstream jitters. Tasks decouple that: each has its own priority and its own stack.

**Each task has its own stack.** That is what makes tasks independent, local variables, call frames, and the saved context all live on that task's stack. When you call `xTaskCreate(..., stackDepth, ...)` the number you pass is that stack's size (in *words*, not bytes, on most ports). Undersize it and the task overflows its stack into memory it does not own, one of the most common and most confusing RTOS bugs. Right-size it by measuring high-water mark (`uxTaskGetStackHighWaterMark`) rather than guessing.

**Task states.** `[WB]` Draw the state machine and walk one task around it:
- **Running** — currently on the CPU (only one task per core at a time).
- **Ready** — able to run, waiting only because something higher-priority is running.
- **Blocked** — waiting for an event or a timeout (a delay, a queue item, a semaphore). Uses *no CPU*.
- **Suspended** — explicitly parked with `vTaskSuspend`, ignored by the scheduler until resumed.
Trace it: task calls `vTaskDelay` → **Blocked**; the tick expires → **Ready**; the scheduler picks it → **Running**; it waits on an empty queue → **Blocked** again.

**Priority-based preemptive scheduling.** The rule is one sentence: *the highest-priority task that is Ready is the one that runs.* In FreeRTOS a **higher number means higher priority** (0 is lowest, `configMAX_PRIORITIES-1` is highest). "Preemptive" means the switch happens *immediately*: the moment a higher-priority task becomes Ready (say, its delay expires or the ISR it was waiting on fires), it takes the CPU from whatever lower-priority task was running, mid-function, at the next instruction boundary. The lower task doesn't get to finish first.

`[Q]` "If two Ready tasks have the *same* priority, what happens?" → FreeRTOS time-slices them round-robin on each tick (if `configUSE_TIME_SLICING` is on). Equal priority = fair sharing; unequal = strict preemption.

**The tick.** A periodic timer interrupt, the **tick**, drives time in the RTOS. Its rate is `configTICK_RATE_HZ` in `FreeRTOSConfig.h` (commonly 1000 Hz = one tick per millisecond). On every tick the kernel checks whether any delayed task's timeout has expired (move it to Ready) and, with time-slicing, whether to rotate equal-priority tasks. This is why delays are quantized to the tick: a `vTaskDelay` of "1 ms" at a 1000 Hz tick is one tick; ask for finer than a tick and you can't get it from `vTaskDelay`. `pdMS_TO_TICKS(ms)` converts milliseconds to ticks against this exact rate, which is why the rate must be set correctly for delays to mean what you think.

**Context switch — what it costs.** `[WB]` When the scheduler switches from task A to task B it must **save A's context** (the CPU registers, the stack pointer, the program counter) onto A's stack and **restore B's context** from B's stack. On a Cortex-M this is a few dozen instructions, fast, but *not free*, and, crucially for real time, *bounded and known*. That boundedness is the point: you can account for switch cost in your timing budget. `[Q]` "If you design tasks that each do a microsecond of work then yield, what dominates, useful work or switching?" (Switching, you'd be paying overhead to accomplish almost nothing. Granularity matters.)

**Blocking is the entire trick.** This is the idea that makes multitasking work. A task that calls `vTaskDelay(pdMS_TO_TICKS(500))` or waits on a queue goes **Blocked** and consumes *no CPU* for that whole period, the scheduler runs other Ready tasks instead. Compare with a **busy-wait**:
```c
/* BUSY-WAIT — bad: burns the CPU, starves lower-priority tasks */
volatile uint32_t i; for (i = 0; i < 1000000; i++) { }

/* BLOCKING DELAY — good: yields the CPU for the duration */
vTaskDelay(pdMS_TO_TICKS(500));
```
The busy-wait keeps the task **Running**, so nothing of lower priority can run for that whole time. The blocking delay frees the CPU. `[DEMO]/[WB]` Preview Lab 13's producer/consumer: the producer is higher priority, but it sends one value and then *blocks* for 500 ms, and it is *only because it blocks* that the lower-priority consumer ever gets the CPU to print. If the producer busy-waited instead, the consumer would never run.

**Starvation.** The flip side: a high-priority task that *never blocks* starves everything below it, the lower tasks are always Ready but never highest, so they never run. Name this now; it comes straight back in priority inversion. A well-behaved high-priority task does a little work and then blocks (on a delay, an event, a queue), leaving room below.

**Check:** *"Why does a lower-priority task ever get to run at all?"* (Only because the higher-priority tasks block, on delays or waits, yielding the CPU.)

---

### 3. Synchronization primitives — semaphores, mutexes, queues (55 min)

**The problem first: races.** Two tasks touching the same data without coordination corrupt it. `[WB]` Show a shared `count++` on a 32-bit value that takes read-modify-write: Task A reads `count`, gets preempted by Task B which also reads the old value, both increment and write back, one increment is lost. On wider data or structs it's worse, a reader can see a half-updated value. The primitives below exist to make these interactions safe.

**Queue — move data between tasks.** A queue is a fixed-length FIFO that stores **copies** of items. `xQueueSend` copies your item in; `xQueueReceive` copies one out. It also *synchronizes*: a receiver on an empty queue **blocks** until an item arrives; a sender to a full queue blocks until space frees. Two properties matter:
- It copies, it does not share a pointer, so the sender can reuse or change its variable immediately after sending. (You *can* queue pointers deliberately, but then you're back to managing shared memory.)
- Blocking receive is the clean producer/consumer pattern: no polling, no busy-wait.
Use a queue as your default for "one task produces work, another consumes it."

**Binary semaphore — signal an event.** Think of it as a one-token flag: a task **takes** it (and blocks if it's not available); another task, *or an interrupt*, **gives** it to signal "the thing happened." The classic use is **ISR-to-task handoff**: the interrupt does the minimum in the handler and `xSemaphoreGiveFromISR` to wake a task that does the real work at task level (deferred processing / "bottom half," the same top-half/bottom-half idea from Day 3, in RTOS form). `[Q]` "Why not do all the work in the ISR?" (Long ISRs block other interrupts and wreck latency, keep them short, defer.)

**Counting semaphore — count available units.** A semaphore initialized to N, where each take consumes one and each give returns one. Models a pool: N free buffers, N slots in a resource. Take blocks when the count hits zero.

**Mutex — protect a shared resource.** A mutex enforces mutual exclusion around a critical section: take it before touching the resource, give it after. Two things make a mutex different from a binary semaphore, and Lab 14 turns on exactly these:
- **Ownership**: the task that takes a mutex is its owner and is the one that must give it back. (A binary semaphore has no owner, anyone can give it.)
- **Priority inheritance**: because the mutex knows its owner, the RTOS can *temporarily raise the owner's priority* when a higher-priority task is waiting for it, the fix for the inversion problem in the next section. Binary semaphores cannot do this because they have no owner to boost.
`[WB]` Put mutex vs binary semaphore in a two-row table: *ownership* (yes/no), *priority inheritance* (yes/no), *typical use* (guard a resource / signal an event).

**The traps.** Name them explicitly, they are where real systems break:
- Forgetting to give the lock back → everything else waiting on it hangs.
- **Blocking while holding a lock** (e.g., calling a long delay or another blocking API inside a critical section) → you extend the time others are shut out, and can deadlock.
- **Deadlock**: two tasks each hold one lock and wait for the other's. `[WB]` A holds lock1 and wants lock2; B holds lock2 and wants lock1, neither ever proceeds. The defense is a discipline: *always take multiple locks in the same global order everywhere.*

**Rule of thumb to leave on the board:** *queue to move data, semaphore to signal an event, mutex to protect a resource.*

**Check:** *"Which primitive for an ISR telling a task 'a byte arrived'?"* (binary semaphore, give-from-ISR.) *"Which to guard a shared I2C bus two tasks both use?"* (mutex.)

---

### 4. Priority inversion and inheritance (45 min) — *sets up Lab 14 directly*

This is the set-piece of the morning: a failure that looks impossible ("my highest-priority task missed its deadline and there's no bug in it") and has a precise cause and a precise fix.

**The cast.** Three tasks: **Low**, **Medium**, **High** (priorities 1, 2, 3). Low and High share a resource guarded by a mutex. Medium needs the mutex not at all, it's just CPU-hungry work at middle priority.

**The failure, as a timeline.** `[WB]` Draw a horizontal time axis and step through it:
1. **Low** runs first (nothing else Ready) and **takes the mutex**, entering its critical section.
2. **High** wakes and tries to **take the mutex** → it's held by Low, so High **blocks**. Fine so far, High is *supposed* to wait briefly for Low to finish.
3. **Medium** wakes. Medium (priority 2) is higher than Low (priority 1) and needs no mutex, so it **preempts Low**. Low is frozen mid-critical-section and cannot release the mutex.
4. Now the trap is sprung: **High (priority 3) is effectively waiting on Medium (priority 2)** to finish, because Medium is starving Low, who holds the lock High needs. The highest-priority task in the system is blocked by a *lower*-priority one, for as long as Medium chooses to run. That is **priority inversion**: priorities have been turned upside down by the lock.

**Why it's dangerous, and a real example.** The delay is unbounded, if Medium keeps finding work, High may *never* run, and there is no obvious bug in any single task. The famous case is **Mars Pathfinder (1997)**: on the surface of Mars, the lander began resetting itself repeatedly. A high-priority bus-management task shared a mutex with a low-priority task; a medium-priority comms task preempted the low one at the wrong moment; the high task missed its deadline; a watchdog saw the miss and reset the system. It was diagnosed and *fixed remotely* by enabling priority inheritance. `[Q]` Ask the room what they'd even look for, the point is that the tasks are individually correct; the fault is emergent, from the interaction through the lock.

**The fix — priority inheritance.** While Low holds a mutex that a higher-priority task (High) is waiting for, the RTOS **temporarily boosts Low's priority up to High's** for the duration it holds the lock. `[WB]` Redraw the same timeline with inheritance on:
1. Low takes the mutex.
2. High tries, blocks, *and* Low is immediately boosted to priority 3.
3. Medium wakes, but Medium (2) can no longer preempt Low (now 3), so Low keeps running.
4. Low finishes its critical section quickly and **gives the mutex**; its priority drops back to 1; High immediately takes the mutex and runs. Deadline met.
The boost lasts only while the lock is held, exactly the window where it's needed.

**Why this is a mutex feature specifically.** Inheritance requires knowing *whose* priority to boost, that is, the lock's owner. A mutex tracks ownership; a binary semaphore does not. This is the concrete reason "use a mutex, not a binary semaphore, to protect a resource," and it's the one line you change in Lab 14.

**Hand-off to the lab:** *"This afternoon in Lab 14 you'll reproduce this exact inversion on the Nucleo with a binary semaphore, watch the high task miss, then switch one line to a mutex (`xSemaphoreCreateMutex`) and watch priority inheritance bring the deadline back."*

**Check:** *"In the broken version, who is actually blocking High, Low or Medium?"* (Medium, by starving Low, who holds the lock High needs. That indirection is the whole lesson.)

---

### 5. Measuring real time — WCET, jitter, latency (40 min) — *sets up the PRU + PREEMPT_RT labs*

**Measure, don't assume.** Everything claimed this morning, "bounded," "deterministic," "meets the deadline", is worthless as an assertion. It has to be a *number you measured*. This block gives the room the vocabulary and the methods, and sets up the afternoon's PRU and PREEMPT_RT labs, which are entirely about getting those numbers.

**The three quantities.** `[WB]` Sketch a timeline: an event happens, then some time later the response starts, then it runs for a while.
- **Latency** — the gap from *event* to *start of response*. "How long before my code even begins reacting?"
- **Jitter** — the *variation* in that latency from one cycle to the next. A control loop can often tolerate a constant delay but hates a delay that wanders, jitter is frequently the real enemy in motor control and signal sampling.
- **WCET (worst-case execution time)** — the longest a piece of code can take to *run*, across all inputs, all code paths, and all cache/pipeline states. You rarely get it exactly; you *bound* it (measure many runs under worst conditions, or analyze). Your deadline math uses the worst case, not the average.

**Why the worst case, not the average.** Restate the section-1 point with numbers. `[Q]` "Average latency is 20 µs, but one cycle in 10,000 is 1 ms, is this real time for a 100 µs deadline?" (No, that one late cycle is a missed deadline, and over hours of operation it *will* happen. The tail decides.)

**How to measure on real hardware.**
- **GPIO + scope/logic analyzer**: toggle a pin high at the start of your response and low at the end (or pulse it at the event and again at response-start). On a scope you can *see* the latency directly and, by watching many cycles, *see* the jitter as the edge "smears." This is the most honest measurement, it's the physical signal, and it's exactly why **Lab 15 puts a scope on the PRU's output pin**: a rock-steady square wave means low jitter; a smeared edge means the timing wanders.
- **`cyclictest`** (Linux, rt-tests): a standard tool that repeatedly sleeps for a set interval and measures how late it actually woke. It reports **min / avg / max** latency. The number that matters is **max under load**, run it *while the system is busy* (`stress-ng`), because the worst case shows up under load, not on an idle box. That's **Lab 16**.

**The comparison you'll draw this afternoon.** `[WB]` Same job, three platforms, three timing profiles:
- **PRU** — no scheduler, no OS, no MMU: the tightest, most deterministic timing available on this board. Best jitter.
- **PREEMPT_RT Linux** — the real-time patch reworks the kernel so most of it is preemptible and bounded: dramatically better worst-case latency than stock Linux, but still larger and less certain than the PRU (it's still a full OS).
- **Stock Linux** — fine on average, but with an unbounded tail, not suitable for hard deadlines.
The afternoon labs produce actual numbers for the second and third and a scope trace for the first, so the room *sees* the trade rather than taking it on faith.

**Check:** *"Your logic-analyzer trace shows the response edge jumping around by ±40 µs cycle to cycle, latency looks fine but which quantity is the problem, and for what kind of system does it matter most?"* (Jitter; it wrecks control loops and precise sampling even when average latency is acceptable.)

---

### Wrap the morning (10 min)
- Recap the chain in one breath: **you need bounded latency → the mechanism is a priority-based preemptive scheduler plus blocking → the hazard is priority inversion, fixed by inheritance → and none of it counts until you measure WCET, jitter, and latency.**
- Preview the four afternoon labs against exactly these ideas: **13** shows preemptive scheduling and blocking; **14** breaks and fixes priority inversion; **15** shows PRU determinism on a scope; **16** puts real latency numbers on PREEMPT_RT under load.
- `[Q]` Return to the deadlines the room listed in section 0. For each, decide together: RTOS, PRU, or is ordinary Linux fine? Make them justify it with the morning's vocabulary (hard vs soft, bounded vs tail, jitter tolerance).


## Hardware for Day 4
| Item | Used by |
|------|---------|
| STM32 Nucleo board + USB (ST-Link) cable | Lab 13, 14 |
| BeagleBone Black + FTDI serial cable | Lab 15, 16 |
| LED + resistor to watch the PRU loop; optional logic analyzer/scope | Lab 15 |

## Install (HOST)
- FreeRTOS/Zephyr toolchain for the Nucleo (`arm-none-eabi-gcc` + your SDK).
- PRU C compiler (TI PRU Code Generation Tools) for Lab 15.
- On the **BOARD** for Lab 16: `sudo apt install -y rt-tests stress-ng`.

> Full source under each `LabN-Name/src/`. The FreeRTOS/PRU code is **reference starter code** — confirm the API/build against your SDK.

---

## Lab 13 — Tasks and scheduling
**Objective:** two tasks at different priorities share a queue; the higher-priority task runs first when ready.
**Source:** [`Lab13-TasksScheduling/src/tasks.c`](Lab13-TasksScheduling/src/tasks.c)
```c
static void producer(void *arg) {          /* higher priority */
    int n = 0;
    for (;;) { xQueueSend(q, &n, portMAX_DELAY); n++; vTaskDelay(pdMS_TO_TICKS(500)); }
}
static void consumer(void *arg) {          /* lower priority */
    int v;
    for (;;) if (xQueueReceive(q, &v, portMAX_DELAY) == pdTRUE) printf("got %d\n", v);
}
int main(void) {
    q = xQueueCreate(8, sizeof(int));
    xTaskCreate(producer, "prod", 256, NULL, 3, NULL);   /* prio 3 */
    xTaskCreate(consumer, "cons", 256, NULL, 1, NULL);   /* prio 1 */
    vTaskStartScheduler(); for (;;);
}
```
**Run — on the HOST:** build with your FreeRTOS project and flash to the Nucleo over USB (ST-Link).
**Expected output:** the consumer prints an increasing sequence; the high-priority producer always runs when ready (visible in the log or on a scope).

---

## Lab 14 — Priority inversion
**Objective:** reproduce inversion (low holds a lock high needs while medium hogs the CPU), then fix it.
**Source:** [`Lab14-PriorityInversion/src/inversion.c`](Lab14-PriorityInversion/src/inversion.c)
```c
// Without inheritance, high waits for medium -> inversion.
// FIX: xSemaphoreCreateMutex() (a MUTEX) boosts 'low' to high's priority
//      while it holds the lock, so medium cannot delay high.
m = xSemaphoreCreateMutex();                         // fixed
// m = xSemaphoreCreateBinary(); xSemaphoreGive(m);   // to reproduce the inversion
```
**Run — on the HOST:** flash to the Nucleo; toggle the semaphore type to switch broken/fixed.
**Expected output:** with a binary semaphore the high task is delayed (inversion); with the mutex, priority inheritance lets it run on time.

---

## Lab 15 — Real-time on the PRU
**Objective:** a tight, deterministic loop on the PRU, independent of Linux; compare against a Linux thread.
**Source:** [`Lab15-PRURealTime/src/pru_blink.c`](Lab15-PRURealTime/src/pru_blink.c)
```c
volatile register uint32_t __R30;   /* PRU output register */
void main(void) {
    CT_CFG.SYSCFG_bit.STANDBY_INIT = 0;
    while (1) {
        __R30 |=  (1 << 5); __delay_cycles(100000000);   /* pin high, exact cycles */
        __R30 &= ~(1 << 5); __delay_cycles(100000000);   /* pin low */
    }
}
```
**Run — on the BOARD**
```bash
# build with the PRU compiler, then load and start via remoteproc:
echo am335x-pru0-fw | sudo tee /sys/class/remoteproc/remoteproc1/firmware
echo start          | sudo tee /sys/class/remoteproc/remoteproc1/state
```
**Expected output:** a scope on the PRU pin shows a rock-steady square wave; the same toggle from a Linux thread jitters under load.

---

## Lab 16 — PREEMPT_RT latency
**Objective:** measure scheduling latency under load and compare against the PRU.
**Run — on the BOARD (running a PREEMPT_RT kernel)**
```bash
sudo apt install -y rt-tests stress-ng
sudo cyclictest -m -p90 -i200 -h400 &      # the MAX latency is what counts
stress-ng --cpu 4 --io 2 --timeout 60s     # load it while measuring
```
**Expected output:** `cyclictest` reports a bounded worst-case latency far tighter than a stock kernel — but still larger and less certain than the PRU.
=======
# Day 4

## Info - General Purpose Operating System

## Info - Real Time Operating System (RTOS)

## Info - General Purpose Operating System Vs RTOS

## Info - MicroProcessor
<pre>
- Powerful Processor
- It can technically connect to any time of devices
- Input/Ouput devices are external to Processor
- It interfaces with powerful external graphics
- It interfaces with external Network 
  - supports Bluetooth, WiFi, LAN, etc.,
</pre>

## Info - Micro-controller

## Info - MicroProcessor vs Micro-controller

## Info - Synchronization Mechanism

## Info - Different types of Synchronization Mechanism
<pre>
- Mutex ( Mutually Exclusive )
- Semaphore
  - Binary Semaphore
  - Counting Semaphore
</pre>

## Info - Monolithic Kernel

## Info - Micro-kernel

## Info - Monolithic vs Micro-Kernel

## Info - Is FreeRTOS a Monolithic or Micro-kernel ?
<pre>
- FreeRTOS is neither
- FreeRTOS doesn't separate the userspace from Kernel Space
- Kernel and application runs in the unified namespace
- There is no memory protection
- Benefits
  - Easy to develop application as there is no user/kernel space seggregations
- Drawbacks
  - A badly written/designed application can corrupt the kernel and bring-down the whole OS 
</pre>

## Lab1 - Mutex
```
cd ~
git clone https://github.com/tektutor/embedded-linux-and-device-drivers-for-ather.git
cd embedded-linux-and-device-drivers-for-ather
git pull
cd Day4/FreeRTOS-Labs-STM32F446RE/Labs/Lab01-Mutex
make
make flash

# ON Terminal tab 1
minicom -D /dev/ttyACM0 -b 115200

# Manually press the reset button on the board
# At this point you see the board output on the Terminal tab 1
```
