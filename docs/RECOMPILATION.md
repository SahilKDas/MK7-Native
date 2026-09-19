# Static recompilation

MK7-Native uses a mechanical static-recompilation workflow. Project code does
not embed or commit Nintendo binaries, assets, generated translations, or
encryption material. Each user supplies their own game image locally.

## Architecture

`mk7-recompile` consumes a decompressed 3DS ExeFS `.code` image and emits C++
that operates on an explicit 32-bit guest CPU and memory ABI. It is built on
the MIT-licensed [`arm-recomp-core`](https://github.com/mstan/arm-recomp-core),
which is pinned as a Git submodule.

The initial bring-up profile uses the core's ARMv5TE decoder and emitter. That
profile translates the ARM instruction subset exercised by the first MK7 code
page, but its surrounding timing calls target Nintendo DS. It is therefore a
decoder/code-generation proof, not yet an executable MK7 port. A correct port
requires an `armv6k_ctr` profile and a CTR runtime implementing memory,
services, scheduling, graphics, audio, input, and networking.

Unsupported instructions fail closed: the tool reports their guest addresses
and exits unsuccessfully instead of silently emitting guessed behavior.

## Build

```sh
git submodule update --init
cmake --preset default
cmake --build --preset default
```

## Local workflow

Use `ctrtool` to extract and decompress the ExeFS code from a legally obtained
CIA. Keep all inputs and outputs outside version control.

```powershell
ctrtool --decompresscode --exefsdir=build/exefs path/to/game.cia

build/default/mk7-recompile.exe `
  --input build/exefs/code.bin `
  --output generated/usa_rev2/text_00100000.cpp `
  --image-base 0x00100000 `
  --start 0x00100000 `
  --size 0x1000
```

The `generated/` directory and common 3DS dump formats are ignored globally by
this repository.

## Next correctness gates

1. Add ARMv6K decode and semantic tests for instructions absent from ARMv5TE.
2. Replace the DS code-generation profile with `armv6k_ctr`.
3. Import function boundaries and names from independently maintained mk7re
   metadata rather than treating mixed code and data as a linear instruction
   stream.
4. Implement the CTR guest-memory and service-call runtime.
5. Differentially test generated blocks against an independent 3DS execution
   oracle before enabling them in the native runner.

