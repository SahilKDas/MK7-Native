# Native coverage

Coverage is measured from the verified USA Rev2 decompressed ExeFS code.bin, not from the CIA container or generated C++ size.

## Current gate

- Code image: 5,730,304 bytes
- Reachable ARM instruction bytes mapped and compiled: 287,232 bytes
- Coverage: 5.0125%
- Recompiled functions registered: 2,032
- Byte-map rows: 71,808
- Unsupported mapped instructions: 0
- First real service boundary reached: SVC 0x21
- Guest PC after the service boundary: 0x00101564

After C++ generation, the Zig map auditor independently rejects malformed rows, duplicates, unsupported instructions, or byte coverage below the configured gate. The build writes generated/mk7_entry.map.csv beneath the selected build directory. Each row records the owning function address, instruction address, raw word, decoded operation, and support status. The map and generated C++ are local build artifacts and must not be committed.

Bootstrap discovery follows reachable ARM control flow from 0x00100000, registers direct call and tail-call targets as functions, follows conditional intra-function edges and validated PC-relative callback pointers, then conservatively seeds unclaimed ARM prologues/veneers when the rooted closure is exhausted, and fails the build if the requested MK7_COVERAGE_PERMILLE target cannot be reached or lowered.
