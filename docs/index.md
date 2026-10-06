---
okf_version: "0.2"
---

# HyperBerry: the map of the code

This folder is a knowledge bundle in [Open Knowledge Format](https://github.com/GoogleCloudPlatform/open-knowledge-format):
plain markdown pages, one per thing, linked to each other. It says what every subsystem and flow
is, what it depends on and what uses it. The rules of the project are in [AGENTS.md](../AGENTS.md);
everything else written about HyperBerry is here. `sphinx/` holds the Doxygen and Sphinx build and
is not part of the bundle.

# Begin

* [Start here](start-here.md) - The whole hypervisor in one page, how to look up a file, and a reading order
* [Glossary](glossary.md) - Every term of art the pages use, in one or two sentences each

# How it fits together

* [Architecture](architecture/) - The layers, the build, the tests, the layout's rough edges
* [Flows](flows/) - What happens, function by function, from reset to a running guest, on a guest trap, and from a device tree to `regs.inc`

# The code

* [Subsystems](subsystems/) - One page per unit: lib, drivers, mm, the device tree, the boot loader, vcpu, vm, vmm, the boards and the scripts

# How the work is done

* [Guides](guides/) - Code style, testing, the guest archive format
* [Design notes](design/) - The vision for the project
* [Roadmap](TODO.md) - Open work

# Why it is this way

* [Decisions](decisions/) - Each big choice and the reason for it

# Keeping it

* [Log](log.md) - What changed in this bundle, newest first
