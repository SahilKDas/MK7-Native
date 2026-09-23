# Native coverage

Coverage is measured from the verified USA Rev2 decompressed ExeFS code.bin, not from the CIA container or generated C++ size.

## Current gate

- Code image: 5,730,304 bytes
- Reachable ARM instruction bytes mapped and compiled: 5,732 bytes
- Coverage: 0.1000%
- Recompiled functions registered: 68
- Byte-map rows: 1,433
- Unsupported mapped instructions: 0
- First real service boundary reached: SVC 0x21
- Guest PC after the service boundary: 0x00101564

The build writes generated/mk7_entry.map.csv beneath the selected build directory. Each row records the owning function address, instruction address, raw word, decoded operation, and support status. The map and generated C++ are local build artifacts and must not be committed.

Bootstrap discovery follows reachable ARM control flow from 0x00100000, registers direct call and tail-call targets as functions, follows conditional intra-function edges, and fails the build if the requested MK7_COVERAGE_PERMILLE target cannot be reached or lowered.
