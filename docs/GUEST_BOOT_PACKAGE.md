# HyperBerry Guest Boot Package

The HyperBerry Guest Boot Package (hvGuestBootPkg) is the v1 container used to boot a
Linux guest without adding storage drivers to the hypervisor. The Raspberry Pi
firmware loads one package through `config.txt` as an initramfs, then exposes
the loaded host RAM range through the host device tree.

## Boot Flow

1. The SD card boot partition contains `kernel8.img`, `guest.hvgbp`, and
   `config.txt`.
2. Raspberry Pi firmware loads `kernel8.img` as HyperBerry.
3. Raspberry Pi firmware loads `guest.hvgbp` as the configured initramfs.
4. Firmware writes `/chosen/linux,initrd-start` and
   `/chosen/linux,initrd-end` into the host DTB.
5. HyperBerry parses the host DTB and reserves the package region from PMM.
6. HyperBerry validates the package, allocates 256 MiB of guest RAM, copies
   the package components into guest RAM, patches the guest DTB placeholders,
   and starts the Linux VM.

`boot/config.txt` should include:

```text
kernel=kernel8.img
initramfs guest.hvgbp followkernel
```

## Package Rules

All multibyte package header fields are little-endian. The package format is
strict in v1:

- `header_size` must be exactly 4096 bytes.
- The firmware-loaded byte count must exactly equal `total_size`.
- Component offsets must be 4 KiB aligned.
- Components must appear in canonical order: kernel, guest DTB, optional initrd.
- Kernel and guest DTB are required and must be non-empty.
- Initrd is optional.
- CRC32 is IEEE CRC32, using polynomial `0xEDB88320`.
- `payload_crc32` covers `package[header_size..total_size]`, including padding.
- `header_crc32` covers `package[0..header_size]` with both checksum fields set
  to zero for the calculation.

## Header Layout

The fixed v1 header lives at the start of the 4096-byte header area. Bytes not
listed here are reserved and should be zero.

| Offset | Size | Field | Description |
| ---: | ---: | --- | --- |
| 0 | 4 | `magic` | `0x50424748` |
| 4 | 2 | `version` | `1` |
| 6 | 2 | `header_size` | `4096` |
| 8 | 8 | `total_size` | Exact package byte count |
| 16 | 4 | `header_crc32` | Header CRC with checksum fields zeroed |
| 20 | 4 | `payload_crc32` | Payload and padding CRC |
| 24 | 4 | `boot_protocol` | `1` for Linux arm64 |
| 28 | 4 | `flags` | Modifier flags |
| 32 | 8 | `kernel_offset` | Must be `4096` in v1 |
| 40 | 8 | `kernel_size` | Stored kernel byte count |
| 48 | 8 | `dtb_offset` | 4 KiB aligned guest DTB offset |
| 56 | 8 | `dtb_size` | Stored guest DTB byte count |
| 64 | 8 | `initrd_offset` | 4 KiB aligned initrd offset, or zero |
| 72 | 8 | `initrd_size` | Stored initrd byte count, or zero |
| 80 | 8 | `entry_offset` | Offset from Linux kernel load IPA |
| 88 | 64 | `build_id` | Optional NUL-terminated identifier |

## Boot Protocols

Only Linux arm64 is implemented in v1.

| Value | Protocol | Status |
| ---: | --- | --- |
| 1 | Linux arm64 | Supported |
| 2 | Bare-metal AArch64 | Reserved |

Linux arm64 boot state:

- `PC = kernel_ipa + entry_offset`
- `x0 = guest_dtb_ipa`
- `x1 = 0`
- `x2 = 0`
- `x3 = 0`
- `SP_EL1` is not supplied by HyperBerry for Linux.

## Flags

| Bit | Name | Meaning |
| ---: | --- | --- |
| 0 | `INITRD_PRESENT` | `initrd_size` is non-zero and an initrd component exists |

All unknown flags are rejected in v1.

## Guest IPA Layout

HyperBerry owns guest placement. The package describes component bytes, not
absolute guest addresses.

```text
guest IPA base:    0x00000000
guest RAM size:    256 MiB
kernel Image IPA:  0x00080000
initrd IPA:        top of RAM minus initrd size, 2 MiB aligned downward
guest DTB IPA:     below initrd, 64 KiB aligned downward
entry IPA:         kernel Image IPA + entry_offset
```

The kernel, DTB, and optional initrd must fit inside the 256 MiB guest RAM
layout. The entry IPA must remain inside the copied kernel image.

## Guest DTB Requirements

The packaged guest DTB must contain fixed-size placeholders that HyperBerry can
overwrite without changing DTB structure:

```dts
/ {
  #address-cells = <2>;
  #size-cells = <2>;

  memory@0 {
    device_type = "memory";
    reg = <0x0 0x00000000 0x0 0x10000000>;
  };

  chosen {
    linux,initrd-start = <0x0 0x00000000>;
    linux,initrd-end = <0x0 0x00000000>;
  };
};
```

HyperBerry overwrites:

- `memory*/reg`
- `/chosen/linux,initrd-start`
- `/chosen/linux,initrd-end`

`bootargs` are owned by the packaged guest DTB in v1.

## Reference Tool

`tools/mkguestpkg` is the reference package builder. It is a normal host-side
Rust CLI and is not part of the hypervisor CMake build.

Build and run:

```bash
cargo run --manifest-path tools/mkguestpkg/Cargo.toml -- \
  --kernel path/to/Image \
  --dtb path/to/guest.dtb \
  --initrd path/to/rootfs.cpio.gz \
  --out boot/guest.hvgbp \
  --build-id buildroot-aarch64-minimal
```

Kernel + DTB only:

```bash
cargo run --manifest-path tools/mkguestpkg/Cargo.toml -- \
  --kernel path/to/Image \
  --dtb path/to/guest.dtb \
  --out boot/guest.hvgbp
```

Generated `.hvgbp` files are ignored by git (`boot/*.hvgbp` in `.gitignore`). Do not commit them.

## V1 Limitations

- Exactly one firmware-loaded package is supported.
- Exactly one Linux VM is created from that package.
- The firmware package region remains reserved after copying.
- Guest RAM must be one contiguous 256 MiB host-physical allocation.
- HyperBerry only overwrites existing fixed-size DTB properties.
- No runtime VM image loading is supported.
- No package signatures or cryptographic verification are supported.
