AS = nasm
AS = nasm
CC = i686-elf-gcc
LD = i686-elf-ld
OBJCOPY = i686-elf-objcopy
GRUB_MKRESCUE = grub-mkrescue

BOOTLOADER_SRC = src/boot/boot.s
BOOTLOADER_BIN = boot.bin

KERNEL_SRC = src/boot/kernel.c
KERNEL_OBJ = kernel.o
KERNEL_ELF = kernel.elf
KERNEL_BIN = kernel.bin

LINKER_SCRIPT = linker.ld
GRUB_CFG = grub.cfg

BUILD_DIR = build
ISO_BOOT_DIR = $(BUILD_DIR)/boot
ISO_GRUB_DIR = $(ISO_BOOT_DIR)/grub
BUILD_IMAGE = output.iso

ASFLAGS = -f bin
CCFLAGS = -ffreestanding -m32 -c -O2 -Wall -Wextra
LDFLAGS = -T $(LINKER_SCRIPT) -m elf_i386 -nostdlib

.PHONY: all clean prepare

all: $(BUILD_IMAGE)

$(BOOTLOADER_BIN): $(BOOTLOADER_SRC)
	$(AS) $(ASFLAGS) $< -o $@

$(KERNEL_OBJ): $(KERNEL_SRC)
	$(CC) $(CCFLAGS) -o $@ -c $<

$(KERNEL_ELF): $(KERNEL_OBJ) $(LINKER_SCRIPT)
	$(LD) $(LDFLAGS) -o $@ $<

$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@

prepare: $(BOOTLOADER_BIN) $(KERNEL_BIN) $(GRUB_CFG)
	wsl mkdir -p $(ISO_GRUB_DIR)
	wsl cp $(BOOTLOADER_BIN) $(ISO_BOOT_DIR)/
	wsl cp $(KERNEL_BIN) $(ISO_BOOT_DIR)/
	wsl cp $(GRUB_CFG) $(ISO_GRUB_DIR)/

$(BUILD_IMAGE): prepare
	wsl grub-mkrescue -o $@ $(BUILD_DIR)

clean:
	rm -rf $(BOOTLOADER_BIN) $(KERNEL_OBJ) $(KERNEL_ELF) $(KERNEL_BIN) $(BUILD_IMAGE) $(BUILD_DIR)

