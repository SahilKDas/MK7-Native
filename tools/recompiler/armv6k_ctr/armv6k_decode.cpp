#include "armv6k_decode.hpp"
namespace mk7::armv6k {
Instruction decode(std::uint32_t w, std::uint32_t pc) noexcept {
  Instruction i{.raw=w,.pc=pc,.rd=static_cast<std::uint8_t>((w>>12)&15),.rn=static_cast<std::uint8_t>((w>>16)&15),.rm=static_cast<std::uint8_t>(w&15)};
  if ((w & 0x0fff0ff0u)==0x06bf0f30u) i.op=Op::Rev;
  else if ((w & 0x0fff0ff0u)==0x06bf0fb0u) i.op=Op::Rev16;
  else if ((w & 0x0fff0ff0u)==0x06ff0fb0u) i.op=Op::Revsh;
  else if ((w & 0x0ff00fffu)==0x01900f9fu) i.op=Op::Ldrex;
  else if ((w & 0x0ff00ff0u)==0x01800f90u) i.op=Op::Strex;
  else if ((w & 0xfffffdffu)==0xf1010000u) { i.op=Op::Setend; i.big_endian=(w&0x200u)!=0; }
  else if ((w & 0xfff10020u)==0xf1000000u) { i.op=Op::Cps; i.enable=(w&0x00080000u)==0; }
  return i;
}
}
