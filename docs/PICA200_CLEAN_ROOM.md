# PICA200 clean-room renderer notes

This document records public hardware behavior and an implementation sequence for
MK7-Native. It is a specification and test plan, not a translation of emulator
code. Claims marked **documented** come from the linked hardware references;
**to verify** means the references are incomplete or the required behavior has
not yet been checked on hardware. The renderer should reject or report unknown
state rather than silently treating a draw as complete.

## Current implementation boundary

At the time of writing, `src/recomp/pica200.cpp` parses command-list parameter
and header pairs, byte-masked register writes, repeated/consecutive writes,
and the two draw-trigger register IDs. It counts draws but does not fetch
vertices or shade/rasterize them. It also performs a memory fill and converts
several guest framebuffer pixel formats to host RGBA. GSP handles a subset of
shared GX queue commands in `src/recomp/ctr_services.inc`. The Vulkan host in
`src/host/application.cpp` uploads those CPU-produced pixels into a swapchain
image. There is no Vulkan graphics pipeline for guest triangles yet.

The command header behavior is **documented**: an initial parameter word and
header word form a command; extra parameters write either the same register or
consecutive registers, and command records are padded to eight-byte boundaries.
See [GPU internal registers, Overview](https://www.3dbrew.org/wiki/GPU/Internal_Registers#Overview).

## Hardware facts to build against

| Stage | Publicly documented behavior | Boundary needing verification |
| --- | --- | --- |
| Shader program | Instructions and operand descriptors are uploaded separately. Instruction formats encode opcode, source/destination indices, and descriptor ID; descriptors carry component swizzles and masks. Control-flow and conditional forms are part of the ISA. [Shader instruction set](https://www.3dbrew.org/wiki/Shader_Instruction_Set) | Exact results for undocumented opcodes and corner cases need hardware tests. The reference itself warns that it is reverse engineered and may contain mistakes. |
| Shader arithmetic | The apparent internal format has one sign bit, seven exponent bits, and sixteen mantissa bits. Hardware tests report subnormal flushing for arithmetic, non-IEEE infinities/NaNs in some operations, and instruction-dependent behavior. [Floating-point behavior](https://www.3dbrew.org/wiki/Shader_Instruction_Set#Floating-Point_Behavior) | Define per-instruction rounding and exceptional-value tests before replacing f24 arithmetic with host `float`. |
| Vertex input | Attribute format, buffer location/stride, index setup, fixed attributes, shader input maps, and output maps are register-controlled. [Geometry pipeline registers](https://www.3dbrew.org/wiki/GPU/Internal_Registers#Geometry_pipeline_registers_.280x200-0x27F.29), [Programming guide](https://www.3dbrew.org/wiki/GPU/Programming_Guide), [GPU pitfalls](https://www.3dbrew.org/wiki/GPU/Pitfalls) | Validate alignment, packed formats, and the interaction of output mask and output mapping with small hardware probes. |
| Texturing | Registers `0x080–0x0ff` control texture unit configuration, dimensions, address, wrapping/filtering, level of detail, and formats; other register groups configure combiners. [Texturing registers](https://www.3dbrew.org/wiki/GPU/Internal_Registers#Texturing_registers_.280x080-0x0FF.29) | Tiled layouts, border cases, mip selection, and less common formats require exact sample tests. |
| Fragment output | Registers `0x100–0x13f` control color operation, blend/logic operation, alpha test, stencil test/ops, depth/color masks, and framebuffer configuration. [Framebuffer registers](https://www.3dbrew.org/wiki/GPU/Internal_Registers#Framebuffer_registers_.280x100-0x13F.29) | Establish ordering of alpha, stencil, depth, blend, logic, and write mask with overlapping-triangle tests. |
| Cache/visibility | Framebuffer flush writes cached results to memory; invalidate discards cached content and is described as following a flush when changing/clearing the target. [Framebuffer flush](https://www.3dbrew.org/wiki/GPU/Internal_Registers#GPUREG_FRAMEBUFFER_FLUSH), [invalidate](https://www.3dbrew.org/wiki/GPU/Internal_Registers#GPUREG_FRAMEBUFFER_INVALIDATE) | Tie guest memory visibility to command ordering and GX completion, rather than assuming every register write is immediately visible. |

## Implementation sequence and acceptance gates

1. **Decode persistent GPU state.** Give named, typed state to the documented
   register families while retaining the raw register file. A draw snapshot
   should preserve the exact state at each trigger. Test masked writes,
   repeated/consecutive headers, upload ports, reset behavior, and malformed
   command bounds. Unsupported writes must remain observable.
2. **Execute vertex programs.** Implement shader instruction and descriptor
   decoding, swizzles, source addressing, uniforms, control flow, and f24
   arithmetic in a CPU reference interpreter. Test one instruction at a time,
   then full programs with branches and relative addressing. Keep instruction
   traces available for divergent results.
3. **Fetch and assemble primitives.** Decode attributes, fixed attributes,
   index buffers, output semantics, clipping, viewport transform, culling, and
   primitive restart/continuation state where documented. Verify triangles
   using positions/colors that make winding and interpolation obvious.
4. **Build a deterministic pixel reference.** Rasterize triangles on the CPU
   with perspective-correct interpolants, scissor, texture sampling, texture
   combiners, alpha test, depth/stencil, blend/logic operations, and color
   masks. Compare framebuffer bytes at command-list boundaries. Start with a
   solid triangle, then textured and overlapping triangles.
5. **Translate verified draws to Vulkan.** Reuse the decoded draw snapshot and
   CPU reference outputs as an oracle. Cache host shader/pipeline variants by
   shader program and relevant fixed-function state. Render to offscreen
   images in the guest formats or a losslessly convertible representation,
   then honor guest framebuffer flush and display-transfer ordering. A Vulkan
   path is accepted only when its output matches the reference for the same
   command stream within a documented tolerance.
6. **Expand unusual registers.** Lighting, fog, procedural textures, shadow
   behavior, geometry shaders, and uncommon formats should each get an
   isolated command-list test and a recorded support status. Avoid a blanket
   “register supported” claim when its state is merely stored.

For each gate, keep test inputs as synthetic command lists and generated vertex,
texture, and framebuffer bytes. Captures from a user's own ROM can be used
locally for diagnosis, but neither ROM content nor extracted assets belong in
the repository. Public docs establish the likely meaning of a register;
hardware observations establish the exact edge behavior.

## Source policy and open questions

The references above are community hardware documentation, including hardware
test results. Their uncertainty notes are part of the specification. The
[GPU pitfalls](https://www.3dbrew.org/wiki/GPU/Pitfalls) page specifically
warns about vertex stride and output-map interactions. Record a minimal
reproduction whenever behavior disagrees with a page, and label the result as
an observation rather than silently rewriting the documented rule.

Open questions that should be answered before asserting MK7 compatibility:

- Which shader instructions, texture formats, combiner modes, and specialized
  registers are actually exercised by the local MK7 command streams?
- What are the exact numerical tolerances for f24 conversion, interpolation,
  and texture filtering on real hardware?
- At which GX completion or cache operation should rendered guest memory become
  visible to the CPU and to the display transfer engine?
- Which guest formats can Vulkan represent directly, and which require an
  explicit conversion without changing blending/depth results?

Do not derive implementation code from GPL emulator sources. Public hardware
register and ISA descriptions, independently written tests, and observed
inputs/outputs are the basis for this implementation.
