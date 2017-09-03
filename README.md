# PintOS

## Synopsis
PintOS is an operating system framework for the 80x86 architecture. It supports kernel threads, loading and running user programs, and a file system, but it implements all of these in a simpler way compared to the most popular operating systems we are using such as Linux, Mac OSX and Windows.

## Files/Directories explained.
1. Starting with ‘pintos/src’:

- ‘devices/’ - Source code for I/O device interfacing: keyboard, timer, disk, etc. You will modify the timer implementation in task 0. Otherwise you should have no need to change

- ‘threads/’ - Source code for the base kernel, responsible for all multi threading in this operating system.

- ‘userprog/’ - Source code for the user program loader.

- ‘vm/’ - Code for the implementation of virtual memory. 

- ‘filesys/’ - Source code for a basic file system.

- ‘lib/’ - An implementation of a subset of the standard C library. The code in this directory is compiled into both the Pintos kernel and user programs that run under it. In both kernel code and user programs, headers in this directory can be included using the #include <...> notation. You should have little need to modify this code.

- ‘lib/kernel/’ - Parts of the C library that are included only in the Pintos kernel. This also includes implementations of some data types that you are free to use in your kernel code: bitmaps, doubly linked lists, and hash tables. In the kernel, headers in this directory can be included using the #include <...> notation.

- ‘lib/user/’ - Parts of the C library that are included only in Pintos user programs. In user programs, headers in this directory can be included using the #include <...> notation.

- ‘tests/’ - Tests for each different part of the code.

- ‘examples/’ - Examples of user programs.

- ‘misc/’, ‘utils/’ - These files may come in handy if you decide to try working with Pintos on your own machine. Otherwise, you can ignore them.

2. Secondly, moving into ‘pintos/build’ directory we have:

- ‘Makefile’ - A copy of ‘pintos/src/Makefile.build’. It describes how to build the kernel.

- ‘kernel.o’ - Object file for the entire kernel. This is the result of linking object files compiled from each individual kernel source file into a single object file. It contains debug information, so you can run GDB or backtrace on it.

- ‘kernel.bin’ - Memory image of the kernel, that is, the exact bytes loaded into memory to run the Pintos kernel. This is just ‘kernel.o’ with debug information stripped out, which saves a lot of space, which in turn keeps the kernel from bumping up against a 512 kB size limit imposed by the kernel loader’s design.
	
 - ‘loader.bin’ - Memory image for the kernel loader, a small chunk of code written in assembly language that reads the kernel from disk into memory and starts it up. It is exactly 512 bytes long, a size fixed by the PC BIOS.

3. Moving now into ‘pintos/devices’ directory, we have:

 - ‘timer.c’
 - ‘timer.h’ - System timer that ticks, by default, 100 times per second. You will modify this code
in this task.

 - ‘vga.c’
 - ‘vga.h’ <: VGA display driver. Responsible for writing text to the screen. You should have no need to look at this code. printf() calls into the VGA display driver for you, so there’s little reason to call this code yourself.

 - ‘serial.c’
 - ‘serial.h’ - Serial port driver. Again, printf() calls this code for you, so you don’t need to do so yourself. It handles serial input by passing it to the input layer (see below).

 - ‘block.c’
 - ‘block.h ’ - An abstraction layer for block devices, that is, random-access, disk-like devices that
are organized as arrays of fixed-size blocks. Out of the box, Pintos supports two types of block devices: IDE disks and partitions.

 - ‘ide.c’
 - ‘ide.h’ - Supports reading and writing sectors on up to 4 IDE disks.

 - ‘partition.c’ 
 - ‘partition.h’ - Understands the structure of partitions on disks, allowing a single disk to be carved up into multiple regions (partitions) for independent use.

 - ‘kbd.c’
 - ‘kbd.h’ <: Keyboard driver. Handles keystrokes passing them to the input layer (see below).

 - ‘input.c’
 - ‘input.h’ - Input layer. Queues input characters passed along by the keyboard or serial drivers.

 - ‘intq.c’
 - ‘intq.h’ - Interrupt queue, for managing a circular queue that both kernel threads and interrupt handlers want to access. Used by the keyboard and serial drivers.

 - ‘rtc.c’
 - ‘rtc.h’ - Real-time clock driver, to enable the kernel to determine the current date and time.  By default, this is only used by ‘thread/init.c’ to choose an initial seed for the random number generator.

 - ‘speaker.c’
 - ‘speaker.h’ - Driver that can produce tones on the PC speaker.

 - ‘pit.c’
 - ‘pit.h’ - Code to configure the 8254 Programmable Interrupt Timer. This code is used by both ‘devices/timer.c’ and ‘devices/speaker.c’ because each device uses one of the PIT’s output channel.


