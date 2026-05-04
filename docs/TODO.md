# TODO

# Goal is to boot ubuntu and run it as a guest VM using our hypervisor

## Immediate Boot Blockers

- [ ] Decode and log lower-EL synchronous exits before panicking.
  - Decode `ESR_EL2`, `FAR_EL2`, access width, read/write direction, target
    register, and fault status code for data/instruction aborts.
  - Keep the panic path, but make the next fault actionable from UART output.

- [ ] Fix guest-visible MMIO for Raspberry Pi 5.
  - Current panic at `FAR_EL2=0x107D001018` is guest Linux touching PL011 UART.
  - Decide whether first RPI5 boot uses direct UART passthrough or virtual UART.
  - If using passthrough temporarily, expand stage-2 IPA coverage beyond 32-bit
    and map the required RPI5 UART/GIC ranges as device memory.

- [ ] Add ARM generic timer virtualization.
  - Expose the timer described in the guest DTB.
  - Configure EL2 timer access policy.
  - Inject guest timer interrupts through the virtual GIC path.

- [ ] Finish minimum GIC virtualization needed by Linux.
  - Validate the guest DTB GIC node matches what HyperBerry actually exposes.
  - Support virtual timer IRQ delivery.
  - Handle EOI/maintenance behavior needed for stable guest interrupts.

- [ ] Make the guest DTB minimal and honest.
  - Advertise only devices HyperBerry maps, emulates, or intentionally
    passthroughs.
  - Keep bootargs aligned with the available console and initrd/root strategy.

## Console Plan

- [ ] Hypervisor owns the physical UART.
  - HyperBerry should keep exclusive ownership of the real PL011 UART so EL2
    diagnostics remain coherent and one guest cannot reconfigure or break the
    hypervisor console.

- [ ] Provide each guest with a virtual UART.
  - Guest UART accesses should trap to EL2.
  - EL2 should serialize and tag output per VM before writing to the physical
    UART.
  - This replaces shared physical UART passthrough for multi-guest support.

- [ ] Allow temporary single-guest UART passthrough only as a bring-up shortcut.
  - Treat passthrough as unsafe for multiple guests.
  - Remove or gate it once virtual UART TX is working.

## Linux/Ubuntu Boot Contract

- [ ] Confirm Linux arm64 entry state.
  - `x0 = guest DTB IPA`.
  - `x1 = x2 = x3 = 0`.
  - Entry point lands inside the copied kernel image.
  - Caches/MMU/system register state matches Linux boot expectations closely
    enough for early boot.

- [ ] Confirm initrd placement and DTB patching.
  - Guest memory range is patched correctly.
  - `linux,initrd-start` and `linux,initrd-end` are patched correctly.
  - Bootargs match the initrd/rootfs being packaged.

- [ ] Boot a minimal Linux/initrd before full Ubuntu.
  - First target: early console output.
  - Second target: initramfs `/init`.
  - Third target: Ubuntu initrd/userspace logs.

## Later Ubuntu Enablement

- [ ] Add a real virtual storage path.
  - Prefer virtio-mmio block once the basic guest/device model is stable.
  - Until then, rely on initrd-only bring-up.

- [ ] Add stronger VM resource ownership.
  - Move beyond one contiguous 256 MiB guest RAM allocation.
  - Track guest-owned memory ranges explicitly for future multi-guest support.

- [ ] Add multi-guest scheduling.
  - Implement a vCPU scheduler.
  - Ensure timer, interrupts, and virtual UART output are VM-aware.

---

1. Set up paging
2. Set up a pipeline to run integration test and unit test also before we are able to push make sure we are not breaking any regression

---

# PRD: Firmware-Preloaded Guest Boot Package

Labels: needs-triage

## Problem Statement

HyperBerry can already enter EL2, parse the firmware-provided DTB, bring up the physical memory allocator, enable the host MMU, initialize a VM, and run a proven minimal Linux-style guest path. The current guest boot path is still too static for real Raspberry Pi 5 Linux VM bring-up: the hypervisor does not yet consume a firmware-preloaded guest payload, does not allocate isolated guest RAM for the payload, and still depends on an obsolete built-in guest stub path.

The developer wants to use the Raspberry Pi firmware as the early storage loader instead of writing SDHC, PCIe, or FAT32 drivers. The firmware should preload one structured guest boot package into host RAM through the initramfs mechanism, advertise that package through the host DTB, and let HyperBerry validate, copy, patch, and boot the Linux VM from that package.

## Solution

