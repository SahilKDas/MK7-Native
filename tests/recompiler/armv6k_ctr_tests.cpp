#include "armv6k_decode.hpp"
#include <cassert>
using namespace mk7::armv6k;
int main() {
  { auto i=decode(0xe6bf1f32u,0x1000); assert(i.op==Op::Rev && i.rd==1 && i.rm==2); }
  { auto i=decode(0xe6bf1fb2u,0x1004); assert(i.op==Op::Rev16); }
  { auto i=decode(0xe6ff1fb2u,0x1008); assert(i.op==Op::Revsh); }
  { auto i=decode(0xe1912f9fu,0x100c); assert(i.op==Op::Ldrex && i.rn==1 && i.rd==2); }
  { auto i=decode(0xe1812f93u,0x1010); assert(i.op==Op::Strex && i.rn==1 && i.rd==2 && i.rm==3); }
  { auto i=decode(0xf1010200u,0x1014); assert(i.op==Op::Setend && i.big_endian); }
  { auto i=decode(0xe3a00000u,0x1018); assert(i.op==Op::Base); }
}
