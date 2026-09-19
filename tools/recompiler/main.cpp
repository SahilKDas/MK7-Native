#include "arm_codegen.h"
#include "arm_decode.h"
#include "armv6k_decode.hpp"
#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {
struct Function { std::uint32_t address{}, size{}; std::string name; bool thumb{}; };
struct Options { std::filesystem::path input, output, symbols; std::uint32_t image_base=0x100000, entry=0x100000; };
std::uint32_t number(std::string_view s) {
  while(!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
  while(!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
  auto base=10; if(s.starts_with("0x")||s.starts_with("0X")){s.remove_prefix(2);base=16;}
  else if(s.find_first_of("abcdefABCDEF")!=s.npos) base=16;
  std::uint32_t v{}; auto [p,e]=std::from_chars(s.data(),s.data()+s.size(),v,base);
  if(e!=std::errc{}||p!=s.data()+s.size()) throw std::runtime_error("invalid integer: "+std::string(s));
  return v;
}
Options options(int ac,char** av) {
  Options o;
  for(int n=1;n<ac;++n){std::string_view a=av[n]; auto next=[&](){if(++n>=ac)throw std::runtime_error("missing argument");return std::string_view(av[n]);};
    if(a=="--input")o.input=next(); else if(a=="--output")o.output=next(); else if(a=="--symbols")o.symbols=next();
    else if(a=="--image-base")o.image_base=number(next()); else if(a=="--entry")o.entry=number(next()); else throw std::runtime_error("unknown argument: "+std::string(a));
  }
  if(o.input.empty()||o.output.empty())throw std::runtime_error("--input and --output are required"); return o;
}
std::uint32_t word(std::span<const std::uint8_t>b,std::size_t x){return std::uint32_t(b[x])|(std::uint32_t(b[x+1])<<8)|(std::uint32_t(b[x+2])<<16)|(std::uint32_t(b[x+3])<<24);}
std::string ident(std::string s,std::uint32_t a){
  if(s.empty() || s=="sub") { std::ostringstream q;q<<"sub_"<<std::hex<<a;s=q.str(); }
  for(auto& c:s)if(!std::isalnum(static_cast<unsigned char>(c))&&c!='_')c='_';
  if(std::isdigit(static_cast<unsigned char>(s.front())))s="_"+s; return "mk7_"+s;
}
std::vector<Function> load_symbols(const std::filesystem::path&p) {
  std::vector<Function> out; if(p.empty())return out; std::ifstream f(p); if(!f)throw std::runtime_error("cannot open symbol map");
  std::string line; while(std::getline(f,line)){auto hash=line.find_first_of("#;");if(hash!=line.npos)line.resize(hash);for(auto&c:line)if(c==',')c=' ';
    std::istringstream s(line);std::string a,b,c,d;if(!(s>>a>>b))continue;Function fn;fn.address=number(a);
    try{fn.size=number(b);if(!(s>>c))c="sub";fn.name=c;}catch(...){fn.name=b;if(s>>c)try{fn.size=number(c);}catch(...){}}
    if(s>>d)fn.thumb=(d=="thumb"||d=="T");out.push_back(fn);
  }
  std::sort(out.begin(),out.end(),[](auto&a,auto&b){return a.address<b.address;});
  for(std::size_t i=0;i<out.size();++i)if(!out[i].size&&i+1<out.size())out[i].size=out[i+1].address-out[i].address;
  return out;
}
std::vector<Function> discover(std::span<const std::uint8_t>b,const Options&o) {
  std::map<std::uint32_t,Function> found; std::vector<std::uint32_t> queue{o.entry}; std::set<std::uint32_t> seen;
  while(!queue.empty()&&found.size()<2){auto start=queue.back();queue.pop_back();if(!seen.insert(start).second)continue;if(start<o.image_base)continue;
    auto off=std::size_t(start-o.image_base);if(off+4>b.size())continue;std::uint32_t size=0;
    for(unsigned count=0;count<1024&&off+size+4<=b.size();++count){auto pc=start+size;auto ins=armv4t::ArmDecoder::decode(word(b,off+size),pc);size+=4;
      if(ins.is_call&&!ins.is_indirect && found.empty() && queue.empty()) queue.push_back(ins.branch_target&~1u);
      if(ins.is_return||(ins.is_branch&&!ins.is_call&&ins.cond==armv4t::Cond::AL))break;
    }
    found.emplace(start,Function{start,size,"sub",false});
  }
  std::vector<Function> r;for(auto&[_,f]:found)r.push_back(f);return r;
}
std::string special(const mk7::armv6k::Instruction&i){
  std::ostringstream s; using mk7::armv6k::Op;
  if(i.op==Op::Rev)s<<"g_cpu.R["<<unsigned(i.rd)<<"] = runtime_rev(g_cpu.R["<<unsigned(i.rm)<<"]);\n";
  else if(i.op==Op::Rev16)s<<"g_cpu.R["<<unsigned(i.rd)<<"] = runtime_rev16(g_cpu.R["<<unsigned(i.rm)<<"]);\n";
  else if(i.op==Op::Revsh)s<<"g_cpu.R["<<unsigned(i.rd)<<"] = runtime_revsh(g_cpu.R["<<unsigned(i.rm)<<"]);\n";
  else if(i.op==Op::Ldrex)s<<"g_cpu.R["<<unsigned(i.rd)<<"] = runtime_ldrex(g_cpu.R["<<unsigned(i.rn)<<"]);\n";
  else if(i.op==Op::Strex)s<<"g_cpu.R["<<unsigned(i.rd)<<"] = runtime_strex(g_cpu.R["<<unsigned(i.rn)<<"], g_cpu.R["<<unsigned(i.rm)<<"]);\n";
  else if(i.op==Op::Cps)s<<"runtime_cps("<<(i.enable?"true":"false")<<", 0x"<<std::hex<<(i.raw&0xe0u)<<"u);\n";
  else if(i.op==Op::Setend)s<<"runtime_setend("<<(i.big_endian?"true":"false")<<");\n"; return s.str();
}
bool emit(const Options&o){
  std::ifstream in(o.input,std::ios::binary);if(!in)throw std::runtime_error("cannot open code.bin");
  std::vector<std::uint8_t>b{std::istreambuf_iterator<char>(in),{}};auto fs=load_symbols(o.symbols);if(fs.empty())fs=discover(b,o);
  if(std::none_of(fs.begin(),fs.end(),[&](auto&f){return f.address==o.entry;}))throw std::runtime_error("entry absent from function metadata");
  std::unordered_map<std::uint64_t,std::string> names;for(auto&f:fs){f.name=ident(f.name,f.address);names[(std::uint64_t(f.address)<<1)|f.thumb]=f.name;}
  std::filesystem::create_directories(o.output.parent_path());std::ofstream out(o.output);if(!out)throw std::runtime_error("cannot create output");
  out<<"// Generated locally from the external ROM. Never commit this file.\n#include <mk7/recomp/runtime.hpp>\n\n";
  for(auto&f:fs)out<<"extern \"C\" void "<<f.name<<"();\n";
  out<<"\nextern \"C\" const CtrGeneratedFunction mk7_generated_functions[] = {\n";for(auto&f:fs)out<<"  {0x"<<std::hex<<f.address<<"u, "<<(f.thumb?"true":"false")<<", &"<<f.name<<"},\n";out<<"};\nextern \"C\" const std::size_t mk7_generated_function_count = sizeof(mk7_generated_functions)/sizeof(mk7_generated_functions[0]);\n\n";
  bool ok=true;for(auto&f:fs){auto off=std::size_t(f.address-o.image_base);if(off+f.size>b.size())throw std::runtime_error("function outside code image");
    out<<"extern \"C\" void "<<f.name<<"() {\n";armv4t::CodegenCtx ctx;ctx.names_by_key=&names;ctx.current_function_addr=f.address;ctx.current_function_end_addr=f.address+f.size;ctx.current_function_thumb=f.thumb;
    for(std::uint32_t x=0;x<f.size;x+=4){auto pc=f.address+x; out<<"L_"<<std::hex; out.width(8); out.fill('0'); out<<pc<<":\n"; auto raw=word(b,off+x);auto ext=mk7::armv6k::decode(raw,pc);if(ext.op!=mk7::armv6k::Op::Base)out<<special(ext);else{bool ni=false;out<<armv4t::ArmCodegen::emit_instr(armv4t::ArmDecoder::decode(raw,pc),ctx,&ni);ok&=!ni;}}
    out<<"}\n\n";
  }
  out<<"extern \"C\" void mk7_recomp_block_100000(){ "<<names[(std::uint64_t(o.entry)<<1)]<<"(); }\n";return ok;
}
}
int main(int ac,char**av){try{return emit(options(ac,av))?0:1;}catch(const std::exception&e){std::cerr<<"mk7-recompile: "<<e.what()<<"\n";return 2;}}
