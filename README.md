# MK7-Native

Porting *Mario Kart 7* to PCs and laptops natively—built with **C++26**

[![Discord](https://img.shields.io/badge/Discord-Join%20Community-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/ZxSJ8f4748)
[![License: BSD 3-Clause](https://img.shields.io/badge/License-BSD%203--Clause-blue.style=for-the-badge)](LICENSE)

---

## 🏎️ Overview

**MK7-Native** is an experimental project aimed at decompiling and natively re-implementing *Mario Kart 7* for modern desktop hardware without relying on full-system 3DS emulation overhead. 

### Why C++26?
Zig was considered, but until Zig has those vtables thingy, nope.

---

## 🛠️ Project Status

> [!NOTE]
> This project is in its early initialization phase.

- [x] Repository initialized & licensed (BSD-3-Clause)
- [ ] Core architecture & rendering setup
- [x] First 0.1% reachable native function/byte-map gate
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

CMake verifies the configured SHA-512, extracts ExeFS only beneath the build directory, generates function-scoped C++, and compiles it. Set `MK7_SYMBOL_MAP` to an mk7re/Ghidra/IDA text or CSV map in `address,size,name,mode` form; size may be omitted and inferred. Without a map, bootstrap discovery deliberately emits the initial closure beginning at `0x00100000 -> 0x00100024`. Unknown branch targets fail closed.

## Native visual host

`mk7-run` now creates a resizable SDL3/Vulkan window after loading the verified external ROM. It displays diagnostic top and bottom 3DS surfaces driven by the current runtime snapshot and remains responsive when guest execution fails closed at an unknown target.

Controls: arrow keys map to the D-pad; I/J/K/L map to the Circle Pad; Z/X map to A/B; S/A map to X/Y; Q/W map to L/R; Enter and Backspace map to Start and Select.
