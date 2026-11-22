# KiT-KERNEL

KiT-KERNEL is the Linux kernel build repository for KiT, a small operating-system project.

## Repository layout

- `kernel/config/kit_defconfig` — KiT's kernel configuration.
- `kernel/patches/` — optional KiT-specific patches applied to the Linux source.
- `kernel/source/linux/` — the upstream Linux kernel Git submodule.
- `build/` — local build output (ignored by Git).
- `scripts/` — optional local build and maintenance scripts.
- `.github/workflows/kernel-build.yml` — continuous integration build workflow.

## Linux source submodule

The `kernel/source/linux` submodule is pinned to Linux commit `c44104f3642da720164042623e483283aefdd8e` and tracks:

```text
https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git
```

Clone it with submodules:

```bash
git clone --recurse-submodules https://github.com/catTeam-OS/KiT-KERNEL.git
```

Existing checkouts can initialize it with:

```bash
git submodule update --init --recursive
```

Do not update the submodule pin unless it is intentionally changed as part of a separate project decision.

## Kernel configuration

The intended build configuration is `kernel/config/kit_defconfig`. It contains the x86-64 target, module support, and the `-KiT` local version.

Before building, preserve the repository root as the working directory.

## Intended build process

```bash
git submodule update --init --recursive
cp kernel/config/kit_defconfig kernel/source/linux/.config
make -C kernel/source/linux olddefconfig
make -C kernel/source/linux -j"$(nproc)"
make -C kernel/source/linux -j"$(nproc)" modules
```

Build outputs remain in `kernel/source/linux` unless a separate build directory is configured. The repository-level `build/` directory is reserved for generated build artifacts and is excluded from Git.

## License

See `LICENSE` for the repository license.
