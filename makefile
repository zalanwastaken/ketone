.PHONY: all run debug clean clean-all

CC = gcc
LD = gcc
AS = nasm

CFLAGS = -m64 -ffreestanding -pedantic -msse2 -mmmx -msse -fno-pie -fno-pic -fno-stack-protector -c
LDFLAGS = -m64 -ffreestanding -fno-pie -fno-pic -no-pie -nostdlib -fno-stack-protector -Wl,--oformat=binary -T link.ld
ASFLAGS = -f elf

# ---- sources ----
SRC_C   := $(shell find src/os/kernel -name "*.c")
SRC_ASM := $(shell find src/os/kernel -name "*.asm")

# ---- objects ----
OBJ_C   := $(patsubst src/%.c, build/%.o, $(SRC_C))
OBJ_ASM := $(patsubst src/%.asm, build/%.o, $(SRC_ASM))
OBJS    := $(OBJ_C) $(OBJ_ASM)

# ---- fs ----
ROOTFS_FILES := $(shell find rootfs -type f)

# ---- main target ----
all: build/kernel.bin build/os.img

# ---- compile C ----
build/%.o: src/%.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $< -o $@
	@echo "CC $<"

# ---- compile ASM ----
build/%.o: src/%.asm
	@mkdir -p $(dir $@)
	@$(AS) $(ASFLAGS) $< -o $@
	@echo "AS $<"

# ---- link kernel ----
build/kernel.bin: $(OBJS)
	@$(LD) $(LDFLAGS) -o $@ $(OBJS)
	@echo "LD $(OBJS)"

# ---- bootloader ----
build/bootloader.bin: src/os/bootloader/bootloader.asm build/kernel.bin
	@mkdir -p build
	$(eval KSIZE := $(shell stat -c%s build/kernel.bin))
	$(eval KSECTORS := $(shell echo $$(( ($(KSIZE) + 511) / 512 )) ))
	@$(AS) -DSECTORS=$(KSECTORS) -f bin $< -o $@
	@echo "AS $< (kernel size=$(KSIZE) bytes, $(KSECTORS) sectors)"

build/bootloader_s2.bin: src/os/bootloader/bootloader_s2.asm build/kernel.bin
	@mkdir -p build
	$(eval KSIZE := $(shell stat -c%s build/kernel.bin))
	$(eval KSECTORS := $(shell echo $$(( ($(KSIZE) + 511) / 512 )) ))
	@$(AS) -DSECTORS=$(KSECTORS) -f bin $< -o $@
	@echo "AS $< (kernel size=$(KSIZE) bytes, $(KSECTORS) sectors)"

# ---- fs ----
build/exfat.img: $(ROOTFS_FILES)
	@mkdir -p build
	$(eval KSIZE := $(shell stat -c%s build/kernel.bin))
	$(eval KSECTORS := $(shell echo $$(( ($(KSIZE) + 511) / 512 )) ))
	$(eval PARTITION_START := $(shell echo $$(( 8 + $(KSECTORS) )) ))
	$(eval PARTITION_SECTORS := $(shell echo $$(( 524288 - $(PARTITION_START) )) ))
	@truncate -s $$(( $(PARTITION_SECTORS) * 512 )) $@
	@echo "FS $@"
	@mkfs.exfat -F $@

	@LOOPDEV=$$(udisksctl loop-setup --file $@ --no-user-interaction | sed -n 's/.* as \(.*\)\./\1/p'); \
	MOUNTPOINT=$$(udisksctl mount -b "$$LOOPDEV" --no-user-interaction | sed -n 's/.* at \(.*\)/\1/p'); \
	cp -r rootfs/. "$$MOUNTPOINT/"; \
	udisksctl unmount -b "$$LOOPDEV" --no-user-interaction; \
	udisksctl loop-delete -b "$$LOOPDEV" --no-user-interaction

# ---- disk image ----
build/os.img: build/bootloader.bin build/bootloader_s2.bin build/kernel.bin build/exfat.img
	dd if=/dev/zero of=$@ bs=512 count=524288
	dd if=build/bootloader.bin of=$@ conv=notrunc
	dd if=build/bootloader_s2.bin of=$@ bs=512 seek=1 conv=notrunc
	dd if=build/kernel.bin of=$@ bs=512 seek=8 conv=notrunc
	$(eval KSIZE := $(shell stat -c%s build/kernel.bin))
	$(eval KSECTORS := $(shell echo $$(( ($(KSIZE) + 511) / 512 )) ))
	$(eval PARTITION_START := $(shell echo $$(( 8 + $(KSECTORS) )) ))
	$(eval PARTITION_SECTORS := $(shell echo $$(( 524288 - $(PARTITION_START) )) ))
	dd if=build/exfat.img of=$@ bs=512 seek=$(PARTITION_START) conv=notrunc

# ---- run ----
run: all
	qemu-system-x86_64 -drive format=raw,file=build/os.img,if=ide -m 4G -serial stdio -audiodev alsa,id=snd0 -machine pcspk-audiodev=snd0 -vga qxl -machine pc

# ---- debug ----
debug: all
	qemu-system-x86_64 -drive format=raw,file=build/os.img,if=ide -m 4G -serial stdio -audiodev alsa,id=snd0 -machine pcspk-audiodev=snd0 -vga qxl -machine pc -s -S

# ---- clean ----
clean:
	rm -rf build
