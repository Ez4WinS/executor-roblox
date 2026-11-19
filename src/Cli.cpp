// Local-only Luau CLI for ExecutorLab. Does not access RobloxPlayerBeta.exe.
#include "Protocol.h"
#include <windows.h>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {
struct OwnedHost {
    HANDLE process=nullptr;
    ~OwnedHost() {
        if(!process)return;
        DWORD code=STILL_ACTIVE;
        if(GetExitCodeProcess(process,&code)&&code==STILL_ACTIVE) {
            TerminateProcess(process,0);
            WaitForSingleObject(process,1500);
        }
        CloseHandle(process);
    }
};
bool ready() { return WaitNamedPipeW(lab::PipeName,150)!=FALSE; }

bool startHost(OwnedHost& owned,std::wstring& error) {
    if(ready())return true;
    wchar_t executable[MAX_PATH]{};
    DWORD n=GetModuleFileNameW(nullptr,executable,MAX_PATH);
    if(!n||n>=MAX_PATH){error=L"Cannot locate CLI";return false;}
    auto folder=std::filesystem::path(executable).parent_path();
    auto host=folder/L"TestHost.exe";
    if(!std::filesystem::is_regular_file(host)){error=L"TestHost.exe missing";return false;}
    std::wstring command=L"\""+host.wstring()+L"\"";
    STARTUPINFOW si{};si.cb=sizeof(si);
    PROCESS_INFORMATION pi{};
    if(!CreateProcessW(host.c_str(),command.data(),nullptr,nullptr,FALSE,
      CREATE_NO_WINDOW,nullptr,folder.c_str(),&si,&pi)) {
        error=L"Could not start TestHost.exe";return false;
    }
    CloseHandle(pi.hThread);
    owned.process=pi.hProcess;
    for(int attempt=0;attempt<50;++attempt) {
        if(ready())return true;
        DWORD code=STILL_ACTIVE;
        if(GetExitCodeProcess(owned.process,&code)&&code!=STILL_ACTIVE) {
            error=L"TestHost.exe exited";return false;
        }
        Sleep(75);
    }
    error=L"TestHost.exe startup timed out";
    return false;
}
int runFile(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary);
    if(!file) {
        std::wcerr<<L"Cannot read script: "<<path.wstring()<<L"\n";return 2;
    }
    std::string script((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
    if(script.empty()||script.size()>lab::MaxScript) {
        std::wcerr<<L"Invalid script size (1.."<<lab::MaxScript<<L" bytes)\n";
        return 3;
    }
    OwnedHost host;
    std::wstring error;
    if(!startHost(host,error)) {
        std::wcerr<<L"Local runtime unavailable: "<<error<<L"\n";
        return 4;
    }
    HANDLE pipe=CreateFileW(lab::PipeName,GENERIC_READ|GENERIC_WRITE,0,nullptr,
                            OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(pipe==INVALID_HANDLE_VALUE) {
        std::cerr<<"Cannot connect to local runtime\n";return 5;
    }
    std::uint32_t length=static_cast<std::uint32_t>(script.size());
    std::uint32_t status=4,outSize=0;
    bool ok=lab::writeExact(pipe,&length,4)
         &&lab::writeExact(pipe,script.data(),length)
         &&lab::readExact(pipe,&status,4)
         &&lab::readExact(pipe,&outSize,4);
    std::string output;
    if(ok&&outSize<=lab::MaxOutput) {
        output.resize(outSize);
        ok=outSize==0||lab::readExact(pipe,output.data(),outSize);
    }else ok=false;
    CloseHandle(pipe);
    if(!ok){std::cerr<<"Invalid runtime response\n";return 6;}
    std::cout<<output;
    return status==0?0:7;
}
}
int wmain(int argc,wchar_t* argv[]) {
    if(argc==2&&std::wstring(argv[1])==L"--help") {
        std::wcout<<L"ExecutorLab local Luau CLI\n"
                    L"Usage: ExecutorLabCli.exe local <script.luau>\n"
                    L"Runs only inside the owned local TestHost.\n";
        return 0;
    }
    if(argc!=3||std::wstring(argv[1])!=L"local") {
        std::wcerr<<L"Usage: ExecutorLabCli.exe local <script.luau>\n";
        return 1;
    }
    return runFile(std::filesystem::path(argv[2]));
}
