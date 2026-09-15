# Guest archives

HyperBerry consumes one uncompressed `newc` CPIO archive through the host
device tree's `/chosen/linux,initrd-start` and `linux,initrd-end` properties.
The physical allocator reserves that entire region for the boot lifetime.
Every guest payload, including integration guest binaries, comes from this archive.

## File paths

| Path | Use |
| --- | --- |
| `linux/Image` | Required nonempty Linux kernel |
| `linux/guest.dtb` | Required nonempty guest device tree |
| `linux/initrd` | Optional nonempty initrd, copied unchanged |
| `tests/vcpu.bin` | vCPU integration guest |
| `tests/gic.bin` | GIC integration guest |

The archive has no manifest. Callers select conventional paths and determine
how to run each payload. Other regular files may be included.

The reader accepts magic `070701`, regular files, and directories.
It validates the complete archive before exposing any file. It rejects malformed
fields, invalid paths, duplicate paths, links, special files, missing trailers,
and nonzero data after `TRAILER!!!`. Leading `./` is accepted.
Files may appear in any order. Archive headers and data use CPIO alignment,
independent of guest memory alignment.

The outer archive has no compression or payload checksum. The Linux initrd
may itself be compressed; HyperBerry does not interpret its contents.
The format follows the
[Linux CPIO description](https://www.kernel.org/doc/html/latest/driver-api/early-userspace/buffer-format.html),
with the restricted entry types and required trailer described above.

## Build and handoff

CMake generates each board's guest DTB and `guest.cpio` under
`build/<mode>/<board>/`. Integration builds produce a separate archive under
`integration/` containing the Linux files and both test binaries.
Generated archives do not belong in source control.

```sh
cmake --preset debug
cmake --build --preset debug --target guest-archive-qemu
just qemu
just test-integration
```

Kernel inputs default to the repository's `Image`. Override
`QEMU_GUEST_KERNEL`, `RPI5_GUEST_KERNEL`, or `FVP_GUEST_KERNEL` when
configuring CMake. By default the production archive contains a BusyBox
initramfs. CMake downloads Debian's static AArch64 BusyBox 1.38.0 package,
verifies SHA256 `968d1aa8f579fa1ac59c26afa365454369e13cf29848e6400b50028fed0ffda0`,
and extracts its `usr/bin/busybox` binary. The rootfs starts BusyBox `init`,
mounts proc, sysfs, and devtmpfs, then opens a shell on `ttyAMA0`.
The Linux `Image` must enable `CONFIG_BLK_DEV_INITRD`.

The integration archive does not include BusyBox because it does not boot the
Linux guest.

Set `*_GUEST_INITRD` to replace the generated BusyBox rootfs. For example:
For example:

```sh
cmake --preset debug -DQEMU_GUEST_INITRD=/path/to/rootfs.cpio.gz
```

To package a directory with the conventional paths:

```sh
just cpio path/to/root path/to/guest.cpio
cpio -it < path/to/guest.cpio
```

The wrapper stages regular files and directories and invokes standard
`cpio -o -H newc`. Build hosts need Python 3, `cpio`, and `dtc`.

QEMU supplies the archive with `-initrd`. Raspberry Pi firmware loads it with:

```text
kernel=kernel8.img
initramfs guest.cpio followkernel
```

The Raspberry Pi flash targets build and copy the matching archive.
`flash-rpi5-test` selects the integration image and archive.
FVP loads the archive at the address in `bsp/fvp/platform.inc` and patches
the host DTB endpoints from its actual byte size.

## Loading guests

The Linux loader allocates a contiguous 256 MiB guest RAM region at guest IPA
zero. It copies the kernel to IPA `0x200000`, places the optional initrd below
the top of RAM with 2 MiB alignment, and places the DTB below it with 64 KiB
alignment. Overlapping or overflowing layouts fail.

The DTB must provide a 16 byte `memory/reg` and 8 byte
`/chosen/linux,initrd-start` and `linux,initrd-end` placeholders.
The loader patches them in the copied DTB after checking its bounds.
Boot arguments remain in the guest DTB. Linux enters at the start of
`Image` with the guest DTB IPA in x0.

Test binaries enter at byte zero and use relative internal addresses.
The GIC guest receives GICV in x0 and a pointer to two shared 32 bit words
(progress, IAR) in x1. The vCPU guest reports its resume address in x6.
The test loader copies each binary into page aligned memory and synchronizes
instruction caches before entering EL1. It releases the allocation on return.

CPIO file views borrow immutable archive bytes. The reader belongs to Lib;
Linux placement belongs to Core. Neither the archive format nor the test
loader introduces a general ELF loader or guest configuration language.