HyperBerry will introduce a v1 HyperBerry Guest Boot Package format. The Raspberry Pi firmware will load this package as the configured initramfs. During boot, HyperBerry will parse the host DTB, find the firmware-loaded package region, reserve that region from the physical memory allocator, validate the package header and CRCs, allocate one contiguous 256 MiB host-physical guest RAM region, copy the package components into the selected guest IPA layout, patch fixed-size guest DTB placeholders, and start a Linux arm64 VM through a non-identity stage-2 mapping.

A Rust host-side reference tool will generate the package from a Linux Image, guest DTB, and optional initrd. The tool is optional for the hypervisor build and will not be wired into CMake in v1. Generated package artifacts will be ignored by git. The boot configuration will reference the generated package by default so the required boot contract is visible.

## User Stories

1. As a HyperBerry developer, I want the Raspberry Pi firmware to preload the guest VM package, so that I do not need to implement SD card, PCIe, or FAT32 drivers before booting Linux guests.
2. As a HyperBerry developer, I want the firmware-loaded package address to come from the host DTB, so that the hypervisor can discover the package without hardcoded physical addresses.
3. As a HyperBerry developer, I want HyperBerry to panic when no guest package is present, so that obsolete fallback guest behavior does not hide boot configuration errors.
4. As a HyperBerry developer, I want a structured guest package instead of a raw Linux Image, so that the kernel, guest DTB, optional initrd, boot protocol, sizes, and validation metadata travel together.
5. As a HyperBerry developer, I want the package format to be Linux-first but not Linux-only forever, so that bare-metal AArch64 payload support can be added later without replacing the package ABI.
6. As a HyperBerry developer, I want an explicit boot protocol field, so that mutually exclusive guest boot conventions are not overloaded into modifier flags.
7. As a HyperBerry developer, I want package flags reserved for modifiers such as initrd presence, so that validation can stay simple and future extensions have a clear home.
8. As a HyperBerry developer, I want the package header to be little-endian only, so that the loader and builder avoid unnecessary cross-endian complexity.
9. As a HyperBerry developer, I want v1 package headers to require a 4096-byte header area, so that components naturally begin on page-aligned offsets and future metadata has reserved room.
10. As a HyperBerry developer, I want package component offsets to be 4 KiB aligned, so that copying, validation, and future page-level mapping remain straightforward.
11. As a HyperBerry developer, I want component sizes to describe the bytes actually stored in the package, so that HyperBerry never needs to infer compression or decompression semantics.
12. As a HyperBerry developer, I want the package to include CRC32 validation, so that packaging mistakes and corrupted guest artifacts fail early with clear diagnostics.
13. As a HyperBerry developer, I want the header CRC to cover the full fixed header area with checksum fields zeroed, so that the loader can validate metadata deterministically.
14. As a HyperBerry developer, I want the payload CRC to cover the full payload area including padding, so that the exact package bytes are validated.
15. As a HyperBerry developer, I want the firmware-loaded size to exactly match the package total size, so that trailing stale bytes or mismatched package data are rejected.
16. As a HyperBerry developer, I want the package to require canonical component order, so that v1 packages are deterministic and easy to test.
17. As a HyperBerry developer, I want kernel and guest DTB components to be required, so that every package has the minimum Linux boot material.
18. As a HyperBerry developer, I want initrd to be optional, so that early kernel bring-up and non-initrd root filesystems remain possible.
19. As a HyperBerry developer, I want an optional build ID in the package, so that UART logs can identify which guest package was loaded during hardware testing.
20. As a HyperBerry developer, I want the firmware-loaded package region reserved from PMM, so that guest RAM allocation cannot overwrite the package before it is copied.
21. As a HyperBerry developer, I want the package source region to stay reserved for v1, so that freeing partial firmware-loaded regions does not complicate the first Linux boot milestone.
22. As a HyperBerry developer, I want guest RAM allocated as one contiguous 256 MiB host-physical region, so that the first non-identity stage-2 implementation is simple and observable.
23. As a HyperBerry developer, I want PMM to support a larger allocation order for v1 guest RAM, so that a minimal Linux VM can fit in one contiguous allocation.
24. As a HyperBerry developer, I want a TODO documenting future block-list guest memory, so that the contiguous allocation shortcut is not mistaken for the final VM memory model.
25. As a HyperBerry developer, I want guest IPA layout owned by HyperBerry rather than the package, so that one package format can work with future VM profiles.
26. As a HyperBerry developer, I want the Linux Image loaded at the standard arm64 offset, so that the VM follows the expected Linux boot convention.
27. As a HyperBerry developer, I want the guest DTB and optional initrd packed near the top of guest RAM, so that low guest memory remains simple.
28. As a HyperBerry developer, I want bounds checks for every copied component, so that invalid packages never overwrite guest memory layout boundaries.
29. As a HyperBerry developer, I want the guest DTB to contain fixed-size placeholder properties, so that HyperBerry can patch runtime values without implementing structural FDT insertion.
30. As a HyperBerry developer, I want HyperBerry to patch guest memory and initrd properties, so that Linux sees the actual runtime guest RAM and initrd placement.
31. As a HyperBerry developer, I want bootargs owned by the packaged guest DTB in v1, so that the hypervisor does not need variable-length string mutation.
32. As a HyperBerry developer, I want DTB patching to validate property sizes rather than exact placeholder values, so that valid external guest DTBs are not rejected unnecessarily.
33. As a HyperBerry developer, I want the DTB patcher private to the boot package loader in v1, so that a narrow boot feature does not force a general mutable FDT library too early.
34. As a HyperBerry developer, I want fixed-size DTB overwrites with no DTB header changes, so that the patcher preserves the existing FDT structure.
35. As a HyperBerry developer, I want the boot package loader to run after the host MMU is initialized, so that copies and validation can use normal C++ ergonomics and cached memory.
36. As a HyperBerry developer, I want a host physical to host virtual helper, so that boot loader code does not hardcode the identity direct-map assumption everywhere.
37. As a HyperBerry developer, I want the helper to document the current identity mapping assumption, so that a future higher-half or non-identity host layout has a clear migration point.
38. As a HyperBerry developer, I want GuestMmu to map guest IPA to explicit host PA, so that guest RAM is isolated from machine physical addresses.
39. As a HyperBerry developer, I want Vm initialization to receive guest RAM host PA and guest IPA metadata, so that VM setup is separate from package parsing and copying.
40. As a HyperBerry developer, I want the vCPU initialized with the Linux arm64 register contract, so that Linux receives the guest DTB address in x0 and starts at the package-derived entry IPA.
41. As a HyperBerry developer, I want obsolete guest stack allocation removed from VM setup, so that non-identity guest mappings do not accidentally seed host physical addresses into guest state.
42. As a HyperBerry developer, I want the built-in guest stub removed, so that all normal VM boot goes through the Linux package path.
43. As a HyperBerry developer, I want all package boot failures to end in the same VM panic string with specific UART logs before it, so that the fatal behavior is consistent while diagnostics remain useful.
44. As a HyperBerry developer, I want one package to produce one Linux VM in v1, so that the first implementation matches the firmware initramfs model and current single-VM flow.
45. As a HyperBerry developer, I want a TODO documenting future multiple VM package support, so that the v1 single-guest assumption is explicit.
46. As a HyperBerry developer, I want a Rust package builder inside the repo, so that package generation is reproducible without adding Rust to the hypervisor build.
47. As a HyperBerry developer, I want the Rust tool to serialize fields explicitly as little-endian bytes, so that package correctness does not depend on host struct layout.
48. As a HyperBerry developer, I want the Rust tool to be the reference implementation but not the only possible implementation, so that the package ABI remains documented and portable.
49. As a HyperBerry developer, I want generated package files ignored by git, so that large or environment-specific guest artifacts do not enter source control.
50. As a HyperBerry developer, I want boot configuration to reference the generated package by default, so that the SD card boot partition reflects the required Linux VM boot flow.

