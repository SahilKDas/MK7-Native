#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
struct ArmCpuState { std::uint32_t R[16]{}; std::uint32_t cpsr{}; std::array<std::uint32_t,6> spsr{}; };
struct CtrLinkSlot { void* resolved{}; std::uint32_t epoch{}, target{}, flags{}; };
struct CtrGeneratedFunction { std::uint32_t address; std::uint32_t end_address; bool thumb; void (*function)(); };
struct RuntimeSnapshot { std::uint32_t pc{}, last_dispatch{}, last_svc{}, input_buttons{}; float circle_x{}, circle_y{}; std::uint64_t instructions{}; bool unwinding{}; };
inline constexpr std::uint32_t CPSR_N_BIT=1u<<31,CPSR_Z_BIT=1u<<30,CPSR_C_BIT=1u<<29,CPSR_V_BIT=1u<<28,CPSR_Q_BIT=1u<<27,CPSR_E_BIT=1u<<9,CPSR_I_BIT=1u<<7,CPSR_F_BIT=1u<<6,CPSR_T_BIT=1u<<5;
inline constexpr std::uint32_t RUNTIME_TRACE_MEM_WRITE=2,RUNTIME_TRACE_BRANCH=5,CTR_LIVE_TRANSFER_BL=1,CTR_LIVE_TRANSFER_BX=2;
extern ArmCpuState g_cpu; extern std::array<std::uint64_t,2> g_insn_count; extern bool g_insn_hook_armed;
extern "C" const CtrGeneratedFunction mk7_generated_functions[]; extern "C" const std::size_t mk7_generated_function_count;
void ctr_runtime_initialize(std::span<std::byte>); void ctr_runtime_reset(); bool ctr_runtime_resume() noexcept; void ctr_runtime_scheduler_initialize() noexcept; bool ctr_runtime_scheduler_rotate() noexcept; bool ctr_runtime_reschedule_requested() noexcept;
std::uint32_t ctr_runtime_last_svc() noexcept; std::uint32_t ctr_runtime_last_dispatch() noexcept; RuntimeSnapshot ctr_runtime_snapshot() noexcept; void ctr_runtime_set_input(std::uint32_t,float,float) noexcept;
bool runtime_should_yield() noexcept; bool runtime_unwinding() noexcept; void runtime_insn_slow() noexcept; void runtime_tick(std::uint32_t) noexcept;
void runtime_call_push_return(std::uint32_t) noexcept; void runtime_call_cancel_return(std::uint32_t) noexcept; bool runtime_call_should_return(std::uint32_t) noexcept;
void runtime_link_call(CtrLinkSlot*) noexcept; void runtime_link_branch(CtrLinkSlot*) noexcept; void runtime_dispatch(std::uint32_t) noexcept; void runtime_dispatch_with_exchange(std::uint32_t) noexcept;
void runtime_swi(std::uint32_t) noexcept; void runtime_irq() noexcept; void runtime_exception_return(std::uint32_t) noexcept; void runtime_unimplemented_op(std::uint32_t,std::uint32_t) noexcept; void runtime_unimplemented_op(const char*,std::uint32_t) noexcept;
std::uint32_t ctr_code_cycles(std::uint32_t) noexcept; std::uint32_t ctr_refill_cycles(std::uint32_t) noexcept; std::uint32_t ctr_cycle_combine(std::uint32_t,std::uint32_t,std::uint32_t,bool) noexcept;
std::uint32_t runtime_code_cycles(std::uint32_t,bool) noexcept; std::uint32_t runtime_mem_cycles(std::uint32_t,std::uint32_t,bool) noexcept; std::uint32_t runtime_mul_cycles(std::uint32_t,bool,std::uint32_t) noexcept;
std::uint32_t runtime_coproc_read(std::uint8_t,std::uint8_t,std::uint8_t,std::uint8_t,std::uint8_t) noexcept;
void runtime_coproc_write(std::uint8_t,std::uint8_t,std::uint8_t,std::uint8_t,std::uint8_t,std::uint32_t) noexcept; void runtime_coproc_cdp(std::uint8_t,std::uint8_t,std::uint8_t,std::uint8_t,std::uint8_t,std::uint32_t) noexcept;
void runtime_vfp_load_store(std::uint32_t) noexcept; std::uint32_t runtime_vfp_word(unsigned) noexcept; void runtime_vfp_set_word(unsigned,std::uint32_t) noexcept;
std::uint32_t runtime_pkhbt(std::uint32_t,std::uint32_t,unsigned) noexcept; std::uint32_t runtime_usat(std::uint32_t,unsigned,unsigned,bool) noexcept; std::uint32_t runtime_uxtah(std::uint32_t,std::uint32_t,unsigned) noexcept; std::uint32_t runtime_sxtah(std::uint32_t,std::uint32_t,unsigned) noexcept;
std::uint32_t runtime_uxtab(std::uint32_t,std::uint32_t,unsigned) noexcept; void runtime_ldrexd(std::uint32_t,std::uint32_t&,std::uint32_t&) noexcept; std::uint32_t runtime_strexd(std::uint32_t,std::uint32_t,std::uint32_t) noexcept; void runtime_udf(std::uint32_t,std::uint32_t) noexcept;
std::uint32_t runtime_clz(std::uint32_t) noexcept; std::uint32_t runtime_rev(std::uint32_t) noexcept; std::uint32_t runtime_uxtb(std::uint32_t,unsigned) noexcept; std::uint32_t runtime_uxth(std::uint32_t,unsigned) noexcept; std::uint32_t runtime_sxtb(std::uint32_t,unsigned) noexcept; std::uint32_t runtime_sxth(std::uint32_t,unsigned) noexcept; std::uint32_t runtime_rev16(std::uint32_t) noexcept; std::uint32_t runtime_revsh(std::uint32_t) noexcept;
std::uint32_t runtime_ldrex(std::uint32_t) noexcept; std::uint32_t runtime_strex(std::uint32_t,std::uint32_t) noexcept; std::uint32_t runtime_ldrexh(std::uint32_t) noexcept; std::uint32_t runtime_strexh(std::uint32_t,std::uint32_t) noexcept; void runtime_cps(bool,std::uint32_t) noexcept; void runtime_setend(bool) noexcept;
std::uint8_t bus_read_u8(std::uint32_t) noexcept; std::uint16_t bus_read_u16(std::uint32_t) noexcept; std::uint32_t bus_read_u32(std::uint32_t) noexcept;
void bus_write_u8(std::uint32_t,std::uint8_t) noexcept; void bus_write_u16(std::uint32_t,std::uint16_t) noexcept; void bus_write_u32(std::uint32_t,std::uint32_t) noexcept;
std::uint32_t cpsr_c() noexcept; bool arm_cond_passes_i(std::uint32_t) noexcept;
std::uint32_t runtime_read_user_reg(unsigned) noexcept; void runtime_write_user_reg(unsigned,std::uint32_t) noexcept;
void arm_set_nzc_logic(std::uint32_t,std::uint32_t) noexcept; void arm_set_nzcv_add(std::uint32_t,std::uint32_t,std::uint32_t) noexcept; void arm_set_nzcv_adc(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t) noexcept; void arm_set_nzcv_sub(std::uint32_t,std::uint32_t,std::uint32_t) noexcept; void arm_set_nzcv_sbc(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t) noexcept;
bool runtime_slice_yield() noexcept; void runtime_trace_event(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t) noexcept; void runtime_msr_cpsr(std::uint32_t,std::uint32_t) noexcept;extern "C" void mk7_recomp_block_100000();
