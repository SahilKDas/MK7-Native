#include "armv6k_decode.hpp"
#include <cassert>
using namespace mk7::armv6k;
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
}
