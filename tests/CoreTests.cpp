#include "../src/Protocol.h"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
extern "C" __declspec(dllimport) std::uint32_t __cdecl RunLuauScript(
 const char*,std::uint32_t,char*,std::uint32_t,std::uint32_t*);
bool check(const char* name,const std::string& source,std::uint32_t expected,const std::string& fragment) {
 std::vector<char> out(lab::MaxOutput);std::uint32_t n=0;
 std::uint32_t code=RunLuauScript(source.data(),static_cast<std::uint32_t>(source.size()),
  out.data(),lab::MaxOutput,&n);
 std::string result(out.data(),std::min<std::size_t>(n,out.size()));
 const bool ok=code==expected&&result.find(fragment)!=std::string::npos;
 std::cout<<(ok?"PASS ":"FAIL ")<<name<<" status "<<code<<": "<<result<<'\n';
 return ok;
}
int main() {
 bool ok=true;
 ok &= check("demo","print(Host.multiply(6,7)); print(game.Players.LocalPlayer.Name)",0,"TestPlayer");
 ok &= check("return","return Host.greet('Entwickler')",0,"Hallo, Entwickler!");
 ok &= check("syntax","local =",1,"[Syntax error]");
 ok &= check("runtime","error('expected error')",2,"expected error");
 ok &= check("timeout","while true do end",2,"timed out");
 ok &= check("sandbox","print(io)",0,"nil");
 ok &= check("fresh_state","assert(custom == nil); custom = 100",0,"[OK]");
 ok &= check("isolated_state","assert(custom == nil)",0,"[OK]");
 return ok?0:1;
}
