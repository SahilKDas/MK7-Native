#include <mk7/recomp/runtime.hpp>
#include <array>
#include <cassert>
extern "C" const CtrGeneratedFunction mk7_generated_functions[] = {};
extern "C" const std::size_t mk7_generated_function_count = 0;
int main(){
 std::array<std::byte,4096> memory{};ctr_runtime_initialize(memory);
 runtime_coproc_write(15,0,13,0,3,0x12345678);assert(runtime_coproc_read(15,0,13,0,3)==0x12345678);
 bus_write_u32(0x100,0xfeedface);assert(runtime_ldrex(0x100)==0xfeedface);assert(runtime_strex(0x100,0x11223344)==0);assert(bus_read_u32(0x100)==0x11223344);assert(runtime_strex(0x100,0)==1);
 runtime_cps(false,0x80);assert(g_cpu.cpsr&CPSR_I_BIT);runtime_cps(true,0x80);assert(!(g_cpu.cpsr&CPSR_I_BIT));
 ctr_runtime_set_input(0x123u,-0.5f,0.75f);auto snapshot=ctr_runtime_snapshot();assert(snapshot.input_buttons==0x123u&&snapshot.circle_x==-0.5f&&snapshot.circle_y==0.75f);
 runtime_setend(true);assert(g_cpu.cpsr&CPSR_E_BIT);runtime_setend(false);assert(!(g_cpu.cpsr&CPSR_E_BIT));
 assert(runtime_rev(0x11223344)==0x44332211);assert(runtime_uxth(0x89abcdef,0)==0xcdef);assert(runtime_uxtb(0x11223344,8)==0x33);assert(runtime_sxth(0x00008001,0)==0xffff8001);assert(runtime_rev16(0x11223344)==0x22114433);
}
