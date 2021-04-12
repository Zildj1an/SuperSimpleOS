# Super Simple OS

In this project we implement an operating system with its main abstractions from scratch:

- A basic UEFI boot loader (boot.c) that loads the kernel and allocates memory for its structures (stacks, etc.) before exiting boot services. 

- A 4-level (4 KB Long Mode) page table structure for the kernel or user-space applications.

- User-space related stuff: System calls, including one for printing on screen, virtual addresses, Task State Segment, Thread Local Storage, etc. 

- Handling of exceptions and interrupts (IDT) and APIC timer.

- Support for the Xen hypervisor, with PV clocks and shared memory between guests, or Qemu.

## Instructions for launching a HVM guest

Tested with Ubuntu 20.04 and Xen 4.11. After installing with aptitude xtightvncviewer and copying the file code-hvm.cfg to /etc/xen/, you can do:

```
$ sudo xl create /etc/xen/code-hvm.cfg

$ sudo vncviewer localhost:0

$ sudo xl list

$ sudo xl destroy <domid>
```

## Contributors

- Carlos Bilbao ([GitHub](https://github.com/Zildj1an/)), contact: bilbao [at] vt.edu

- Ashwin Krishnakumar
