# Native coverage

Coverage is measured against the verified USA Rev2 decompressed ExeFS code.bin, not from the CIA container or generated C++ size. Discovery is bounded by the authoritative 0x004EB57C-byte executable text region from the ExHeader, so it cannot inflate coverage by decoding read-only data.

## Current gate

- Code image: 5,730,304 bytes
- Reachable ARM instruction bytes mapped and compiled: 4,526,956 bytes
- Coverage: 79.0003%
- Recompiled functions registered: 29,954
- Byte-map rows: 1,131,739
- Unsupported mapped instructions: 0
- First real service boundary reached: SVC 0x21
- Guest PC after the service boundary: 0x00101564

The generated C++ is divided into 64 translation units so this scale remains buildable with bounded compiler memory. During generation, the recompiler rejects malformed or duplicate mappings, unsupported instructions, and byte coverage below the configured gate. The build writes generated/mk7_entry.map.csv beneath the selected build directory. Each row records the owning function address, instruction address, raw word, decoded operation, and support status. The map and generated C++ are local build artifacts and must not be committed.

Bootstrap discovery follows reachable ARM control flow from 0x00100000, registers direct call and tail-call targets as functions, follows conditional intra-function edges and validated PC-relative callback pointers, then conservatively seeds unclaimed ARM prologues/veneers when the rooted closure is exhausted, and fails the build if the requested MK7_COVERAGE_PERMILLE target cannot be reached or lowered.
