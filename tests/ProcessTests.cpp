// End-to-end verification: launch TestHost.exe, send Luau, receive output.
#include "../src/Protocol.h"
#include <windows.h>
#include <filesystem>
#include <iostream>
#include <string>
int wmain() {
 wchar_t exe[MAX_PATH]{};
 if(!GetModuleFileNameW(nullptr,exe,MAX_PATH))return 1;
 auto folder=std::filesystem::path(exe).parent_path();
 auto host=folder/L"TestHost.exe";
 std::wstring command=L"\""+host.wstring()+L"\"";
 STARTUPINFOW si{};si.cb=sizeof(si);
 PROCESS_INFORMATION pi{};
 if(!CreateProcessW(host.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,
                    nullptr,folder.c_str(),&si,&pi)) {
  std::cerr<<"Cannot start TestHost: "<<GetLastError()<<"\n";return 2;
 }
 CloseHandle(pi.hThread);
 int result=0;
 bool ready=false;
 for(int i=0;i<50;++i) {
  if(WaitNamedPipeW(lab::PipeName,100)){ready=true;break;}
  Sleep(75);
 }
 if(!ready) {std::cerr<<"TestHost pipe not ready\n";result=3;}
 else {
  HANDLE pipe=CreateFileW(lab::PipeName,GENERIC_READ|GENERIC_WRITE,0,nullptr,
    OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(pipe==INVALID_HANDLE_VALUE){std::cerr<<"Cannot open pipe\n";result=4;}
  else {
   const std::string script="print('integration-ok'); return 42";
   std::uint32_t n=static_cast<std::uint32_t>(script.size()),status=4,length=0;
   bool ok=lab::writeExact(pipe,&n,4)&&lab::writeExact(pipe,script.data(),n)
        &&lab::readExact(pipe,&status,4)&&lab::readExact(pipe,&length,4);
   if(!ok||length>lab::MaxOutput)result=5;
   else {
    std::string response(length,'\0');
    if(!lab::readExact(pipe,response.data(),length))result=6;
    else if(status!=0||response.find("integration-ok")==std::string::npos||
            response.find("[Return] 42")==std::string::npos) {
      std::cerr<<"Bad response: "<<response<<"\n";result=7;
    }else std::cout<<"PASS: Host subprocess + DLL + named pipe + Luau VM\n";
   }
   CloseHandle(pipe);
  }
 }
 TerminateProcess(pi.hProcess,0);
 WaitForSingleObject(pi.hProcess,2000);
 CloseHandle(pi.hProcess);
 return result;
}
