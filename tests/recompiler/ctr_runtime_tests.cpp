#include <mk7/recomp/runtime.hpp>
#include <mk7/recomp/pica200.hpp>
#include <array>
#include <cassert>
#include <bit>
#include <vector>
extern "C" const CtrGeneratedFunction mk7_generated_functions[] = {};
extern "C" const std::size_t mk7_generated_function_count = 0;
int main(){
 std::vector<std::byte> memory(0x4000);ctr_runtime_initialize(memory);
 assert(bus_read_u8(0x1ff80014u)==1u);
 runtime_coproc_write(15,0,13,0,3,0x12345678);assert(runtime_coproc_read(15,0,13,0,3)==0x12345678);
 bus_write_u32(0x100,0xfeedface);assert(runtime_ldrex(0x100)==0xfeedface);assert(runtime_strex(0x100,0x11223344)==0);assert(bus_read_u32(0x100)==0x11223344);assert(runtime_strex(0x100,0)==1);
 bus_write_u16(0x120,0xabcd);assert(runtime_ldrexh(0x120)==0xabcd);assert(runtime_strexh(0x120,0x12345)==0);assert(bus_read_u16(0x120)==0x2345);assert(runtime_strexh(0x120,0)==1);
 runtime_write_user_reg(7,0x76543210);assert(runtime_read_user_reg(7)==0x76543210);
 g_cpu.R[0]=0x1000;g_cpu.R[1]=0;g_cpu.R[2]=0x1000;g_cpu.R[3]=3;runtime_swi(0x1);assert(g_cpu.R[0]==0&&g_cpu.R[1]==0x1000);
 g_cpu.R[0]=0xffffffffu;g_cpu.R[1]=0;runtime_swi(0x21);assert(g_cpu.R[0]==0&&g_cpu.R[1]>=0x100u);
 g_cpu.R[0]=g_cpu.R[1];runtime_swi(0x22);assert(g_cpu.R[0]==0);runtime_swi(0x35);assert(g_cpu.R[0]==0&&g_cpu.R[1]==1);
bus_write_u32(0x210,0x3a727265u);bus_write_u8(0x214,'f');bus_write_u8(0x215,0);g_cpu.R[1]=0x210;runtime_swi(0x2d);assert(g_cpu.R[0]==0&&g_cpu.R[1]>=0x100u);g_cpu.R[0]=g_cpu.R[1];runtime_swi(0x23);assert(g_cpu.R[0]==0);
bus_write_u32(0x200,0x3a767273u);bus_write_u8(0x204,0);g_cpu.R[1]=0x200;runtime_swi(0x2d);assert(g_cpu.R[0]==0);const auto srv_handle=g_cpu.R[1];
 runtime_coproc_write(15,0,13,0,3,0x300);bus_write_u32(0x380,0x00010002u);g_cpu.R[0]=srv_handle;runtime_swi(0x32);assert(g_cpu.R[0]==0&&bus_read_u32(0x384)==0);
 auto get_service=[&](const char* name){for(unsigned i=0;i<8;++i)bus_write_u8(0x384+i,0);unsigned length=0;while(name[length]&&length<8){bus_write_u8(0x384+length,std::uint8_t(name[length]));++length;}bus_write_u32(0x380,0x00050100u);bus_write_u32(0x38c,length);bus_write_u32(0x390,0);g_cpu.R[0]=srv_handle;runtime_swi(0x32);assert(bus_read_u32(0x384)==0);return bus_read_u32(0x38c);};
 auto ipc=[&](std::uint32_t handle,std::uint16_t command){bus_write_u32(0x380,std::uint32_t(command)<<16);g_cpu.R[0]=handle;runtime_swi(0x32);assert(g_cpu.R[0]==0);};
 const auto hid=get_service("hid:USER");ipc(hid,0x000a);assert(bus_read_u32(0x384)==0&&bus_read_u32(0x38c)>=0x100);ctr_runtime_set_input(0x55u,0.5f,-0.25f);assert(bus_read_u32(0x804)==0x55u);
 const auto gsp=get_service("gsp::Gpu");ipc(gsp,0x0013);assert(bus_read_u32(0x384)==0x2a07u&&bus_read_u32(0x390)>=0x100);ipc(gsp,0x0016);assert(bus_read_u32(0x384)==0);bus_write_u32(0x2000,0x100);bus_write_u32(0x2020,1);bus_write_u32(0x2024,0x3000);bus_write_u32(0x2028,8);bus_write_u32(0x3000,1);bus_write_u32(0x3004,0x000f022e);ipc(gsp,0x000c);assert(pica200_snapshot().draw_calls==1&&bus_read_u32(0x2000)==1);
 const auto fs=get_service("fs:USER");ipc(fs,0x0861);assert(bus_read_u32(0x384)==0);ipc(fs,0x080c);assert(bus_read_u32(0x384)==0);const auto archive_lo=bus_read_u32(0x388),archive_hi=bus_read_u32(0x38c);bus_write_u32(0x384,archive_lo);bus_write_u32(0x388,archive_hi);ipc(fs,0x080e);assert(bus_read_u32(0x384)==0);
 const auto apt=get_service("APT:U");ipc(apt,0x0002);assert(bus_read_u32(0x384)==0&&bus_read_u32(0x38c)>=0x100&&bus_read_u32(0x390)>=0x100);ipc(apt,0x0003);assert(bus_read_u32(0x384)==0);bus_write_u32(0x388,45);ipc(apt,0x004f);ipc(apt,0x0050);assert(bus_read_u32(0x388)==45);ipc(apt,0x0004);assert(bus_read_u32(0x384)==0);
 bus_write_u32(0x380,0x00050100u);bus_write_u32(0x384,0x3a646968u);bus_write_u32(0x388,0x52455355u);bus_write_u32(0x38c,8);bus_write_u32(0x390,0);g_cpu.R[0]=srv_handle;runtime_swi(0x32);assert(g_cpu.R[0]==0&&bus_read_u32(0x384)==0&&bus_read_u32(0x38c)>=0x100u);
 runtime_cps(false,0x80);assert(g_cpu.cpsr&CPSR_I_BIT);runtime_cps(true,0x80);assert(!(g_cpu.cpsr&CPSR_I_BIT));
 ctr_runtime_set_input(0x123u,-0.5f,0.75f);auto snapshot=ctr_runtime_snapshot();assert(snapshot.input_buttons==0x123u&&snapshot.circle_x==-0.5f&&snapshot.circle_y==0.75f);
 runtime_setend(true);assert(g_cpu.cpsr&CPSR_E_BIT);runtime_setend(false);assert(!(g_cpu.cpsr&CPSR_E_BIT));
 assert(runtime_clz(0x00010000)==15);
 runtime_vfp_set_word(0,0x3fc00000u);assert(runtime_coproc_read(10,0,0,0,0)==0x3fc00000u);runtime_coproc_write(10,0,0,0,0,0x40000000u);assert(runtime_vfp_word(0)==0x40000000u);
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
