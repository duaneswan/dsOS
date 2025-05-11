#!/bin/bash
# dsOS Build Script
# Main build script for compiling the complete operating system

set -e

# Directory setup
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$ROOT_DIR/build"
SYSROOT_DIR="$ROOT_DIR/sysroot"
ISO_ROOT="$ROOT_DIR/iso_root"

# Compiler settings
export CFLAGS_DEBUG="-Og -g -Wall -Wextra"
export CFLAGS_RELEASE="-O2 -Wall -Wextra"
export CFLAGS="$CFLAGS_RELEASE"
export LDFLAGS="-T $ROOT_DIR/kernel/arch/x86_64/linker.ld"

# Determine if we're building in debug mode
if [ "$1" = "debug" ]; then
    export CFLAGS="$CFLAGS_DEBUG"
    echo "Building in DEBUG mode"
else
    echo "Building in RELEASE mode"
fi

# Create necessary directories
mkdir -p "$BUILD_DIR"
mkdir -p "$SYSROOT_DIR/System/bin"
mkdir -p "$ISO_ROOT"

# Build stages
build_bootloader() {
    echo "Building bootloader (dsBoot)..."
    # Stage 1 (MBR)
    nasm -f bin "$ROOT_DIR/boot/stage1/mbr.asm" -o "$BUILD_DIR/mbr.bin"
    
    # Stage 2
    nasm -f bin "$ROOT_DIR/boot/stage2/stage2.asm" -o "$BUILD_DIR/stage2.bin"
    
    # Create the boot image
    cat "$BUILD_DIR/mbr.bin" "$BUILD_DIR/stage2.bin" > "$BUILD_DIR/boot.img"
    cp "$BUILD_DIR/boot.img" "$ISO_ROOT/boot.img"
    
    echo "Bootloader built successfully."
}

build_kernel() {
    echo "Building kernel (dKernel)..."
    
    # Compile kernel assembly files
    nasm -f elf64 "$ROOT_DIR/kernel/arch/x86_64/boot.asm" -o "$BUILD_DIR/boot.o"
    
    # Find all C files in kernel directory
    KERNEL_SOURCES=$(find "$ROOT_DIR/kernel" -name "*.c")
    
    # Compile each kernel C file
    KERNEL_OBJECTS="$BUILD_DIR/boot.o"
    for src in $KERNEL_SOURCES; do
        obj="$BUILD_DIR/$(basename "${src%.c}.o")"
        gcc -c $CFLAGS -ffreestanding -mcmodel=kernel -mno-red-zone -mno-mmx -mno-sse \
            -I"$ROOT_DIR/kernel/include" -o "$obj" "$src"
        KERNEL_OBJECTS="$KERNEL_OBJECTS $obj"
    done
    
    # Link the kernel
    ld -o "$BUILD_DIR/kernel.elf" $LDFLAGS $KERNEL_OBJECTS
    
    # Extract binary
    objcopy -O binary "$BUILD_DIR/kernel.elf" "$BUILD_DIR/kernel.bin"
    
    # Copy to ISO root
    cp "$BUILD_DIR/kernel.bin" "$ISO_ROOT/kernel.bin"
    
    echo "Kernel built successfully."
}

build_filesystem() {
    echo "Building filesystem tools (swanFS)..."
    
    # Find all C files in fs directory
    FS_SOURCES=$(find "$ROOT_DIR/fs" -name "*.c")
    
    # Build mkfs_swan
    gcc $CFLAGS -I"$ROOT_DIR/fs/include" -o "$BUILD_DIR/mkfs_swan" \
        "$ROOT_DIR/fs/tools/mkfs_swan.c" \
        $(echo "$FS_SOURCES" | grep -v "tools" | tr '\n' ' ')
    
    # Build fsck_swan
    gcc $CFLAGS -I"$ROOT_DIR/fs/include" -o "$BUILD_DIR/fsck_swan" \
        "$ROOT_DIR/fs/tools/fsck_swan.c" \
        $(echo "$FS_SOURCES" | grep -v "tools" | tr '\n' ' ')
    
    # Copy to sysroot
    cp "$BUILD_DIR/mkfs_swan" "$SYSROOT_DIR/System/bin/"
    cp "$BUILD_DIR/fsck_swan" "$SYSROOT_DIR/System/bin/"
    
    echo "Filesystem tools built successfully."
}

build_userland() {
    echo "Building userland applications..."
    
    # Build dsGUI
    gcc $CFLAGS -I"$ROOT_DIR/userland/dsGUI/include" -o "$BUILD_DIR/dsGUI" \
        $(find "$ROOT_DIR/userland/dsGUI" -name "*.c" | tr '\n' ' ')
    
    # Build dsSessiond
    gcc $CFLAGS -I"$ROOT_DIR/userland/dsSessiond/include" -o "$BUILD_DIR/dsSessiond" \
        $(find "$ROOT_DIR/userland/dsSessiond" -name "*.c" | tr '\n' ' ')
    
    # Build dsSetupTool
    gcc $CFLAGS -I"$ROOT_DIR/userland/dsSetupTool/include" -o "$BUILD_DIR/dsSetupTool" \
        $(find "$ROOT_DIR/userland/dsSetupTool" -name "*.c" | tr '\n' ' ')
    
    # Copy to sysroot
    cp "$BUILD_DIR/dsGUI" "$SYSROOT_DIR/System/bin/"
    cp "$BUILD_DIR/dsSessiond" "$SYSROOT_DIR/System/bin/"
    cp "$BUILD_DIR/dsSetupTool" "$SYSROOT_DIR/System/bin/"
    cp "$BUILD_DIR/dsSetupTool" "$ISO_ROOT/dsSetupTool"
    
    echo "Userland applications built successfully."
}

prepare_sysroot() {
    echo "Preparing system root for ISO..."
    
    # Create necessary directories
    mkdir -p "$SYSROOT_DIR/System/bin"
    mkdir -p "$SYSROOT_DIR/System/lib"
    mkdir -p "$SYSROOT_DIR/System/fonts"
    mkdir -p "$SYSROOT_DIR/Applications"
    
    # Copy assets
    cp "$ROOT_DIR/assets/dsOS.png" "$SYSROOT_DIR/System/"
    
    # Copy necessary files to ISO root
    cp -r "$SYSROOT_DIR/"* "$ISO_ROOT/"
    
    echo "System root prepared successfully."
}

create_iso() {
    echo "Creating ISO image..."
    
    # Use xorriso to create bootable ISO
    xorriso -as mkisofs \
        -R -J -D -relaxed-filenames \
        -V DSOS_0_1 \
        -b boot.img -boot-load-size 4 -boot-info-table -no-emul-boot \
        -o "$ROOT_DIR/dsos.iso" "$ISO_ROOT"
    
    echo "ISO image created successfully: $ROOT_DIR/dsos.iso"
}

# Main build process
echo "Starting dsOS build process..."

# Execute build stages
build_bootloader
build_kernel
build_filesystem
build_userland
prepare_sysroot
create_iso

echo "dsOS build completed successfully!"
echo "To test with QEMU, run: qemu-system-x86_64 -cdrom dsos.iso -boot d -serial stdio -m 512M"

exit 0
