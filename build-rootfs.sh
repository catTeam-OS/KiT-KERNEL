#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
ROOTFS_DIR="$BUILD_DIR/rootfs"
BUSYBOX_BIN="${BUSYBOX:-$(command -v busybox || true)}"

if [[ -z "$BUSYBOX_BIN" || ! -x "$BUSYBOX_BIN" ]]; then
    echo "Error: BusyBox not found."
    exit 1
fi

for cmd in cpio gzip find; do
    command -v "$cmd" >/dev/null || {
        echo "Error: $cmd is required."
        exit 1
    }
done

rm -rf "$ROOTFS_DIR"
mkdir -p "$ROOTFS_DIR"

cp -a "$PROJECT_DIR/rootfs/." "$ROOTFS_DIR/"
cp "$BUSYBOX_BIN" "$ROOTFS_DIR/bin/busybox"

while IFS= read -r applet; do
    ln -sfn busybox "$ROOTFS_DIR/bin/$applet"
done < <("$ROOTFS_DIR/bin/busybox" --list)

mkdir -p "$BUILD_DIR"

(
    cd "$ROOTFS_DIR"
    find . -print0 | cpio --null -o --format=newc | gzip -9 \
        > "$BUILD_DIR/initramfs-KiT.img"
)

echo "Initramfs built: $BUILD_DIR/initramfs-KiT.img"