# MK7-Native

Porting *Mario Kart 7* to PCs and laptops natively—built with **C++26**

[![Discord](https://img.shields.io/badge/Discord-Join%20Community-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/ZxSJ8f4748)
[![License: BSD 3-Clause](https://img.shields.io/badge/License-BSD%203--Clause-blue.style=for-the-badge)](LICENSE)

---

## 🏎️ Overview

**MK7-Native** is an experimental project aimed at decompiling and natively re-implementing *Mario Kart 7* for modern desktop hardware without relying on full-system 3DS emulation overhead. 

### Why C++26?
The native runtime and current generated game code use C++. The recompiler validates coverage and instruction support without requiring a second language toolchain.

---

## 🛠️ Project Status

> [!NOTE]
> This project is in its early initialization phase.

- [x] Repository initialized & licensed (BSD-3-Clause)
- [ ] Core architecture & rendering setup
- [x] Verified 79% native function/byte-map gate (79.0003%)
- [ ] Broader static decompilation / structural mapping
- [ ] Asset loading pipeline

---

## 🚀 Building

MK7-Native currently requires CMake 3.25+, Ninja, the Vulkan SDK, and a compiler with C++26
language-mode support. SDL3 is supplied as a pinned submodule.

```sh
git submodule update --init
cmake --preset default
cmake --build --preset default
ctest --preset default
```

The first available tool is `mk7-inspect`, a small file-signature inspector for
the Nintendo and Mario Kart 7 formats that will underpin the asset pipeline:

```sh
./build/default/mk7-inspect path/to/archive.szs
```

> [!IMPORTANT]
> Never commit game ROMs, extracted assets, encryption keys, or other
> copyrighted game data. MK7-Native is developed from independently written
> code and user-supplied game data.

See [Native coverage](docs/COVERAGE.md) for measured progress and [Static recompilation](docs/RECOMPILATION.md) for the local ROM-to-C++
workflow, current architecture limits, and correctness gates.

The game CIA remains wherever the user stores it; pass its absolute path to
`mk7-run`. MK7-Native never copies the full ROM into the source or build tree.

---

## 💬 Community & Discussion

Want to talk about the decompilation progress, modern C++ patterns, or track reversing? Join the Discord server:

👉 **[Join the Discord Server](https://discord.gg/ZxSJ8f4748)**

---

## 📄 License

This project is licensed under the BSD 3-Clause License. See the [LICENSE](LICENSE) file for full details.

## External-ROM native build

The CIA stays at its original location and is never copied into this repository. Configure a build with:

    cmake -S . -B build/native -G Ninja -DMK7_ROM_PATH="C:/path/to/MARIO KART 7.cia" -DCTRTOOL_PATH="C:/path/to/ctrtool.exe"
    cmake --build build/native --target mk7-run

After linking, reclaim generated-source and object storage while preserving `mk7-run.exe`:

    cmake --build build/native --target mk7-clean-generated

CMake verifies the configured SHA-512, extracts ExeFS only beneath the build directory, generates function-scoped C++, and compiles it. Set `MK7_SYMBOL_MAP` to an mk7re/Ghidra/IDA text or CSV map in `address,size,name,mode` form; size may be omitted and inferred. Without a map, bootstrap discovery deliberately emits the initial closure beginning at `0x00100000 -> 0x00100024`. Unknown branch targets fail closed.

## Native visual host

`mk7-run` now creates a resizable SDL3/Vulkan window after loading the verified external ROM. It presents guest top and bottom framebuffers through Vulkan after GSP buffer swaps, with the diagnostic surfaces retained until a valid guest framebuffer is available. The initial PICA200 path decodes masked command-list register writes, draw triggers, GX memory fills, copies, and shared command-queue submission.

Azahar default controls: A/S/Z/X map to A/B/X/Y; T/G/F/H map to the D-pad; arrow keys map to the Circle Pad; Q/W map to L/R; M/N map to Start/Select. SDL gamepads use face buttons, shoulders, D-pad, Start/Back, and the left stick.