4. Moreover, we have ‘pintos/threads’ directory which contains:

 - ‘loader.S’
 - ‘loader.h’ - The kernel loader. Assembles to 512 bytes of code and data that the PC BIOS loads into memory and which in turn finds the kernel on disk, loads it into memory, and jumps to start() in ‘start.S’. 

 - ‘start.S’ - Does basic setup needed for memory protection and 32-bit operation on 80x86 CPUs. Unlike the loader, this code is actually part of the kernel.

 - ‘kernel.lds.S’ - The linker script used to link the kernel. Sets the load address of the kernel and ar- ranges for ‘start.S’ to be near the beginning of the kernel image.

 - ‘init.c’
 - ‘init.h’ - Kernel initialization, including main(), the kernel’s “main program.” You should look over main() at least to see what gets initialized.

 - ‘thread.c’
 - ‘thread.h’ - Basic thread support. ‘thread.h’ defines struct thread, which you are likely to modify in all four tasks.

 - ‘switch.S’
 - ‘switch.h’ - Basic thread support. ‘thread.h’ defines struct thread, which you are likely to modify in all four tasks.

 - ‘palloc.c’
 - ‘palloc.h’ - Page allocator, which hands out system memory in multiples of 4 kB pages.

 - ‘malloc.c’
 - ‘malloc.h’ - A simple implementation of malloc() and free() for the kernel.

 - ‘interrupt.c’
 - ‘interrupt.h’ - Basic interrupt handling and functions for turning interrupts on and off.

 - ‘intr-stubs.S’
 - ‘intr-stubs.h’ <: Assembly code for low-level interrupt handling.

 - ‘synch.c’
 - ‘synch.h’ - Basic synchronization primitives: semaphores, locks, condition variables, and opti-
mization barriers. 

 - ‘io.h’ - Functions for I/O port access. This is mostly used by source code in the ‘devices’ directory that you won’t have to touch.

 - ‘vaddr.h’
 - ‘pte.h’ - Functions and macros for working with virtual addresses and page table entries.  These will be more important to you in task 3. For now, you can ignore them ‘flags.h’ Macros that define a few bits in the 80x86 “flags” register. Probably of no interest. See [IA32-v1], section 3.4.3, “EFLAGS Register,” for more information.

5. Moving into ‘pintos/lib’ files:

 - ‘ctype.h’
 - ‘inttypes.h’
 - ‘limits.h’
 - ‘stdarg.h’
 - ‘stdbool.h’
 - ‘stddef.h’
 - ‘stdint.h’
 - ‘stdio.c’
 - ‘stdio.h’
 - ‘stdlib.c’
 - ‘stdlib.h’
 - ‘string.c’
 - ‘string.h’ - A subset of the standard C library. See Section C.2 [C99], page 82, for information on a few recently introduced pieces of the C library that you might not have encountered before.

 - ‘debug.c’
 - ‘debug.h’ - Functions and macros to aid debugging. See Appendix E [Debugging Tools], page 88, for more information.

 - ‘random.c’
 - ‘random.h’ - Pseudo-random number generator. The actual sequence of random values may vary from one Pintos run to another.

 - ‘round.h’ - Macros for rounding.

 - ‘syscall-nr.h’ - System call numbers.

 - ‘kernel/list.c’ 
 - ‘kernel/list.h’ - Doubly linked list implementation. Used all over the Pintos code, and you’ll prob- ably want to use it a few places yourself in task 0 and task 1.

 - ‘kernel/bitmap.c’ 
 - ‘kernel/bitmap.h’ - Bitmap implementation. 

 - ‘kernel/hash.c’ 
 - ‘kernel/hash.h’ - Hash table implementation.

 - ‘kernel/console.c’
 - ‘kernel/console.h’
 - ‘kernel/stdio.h’ - Implements printf() and a few other functions.


6. Moving into user programs directory we have:

 - ‘process.c’
 - ‘process.h’ - Loads ELF binaries and starts processes.

 - ‘pagedir.c’
 - ‘pagedir.h’ - A simple manager for 80x86 hardware page tables. Although you probably won’t want to modify this code for this task, you may want to call some of its functions. 

 - ‘syscall.c’
 - ‘syscall.h’ - Whenever a user process wants to access some kernel functionality, it invokes a system call. This is a skeleton system call handler. Currently, it just prints a message and terminates the user process. In part 2 of this task you will add code to do everything else needed by system calls.

 - ‘exception.c’ 
 - ‘exception.h’ - When a user process performs a privileged or prohibited operation, it traps into the kernel as an “exception” or “fault.”1 These files handle exceptions. Currently all exceptions simply print a message and terminate the process.

 - ‘gdt.c’
 - ‘gdt.h’ - The 80x86 is a segmented architecture. The Global Descriptor Table (GDT) is a table that describes the segments in use. These files set up the GDT. You should not need to modify these files for any of the tasks. You can read the code if you’re interested in how the GDT works.

 - ‘tss.c’
 - ‘tss.h’ - The Task-State Segment (TSS) is used for 80x86 architectural task switching. Pintos
uses the TSS only for switching stacks when a user process enters an interrupt handler, as does Linux.

## License
1. The copyright of this project belongs to Imperial College London.
2. From this code I removed some parts on purpose to make it broken.