---
type: Decision
title: A VM is described by a VmConfig
description: Why Vm takes one VmConfig struct, filled with designated initializers, in place of a list of positional arguments.
resource: ../../core/vm/vm.h
tags: [decision, vm]
status: unverified
decided: 2026-09-25
generated:
  by: claude-code/claude-sonnet-5-5
  at: 2026-10-06T12:00:00Z
sources:
  - resource: ../../core/vm/vm.h
  - resource: ../../core/vmm/vmm.cpp
  - resource: ../../core/main.cpp
---

# The decision

`Vm`'s constructor takes a `VmConfig` and the device map. `VmConfig` is a plain struct with the fields `name`, `ipaBase`, `ramHostPa`, `ramSize`, `vmid`, `entry` and `dtb`. `Vmm` passes the same struct on, and `hmain` builds it with designated initializers.

# Why

- Commit 6811ca2 (2026-09-25) replaced eight positional arguments. Several of the fields are plain 64 bit numbers, such as the host RAM base, the entry address and the device tree address, so a positional call does not show which is which.
- Designated initializers name each value at the call site, in `core/main.cpp`.
- The guest name became a `std::string_view` in the same change.

# Consequences

- A new per VM setting is a new field, not a longer constructor.
- `Vm` and `Vmm` have the same constructor shape, since `Vmm` takes the config and the device map and forwards them.
- The `vmid` field is documented as nonzero and unique across live VMs.

See [Vm](../subsystems/vm.md) and [Vmm](../subsystems/vmm.md).
