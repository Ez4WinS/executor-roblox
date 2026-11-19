// Native Windows Luau editor. Connects ONLY to our TestHost named pipe.
#include "Protocol.h"
#include <windows.h>
#include <commdlg.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
namespace {
HWND editor=nullptr,output=nullptr,state=nullptr;
HWND buttons[4]{};
HFONT font=nullptr,mono=nullptr;
std::string utf8(const std::wstring& value) {
 if(value.empty())return {};
 int n=WideCharToMultiByte(CP_UTF8,0,value.data(),static_cast<int>(value.size()),nullptr,0,nullptr,nullptr);
 std::string out(n,'\0');
 if(n)WideCharToMultiByte(CP_UTF8,0,value.data(),static_cast<int>(value.size()),out.data(),n,nullptr,nullptr);
 return out;
}
std::wstring wide(const std::string& value) {
 if(value.empty())return {};
 int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0);
 if(!n)return L"Ungueltige UTF-8-Ausgabe";
 std::wstring out(n,L'\0');
 MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),out.data(),n);
 return out;
}
std::wstring readText(HWND h) {
 int n=GetWindowTextLengthW(h);
 std::wstring result(static_cast<std::size_t>(n)+1,L'\0');
 int used=GetWindowTextW(h,result.data(),n+1);
 result.resize(used);
 return result;
}
std::wstring lines(const std::wstring& text) {
 std::wstring out;
 for(std::size_t i=0;i<text.size();++i) {
  if(text[i]==L'\n'&&(i==0||text[i-1]!=L'\r'))out+=L'\r';
  out+=text[i];
 }
 return out;
}
void show(const std::wstring& text){SetWindowTextW(output,lines(text).c_str());}
HWND create(HWND parent,const wchar_t* cls,const wchar_t* label,DWORD style,DWORD ex,int id,HFONT f) {
 HWND h=CreateWindowExW(ex,cls,label,WS_CHILD|WS_VISIBLE|style,0,0,1,1,
  parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
 SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(f),TRUE);
 return h;
}
void arrange(HWND window) {
 RECT r{};GetClientRect(window,&r);
 int w=std::max(440,static_cast<int>(r.right)),h=std::max(380,static_cast<int>(r.bottom));
 const int m=16,gap=10,btnH=36;
 int bw=(w-2*m-3*gap)/4;
 for(int i=0;i<4;++i)MoveWindow(buttons[i],m+i*(bw+gap),m,bw,btnH,TRUE);
 int top=m+btnH+12;
 int available=h-top-55;
 int editH=available*2/3;
 MoveWindow(editor,m,top,w-2*m,editH,TRUE);
 MoveWindow(output,m,top+editH+gap,w-2*m,available-editH-gap,TRUE);
 MoveWindow(state,m,h-28,w-2*m,20,TRUE);
}
void execute() {
 auto script=utf8(readText(editor));
 if(script.empty()||script.size()>lab::MaxScript) {
  show(L"Bitte 1 bis 131072 Bytes Luau-Code eingeben.");return;
 }
 if(!WaitNamedPipeW(lab::PipeName,1000)) {
  show(L"TestHost.exe ist nicht gestartet. Bitte zuerst TestHost.exe oeffnen.");
  SetWindowTextW(state,L"Offline");return;
 }
 HANDLE pipe=CreateFileW(lab::PipeName,GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(pipe==INVALID_HANDLE_VALUE){show(L"Verbindung fehlgeschlagen.");return;}
 std::uint32_t n=static_cast<std::uint32_t>(script.size()),code=4,size=0;
 bool ok=lab::writeExact(pipe,&n,4)&&lab::writeExact(pipe,script.data(),n)
         &&lab::readExact(pipe,&code,4)&&lab::readExact(pipe,&size,4);
 if(ok&&size<=lab::MaxOutput) {
  std::string msg(size,'\0');
  ok=lab::readExact(pipe,msg.data(),size);
  if(ok)show(wide(msg));
 }else ok=false;
 CloseHandle(pipe);
 if(!ok){show(L"Die Antwort des TestHosts konnte nicht gelesen werden.");SetWindowTextW(state,L"Fehler");return;}
 const wchar_t* label=code==0?L"Erfolgreich":code==1?L"Syntaxfehler":
  code==2?L"Laufzeitfehler":L"Interner Fehler";
 SetWindowTextW(state,label);
}
bool dialog(HWND parent,bool save,wchar_t* path) {
 OPENFILENAMEW o{};
 o.lStructSize=sizeof(o);o.hwndOwner=parent;
 o.lpstrFilter=L"Luau (*.luau;*.lua)\0*.luau;*.lua\0Alle Dateien\0*.*\0";
 o.lpstrFile=path;o.nMaxFile=MAX_PATH;o.lpstrDefExt=L"luau";
 o.Flags=OFN_PATHMUSTEXIST|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
 return (save?GetSaveFileNameW(&o):GetOpenFileNameW(&o))!=FALSE;
}
void openFile(HWND parent) {
 wchar_t path[MAX_PATH]{};
 if(!dialog(parent,false,path))return;
 std::ifstream file(std::filesystem::path(path),std::ios::binary);
 if(!file){show(L"Datei konnte nicht geoeffnet werden.");return;}
 std::string data((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
 if(data.size()>lab::MaxScript){show(L"Datei ist zu gross (max. 128 KiB).");return;}
 SetWindowTextW(editor,lines(wide(data)).c_str());
 SetWindowTextW(state,L"Datei geladen");
}
void saveFile(HWND parent) {
 wchar_t path[MAX_PATH]{};
 if(!dialog(parent,true,path))return;
 std::ofstream file(std::filesystem::path(path),std::ios::binary);
 if(!file){show(L"Datei konnte nicht geschrieben werden.");return;}
 auto data=utf8(readText(editor));
 file.write(data.data(),static_cast<std::streamsize>(data.size()));
 SetWindowTextW(state,file?L"Gespeichert":L"Speichern fehlgeschlagen");
}
LRESULT CALLBACK windowProc(HWND h,UINT msg,WPARAM w,LPARAM l) {
 switch(msg) {
 case WM_CREATE:
  font=CreateFontW(-17,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
  mono=CreateFontW(-18,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,FIXED_PITCH,L"Consolas");
  buttons[0]=create(h,L"BUTTON",L"Ausfuehren",BS_PUSHBUTTON,0,101,font);
  buttons[1]=create(h,L"BUTTON",L"Oeffnen",BS_PUSHBUTTON,0,102,font);
  buttons[2]=create(h,L"BUTTON",L"Speichern",BS_PUSHBUTTON,0,103,font);
  buttons[3]=create(h,L"BUTTON",L"Konsole leeren",BS_PUSHBUTTON,0,104,font);
  editor=create(h,L"EDIT",
    L"-- ExecutorLab: Eigene Testumgebung, kein Roblox Client\r\n"
    L"print(\"Hallo!\")\r\n"
    L"print(\"6 * 7 =\", Host.multiply(6, 7))\r\n"
    L"print(\"Spieler:\", game.Players.LocalPlayer.Name)\r\n"
    L"return Host.greet(\"Entwickler\")",
    ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN|WS_VSCROLL|WS_HSCROLL|ES_AUTOHSCROLL,WS_EX_CLIENTEDGE,201,mono);
  output=create(h,L"EDIT",L"Zuerst TestHost.exe starten.",
    ES_MULTILINE|ES_READONLY|WS_VSCROLL|ES_AUTOVSCROLL,WS_EX_CLIENTEDGE,202,mono);
  state=create(h,L"STATIC",L"Bereit | Luau 0.747 | TestHost offline",SS_LEFT,0,203,font);
  arrange(h);return 0;
 case WM_SIZE:arrange(h);return 0;
 case WM_COMMAND:
  switch(LOWORD(w)){
  case 101:execute();break;
  case 102:openFile(h);break;
  case 103:saveFile(h);break;
  case 104:show(L"");break;
  default:break;
  }
  return 0;
 case WM_DESTROY:
  DeleteObject(font);DeleteObject(mono);PostQuitMessage(0);return 0;
 }
 return DefWindowProcW(h,msg,w,l);
}
}
int WINAPI wWinMain(HINSTANCE inst,HINSTANCE,PWSTR,int showMode) {
 WNDCLASSW c{};c.lpfnWndProc=windowProc;c.hInstance=inst;
 c.lpszClassName=L"ExecutorLabMain";
 c.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
 c.hCursor=LoadCursorW(nullptr,IDC_ARROW);
 RegisterClassW(&c);
 HWND h=CreateWindowExW(0,c.lpszClassName,L"ExecutorLab - Luau Test Executor",
  WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,940,680,nullptr,nullptr,inst,nullptr);
 if(!h)return 1;
 ShowWindow(h,showMode);UpdateWindow(h);
 MSG msg{};
 while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
 return static_cast<int>(msg.wParam);
}
