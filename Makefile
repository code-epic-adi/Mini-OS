ISO=build/os.iso
CC=gcc
LD=ld
AS=nasm
CFLAGS=-m32 -ffreestanding -fno-stack-protector -fno-pic -fno-pie -O2 -Wall -Wextra
LDFLAGS=-melf_i386

OBJS = build/boot.o build/kernel.o build/vga.o build/kbd.o build/irq.o build/kalloc.o build/rtc.o build/paging.o build/task.o


all: $(ISO)

build:
	mkdir -p build

build/boot.o: src/boot.s | build
	$(AS) -f elf32 src/boot.s -o $@

build/kernel.o: src/kernel.c | build
	$(CC) $(CFLAGS) -c src/kernel.c -o $@

build/vga.o: src/vga.c | build
	$(CC) $(CFLAGS) -c src/vga.c -o $@

build/kbd.o: src/kbd.c | build
	$(CC) $(CFLAGS) -c src/kbd.c -o $@

build/kernel.elf: $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -T linker.ld -o $@ $(OBJS)

build/irq.o: src/irq.c | build
	$(CC) $(CFLAGS) -c src/irq.c -o $@

build/kalloc.o: src/kalloc.c | build
	$(CC) $(CFLAGS) -c src/kalloc.c -o $@

build/rtc.o: src/rtc.c | build
	$(CC) $(CFLAGS) -c src/rtc.c -o $@

build/paging.o: src/paging.c | build
	$(CC) $(CFLAGS) -c src/paging.c -o $@

build/task.o: src/task.c | build
	$(CC) $(CFLAGS) -c src/task.c -o $@

$(ISO): build/kernel.elf grub/grub.cfg
	mkdir -p build/isodir/boot/grub
	cp build/kernel.elf build/isodir/boot/kernel.elf
	cp grub/grub.cfg build/isodir/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) build/isodir >/dev/null 2>&1

run: all
	qemu-system-i386 -cdrom $(ISO)

clean:
	rm -rf build
