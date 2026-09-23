# Static recompilation

MK7-Native uses a mechanical static-recompilation workflow. Project code does
not embed or commit Nintendo binaries, assets, generated translations, or
encryption material. Each user supplies their own game image locally.

## Architecture

`mk7-recompile` consumes the decompressed 3DS ExeFS code image and emits function-scoped C++ over an explicit 32-bit guest CPU and memory ABI. The local `armv6k_ctr` profile adds ARM11/ARMv6K decoding and a CTR-specific runtime; optional `MK7_SYMBOL_MAP` metadata supplies mk7re/Ghidra/IDA function boundaries. Without metadata, the reachability mapper follows ARM control flow plus validated PC-relative callback pointers, then conservatively indexes unclaimed ARM prologues and veneers within the ExHeader-defined executable text region until `MK7_COVERAGE_PERMILLE` is satisfied and emits a local CSV byte map. Generated functions are distributed across `MK7_GENERATED_SHARDS` C++ translation units to bound compiler memory. A Zig audit then verifies map uniqueness, instruction support, function presence, and the requested byte-coverage threshold before the generated C++ may compile.

Unsupported instructions and unknown control-flow targets fail closed. The native SDL3/Vulkan host remains alive and visualizes the resulting runtime snapshot instead of treating a guest halt as a host crash.

The CTR runtime now implements typed kernel handles, page-aligned `ControlMemory`, `CreateAddressArbiter`, `ConnectToPort`, `SendSyncRequest`, TLS IPC command buffers, `srv:` client registration, and validated service-handle acquisition for the initial APT, CFG, CSND, DSP, FS, GSP, HID, IR, NDM, PTM, and SOC endpoints. Unknown handles, ports, services, and command IDs return explicit errors instead of blanket success.
## Build

```sh
git submodule update --init
cmake --preset default
cmake --build --preset default
```

## Local workflow

Point CMake at a legally obtained CIA wherever it already lives. The build verifies it in place and writes extracted/generated data only below the selected build directory.

```powershell
cmake -S . -B build/native -G Ninja `
  -DMK7_ROM_PATH="C:/path/to/your/game.cia" `
  -DCTRTOOL_PATH="C:/path/to/ctrtool.exe" `
  -DMK7_SYMBOL_MAP="C:/optional/path/to/mk7re-symbols.csv"
cmake --build build/native --target mk7-run
```
## Native bring-up runner

Run `mk7-run` with the same external CIA and ctrtool. The runner verifies and extracts locally, initializes the CTR memory map, executes the generated entry closure, then keeps a resizable SDL3/Vulkan diagnostic window alive. The current 79% closure registers 29,954 functions, compiles 4,526,956 mapped bytes (1,131,739 ARM instructions), and reaches the real startup `SVC 0x21` at guest PC `0x00101564`. The Vulkan host now presents valid guest top and bottom framebuffers. The first PICA200 slice decodes command headers, masked and consecutive register writes, draw triggers, memory fills, copies, GSP queue submission, and framebuffer swaps; unsupported shader/raster state remains future work.

```powershell
build/native/mk7-run.exe "$MK7_CIA" --ctrtool path/to/ctrtool.exe
```
## Next correctness gates

1. Import trustworthy USA Rev2 mk7re function metadata and expand the verified call closure.
2. Differentially test generated ARMv6K blocks against an independent 3DS execution oracle.
3. Expand the initial HID, GSP, FS, and APT commands into complete service coverage.
4. Extend the initial PICA200 decoder into vertex shading, rasterization, texturing, blending, and depth/stencil Vulkan pipelines.
5. Add audio, scheduling, filesystem, and networking services incrementally.
