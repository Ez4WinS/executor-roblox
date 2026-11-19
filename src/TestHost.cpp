// This host loads a DLL in its own process. It does not inject any process.
#include "Protocol.h"
#include <windows.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
namespace {
bool serve(HANDLE pipe,lab::RunFn run) {
 std::uint32_t length=0;
 if(!lab::readExact(pipe,&length,4))return false;
 if(length==0||length>lab::MaxScript) {
  std::uint32_t status=3;
  const std::string text="[Input error] Script length invalid.\n";
  std::uint32_t n=static_cast<std::uint32_t>(text.size());
  lab::writeExact(pipe,&status,4);lab::writeExact(pipe,&n,4);
  lab::writeExact(pipe,text.data(),n);return false;
 }
 std::string code(length,'\0');
 if(!lab::readExact(pipe,code.data(),length))return false;
 std::vector<char> out(lab::MaxOutput);
 std::uint32_t n=0;
 const std::uint32_t status=run(code.data(),length,out.data(),lab::MaxOutput,&n);
 if(n>out.size())return false;
 bool ok=lab::writeExact(pipe,&status,4)&&lab::writeExact(pipe,&n,4)
   &&lab::writeExact(pipe,out.data(),n);
 std::cout<<"Run complete, status="<<status<<", output="<<n<<" bytes\n";
 return ok;
}
}
int wmain() {
 wchar_t executable[MAX_PATH]{};
 if(!GetModuleFileNameW(nullptr,executable,MAX_PATH))return 1;
 auto path=std::filesystem::path(executable).parent_path()/L"ExecutorCore.dll";
 HMODULE module=LoadLibraryExW(path.c_str(),nullptr,
  LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
 if(!module){std::cerr<<"Could not load ExecutorCore.dll: "<<GetLastError()<<"\n";return 2;}
 auto run=reinterpret_cast<lab::RunFn>(GetProcAddress(module,"RunLuauScript"));
 if(!run){std::cerr<<"Missing RunLuauScript export\n";FreeLibrary(module);return 3;}
 std::cout<<"ExecutorLab TestHost started. Start ExecutorGUI.exe next.\n"
            "No Roblox process is accessed. Press Ctrl+C to stop.\n";
 for(;;) {
  HANDLE pipe=CreateNamedPipeW(lab::PipeName,PIPE_ACCESS_DUPLEX,
   PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,
   1,4096,4096,0,nullptr);
  if(pipe==INVALID_HANDLE_VALUE){std::cerr<<"Pipe create failed\n";break;}
  BOOL connected=ConnectNamedPipe(pipe,nullptr)?TRUE:(GetLastError()==ERROR_PIPE_CONNECTED);
  if(connected)serve(pipe,run);
  FlushFileBuffers(pipe);
  DisconnectNamedPipe(pipe);CloseHandle(pipe);
 }
 FreeLibrary(module);return 4;
}
