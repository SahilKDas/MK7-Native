#include <mk7/recomp/runtime.hpp>
#include <array>
#include <cassert>
#include <bit>
extern "C" const CtrGeneratedFunction mk7_generated_functions[] = {};
extern "C" const std::size_t mk7_generated_function_count = 0;
int main(){
 std::array<std::byte,4096> memory{};ctr_runtime_initialize(memory);
 runtime_coproc_write(15,0,13,0,3,0x12345678);assert(runtime_coproc_read(15,0,13,0,3)==0x12345678);
 bus_write_u32(0x100,0xfeedface);assert(runtime_ldrex(0x100)==0xfeedface);assert(runtime_strex(0x100,0x11223344)==0);assert(bus_read_u32(0x100)==0x11223344);assert(runtime_strex(0x100,0)==1);
 bus_write_u16(0x120,0xabcd);assert(runtime_ldrexh(0x120)==0xabcd);assert(runtime_strexh(0x120,0x12345)==0);assert(bus_read_u16(0x120)==0x2345);assert(runtime_strexh(0x120,0)==1);
 runtime_write_user_reg(7,0x76543210);assert(runtime_read_user_reg(7)==0x76543210);
 runtime_cps(false,0x80);assert(g_cpu.cpsr&CPSR_I_BIT);runtime_cps(true,0x80);assert(!(g_cpu.cpsr&CPSR_I_BIT));
 ctr_runtime_set_input(0x123u,-0.5f,0.75f);auto snapshot=ctr_runtime_snapshot();assert(snapshot.input_buttons==0x123u&&snapshot.circle_x==-0.5f&&snapshot.circle_y==0.75f);
 runtime_setend(true);assert(g_cpu.cpsr&CPSR_E_BIT);runtime_setend(false);assert(!(g_cpu.cpsr&CPSR_E_BIT));
 assert(runtime_clz(0x00010000)==15);
 runtime_vfp_set_word(0,std::bit_cast<std::uint32_t>(1.5f));runtime_vfp_set_word(2,std::bit_cast<std::uint32_t>(2.25f));runtime_coproc_cdp(10,3,0,1,0,0xee301a01u);assert(std::bit_cast<float>(runtime_vfp_word(2))==3.75f);
 g_cpu.cpsr=0;arm_set_nzcv_adc(0xffffffffu,0u,1u,0u);assert((g_cpu.cpsr&(CPSR_Z_BIT|CPSR_C_BIT))==(CPSR_Z_BIT|CPSR_C_BIT));
 g_cpu.cpsr=0;arm_set_nzcv_adc(0x7fffffffu,0u,1u,0x80000000u);assert((g_cpu.cpsr&(CPSR_N_BIT|CPSR_V_BIT))==(CPSR_N_BIT|CPSR_V_BIT));
 assert(runtime_pkhbt(0xaaaabbbb,0xccccdddd,0)==0xccccbbbb);
 g_cpu.cpsr=0;assert(runtime_usat(300,8,0,false)==255);assert(g_cpu.cpsr&CPSR_Q_BIT);g_cpu.cpsr=0;assert(runtime_usat(std::uint32_t(-1),7,0,false)==0);assert(g_cpu.cpsr&CPSR_Q_BIT);
 assert(runtime_uxtah(1,0x1234ffff,0)==0x10000);assert(runtime_sxtah(1,0x1234ffff,0)==0);
 assert(runtime_uxtab(1,0x1234ffff,0)==0x100);bus_write_u32(0x180,0x11223344);bus_write_u32(0x184,0x55667788);std::uint32_t lo=0,hi=0;runtime_ldrexd(0x180,lo,hi);assert(lo==0x11223344&&hi==0x55667788);assert(runtime_strexd(0x180,0xaabbccdd,0xeeff0011)==0);assert(bus_read_u32(0x180)==0xaabbccdd&&bus_read_u32(0x184)==0xeeff0011);assert(runtime_strexd(0x180,0,0)==1);
 assert(runtime_rev(0x11223344)==0x44332211);assert(runtime_uxth(0x89abcdef,0)==0xcdef);assert(runtime_uxtb(0x11223344,8)==0x33);assert(runtime_sxth(0x00008001,0)==0xffff8001);assert(runtime_rev16(0x11223344)==0x22114433);
 g_cpu.R[13]=0x300;runtime_vfp_set_word(16,0x11223344);runtime_vfp_set_word(17,0x55667788);runtime_vfp_load_store(0xed2d8b02u);assert(g_cpu.R[13]==0x2f8);assert(bus_read_u32(0x2f8)==0x11223344);assert(bus_read_u32(0x2fc)==0x55667788);runtime_vfp_set_word(16,0);runtime_vfp_set_word(17,0);runtime_vfp_load_store(0xecbd8b02u);assert(g_cpu.R[13]==0x300);assert(runtime_vfp_word(16)==0x11223344);assert(runtime_vfp_word(17)==0x55667788);
}
