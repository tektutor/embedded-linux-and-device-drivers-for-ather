# Day 4

## Info - General Purpose Operating System

## Info - Real Time Operating System (RTOS)

## Info - General Purpose Operating System Vs RTOS

## Info - MicroProcessor

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
