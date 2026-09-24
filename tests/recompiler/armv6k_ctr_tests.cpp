#include "armv6k_decode.hpp"
#include "arm_decode.h"
#include "interpreter.h"
#include <array>
#include <cassert>
using namespace mk7::armv6k;
namespace {
struct Armv6Bus final : armv4t::Bus {
  std::array<unsigned char, 32> data{};
  bool supports_unaligned_access() const override { return true; }
  std::uint8_t read8(std::uint32_t a) override { return data.at(a); }
  std::uint16_t read16(std::uint32_t a) override { return read8(a) | (std::uint16_t(read8(a + 1)) << 8); }
  std::uint32_t read32(std::uint32_t a) override { return read16(a) | (std::uint32_t(read16(a + 2)) << 16); }
  void write8(std::uint32_t a, std::uint8_t v) override { data.at(a) = v; }
  void write16(std::uint32_t a, std::uint16_t v) override { write8(a, v); write8(a + 1, v >> 8); }
  void write32(std::uint32_t a, std::uint32_t v) override { write16(a, v); write16(a + 2, v >> 16); }
};
}
int main() {
  { auto i=decode(0xe6bf1f32u,0x1000); assert(i.op==Op::Rev && i.rd==1 && i.rm==2); }
  { auto i=decode(0xe6bf1fb2u,0x1004); assert(i.op==Op::Rev16); }
  { auto i=decode(0xe6ff1fb2u,0x1008); assert(i.op==Op::Revsh); }
  { auto i=decode(0xe1912f9fu,0x100c); assert(i.op==Op::Ldrex && i.rn==1 && i.rd==2); }
  { auto i=decode(0xe1812f93u,0x1010); assert(i.op==Op::Strex && i.rn==1 && i.rd==2 && i.rm==3); }
  { auto i=decode(0xe1f1cf9fu,0x1012); assert(i.op==Op::Ldrexh && i.rn==1 && i.rd==12); }
  { auto i=decode(0xe1e14f9cu,0x1014); assert(i.op==Op::Strexh && i.rn==1 && i.rd==4 && i.rm==12); }
  { auto i=decode(0xf1010200u,0x1014); assert(i.op==Op::Setend && i.big_endian); }
  { auto i=decode(0xe6ff3073u,0x1018); assert(i.op==Op::Uxth && i.rd==3 && i.rm==3); }
  { auto i=decode(0xe6ef2471u,0x101c); assert(i.op==Op::Uxtb && i.rd==2 && i.rm==1); }
  { auto i=decode(0xe3a00000u,0x1020); assert(i.op==Op::Base); }
  { auto i=decode(0xed9f0a3du,0x1024); assert(i.op==Op::VfpLoadStore); }
  { auto i=decode(0xed2d8b02u,0x1028); assert(i.op==Op::VfpLoadStore); }
  { assert(decode(0xe68ba01au,0x102c).op==Op::Pkhbt); }
  { assert(decode(0xe6e76011u,0x1030).op==Op::Usat); }
  { assert(decode(0xe6f70070u,0x1034).op==Op::Uxtah); }
  { assert(decode(0xe6b20070u,0x1038).op==Op::Sxtah); }
  { assert(decode(0xe6eb0079u,0x103c).op==Op::Uxtab); }
  { assert(decode(0xe1bc0f9fu,0x1040).op==Op::Ldrexd); }
  { assert(decode(0xe1ac6f90u,0x1044).op==Op::Strexd); }
  { assert(decode(0xe7f000f0u,0x1048).op==Op::Udf); }
  {
    Armv6Bus bus;
    bus.data = {0, 0, 0x41, 0x50, 0x54, 0x3a, 0x55, 0};
    armv4t::CPUState cpu{};
    cpu.R[1] = 2;
    auto ldr = armv4t::ArmDecoder::decode(0xe5912000u, 0x1000);
    assert(armv4t::Interpreter::step(cpu, bus, ldr) == armv4t::Interpreter::Result::Normal);
    assert(cpu.R[2] == 0x3a545041u);
    cpu.R[2] = 0x12345678u;
    cpu.R[1] = 3;
    auto str = armv4t::ArmDecoder::decode(0xe5812000u, 0x1004);
    assert(armv4t::Interpreter::step(cpu, bus, str) == armv4t::Interpreter::Result::Normal);
    assert(bus.read32(3) == 0x12345678u);
  }
}
