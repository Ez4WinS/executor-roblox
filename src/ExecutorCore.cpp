// Standalone Luau executor core. No attachment to the Roblox client.
#include "Protocol.h"
#include "Luau/Compiler.h"
#include "lua.h"
#include "lualib.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>

namespace {
constexpr std::size_t MemLimit=64u*1024u*1024u;
struct Context {
 std::size_t used=0;
 std::chrono::steady_clock::time_point deadline{};
 std::string output;
};
void* allocate(void* data,void* p,std::size_t oldSize,std::size_t newSize) {
 auto& c=*static_cast<Context*>(data);
 if(!newSize) {std::free(p);c.used-=std::min(c.used,oldSize);return nullptr;}
 if(newSize>oldSize && newSize-oldSize>MemLimit-c.used) return nullptr;
 void* q=std::realloc(p,newSize);
 if(!q)return nullptr;
 if(newSize>oldSize)c.used+=newSize-oldSize;
 else c.used-=std::min(c.used,oldSize-newSize);
 return q;
}
Context& context(lua_State* L) {
 return *static_cast<Context*>(lua_callbacks(L)->userdata);
}
void interrupt(lua_State* L,int gc) {
 if(gc>=0)return;
 if(std::chrono::steady_clock::now()>context(L).deadline)
   luaL_error(L,"Execution timed out (1500 ms)");
}
void append(Context& c,const std::string& s) {
 if(c.output.size()>=lab::MaxOutput-1)return;
 std::size_t left=lab::MaxOutput-1-c.output.size();
 c.output.append(s.data(),std::min(left,s.size()));
}
std::string value(lua_State* L,int i) {
 switch(lua_type(L,i)) {
 case LUA_TNIL:return "nil";
 case LUA_TBOOLEAN:return lua_toboolean(L,i)?"true":"false";
 case LUA_TSTRING:return lua_tostring(L,i);
 case LUA_TNUMBER: {std::ostringstream o;o<<std::setprecision(14)<<lua_tonumber(L,i);return o.str();}
 default:return std::string("<")+lua_typename(L,lua_type(L,i))+">";
 }
}
int printCall(lua_State* L) {
 auto& c=context(L);
 for(int i=1;i<=lua_gettop(L);++i) {if(i>1)append(c,"\t");append(c,value(L,i));}
 append(c,"\n");
 return 0;
}
int multiply(lua_State* L) {
 double a=luaL_checknumber(L,1),b=luaL_checknumber(L,2);
 lua_pushnumber(L,a*b);return 1;
}
int greet(lua_State* L) {
 const char* name=luaL_checkstring(L,1);
 std::string s=std::string("Hallo, ")+name+"!";
 lua_pushlstring(L,s.data(),s.size());return 1;
}
void initAPI(lua_State* L) {
 lua_pushcfunction(L,printCall,"print");lua_setglobal(L,"print");
 lua_newtable(L);
 lua_pushcfunction(L,multiply,"multiply");lua_setfield(L,-2,"multiply");
 lua_pushcfunction(L,greet,"greet");lua_setfield(L,-2,"greet");
 lua_setreadonly(L,-1,true);lua_setglobal(L,"Host");
 // Mock of game.Players.LocalPlayer. Not connected to any Roblox instance.
 lua_newtable(L);lua_newtable(L);lua_newtable(L);
 lua_pushstring(L,"TestPlayer");lua_setfield(L,-2,"Name");
 lua_pushinteger(L,12345);lua_setfield(L,-2,"UserId");
 lua_setreadonly(L,-1,true);lua_setfield(L,-2,"LocalPlayer");
 lua_setreadonly(L,-1,true);lua_setfield(L,-2,"Players");
 lua_setreadonly(L,-1,true);lua_setglobal(L,"game");
}
std::uint32_t run(Context& c,const std::string& source) {
 std::unique_ptr<lua_State,decltype(&lua_close)> vm(lua_newstate(allocate,&c),lua_close);
 if(!vm){append(c,"[Internal error] Could not create Luau VM\n");return 4;}
 lua_State* L=vm.get();
 lua_callbacks(L)->userdata=&c;
 lua_callbacks(L)->interrupt=interrupt;
 luaL_openlibs(L);
 initAPI(L);
 luaL_sandbox(L);
 lua_State* thread=lua_newthread(L);
 luaL_sandboxthread(thread);
 Luau::CompileOptions options;
 options.optimizationLevel=1;
 options.debugLevel=1;
 std::string bytecode=Luau::compile(source,options);
 if(luau_load(thread,"=user-script",bytecode.data(),bytecode.size(),0)!=0) {
  append(c,"[Syntax error] ");
  const char* e=lua_tostring(thread,-1);append(c,e?e:"Unknown compile error");append(c,"\n");
  return 1;
 }
 c.deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(1500);
 if(lua_pcall(thread,0,LUA_MULTRET,0)!=LUA_OK) {
  append(c,"[Runtime error] ");
  const char* e=lua_tostring(thread,-1);append(c,e?e:"Unknown runtime error");append(c,"\n");
  return 2;
 }
 for(int i=1;i<=std::min(lua_gettop(thread),8);++i) {
  append(c,"[Return] ");append(c,value(thread,i));append(c,"\n");
 }
 if(c.output.empty())append(c,"[OK] Script finished without output.\n");
 return 0;
}
void fill(const std::string& result,char* output,std::uint32_t cap,std::uint32_t* n) {
 *n=static_cast<std::uint32_t>(std::min(result.size(),static_cast<std::size_t>(cap-1)));
 if(*n)std::memcpy(output,result.data(),*n);
 output[*n]=0;
}
}
extern "C" __declspec(dllexport) std::uint32_t __cdecl RunLuauScript(
 const char* script,std::uint32_t length,char* output,std::uint32_t cap,std::uint32_t* written) noexcept {
 if(!output||cap<2||!written)return 3;
 *written=0;
 if(!script||!length||length>lab::MaxScript){
  fill("[Input error] Expected 1..131072 bytes.\n",output,cap,written);return 3;
 }
 Context c;
 try {
  std::uint32_t result=run(c,std::string(script,length));
  fill(c.output,output,cap,written);return result;
 }catch(const std::exception& e){
  fill(std::string("[Internal error] ")+e.what()+"\n",output,cap,written);return 4;
 }catch(...){
  fill("[Internal error] Unknown exception\n",output,cap,written);return 4;
 }
}
