#!/bin/bash
# Upload build/firmware.uf2 to a Sensor Watch in UF2 bootloader mode.

set -e

echo "[debug] script started"

cd "$(dirname "$0")/.."
echo "[debug] working directory: $(pwd)"

UF2_FILE="build/firmware.uf2"
if [ ! -f "$UF2_FILE" ]; then
    echo "No $UF2_FILE found. Build first with:"
    echo "  make BOARD=sensorwatch_pro DISPLAY=custom"
    exit 1
fi
echo "[debug] UF2 file: $UF2_FILE ($(stat -c%s "$UF2_FILE") bytes)"

# fwupd will aggressively probe the UF2 bootloader and can reset the watch before
# the copy finishes, so stop it for the duration of the upload.
FWUPD_WAS_RUNNING=0
cleanup_fwupd() {
    if [ "$FWUPD_WAS_RUNNING" -eq 1 ]; then
        echo "[debug] restarting fwupd.service"
        sudo systemctl start fwupd.service >/dev/null 2>&1 || true
    fi
}
if systemctl is-active --quiet fwupd.service 2>/dev/null && \
   systemctl cat fwupd.service >/dev/null 2>&1; then
    echo "[debug] stopping fwupd.service"
    sudo systemctl stop fwupd.service
    FWUPD_WAS_RUNNING=1
    trap cleanup_fwupd EXIT
fi

echo "[debug] looking for auto-mounted UF2 drive..."

# Try to find an auto-mounted UF2 drive first.
for base in "/run/media/$USER" "/media/$USER" "/media"; do
    echo "[debug] checking base: $base"
    for d in "$base"/*/; do
        echo "[debug] checking candidate: $d"
        if [ -f "$d/INFO_UF2.TXT" ]; then
            echo "[debug] found INFO_UF2.TXT at $d"
            OUT="$d/NEW.UF2"
            echo "[debug] copying $UF2_FILE -> $OUT"
            cp "$UF2_FILE" "$OUT"
            echo "[debug] copy finished; flushing..."
            sync -f "$OUT" 2>/dev/null || true
            echo "[debug] flush finished"
            echo "Done. The watch will reset automatically."
            exit 0
        fi
    done
done

echo "[debug] no auto-mounted drive found; scanning lsblk -f..."

# Not auto-mounted: use lsblk -f to find an unmounted USB FAT partition.
lsblk -f -p -n -o NAME,FSTYPE,MOUNTPOINT
echo "[debug] lsblk output above"
DEV=$(lsblk -f -p -n -o NAME,FSTYPE,MOUNTPOINT | awk '$2 ~ /(vfat|fat)/ && $3 == "" && $1 !~ /nvme/ {print $1; exit}')

if [ -z "$DEV" ]; then
    echo "[debug] no unmounted vfat partition found"
    echo "No Sensor Watch UF2 drive found."
    echo "Make sure the watch is in bootloader mode (double-tap reset)."
    exit 1
fi

echo "[debug] chose unmounted FAT device: $DEV"

# Try udisksctl first (with a timeout), fall back to manual sudo mount.
MP=""
if command -v udisksctl >/dev/null 2>&1; then
    echo "[debug] trying udisksctl mount (5s timeout)..."
    MP=$(timeout 5 udisksctl mount --no-user-interaction -b "$DEV" 2>/dev/null | awk '{print $NF}' | sed 's/\.//')
    echo "[debug] udisksctl returned: $MP"
fi

if [ -z "$MP" ] || [ ! -d "$MP" ]; then
    echo "[debug] udisksctl failed or missing; using manual mount"
    MP="/tmp/sensorwatch-uf2"
    mkdir -p "$MP"
    if mountpoint -q "$MP"; then
        echo "[debug] cleaning up stale mountpoint"
        sudo umount "$MP" 2>/dev/null || true
    fi
    echo "[debug] sudo mount $DEV $MP"
    sudo mount -t vfat -o flush,rw "$DEV" "$MP"
fi

echo "[debug] mounted at: $MP"

if [ ! -f "$MP/INFO_UF2.TXT" ]; then
    echo "[debug] INFO_UF2.TXT missing at $MP"
    echo "Mounted $DEV at $MP, but it does not look like a UF2 drive (missing INFO_UF2.TXT)."
    echo "Unmounting and aborting."
    mountpoint -q "$MP" && sudo umount "$MP"
    exit 1
fi

OUT="$MP/NEW.UF2"
echo "[debug] copying $UF2_FILE -> $OUT"
cp "$UF2_FILE" "$OUT"
echo "[debug] copy finished; flushing..."
sync -f "$OUT" 2>/dev/null || true
echo "[debug] flush finished; unmounting..."
mountpoint -q "$MP" && sudo umount "$MP"
echo "[debug] unmount finished"
echo "Done. The watch will reset automatically."
