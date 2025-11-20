# Mini-OS

A minimal 32-bit x86 operating system demonstrating fundamental OS concepts including cooperative multitasking, memory management, and interrupt handling. Built from scratch using C and Assembly, booted via GRUB2 on QEMU.

## Features

### Core Systems
- **Multiboot2 boot** via GRUB with memory map parsing
- **VGA text driver** with colors and formatting
- **Interrupt handling** with PIC remapping and PIT timer (100 Hz)
- **Memory management** including bump allocator and 32 MiB identity-mapped paging
- **CMOS RTC** for system time reading

### Multitasking & Scheduling
- **Cooperative multitasking** with round-robin scheduling
- **Timer-driven scheduling hints** (soft preemption) for time-slicing behavior
- **Per-task runtime accounting** tracking CPU ticks and utilization
- **Shell as a kernel task** participating in the scheduler
- **Dynamic task creation** at runtime via shell commands

### User Interface
- **Interactive shell** with command history and recall (`!!`)
- **Keyboard driver** with modifier key support (Shift)
- **Task management commands** for creating, listing, and monitoring tasks
- **Quiet/verbose modes** to control background task output

### Integration
- **QEMU poweroff** integration for clean exits
- **Debug checkpoints** for boot sequence verification

## Build & Run

### Prerequisites (Ubuntu 24.04)
```bash
sudo apt update
sudo apt install build-essential gcc-multilib nasm grub-pc-bin xorriso qemu-system-i386
```

### Build
```bash
make clean && make
```

### Run
```bash
make run
```

Or manually:
```bash
qemu-system-i386 -rtc base=localtime -cdrom build/os.iso -boot d -display gtk,gl=off \
  -no-reboot -no-shutdown -device isa-debug-exit,iobase=0xf4,iosize=0x04
```

## Architecture

```
.
├── Makefile
├── linker.ld
├── grub/
│   └── grub.cfg
├── src/
│   ├── boot.s          # Multiboot header and entry point
│   ├── kernel.c        # Shell, command dispatcher, main loop
│   ├── vga.c           # VGA text mode driver
│   ├── kbd.c           # Keyboard driver with scheduler cooperation
│   ├── irq.c           # Interrupt handling, PIC, PIT, IDT
│   ├── task.c/.h       # Task control blocks, scheduling, context switching
│   ├── kalloc.c/.h     # Heap allocator (bump allocator)
│   ├── paging.c/.h     # Page directory/tables, 32 MiB identity map
│   ├── rtc.c/.h        # CMOS RTC interface
│   └── ...
└── build/              # Generated artifacts
```

### Boot Sequence
1. GRUB loads `kernel.elf` using Multiboot2
2. `boot.s` initializes stack and calls `kernel_main`
3. Kernel initializes VGA, interrupts, timer, keyboard, heap, and paging
4. Shell task is created and scheduler begins execution

### Scheduling Model
- **Cooperative**: Tasks voluntarily yield CPU via `task_yield()`
- **Timer hints**: PIT timer sets `need_resched` flag every 10ms
- **Scheduler hook**: `scheduler_maybe_yield()` checks flag and yields if set
- **Keyboard integration**: Shell waits cooperatively, allowing background tasks to run

## Shell Commands

### System Information
- `mem` — Display usable RAM summary
- `memmap` — Print full Multiboot memory map
- `heap` / `kmstat` — Show heap statistics
- `time` — Display RTC time/date
- `cpuid` — Show CPU information
- `uptime` — Display system uptime

### Memory Management
- `alloc <n>` — Allocate `n` bytes from kernel heap

### Task Management
- `taskrun` — Create a test kernel task
- `tasks` — List all tasks with IDs and states
- `tstat` — Show per-task tick counts and CPU utilization
- `tquiet` — Mute background task output
- `tverbose` — Enable background task output

### Utilities
- `echo <text>` — Print text to console
- `clear` — Clear screen
- `history` — Display command history
- `!!` — Re-execute last command
- `help` — List available commands

### System Control
- `halt` — Halt CPU
- `reboot` — Reboot system
- `poweroff` — Exit QEMU cleanly

## Development Notes

### Current Limitations
- **Heap allocator**: Simple bump allocator without free/deallocation
- **Paging**: Fixed 32 MiB identity map (update `kalloc_init` and `paging.c` if extending)
- **Task lifecycle**: No task termination or cleanup yet
- **Scheduling**: Soft preemption only (no hard preemption in IRQ context)

### Debugging Tips
- Use `-serial stdio` with QEMU for kernel output
- Check `[dbg]` checkpoints in boot sequence if system hangs
- Ensure `isa-debug-exit` device is configured for clean poweroff

## Roadmap

### Potential Enhancements
1. **Hard preemption**: Full context switching in timer IRQ handler
2. **Priority scheduling**: Task priorities and weighted time slices
3. **Task lifecycle**: Implement `tkill` command and proper task cleanup
4. **Advanced scheduling**: RL-based or heuristic scheduler using runtime statistics
5. **Memory management**: Implement `kfree()` and proper heap management

## Contributing

Contributions should:
- Focus on one feature per PR
- Include clear commit messages
- Maintain code consistency with existing style
- Update relevant documentation

## License

Educational use only. Contact: aditya308989@gmail.com