#pragma once
#include <windows.h>
#include <cstdint>
namespace lab {
inline constexpr wchar_t PipeName[] = LR"(\\.\pipe\ExecutorLab_v1)";
inline constexpr std::uint32_t MaxScript = 131072;
inline constexpr std::uint32_t MaxOutput = 65536;
using RunFn = std::uint32_t (__cdecl *)(const char*,std::uint32_t,char*,std::uint32_t,std::uint32_t*);
inline bool readExact(HANDLE h,void* p,std::uint32_t n) {
 auto* b=static_cast<char*>(p);
 while(n) { DWORD k=0; if(!ReadFile(h,b,n,&k,nullptr)||!k)return false; b+=k; n-=k; }
 return true;
}
inline bool writeExact(HANDLE h,const void* p,std::uint32_t n) {
 auto* b=static_cast<const char*>(p);
 while(n) { DWORD k=0; if(!WriteFile(h,b,n,&k,nullptr)||!k)return false; b+=k; n-=k; }
 return true;
}
}
