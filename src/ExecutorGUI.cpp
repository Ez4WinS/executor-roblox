// ExecutorLab: modern dark Windows UI for the local Luau sandbox.
// The GUI starts its own TestHost; it does NOT interact with RobloxPlayerBeta.exe.
#include "Protocol.h"
#include <windows.h>
#include <commdlg.h>
#include <richedit.h>
#include <algorithm>
#include <cstdint>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace {
constexpr COLORREF Bg=RGB(9,14,25);
constexpr COLORREF Side=RGB(14,21,36);
constexpr COLORREF Card=RGB(19,28,45);
constexpr COLORREF EditorBg=RGB(13,21,36);
constexpr COLORREF Stroke=RGB(40,54,76);
constexpr COLORREF White=RGB(230,237,250);
constexpr COLORREF Muted=RGB(144,158,183);
constexpr COLORREF Purple=RGB(123,103,242);
constexpr COLORREF PurpleDim=RGB(37,35,75);
constexpr COLORREF Mint=RGB(79,216,185);
constexpr COLORREF Orange=RGB(241,184,109);
constexpr COLORREF Sky=RGB(113,188,255);
constexpr COLORREF Comment=RGB(106,128,152);

constexpr int EditorId=201;
constexpr int ConsoleId=202;
constexpr int RunId=101;
constexpr int OpenId=102;
constexpr int SaveId=103;
constexpr int ClearId=104;

HWND mainWindow=nullptr;
HWND editor=nullptr, console=nullptr;
HWND buttons[4]{};
HFONT titleFont=nullptr, headingFont=nullptr, bodyFont=nullptr, tinyFont=nullptr, codeFont=nullptr;
HANDLE ownedHostProcess=nullptr;
bool backendReady=false, highlighting=false, dirty=false;
std::wstring filename=L"untitled.luau";
std::wstring statusText=L"Ready to execute";