## Implementation Decisions

- Build a dedicated boot package module as a deep module that owns package validation, CRC checking, component layout calculation, copying into guest RAM, and fixed guest DTB patching behind a small panic-on-failure API.
- Extend host DTB parsing and boot memory metadata to expose the firmware initramfs region as the HyperBerry guest boot package source region.
- Reserve the firmware-loaded package region during PMM initialization and keep it reserved for the lifetime of the hypervisor in v1.
- Increase PMM maximum allocation order enough to allocate one contiguous 256 MiB guest RAM block.
- Add a TODO near the PMM guest RAM sizing decision that future VM memory should move to block-list ownership rather than requiring one large contiguous host-physical allocation.
- Add a host physical to host virtual helper in the host MMU interface. It will return the identity-mapped address in v1 and document the direct-map assumption.
- Run the boot package loader after heap and host MMU initialization.
- Change GuestMmu initialization to accept guest IPA base, host physical base, and size, and map the guest IPA range to the explicit host physical range.
- Change VM initialization to accept loaded guest metadata from the boot package loader, seed the Linux entry IPA, seed x0 with the guest DTB IPA, and avoid host-physical guest stack allocation.
- Remove the built-in guest stub and make Linux package boot the required VM path.
- Use a v1 package format with little-endian fields, fixed 4096-byte header area, exact total size, CRC32 header validation, CRC32 payload validation, canonical kernel to DTB to initrd ordering, 4 KiB component offset alignment, optional build ID, explicit boot protocol, and modifier flags.
- Treat Linux arm64 as the only implemented boot protocol in v1 while reserving the ABI shape for future bare-metal AArch64 support.
- Use a 256 MiB guest RAM layout with guest IPA base at zero, Linux Image at the standard arm64 load offset, optional initrd near the top of guest RAM, and guest DTB below the initrd.
- Require packaged guest DTBs to include fixed-size placeholder properties for memory range and initrd range. HyperBerry will overwrite those values using big-endian FDT cells.
- Keep bootargs owned by the packaged guest DTB in v1.
- Keep the v1 guest DTB patcher narrow and private to the boot package module.
- On any missing, malformed, oversized, checksum-invalid, unsupported, or unpatchable package, log a specific reason and then panic with the agreed Linux VM failure string.
- Support exactly one firmware-preloaded package and one Linux VM in v1.
- Add a TODO near the boot package loader entry point documenting future multiple VM support.
- Add an optional manual Rust host-side package builder as the reference tool. It will not be required by the hypervisor build system in v1.
- Document the package ABI formally so the Rust tool is a reference implementation rather than hidden tribal knowledge.
- Ignore generated package files in source control.
- Update Raspberry Pi boot configuration so the firmware preloads the generated package by default.

