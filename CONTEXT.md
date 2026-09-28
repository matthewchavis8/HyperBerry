# HyperBerry domain terms

A VM owns one guest address space, its stage two mappings, one VCPU, and its lifecycle state. The current implementation runs one VM with one VCPU.

A VCPU holds saved EL1 registers. Entering it returns a guest exit with the exception reason, syndrome, and abort addresses. It does not decide whether the VM resumes.

The VMM owns the VM and coordinates guest exits. HVC and SMC handlers update guest registers and report the action requested by a call. The VMM applies that action to the VM lifecycle.

An EL2 exception is a fault or interrupt taken while the hypervisor runs. Its handlers use a saved EL2 frame and are separate from guest exit handling.
