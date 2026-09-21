#!/usr/bin/env bash

set -euo pipefail

if [[ $# -ne 3 ]]; then
  echo "usage: $0 image guest-archive sd-partition" >&2
  exit 2
fi

image=$1
archive=$2
sd_dev=$3
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
mount=/mnt/sdcard

if [[ ! -f "$image" ]]; then
  echo "Raspberry Pi image is missing: $image" >&2
  exit 1
fi

if [[ ! -f "$archive" ]]; then
  echo "Raspberry Pi guest archive is missing: $archive" >&2
  exit 1
fi

sudo mkdir -p "$mount"
sudo mount -o "uid=$(id -u),gid=$(id -g)" "$sd_dev" "$mount"
trap 'sudo umount "$mount"' EXIT

cp "$image" "$mount/kernel8.img"
cp "$archive" "$mount/guest.cpio"
cp "$root/bsp/rpi5/firmware/start4.elf" "$mount"
cp "$root/bsp/rpi5/firmware/bcm2712-rpi-5-b.dtb" "$mount"
cp "$root/bsp/rpi5/firmware/config.txt" "$mount"
cp "$root/bsp/rpi5/firmware/fixup4.dat" "$mount"
sync
