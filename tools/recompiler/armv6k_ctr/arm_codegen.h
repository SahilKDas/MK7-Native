// arm_codegen.h — IR → C code emission.
//
// Lowers each decoded `Instr` to a sequence of C statements that
// operate on the recomp ABI (g_cpu / bus_read_* / bus_write_* /
// arm_shift_* / arm_set_* / runtime_dispatch — see runtime_arm.h).
//
// The output is meant to be dropped directly into a `void fname(void)`
// body emitted by tools/gba_recompile/main.cpp. Each call to
// emit_instr returns a block of C source that:
//   - wraps in `if (arm_cond_passes(...))` for non-AL conditions,
//   - reads operands from g_cpu (with PC = pc+8 / pc+4 baked in
//     statically when R15 is read in operand position),
//   - writes results to g_cpu and (when the instruction writes PC)
//     emits a trailing `return;` so the runtime exec loop re-enters
//     runtime_dispatch with the new PC.
//
// PRINCIPLES.md "Interpreter is informative, never load-bearing":
// the interpreter is the semantic reference but is NEVER called from
// generated code. If emit_instr can't lower an op yet it returns
// `not_implemented = true` and the caller emits a
// `runtime_unimplemented_op(...)` abort — never an interpreter
// fallback.

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "arm_ir.h"

namespace armv4t {

struct CodegenResult {
    std::string text;          // emitted C source for the block
    bool not_implemented;      // true if at least one Instr fell through
    std::size_t emitted_count;
};

// A tiny leaf guest function whose body may be expanded at direct BL
// call sites instead of paying a dispatch/link round trip (beads-yjp.67).
//
// `body` holds every instruction BEFORE the terminating `bx lr`;
// `terminator_pc` is that `bx lr`'s guest PC. `owner_symbol` is the
// generated C function the dispatcher would enter for a transfer to
// `addr` (the leaf's superblock leader).
//
// The expansion is NEVER unconditional: it is admitted at run time only
// when the call site's own B2 link slot has already resolved this target
// to exactly `owner_symbol` under the current link epoch AND that row's
// content guard is still live. That is the same byte-identity proof a
// linked dispatch performs, so an inlined body can never outlive the
// proof that licenses it; any other outcome falls back to the ordinary
// link call, which re-resolves and re-proves.
struct InlineLeaf {
    std::vector<Instr> body;
    uint32_t addr = 0;
    uint32_t terminator_pc = 0;
    bool thumb = false;
    std::string owner_symbol;
};

// Context passed to per-instruction emission. The function-name map
// lets direct B/BL targets lower to a C function call when the
// target is known to be a recompiled function in the same dispatch
// table; unknown targets fall back to runtime_dispatch.
struct CodegenCtx {
    // Key is (addr << 1) | thumb_bit. Direct B/BL preserves the
    // current instruction-set state, so codegen can resolve the
    // correct same-mode callee even when ARM and THUMB entries share
    // the same numeric address.
    const std::unordered_map<uint64_t, std::string>* names_by_key = nullptr;
    uint32_t current_function_addr = 0xFFFFFFFFu;
    uint32_t current_function_end_addr = 0xFFFFFFFFu;
    bool current_function_thumb = false;
    bool force_bx_c_return = false;
    bool trace_live_transfers = false;
    // MSR CPSR fast path: write the masked CPSR bytes inline when the
    // write provably cannot change mode (so no register bank swap can be
    // required) and the current mode is privileged. Every other case
    // still calls runtime_msr_cpsr. Set false to force the faithful
    // out-of-line path at every site.
    bool msr_fast_path = true;
    // Tiny-leaf body inlining at direct BL sites, keyed by
    // (addr << 1) | thumb — the same key shape as names_by_key.
    const std::unordered_map<uint64_t, InlineLeaf>* inline_leaves = nullptr;
    // Incremented once per emitted inline expansion, so the emitter can
    // report how many BL sites were inlined rather than asserting it.
    unsigned* inline_leaf_sites = nullptr;
};

class ArmCodegen {
public:
    // Emit C source for one decoded instruction. `not_implemented`
    // is set true if the IR shape is not yet lowered. The string
    // ends in a newline; callers may indent it as they please.
    static std::string emit_instr(const Instr& i, const CodegenCtx& ctx,
                                  bool* not_implemented);

    // Block-level helper: invoke emit_instr on every entry,
    // concatenate the result, and report whether any instruction
    // fell through.
    static CodegenResult emit_block(const std::vector<Instr>& block,
                                    const CodegenCtx& ctx);
};

}  // namespace armv4t