## Testing Decisions

- Tests should target externally observable behavior: accepted packages produce the expected loaded guest metadata and patched guest DTB values; invalid packages fail validation with the expected error category; VM initialization seeds the expected guest-visible state. Tests should not lock onto private loop structure or internal helper names.
- Add unit coverage for package header validation, including magic, version, header size, total size equality, little-endian fields, alignment, canonical component order, initrd flag consistency, unsupported boot protocol, unknown flags, and component bounds.
- Add unit coverage for CRC behavior, including header CRC with checksum fields zeroed and payload CRC over the full payload-and-padding range.
- Add unit coverage for Linux guest layout calculation, including kernel placement, high-memory DTB placement, optional initrd placement, alignment, and overflow rejection in a 256 MiB guest.
- Add unit coverage for fixed guest DTB patching, including successful memory and initrd patching, missing chosen properties, wrong property sizes, no-initrd zeroing, and big-endian cell writes.
- Update VM unit tests to reflect non-identity stage-2 initialization, Linux x0 seeding, removal of guest stack allocation, and the new VM initialization contract.
- Update GuestMmu unit or integration coverage to verify that stage-2 mapping receives explicit host physical addresses rather than identity-mapping guest IPA to host PA.
- Update PMM tests or add coverage to confirm the larger allocation order is valid and still rejects unsupported orders above the configured maximum.
- Add Rust package builder tests for exact header bytes, canonical component offsets, CRC output, optional initrd handling, build ID handling, and error cases for missing or empty required inputs.
- Reuse existing unit-test style for parser-like modules and existing integration-test style for boot-time allocator and VM bring-up paths.
- Preserve QEMU/integration coverage where possible, but hardware boot validation on Raspberry Pi 5 remains necessary for the firmware initramfs handoff.

## Out of Scope

- Runtime loading of new VM images after boot.
- SDHC, PCIe, FAT32, filesystem, or block-device drivers.
- Multiple guest packages or multiple Linux VMs in v1.
- General mutable FDT insertion, DTB synthesis, or variable-length bootargs patching.
- Cryptographic signature verification or secure boot.
- Package-level compression or decompression.
- Non-Linux guest boot implementation, even though the ABI leaves room for it.
- Returning the firmware-loaded package source region to PMM after copying.
- Wiring the Rust package builder into CMake or making Rust a required hypervisor build dependency.
- Committing generated guest package artifacts.

## Further Notes

The v1 design intentionally uses the Raspberry Pi firmware as a boot-time storage loader and keeps HyperBerry focused on validation, memory ownership, stage-2 isolation, and Linux boot correctness. The main engineering shortcut is the contiguous 256 MiB guest RAM allocation; this is acceptable for proving the boot path but should become a real guest-memory ownership model before dynamic or multiple VM support.