struct Layout {
 int width=0,height=0,left=0,right=0;
 RECT toolbar{};
 RECT editorCard{},editorArea{},consoleCard{},consoleArea{};
};
Layout layoutOf(HWND window) {
 RECT c{};GetClientRect(window,&c);
 Layout o{};
 o.width=c.right; o.height=c.bottom;
 o.left=262; o.right=c.right-26;
 int top=184;
 int available=std::max(410,c.bottom-top-58);
 int first=std::max(264,available*62/100);
 o.toolbar={o.left,117,o.right,159};
 o.editorCard={o.left,top,o.right,top+first};
 o.consoleCard={o.left,top+first+16,o.right,c.bottom-57};
 o.editorArea={o.left+19,top+56,o.right-19,top+first-19};
 o.consoleArea={o.left+19,o.consoleCard.top+53,o.right-19,o.consoleCard.bottom-17};
 return o;
}
HFONT makeFont(int px,int weight,const wchar_t* family) {
 return CreateFontW(-px,0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
  OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,family);
}
void fill(HDC dc,const RECT& box,COLORREF color) {
 HBRUSH brush=CreateSolidBrush(color);
 FillRect(dc,&box,brush);
 DeleteObject(brush);
}
void roundBox(HDC dc,RECT box,int radius,COLORREF color,COLORREF border) {
 HBRUSH brush=CreateSolidBrush(color);
 HPEN pen=CreatePen(PS_SOLID,1,border);
 auto oldBrush=SelectObject(dc,brush);
 auto oldPen=SelectObject(dc,pen);
 RoundRect(dc,box.left,box.top,box.right,box.bottom,radius,radius);
 SelectObject(dc,oldBrush);
 SelectObject(dc,oldPen);
 DeleteObject(brush);DeleteObject(pen);
}
void text(HDC dc,const std::wstring& value,int x,int y,int width,int height,
          HFONT font,COLORREF color,UINT flags=DT_LEFT|DT_VCENTER|DT_SINGLELINE) {
 RECT rect{x,y,x+std::max(0,width),y+std::max(0,height)};
 SetBkMode(dc,TRANSPARENT);
 SetTextColor(dc,color);
 HFONT previous=static_cast<HFONT>(SelectObject(dc,font));
 DrawTextW(dc,value.c_str(),-1,&rect,flags|DT_NOPREFIX);
 SelectObject(dc,previous);
}
void repaint() {
 if(mainWindow)InvalidateRect(mainWindow,nullptr,FALSE);
}
void setStatus(const std::wstring& message) {
 statusText=message;
 repaint();
}
void paintPage(HWND window,HDC dc) {
 Layout v=layoutOf(window);
 RECT whole{0,0,v.width,v.height};
 fill(dc,whole,Bg);
 RECT sidebar{0,0,233,v.height};
 fill(dc,sidebar,Side);
 RECT border{232,0,233,v.height};fill(dc,border,Stroke);

 // Brand
 roundBox(dc,{23,26,64,67},12,Purple,Purple);
 text(dc,L"{ }",25,28,38,36,headingFont,White,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
 text(dc,L"ExecutorLab",76,26,143,26,headingFont,White);
 text(dc,L"LUAU WORKSPACE",77,51,142,15,tinyFont,Muted);
 RECT sideLine{23,89,211,90};fill(dc,sideLine,Stroke);

 text(dc,L"WORKSPACE",25,111,175,22,tinyFont,Muted);
 roundBox(dc,{14,143,219,191},13,PurpleDim,PurpleDim);
 roundBox(dc,{17,154,21,180},4,Purple,Purple);
 text(dc,L"</>",37,147,43,38,headingFont,Purple);
 text(dc,L"Script Editor",84,148,123,37,bodyFont,White);

 text(dc,L"ENVIRONMENT",25,226,185,20,tinyFont,Muted);
 roundBox(dc,{14,258,219,380},14,Card,Stroke);
 text(dc,L"LOCAL SANDBOX",30,272,166,24,bodyFont,White);
 roundBox(dc,{30,307,40,317},10,backendReady?Mint:Orange,backendReady?Mint:Orange);
 text(dc,backendReady?L"Runtime connected":L"Runtime offline",48,300,152,25,tinyFont,backendReady?Mint:Orange);
 text(dc,L"Luau 0.747",30,334,160,17,tinyFont,Muted);
 text(dc,L"Isolated script execution",30,355,173,17,tinyFont,Muted);

 RECT footCard{14,v.height-157,219,v.height-65};
 roundBox(dc,footCard,13,Card,Stroke);
 text(dc,L"SAFE TEST MODE",27,v.height-148,175,21,tinyFont,Mint);
 text(dc,L"No Roblox client access",27,v.height-124,180,21,tinyFont,White);
 text(dc,L"Own process + Luau VM",27,v.height-100,180,21,tinyFont,Muted);

 // Main header
 text(dc,L"Luau Playground",v.left,28,v.right-v.left-170,38,titleFont,White);
 text(dc,L"Write, run and inspect Luau scripts in a local test environment.",
      v.left,66,v.right-v.left,26,bodyFont,Muted);
 RECT pill{v.right-152,40,v.right,72};
 roundBox(dc,pill,19,PurpleDim,RGB(61,55,113));
 text(dc,L"●  LOCAL RUNTIME",pill.left+9,pill.top,pill.right-pill.left-18,32,
      tinyFont,RGB(194,181,255),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
 text(dc,L"TOOLS",v.left,110,80,30,tinyFont,Muted);

 // Rounded cards
 roundBox(dc,v.editorCard,18,Card,Stroke);
 text(dc,L"CODE EDITOR",v.left+20,v.editorCard.top+13,220,26,bodyFont,White);
 RECT fileBadge{v.right-235,v.editorCard.top+15,v.right-20,v.editorCard.top+42};
 roundBox(dc,fileBadge,10,RGB(26,37,57),Stroke);
 text(dc,(dirty?L"●  ":L"") + filename,fileBadge.left+10,fileBadge.top,
      fileBadge.right-fileBadge.left-20,26,tinyFont,Muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
 roundBox(dc,{v.editorArea.left-1,v.editorArea.top-1,v.editorArea.right+1,v.editorArea.bottom+1},
      11,EditorBg,RGB(35,49,69));
 roundBox(dc,v.consoleCard,18,Card,Stroke);
 text(dc,L"OUTPUT CONSOLE",v.left+20,v.consoleCard.top+13,260,26,bodyFont,White);
 text(dc,L"Errors, print() and return values",v.right-315,v.consoleCard.top+13,
      294,26,tinyFont,Muted,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
 roundBox(dc,{v.consoleArea.left-1,v.consoleArea.top-1,v.consoleArea.right+1,v.consoleArea.bottom+1},
      11,EditorBg,RGB(35,49,69));

 // Bottom status line
 RECT lowerLine{v.left,v.height-40,v.right,v.height-39};fill(dc,lowerLine,Stroke);
 roundBox(dc,{v.left+2,v.height-25,v.left+10,v.height-17},8,backendReady?Mint:Orange,backendReady?Mint:Orange);
 text(dc,statusText,v.left+19,v.height-31,v.right-v.left-150,24,tinyFont,Muted);
 text(dc,L"Ctrl+Enter to run",v.right-143,v.height-31,144,24,tinyFont,Muted,
      DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
}
void paintButton(const DRAWITEMSTRUCT& item) {
 HDC dc=item.hDC;
 RECT rect=item.rcItem;
 bool primary=item.CtlID==RunId;
 bool pressed=(item.itemState&ODS_SELECTED)!=0;
 bool disabled=(item.itemState&ODS_DISABLED)!=0;
 COLORREF background=primary?(pressed?RGB(91,73,204):Purple):
    (pressed?RGB(45,55,79):RGB(30,42,61));
 COLORREF edge=primary?background:Stroke;
 roundBox(dc,rect,12,background,edge);
 const wchar_t* label=item.CtlID==RunId?L"▶  Run Script":
    item.CtlID==OpenId?L"↳  Open File":
    item.CtlID==SaveId?L"▣  Save":
    L"✕  Clear Output";
 text(dc,label,rect.left+8,rect.top,rect.right-rect.left-16,rect.bottom-rect.top,
      bodyFont,disabled?Muted:White,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
 if(item.itemState&ODS_FOCUS) {
  RECT focus=rect;InflateRect(&focus,-5,-5);DrawFocusRect(dc,&focus);
 }
}
std::wstring currentText(HWND control) {
 int n=GetWindowTextLengthW(control);
 std::wstring outputText(static_cast<std::size_t>(n)+1,L'\0');
 int count=GetWindowTextW(control,outputText.data(),n+1);
 outputText.resize(count);
 return outputText;
}
std::string utf8(const std::wstring& source) {
 if(source.empty())return {};
 int n=WideCharToMultiByte(CP_UTF8,0,source.data(),static_cast<int>(source.size()),
    nullptr,0,nullptr,nullptr);
 if(!n)return {};
 std::string result(static_cast<std::size_t>(n),'\0');
 WideCharToMultiByte(CP_UTF8,0,source.data(),static_cast<int>(source.size()),
    result.data(),n,nullptr,nullptr);
 return result;
}
std::wstring wide(const std::string& source) {
 if(source.empty())return {};
 int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,
      source.data(),static_cast<int>(source.size()),nullptr,0);
 if(!n)return L"[Invalid UTF-8 in output]";
 std::wstring result(static_cast<std::size_t>(n),L'\0');
 MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,source.data(),
      static_cast<int>(source.size()),result.data(),n);
 return result;
}
void setEditorText(HWND view,const std::wstring& s) {SetWindowTextW(view,s.c_str());}
void setConsoleText(const std::wstring& s) {
 SetWindowTextW(console,s.c_str());
}
// Minimal Luau lexer for visual highlighting. Does not execute or parse Luau.
bool isIdentifier(wchar_t c) {
 return std::iswalnum(c)!=0||c==L'_';
}
bool keyword(const std::wstring& token) {
 static const wchar_t* keys[]={
 L"local",L"function",L"return",L"if",L"then",L"else",L"elseif",L"end",
 L"for",L"while",L"do",L"in",L"repeat",L"until",L"break",L"continue",
 L"and",L"or",L"not",L"true",L"false",L"nil",L"type",L"export"
 };
 for(const auto* key:keys)if(token==key)return true;
 return false;
}
void syntaxHighlight() {
 if(highlighting||!editor)return;
 std::wstring source=currentText(editor);
 if(source.size()>35000)return; // keep editing responsive for large scripts
 highlighting=true;
 CHARRANGE previous{};
 SendMessageW(editor,EM_EXGETSEL,0,reinterpret_cast<LPARAM>(&previous));
 SendMessageW(editor,WM_SETREDRAW,FALSE,0);
 CHARFORMAT2W format{};
 format.cbSize=sizeof(format);
 format.dwMask=CFM_COLOR;
 format.crTextColor=White;
 SendMessageW(editor,EM_SETCHARFORMAT,SCF_ALL,reinterpret_cast<LPARAM>(&format));
 auto paintSpan=[&](std::size_t start,std::size_t end,COLORREF color) {
  CHARRANGE span{static_cast<LONG>(start),static_cast<LONG>(end)};
  SendMessageW(editor,EM_EXSETSEL,0,reinterpret_cast<LPARAM>(&span));
  format.crTextColor=color;
  SendMessageW(editor,EM_SETCHARFORMAT,SCF_SELECTION,reinterpret_cast<LPARAM>(&format));
 };
 for(std::size_t i=0;i<source.size();) {
  std::size_t start=i;
  wchar_t ch=source[i];
  if(ch==L'-'&&i+1<source.size()&&source[i+1]==L'-') {
   i+=2;
   while(i<source.size()&&source[i]!=L'\n')++i;
   paintSpan(start,i,Comment);
  }else if(ch==L'"'||ch==L'\'') {
   ++i;
   while(i<source.size()) {
    if(source[i]==L'\\'&&i+1<source.size()) {i+=2;continue;}
    if(source[i++]==ch)break;
   }
   paintSpan(start,i,Mint);
  }else if(std::iswdigit(ch)) {
   while(i<source.size()&&(std::iswdigit(source[i])||source[i]==L'.'))++i;
   paintSpan(start,i,Orange);
  }else if(std::iswalpha(ch)||ch==L'_') {
   while(i<source.size()&&isIdentifier(source[i]))++i;
   std::wstring token=source.substr(start,i-start);
   if(keyword(token))paintSpan(start,i,Purple);
   else if(token==L"print"||token==L"Host"||token==L"game"||
           token==L"assert"||token==L"error")paintSpan(start,i,Sky);
  }else ++i;
 }
 SendMessageW(editor,EM_EXSETSEL,0,reinterpret_cast<LPARAM>(&previous));
 SendMessageW(editor,WM_SETREDRAW,TRUE,0);
 InvalidateRect(editor,nullptr,FALSE);
 highlighting=false;
}
void configureRichEdit(HWND h,bool readOnly) {
 SendMessageW(h,EM_SETBKGNDCOLOR,0,static_cast<LPARAM>(EditorBg));
 SendMessageW(h,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,MAKELONG(11,11));
 CHARFORMAT2W format{};
 format.cbSize=sizeof(format);
 format.dwMask=CFM_COLOR;
 format.crTextColor=readOnly?Muted:White;
 SendMessageW(h,EM_SETCHARFORMAT,SCF_ALL,reinterpret_cast<LPARAM>(&format));
 if(!readOnly) {
  SendMessageW(h,EM_EXLIMITTEXT,0,lab::MaxScript*2);
  SendMessageW(h,EM_SETEVENTMASK,0,ENM_CHANGE);
 }
}
bool waitForHost() {
 for(int i=0;i<50;++i) {
  if(WaitNamedPipeW(lab::PipeName,100))return true;
  if(ownedHostProcess) {
   DWORD state=STILL_ACTIVE;
   if(GetExitCodeProcess(ownedHostProcess,&state)&&state!=STILL_ACTIVE)return false;
  }
  Sleep(75);
 }
 return false;
}
bool ensureHost() {
 if(WaitNamedPipeW(lab::PipeName,200))return true;
 if(ownedHostProcess) {
  DWORD state=STILL_ACTIVE;
  if(GetExitCodeProcess(ownedHostProcess,&state)&&state==STILL_ACTIVE)return waitForHost();
  CloseHandle(ownedHostProcess);ownedHostProcess=nullptr;
 }
 wchar_t executable[MAX_PATH]{};
 DWORD size=GetModuleFileNameW(nullptr,executable,MAX_PATH);
 if(!size||size>=MAX_PATH)return false;
 std::filesystem::path folder=std::filesystem::path(executable).parent_path();
 std::filesystem::path host=folder/L"TestHost.exe";
 if(!std::filesystem::is_regular_file(host))return false;
 std::wstring command=L"\""+host.wstring()+L"\"";
 STARTUPINFOW startup{};startup.cb=sizeof(startup);
 PROCESS_INFORMATION info{};
 if(!CreateProcessW(host.c_str(),command.data(),nullptr,nullptr,FALSE,
     CREATE_NO_WINDOW,nullptr,folder.c_str(),&startup,&info))return false;
 ownedHostProcess=info.hProcess;
 CloseHandle(info.hThread);
 return waitForHost();
}
void stopOwnedHost() {
 if(!ownedHostProcess)return;
 DWORD state=STILL_ACTIVE;
 if(GetExitCodeProcess(ownedHostProcess,&state)&&state==STILL_ACTIVE) {
  TerminateProcess(ownedHostProcess,0);
  WaitForSingleObject(ownedHostProcess,1000);
 }
 CloseHandle(ownedHostProcess);
 ownedHostProcess=nullptr;
}
void execute() {
 std::string script=utf8(currentText(editor));
 if(script.empty()||script.size()>lab::MaxScript) {
  setConsoleText(L"Please enter a script between 1 and 131072 bytes.");
  setStatus(L"Invalid script size");return;
 }
 setStatus(L"Connecting to local runtime...");
 backendReady=ensureHost();
 if(!backendReady) {
  setConsoleText(L"TestHost.exe could not start. Keep all three program files in the same folder.");
  setStatus(L"Runtime unavailable");return;
 }
 HANDLE pipe=CreateFileW(lab::PipeName,GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL,nullptr);
 if(pipe==INVALID_HANDLE_VALUE) {
  setConsoleText(L"Unable to connect to the local test host.");
  setStatus(L"Connection failed");return;
 }
 std::uint32_t n=static_cast<std::uint32_t>(script.size()),resultCode=4,outLength=0;
 bool success=lab::writeExact(pipe,&n,4)&&lab::writeExact(pipe,script.data(),n)
    &&lab::readExact(pipe,&resultCode,4)&&lab::readExact(pipe,&outLength,4);
 if(success&&outLength<=lab::MaxOutput) {
  std::string response(outLength,'\0');
  success=lab::readExact(pipe,response.data(),outLength);
  if(success)setConsoleText(wide(response));
 } else success=false;
 CloseHandle(pipe);
 if(!success) {
  setConsoleText(L"Runtime response failed or was interrupted.");
  setStatus(L"IPC error");return;
 }
 setStatus(resultCode==0?L"Script executed successfully":
   resultCode==1?L"Syntax error - see console":
   resultCode==2?L"Runtime error - see console":L"Script rejected");
}
bool dialog(HWND owner,bool save,wchar_t* path) {
 OPENFILENAMEW options{};
 options.lStructSize=sizeof(options);options.hwndOwner=owner;
 options.lpstrFilter=L"Luau scripts (*.luau;*.lua)\0*.luau;*.lua\0All files (*.*)\0*.*\0";
 options.lpstrFile=path;options.nMaxFile=MAX_PATH;options.lpstrDefExt=L"luau";
 options.Flags=OFN_PATHMUSTEXIST|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
 return (save?GetSaveFileNameW(&options):GetOpenFileNameW(&options))!=FALSE;
}
void openFile() {
 wchar_t path[MAX_PATH]{};
 if(!dialog(mainWindow,false,path))return;
 std::ifstream file(std::filesystem::path(path),std::ios::binary);
 if(!file){setStatus(L"Could not open file");return;}
 std::string bytes((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
 if(bytes.size()>lab::MaxScript){setStatus(L"File exceeds the 128 KiB limit");return;}
 setEditorText(editor,wide(bytes));
 filename=std::filesystem::path(path).filename().wstring();
 dirty=false;
 syntaxHighlight();
 setStatus(L"File loaded");
}
void saveFile() {
 wchar_t path[MAX_PATH]=L"script.luau";
 if(!dialog(mainWindow,true,path))return;
 std::ofstream file(std::filesystem::path(path),std::ios::binary);
 if(!file){setStatus(L"Could not save file");return;}
 std::string source=utf8(currentText(editor));
 file.write(source.data(),static_cast<std::streamsize>(source.size()));
 if(!file){setStatus(L"Write failed");return;}
 filename=std::filesystem::path(path).filename().wstring();
 dirty=false;
 setStatus(L"Script saved");
}
void resizeChildren() {
 if(!mainWindow||!editor||!console)return;
 Layout v=layoutOf(mainWindow);
 int y=v.toolbar.top,buttonHeight=42;
 const int widths[]={159,123,119,143};
 int x=v.left;
 for(int i=0;i<4;++i) {
  MoveWindow(buttons[i],x,y,widths[i],buttonHeight,TRUE);
  x+=widths[i]+11;
 }
 MoveWindow(editor,v.editorArea.left+8,v.editorArea.top+8,
   v.editorArea.right-v.editorArea.left-16,
   v.editorArea.bottom-v.editorArea.top-16,TRUE);
 MoveWindow(console,v.consoleArea.left+8,v.consoleArea.top+8,
   v.consoleArea.right-v.consoleArea.left-16,
   v.consoleArea.bottom-v.consoleArea.top-16,TRUE);
 repaint();
}
HWND createButton(HWND owner,int id,const wchar_t* label) {
 HWND h=CreateWindowExW(0,L"BUTTON",label,WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,
     0,0,1,1,owner,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
     GetModuleHandleW(nullptr),nullptr);
 SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(bodyFont),TRUE);
 return h;
}
LRESULT CALLBACK windowProc(HWND window,UINT message,WPARAM wp,LPARAM lp) {
 switch(message) {
 case WM_CREATE: {
  mainWindow=window;
  titleFont=makeFont(30,FW_BOLD,L"Segoe UI");
  headingFont=makeFont(19,FW_SEMIBOLD,L"Segoe UI");
  bodyFont=makeFont(15,FW_MEDIUM,L"Segoe UI");
  tinyFont=makeFont(12,FW_MEDIUM,L"Segoe UI");
  codeFont=makeFont(16,FW_NORMAL,L"Consolas");
  buttons[0]=createButton(window,RunId,L"Run Script");
  buttons[1]=createButton(window,OpenId,L"Open File");
  buttons[2]=createButton(window,SaveId,L"Save");
  buttons[3]=createButton(window,ClearId,L"Clear Output");
  constexpr DWORD editorStyle=WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|
                       ES_AUTOVSCROLL|ES_WANTRETURN|ES_NOHIDESEL;
  editor=CreateWindowExW(0,MSFTEDIT_CLASS,
   L"-- Your Luau sandbox starts here!\r\n"
   L"print(\"Hello from ExecutorLab\")\r\n"
   L"print(\"6 * 7 =\", Host.multiply(6, 7))\r\n"
   L"print(\"Player:\", game.Players.LocalPlayer.Name)\r\n"
   L"return Host.greet(\"Developer\")",
   editorStyle,0,0,1,1,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(EditorId)),
   GetModuleHandleW(nullptr),nullptr);
  console=CreateWindowExW(0,MSFTEDIT_CLASS,
   L"Output will appear here after your script runs.",
   WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL,
   0,0,1,1,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(ConsoleId)),
   GetModuleHandleW(nullptr),nullptr);
  if(!editor||!console)return -1;
  SendMessageW(editor,WM_SETFONT,reinterpret_cast<WPARAM>(codeFont),TRUE);
  SendMessageW(console,WM_SETFONT,reinterpret_cast<WPARAM>(codeFont),TRUE);
  configureRichEdit(editor,false);
  configureRichEdit(console,true);
  syntaxHighlight();
  dirty=false;
  resizeChildren();
  backendReady=ensureHost();
  setStatus(backendReady?L"Local runtime connected - ready":L"Runtime not found - check program files");
  return 0;
 }
 case WM_SIZE:resizeChildren();return 0;
 case WM_GETMINMAXINFO: {
  auto* limits=reinterpret_cast<MINMAXINFO*>(lp);
  limits->ptMinTrackSize.x=940;
  limits->ptMinTrackSize.y=680;
  return 0;
 }
 case WM_COMMAND: {
  int id=LOWORD(wp);
  if(id==EditorId&&HIWORD(wp)==EN_CHANGE&&!highlighting) {
   dirty=true;syntaxHighlight();repaint();
  } else if(id==RunId)execute();
  else if(id==OpenId)openFile();
  else if(id==SaveId)saveFile();
  else if(id==ClearId){setConsoleText(L"");setStatus(L"Output cleared");}
  return 0;
 }
 case WM_DRAWITEM: {
  auto* info=reinterpret_cast<DRAWITEMSTRUCT*>(lp);
  if(info&&info->CtlType==ODT_BUTTON) {paintButton(*info);return TRUE;}
  break;
 }
 case WM_ERASEBKGND:return TRUE;
 case WM_PAINT: {
  PAINTSTRUCT ps{};
  HDC dc=BeginPaint(window,&ps);
  paintPage(window,dc);
  EndPaint(window,&ps);
  return 0;
 }
 case WM_DESTROY:
  stopOwnedHost();
  for(HFONT f:{titleFont,headingFont,bodyFont,tinyFont,codeFont})if(f)DeleteObject(f);
  PostQuitMessage(0);
  return 0;
 }
 return DefWindowProcW(window,message,wp,lp);
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int showMode) {
 HMODULE rich=LoadLibraryW(L"Msftedit.dll");
 if(!rich) {
  MessageBoxW(nullptr,L"Microsoft RichEdit is missing from Windows.",L"ExecutorLab",MB_OK|MB_ICONERROR);
  return 1;
 }
 WNDCLASSW wc{};
 wc.lpfnWndProc=windowProc;
 wc.hInstance=instance;
 wc.lpszClassName=L"ExecutorLabDarkWindow";
 wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
 wc.hIcon=LoadIconW(nullptr,IDI_APPLICATION);
 wc.hbrBackground=CreateSolidBrush(Bg);
 if(!RegisterClassW(&wc))return 2;
 HWND window=CreateWindowExW(0,wc.lpszClassName,L"ExecutorLab - Luau Developer Studio",
   WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,1170,785,
   nullptr,nullptr,instance,nullptr);
 if(!window)return 3;
 ShowWindow(window,showMode);
 UpdateWindow(window);
 ACCEL shortcuts[]={
  {static_cast<BYTE>(FVIRTKEY|FCONTROL),VK_RETURN,RunId},
  {static_cast<BYTE>(FVIRTKEY|FCONTROL),'O',OpenId},
  {static_cast<BYTE>(FVIRTKEY|FCONTROL),'S',SaveId},
 };
 HACCEL accelerators=CreateAcceleratorTableW(shortcuts,3);
 MSG msg{};
 while(GetMessageW(&msg,nullptr,0,0)>0) {
  if(!TranslateAcceleratorW(window,accelerators,&msg)) {
   TranslateMessage(&msg);
   DispatchMessageW(&msg);
  }
 }
 if(accelerators)DestroyAcceleratorTable(accelerators);
 DeleteObject(wc.hbrBackground);
 FreeLibrary(rich);
 return static_cast<int>(msg.wParam);
}
