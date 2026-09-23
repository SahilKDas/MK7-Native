#include "armv6k_decode.hpp"
namespace mk7::armv6k {
Instruction decode(std::uint32_t w, std::uint32_t pc) noexcept {
  Instruction i{.raw=w,.pc=pc,.rd=static_cast<std::uint8_t>((w>>12)&15),.rn=static_cast<std::uint8_t>((w>>16)&15),.rm=static_cast<std::uint8_t>(w&15)};
  if ((w & 0x0e000e00u)==0x0c000a00u) i.op=Op::VfpLoadStore;
  else if ((w & 0x0fff0ff0u)==0x06bf0f30u) i.op=Op::Rev;
  else if ((w & 0x0fff0ff0u)==0x06bf0fb0u) i.op=Op::Rev16;
  else if ((w & 0x0fff0ff0u)==0x06ff0fb0u) i.op=Op::Revsh;
  else if ((w & 0x0ff00fffu)==0x01900f9fu) i.op=Op::Ldrex;
  else if ((w & 0x0ff00ff0u)==0x01800f90u) i.op=Op::Strex;
  else if ((w & 0x0ff00fffu)==0x01f00f9fu) i.op=Op::Ldrexh;
  else if ((w & 0x0ff00ff0u)==0x01e00f90u) i.op=Op::Strexh;
  else if ((w & 0x0ff00fffu)==0x01b00f9fu) i.op=Op::Ldrexd;
  else if ((w & 0x0ff00ff0u)==0x01a00f90u) i.op=Op::Strexd;
  else if ((w & 0x0fff03f0u)==0x06ef0070u) i.op=Op::Uxtb;
  else if ((w & 0x0fff03f0u)==0x06ff0070u) i.op=Op::Uxth;
  else if ((w & 0x0fff03f0u)==0x06af0070u) i.op=Op::Sxtb;
  else if ((w & 0x0fff03f0u)==0x06bf0070u) i.op=Op::Sxth;
  else if ((w & 0x0ff00070u)==0x06800010u) i.op=Op::Pkhbt;
  else if ((w & 0x0fe00030u)==0x06e00010u) i.op=Op::Usat;
  else if ((w & 0x0ff003f0u)==0x06f00070u) i.op=Op::Uxtah;
  else if ((w & 0x0ff003f0u)==0x06b00070u) i.op=Op::Sxtah;
  else if ((w & 0x0ff003f0u)==0x06e00070u) i.op=Op::Uxtab;
  else if ((w & 0x0ff000f0u)==0x07f000f0u) i.op=Op::Udf;
  else if ((w & 0xfffffdffu)==0xf1010000u) { i.op=Op::Setend; i.big_endian=(w&0x200u)!=0; }
  else if ((w & 0xfff10020u)==0xf1000000u) { i.op=Op::Cps; i.enable=(w&0x00080000u)==0; }
  return i;
}
}
