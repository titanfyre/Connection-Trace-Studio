#include "../SDK/ConnectionTrace.h"
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <deque>
#include <thread>
#include <mutex>
#include <atomic>
#include <string>
#include <vector>
#include <sstream>
namespace {
HWND window{},start{},stop{},status{},list{},counts{},paths{};HFONT font{},heading{};HBRUSH background{},panel{};
std::thread worker;std::atomic_bool ending=false;std::mutex mutex;std::deque<CT_EVENT> pending;std::wstring failure;CT_STATUS latest{};bool done=false,active=false,preview=false;unsigned long long uiDropped=0,shown=0;
std::wstring Wide(const std::string& s){int n=MultiByteToWideChar(CP_UTF8,0,s.data(),static_cast<int>(s.size()),nullptr,0);std::wstring r(static_cast<size_t>(n),0);if(n)MultiByteToWideChar(CP_UTF8,0,s.data(),static_cast<int>(s.size()),r.data(),n);return r;}
void Capture(bool includePaths){
 try{ConnectionTrace::Session capture;capture.Start(includePaths);{
  std::lock_guard<std::mutex> lock(mutex);latest=capture.Status();}
  while(!ending){auto events=capture.Read();auto state=capture.Status();{
   std::lock_guard<std::mutex> lock(mutex);latest=state;for(const auto& e:events){if(pending.size()==2048){pending.pop_front();++uiDropped;}pending.push_back(e);}}
   Sleep(20);
  }
 }catch(const std::exception& e){std::lock_guard<std::mutex> lock(mutex);failure=Wide(e.what());}
 {std::lock_guard<std::mutex> lock(mutex);done=true;}
}
void Run(){if(active||preview)return;if(worker.joinable())worker.join();ending=false;{
 std::lock_guard<std::mutex> lock(mutex);failure.clear();done=false;pending.clear();latest={};uiDropped=0;}
 try{worker=std::thread(Capture,SendMessageW(paths,BM_GETCHECK,0,0)==BST_CHECKED);active=true;EnableWindow(paths,FALSE);EnableWindow(start,FALSE);EnableWindow(stop,TRUE);SetWindowTextW(status,L"Starting an explicit, read-only capture...");}
 catch(...){SetWindowTextW(status,L"Could not create the capture worker.");}
}
void Add(const CT_EVENT& e){
 if(ListView_GetItemCount(list)>=500)ListView_DeleteItem(list,499);
 std::wstring seq=std::to_wstring(e.Sequence),pid=e.Flags&CT_FLAG_PID?std::to_wstring(e.ProcessId):L"unknown";
 LVITEMW item{};item.mask=LVIF_TEXT;item.iItem=0;item.pszText=seq.data();ListView_InsertItem(list,&item);
 std::wstring text[5]={ConnectionTrace::Stage(e),pid,ConnectionTrace::Address(e,false),ConnectionTrace::Address(e,true),ConnectionTrace::Application(e)};
 if(e.Flags&CT_FLAG_PATH_TRUNCATED)text[4]+=L" [truncated]";
 for(int i=0;i<5;++i)ListView_SetItemText(list,0,i+1,text[i].data());++shown;
}
void Tick(){
 std::deque<CT_EVENT> events;CT_STATUS state{};std::wstring error;bool finished;unsigned long long dropped;
 {std::lock_guard<std::mutex> lock(mutex);events.swap(pending);state=latest;error=failure;finished=done;dropped=uiDropped;}
 if(!events.empty()){SendMessageW(list,WM_SETREDRAW,FALSE,0);for(const auto& e:events)Add(e);SendMessageW(list,WM_SETREDRAW,TRUE,0);InvalidateRect(list,nullptr,FALSE);}
 if(active&&finished){if(worker.joinable())worker.join();active=false;EnableWindow(paths,TRUE);EnableWindow(start,TRUE);EnableWindow(stop,FALSE);SetWindowTextW(status,error.empty()?L"Capture stopped. This application's WFP filters were removed.":error.c_str());}
 else if(active&&state.Active)SetWindowTextW(status,L"Observing WFP connection stages. Packet contents are never captured.");
 if(!preview){auto text=L"Observed "+std::to_wstring(state.Produced)+L"   |   Driver drops "+std::to_wstring(state.Dropped)+L"   |   UI drops "+std::to_wstring(dropped)+L"   |   Latest 500 rows";SetWindowTextW(counts,text.c_str());}
}
void Layout(HWND h){RECT r{};GetClientRect(h,&r);int width=r.right,height=r.bottom;MoveWindow(start,24,84,150,38,TRUE);MoveWindow(stop,186,84,110,38,TRUE);MoveWindow(status,320,84,width-344,44,TRUE);MoveWindow(paths,24,136,190,30,TRUE);MoveWindow(counts,228,140,width-252,30,TRUE);MoveWindow(list,24,184,width-48,height-246,TRUE);}
LRESULT CALLBACK Proc(HWND h,UINT m,WPARAM w,LPARAM l){
 switch(m){case WM_CREATE:{
  font=CreateFontW(17,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");heading=CreateFontW(28,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
  start=CreateWindowW(L"BUTTON",L"Start capture",WS_CHILD|WS_VISIBLE|WS_TABSTOP,0,0,0,0,h,reinterpret_cast<HMENU>(1),nullptr,nullptr);
  stop=CreateWindowW(L"BUTTON",L"Stop",WS_CHILD|WS_VISIBLE|WS_TABSTOP,0,0,0,0,h,reinterpret_cast<HMENU>(2),nullptr,nullptr);EnableWindow(stop,FALSE);
  status=CreateWindowW(L"STATIC",L"Inactive. Start capture explicitly; administrator permission is required.",WS_CHILD|WS_VISIBLE,0,0,0,0,h,nullptr,nullptr,nullptr);
  counts=CreateWindowW(L"STATIC",L"No telemetry is uploaded. Exports may contain sensitive application and address metadata.",WS_CHILD|WS_VISIBLE,0,0,0,0,h,nullptr,nullptr,nullptr);
  paths=CreateWindowW(L"BUTTON",L"Include application paths",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_AUTOCHECKBOX,0,0,0,0,h,reinterpret_cast<HMENU>(3),nullptr,nullptr);
  list=CreateWindowExW(0,WC_LISTVIEWW,L"",WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,0,0,0,0,h,nullptr,nullptr,nullptr);
  ListView_SetExtendedListViewStyle(list,LVS_EX_DOUBLEBUFFER|LVS_EX_FULLROWSELECT);
  ListView_SetBkColor(list,RGB(19,29,43));ListView_SetTextBkColor(list,RGB(19,29,43));ListView_SetTextColor(list,RGB(235,241,249));
  const wchar_t* columns[]={L"Sequence",L"WFP stage",L"PID",L"Local",L"Remote",L"Application (bounded path)"};const int widths[]={86,205,76,180,180,300};
  for(int i=0;i<6;++i){LVCOLUMNW column{};column.mask=LVCF_TEXT|LVCF_WIDTH;column.cx=widths[i];column.pszText=const_cast<wchar_t*>(columns[i]);ListView_InsertColumn(list,i,&column);}
  for(auto child:{start,stop,status,counts,list,paths})SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
  if(preview){EnableWindow(start,FALSE);SetWindowTextW(status,L"OFFLINE PREVIEW - synthetic documentation data. No driver opened.");SetWindowTextW(counts,L"Read-only metadata | No packet contents | Explicit Start | Bounded memory");for(unsigned i=1;i<=6;++i){CT_EVENT e{};e.Sequence=i;e.TimestampQpc=1000+i;e.ProcessId=4200;e.EndpointId=77;e.Flags=CT_FLAG_PID|CT_FLAG_ENDPOINT|CT_FLAG_LOCAL_ADDRESS|CT_FLAG_REMOTE_ADDRESS|CT_FLAG_PORTS|CT_FLAG_PATH;e.Family=i%2?4:6;e.Stage=static_cast<CT_U8>((i-1)%3+1);e.LocalPort=53000;e.RemotePort=443;e.LocalAddress[0]=127;e.LocalAddress[3]=1;e.RemoteAddress[0]=192;e.RemoteAddress[1]=0;e.RemoteAddress[2]=2;e.RemoteAddress[3]=10;if(e.Family==6){ZeroMemory(e.LocalAddress,16);ZeroMemory(e.RemoteAddress,16);e.LocalAddress[15]=1;e.RemoteAddress[0]=0x20;e.RemoteAddress[1]=1;e.RemoteAddress[2]=0x0d;e.RemoteAddress[3]=0xb8;e.RemoteAddress[15]=10;}const wchar_t path[]=L"\\device\\example\\sample-client.exe";e.PathLength=static_cast<CT_U16>(wcslen(path));memcpy(e.Application,path,sizeof(path));Add(e);}}
  SetTimer(h,1,50,nullptr);Layout(h);return 0;}
 case WM_SIZE:Layout(h);return 0;
 case WM_GETMINMAXINFO:reinterpret_cast<MINMAXINFO*>(l)->ptMinTrackSize={1000,520};return 0;
 case WM_TIMER:Tick();return 0;
 case WM_COMMAND:if(LOWORD(w)==1)Run();else if(LOWORD(w)==2){ending=true;SetWindowTextW(status,L"Stopping capture and removing this session's filters...");}return 0;
 case WM_CTLCOLORSTATIC:{auto dc=reinterpret_cast<HDC>(w);SetTextColor(dc,RGB(216,227,241));SetBkColor(dc,RGB(11,18,29));return reinterpret_cast<LRESULT>(background);}
 case WM_SETCURSOR:if(reinterpret_cast<HWND>(w)==start||reinterpret_cast<HWND>(w)==stop||reinterpret_cast<HWND>(w)==paths){SetCursor(LoadCursorW(nullptr,IDC_HAND));return TRUE;}break;
 case WM_PAINT:{PAINTSTRUCT p{};HDC dc=BeginPaint(h,&p);RECT r{};GetClientRect(h,&r);FillRect(dc,&r,background);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(246,249,255));SelectObject(dc,heading);RECT title{24,22,r.right-24,64};DrawTextW(dc,L"Connection Trace Studio",-1,&title,DT_LEFT|DT_SINGLELINE);SelectObject(dc,font);SetTextColor(dc,RGB(154,179,210));RECT foot{24,r.bottom-44,r.right-24,r.bottom-12};DrawTextW(dc,L"WFP stage observations are not end-to-end latency or a complete record of all network activity.",-1,&foot,DT_LEFT|DT_SINGLELINE);EndPaint(h,&p);return 0;}
 case WM_CLOSE:ending=true;if(worker.joinable())worker.join();DestroyWindow(h);return 0;
 case WM_DESTROY:KillTimer(h,1);DeleteObject(font);DeleteObject(heading);PostQuitMessage(0);return 0;
 }return DefWindowProcW(h,m,w,l);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR args,int show){
 (void)args;int argumentCount=0;auto arguments=CommandLineToArgvW(GetCommandLineW(),&argumentCount);preview=arguments&&argumentCount==2&&std::wstring(arguments[1])==L"--preview";if(arguments)LocalFree(arguments);SetProcessDPIAware();INITCOMMONCONTROLSEX common{sizeof(common),ICC_LISTVIEW_CLASSES};InitCommonControlsEx(&common);
 background=CreateSolidBrush(RGB(11,18,29));panel=CreateSolidBrush(RGB(19,29,43));WNDCLASSW c{};c.lpfnWndProc=Proc;c.hInstance=instance;c.lpszClassName=L"TitanConnectionTraceStudio";c.hbrBackground=background;c.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&c);
 window=CreateWindowW(c.lpszClassName,L"Connection Trace Studio - read-only WFP metadata",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1210,670,nullptr,nullptr,instance,nullptr);if(!window)return 1;ShowWindow(window,show);MSG message{};while(GetMessageW(&message,nullptr,0,0)>0){if(!IsDialogMessageW(window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}}DeleteObject(background);DeleteObject(panel);return 0;
}
