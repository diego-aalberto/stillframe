// Native story frontend using the same scene model, skeletal animation, and renderer.
#include "render.hpp"
#include "story_art.hpp"
#include <winhttp.h>
#include <shellapi.h>
#include <objbase.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <thread>
#include <functional>
#include <cmath>
#include <mmsystem.h>
#include <gdiplus.h>

using namespace comic;
namespace {
constexpr UINT Ready = WM_APP + 1, Progress = WM_APP + 2;
enum { Send=301, Next, Leave, Visit, Retry, NewGame, Speaker, VisitList, Input, Transcript, Info, KeepTalking, Menu, Resume, Help, Quit, History, Dialogue, Heading, Brand, Hints, NameTitle, NameInput, NameAccept, NameCancel, Tagline };
HWND window=nullptr,canvas=nullptr;
HFONT uiFont=nullptr;
HFONT titleFont=nullptr;
HBRUSH panelBrush=CreateSolidBrush(RGB(246,241,250));
HBRUSH inputBrush=CreateSolidBrush(RGB(255,252,255));
std::map<int,int> selections;
std::map<int,float> hover;
std::string speaking;
int dialogueScroll=0;
float bubbleBirth=0;
std::string musicNode;
float clockTime(){static auto start=std::chrono::steady_clock::now();return std::chrono::duration<float>(std::chrono::steady_clock::now()-start).count();}
COLORREF accent(const std::string &who) {
    if(who=="hana")return RGB(42,139,124);
    if(who=="mira")return RGB(202,107,64);
    if(who=="leila")return RGB(143,99,191);
    if(who=="tessa")return RGB(78,129,198);
    return RGB(143,107,160);
}
Gdiplus::Color color(COLORREF c,int alpha=255){return Gdiplus::Color(alpha,GetRValue(c),GetGValue(c),GetBValue(c));}
void rounded(Gdiplus::Graphics &g,float x,float y,float w,float h,float radius,Gdiplus::Color fill) {
    Gdiplus::GraphicsPath path;float d=radius*2;
    path.AddArc(x,y,d,d,180,90);path.AddArc(x+w-d,y,d,d,270,90);
    path.AddArc(x+w-d,y+h-d,d,d,0,90);path.AddArc(x,y+h-d,d,d,90,90);path.CloseFigure();
    Gdiplus::SolidBrush brush(fill);g.FillPath(&brush,&path);
}
bool mainMenu=true,historyOpen=false,sceneIntro=false;
bool naming=false,creatingSave=false,layingOut=false;
int inputHeight=42;
Renderer renderer;
Scene scene;
Font gameFont;
std::unique_ptr<StoryArt> art;
std::map<std::string,std::string> view;
std::vector<std::string> speakers,visits;
std::wstring token;
int port=0;
bool initialized=false,busy=false;
std::string sceneKey;
std::chrono::steady_clock::time_point animationStart;
std::filesystem::path root;
std::filesystem::path smokeCapture;
bool streamSmoke=false,streamSmokeStarted=false;
int streamSmokeUpdates=0;
int smokeNamingStage=0;

std::string getText(HWND h) {
    int n=GetWindowTextLengthW(h);
    std::wstring s(size_t(n)+1,L'\0');
    GetWindowTextW(h,s.data(),n+1);s.resize(n);return utf8(s);
}
void set(int id,const std::string &s) { SetWindowTextW(GetDlgItem(window,id),wide(s).c_str()); }
std::string hex(const std::string &s) {
    const char *digits="0123456789abcdef";std::string result;
    for(unsigned char c:s) { result+=digits[c>>4];result+=digits[c&15]; } return result;
}
std::string unhex(const std::string &s) {
    if(s.size()%2) throw std::runtime_error("Invalid bridge response");
    std::string out;
    auto digit=[](char c)->int { if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;throw std::runtime_error("Invalid bridge encoding"); };
    for(size_t i=0;i<s.size();i+=2)out+=char(digit(s[i])*16+digit(s[i+1]));
    return out;
}
struct Internet {
    HINTERNET h=nullptr;
    ~Internet(){if(h)WinHttpCloseHandle(h);}
};
std::string request(const std::wstring &path,const std::string &body,bool post,const std::function<void(const std::string&)> &progress) {
    Internet session{WinHttpOpen(L"LastLight/1",WINHTTP_ACCESS_TYPE_NO_PROXY,nullptr,nullptr,0)};
    if(!session.h)throw std::runtime_error("Cannot open the local game connection");
    WinHttpSetTimeouts(session.h,5000,5000,10000,680000);
    Internet connection{WinHttpConnect(session.h,L"127.0.0.1",INTERNET_PORT(port),0)};
    Internet req{WinHttpOpenRequest(connection.h,post?L"POST":L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,0)};
    if(!req.h)throw std::runtime_error("Cannot connect to the story director");
    auto headers=L"Authorization: Bearer "+token+L"\r\nContent-Type: text/plain\r\n";
    if(!WinHttpSendRequest(req.h,headers.c_str(),DWORD(-1),body.empty()?nullptr:const_cast<char*>(body.data()),DWORD(body.size()),DWORD(body.size()),0)||!WinHttpReceiveResponse(req.h,nullptr))
        throw std::runtime_error("The response was interrupted. Progress is saved; use Retry response.");
    DWORD status=0,n=sizeof(status);
    WinHttpQueryHeaders(req.h,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&n,WINHTTP_NO_HEADER_INDEX);
    if(status!=200)throw std::runtime_error("The local game connection rejected the request. Restart to resume the saved game.");
    std::string out,frames;char buffer[4096];DWORD got=0;
    bool streaming=path==L"/chat-stream"||path==L"/retry-stream",done=false;
    do {
      DWORD available=0;
      if(!WinHttpQueryDataAvailable(req.h,&available))throw std::runtime_error("Her reply was interrupted. Retry to continue.");
      if(!available)break;
      if(!WinHttpReadData(req.h,buffer,std::min(DWORD(sizeof(buffer)),available),&got))throw std::runtime_error("Her reply was interrupted. Retry to continue.");
      if(streaming){
        frames.append(buffer,got);
        size_t cut;
        while((cut=frames.find("\n\n"))!=std::string::npos){
          auto frame=frames.substr(0,cut);frames.erase(0,cut+2);
          if(frame.rfind("stream=646f6e65\n",0)==0){out=frame.substr(std::string("stream=646f6e65\n").size());done=true;}
          else progress(frame);
        }
        if(frames.size()>2000000)throw std::runtime_error("Response too large");
      }else out.append(buffer,got);
      if(out.size()>2000000)throw std::runtime_error("Response too large");
    }while(got&&!done);
    if(streaming&&!done)throw std::runtime_error("Her reply was interrupted. Retry to continue.");

    return out;
}
struct Result {std::string data,error;bool sent=false,named=false;};
void refreshControls();
void layout();
void updateDialogue();
void dispatch(const std::wstring &path,const std::string &body="",bool post=true) {
    if(busy)return;busy=true;refreshControls();set(Info,"");
    if(path==L"/chat-stream"||path==L"/retry-stream"){
      speaking=view["speaker"];set(Dialogue,"");bubbleBirth=clockTime();dialogueScroll=0;
    }
    HWND target=window;
    std::thread([target,path,body,post]{
        auto result=std::make_unique<Result>();result->sent=path==L"/chat-stream";result->named=path==L"/new"||path==L"/name";
        try {result->data=request(path,body,post,[target](const std::string &frame){
          auto *message=new std::string(frame);if(!PostMessageW(target,Progress,0,LPARAM(message)))delete message;
        });}catch(const std::exception &e){result->error=e.what();}
        auto *p=result.release();if(!PostMessageW(target,Ready,0,LPARAM(p)))delete p;
    }).detach();
}
std::vector<std::string> split(const std::string &s) {
    std::vector<std::string> v;std::istringstream input(s);std::string item;
    while(std::getline(input,item,','))if(!item.empty())v.push_back(item);return v;
}
std::string name(std::string s) {if(!s.empty())s[0]=char(toupper(s[0]));return s;}
void combo(int id,const std::vector<std::string> &values) {
    int &selected=selections[id];if(selected>=int(values.size()))selected=0;
    set(id,values.empty()?"Nobody available":name(values[size_t(selected)]));
}
void choosePerson(int id,const std::vector<std::string> &values){
    if(values.empty())return;
    HMENU menu=CreatePopupMenu();
    for(size_t i=0;i<values.size();++i)AppendMenuW(menu,MF_STRING|(selections[id]==int(i)?MF_CHECKED:0),i+1,wide(name(values[i])).c_str());
    RECT r{};GetWindowRect(GetDlgItem(window,id),&r);
    int choice=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,r.left,r.top,0,window,nullptr);
    DestroyMenu(menu);if(choice>0){selections[id]=choice-1;combo(id,values);}
}
void playMusic(){
    if(view["node"]==musicNode)return;
    musicNode=view["node"];PlaySoundW(nullptr,nullptr,0);
    auto path=root.parent_path()/"Game"/"music"/(musicNode+".wav");
    if(std::filesystem::exists(path))PlaySoundW(path.c_str(),nullptr,SND_FILENAME|SND_ASYNC|SND_LOOP|SND_NODEFAULT);
}

void installScene() {
    std::string key=view["background"]+view["cast"];
    if(key!=sceneKey){
      scene=art->make(view["background"],split(view["cast"]),gameFont);
      renderer.upload(scene);sceneKey=key;
    }
    float now=std::chrono::duration<float>(std::chrono::steady_clock::now()-animationStart).count();
    for(auto &c:scene.characters){
      auto pose=view["pose_"+c.id],gesture=view["gesture_"+c.id];
      if(pose.empty())pose=view["background"]=="car"||view["background"]=="cinema"?"seated":"neutral";
      if(gesture.empty())gesture="breathe";
      art->direct(scene,c.id,pose,gesture,now);
      c.text.clear();
    }
}
void updateDialogue() {
    bool ended=view["ended"]=="True",hub=view["hub"]=="True";
    set(Heading,ended?"The light is still on":sceneIntro?view["title"]:hub?"A little time together":view["reply"].empty()?view["title"]:name(view["speaker"]));
    std::string text=ended?view["ending"]:sceneIntro?view["opening"]:hub?
      "Later, there is time to spend together. Choose someone to visit, or let the next day begin.":
      view["reply"].empty()?view["opening"]:view["reply"];
    speaking=(!sceneIntro&&!hub&&!ended&&!view["reply"].empty())?view["speaker"]:"";
    set(Dialogue,text);dialogueScroll=0;InvalidateRect(GetDlgItem(window,Dialogue),nullptr,FALSE);
}
void refreshControls() {
    bool ended=view["ended"]=="True",hub=view["hub"]=="True",pending=view["pending"]=="True",choice=view["choice"]=="True";
    auto enable=[&](int id,bool yes){EnableWindow(GetDlgItem(window,id),yes&&!busy);};
    bool chat=!ended&&!hub&&!pending&&!choice&&!sceneIntro;
    enable(Send,chat);EnableWindow(GetDlgItem(window,Input),chat||busy);enable(Speaker,chat);
    enable(Next,!ended&&!pending&&(sceneIntro||hub||choice));
    enable(Leave,!ended&&!pending&&!hub&&!choice&&!sceneIntro);
    enable(Visit,!ended&&hub&&!visits.empty());enable(VisitList,!ended&&hub&&!visits.empty());
    enable(Retry,!ended);enable(NewGame,true);enable(Resume,!view["node"].empty());
    enable(KeepTalking,!ended&&!pending&&!hub&&choice&&!sceneIntro);
    set(Next,sceneIntro?"Begin scene":hub?"Next day":"Continue");
    set(Resume,view["turns"]=="0"&&view["node"]=="S01"?"Begin your story":"Continue your story");
    layout();
}

void apply(const std::string &data) {
    std::string previous=view["node"];
    std::istringstream input(data);std::string line;
    while(std::getline(input,line)){auto p=line.find('=');if(p!=std::string::npos)view[line.substr(0,p)]=unhex(line.substr(p+1));}
    speakers=split(view["cast"]);visits=split(view["available"]);combo(Speaker,speakers);combo(VisitList,visits);
    set(Transcript,view["transcript"]);auto h=GetDlgItem(window,Transcript);SendMessageW(h,EM_SETSEL,WPARAM(-1),LPARAM(-1));SendMessageW(h,EM_SCROLLCARET,0,0);
    SetWindowTextW(window,wide("The Light We Leave On | "+view["title"]).c_str());
    std::string info=view["ended"]=="True"?"The end. Your story is saved.":view["hub"]=="True"?"Choose a personal visit, or move to the next shared day.":view["pending"]=="True"?"A response is pending. Use Retry response to recover it.":view["choice"]=="True"?view["note"]+" - Continue when ready.":"A little honesty goes a long way.";
    if(!view["error"].empty())info=view["error"];
    if(previous!=view["node"])sceneIntro=view["turns"]=="0"&&view["hub"]!="True"&&view["ended"]!="True";
    set(Info,info);installScene();playMusic();updateDialogue();refreshControls();bubbleBirth=clockTime();
}
void layout() {
    if(layingOut)return;layingOut=true;
    HWND focused=GetFocus();
    RECT r{};GetClientRect(window,&r);int w=r.right,h=r.bottom;
    auto place=[&](int id,int x,int y,int width,int height){MoveWindow(GetDlgItem(window,id),x,y,width,height,TRUE);};
    for(int id=Send;id<=Tagline;++id)ShowWindow(GetDlgItem(window,id),SW_HIDE);
    auto show=[&](int id){if(id!=Dialogue)ShowWindow(GetDlgItem(window,id),SW_SHOW);};
    if(naming){
      int panel=std::min(520,w/2);MoveWindow(canvas,panel,0,w-panel,h,TRUE);
      for(int id:{NameTitle,NameInput,NameAccept,NameCancel,Info})show(id);
      place(NameTitle,40,150,panel-80,80);place(NameInput,40,260,panel-80,40);
      place(NameAccept,40,325,160,44);place(NameCancel,215,325,110,44);place(Info,40,390,panel-80,100);
    }else if(mainMenu){
      int panel=std::min(470,w/2);MoveWindow(canvas,panel,0,w-panel,h,TRUE);
      show(Brand);show(Tagline);show(Resume);show(NewGame);show(Help);show(Quit);show(Info);
      place(Brand,42,80,panel-74,140);place(Tagline,44,230,panel-80,75);
      place(Resume,44,330,panel-88,48);place(NewGame,44,392,panel-88,44);
      place(Help,44,450,panel-88,44);place(Quit,44,508,panel-88,44);
      place(Info,44,h-105,panel-80,85);
    }else{
      int extra=inputHeight-42;MoveWindow(canvas,0,0,w,h-142-extra,TRUE);
      show(Dialogue);show(Info);show(Menu);show(History);show(Hints);
      place(Heading,32,h-286-extra,w-300,28);place(Hints,w-284,18,42,38);place(Menu,w-230,18,92,38);place(History,w-126,18,100,38);
      place(Dialogue,24,h-330-extra,w-48,188);
      place(Info,40,h-138-extra,w-80,25);
      bool ended=view["ended"]=="True",hub=view["hub"]=="True",choice=view["choice"]=="True",pending=view["pending"]=="True";
      if(!ended&&!hub&&!sceneIntro&&!choice&&!pending){
        show(Speaker);show(Input);show(Send);
        place(Speaker,32,h-106-extra,130,46);place(Input,180,h-103-extra,w-350,inputHeight);place(Send,w-150,h-106-extra,118,46);
      }
      int x=32;
      auto action=[&](int id,int width){show(id);place(id,x,h-46,width,38);x+=width+12;};
      if(sceneIntro||hub||choice)action(Next,126);
      if(choice&&!sceneIntro)action(KeepTalking,130);
      if(hub&&!visits.empty()){show(VisitList);place(VisitList,x,h-46,135,38);x+=147;action(Visit,80);}
      if(!ended&&!hub&&!sceneIntro&&!choice)action(Leave,120);
      if(pending||!view["error"].empty())action(Retry,150);
      if(ended){place(Dialogue,24,h-330,w-48,266);action(NewGame,130);}
      if(historyOpen){show(Transcript);place(Transcript,32,64,w-64,h-410);SetWindowPos(GetDlgItem(window,Transcript),HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);}
    }
    InvalidateRect(window,nullptr,TRUE);
    layingOut=false;
    if(focused&&IsChild(window,focused)&&(GetWindowLongPtrW(focused,GWL_STYLE)&WS_VISIBLE)&&IsWindowEnabled(focused))SetFocus(focused);
}

void growInput(){
    if(layingOut)return;
    HWND input=GetDlgItem(window,Input);RECT area{},client{};GetClientRect(input,&area);GetClientRect(window,&client);
    HDC dc=GetDC(input);auto old=SelectObject(dc,uiFont);auto text=wide(getText(input));
    RECT measure{0,0,std::max(40,int(area.right)-24),0};
    DrawTextW(dc,text.empty()?L" ":text.c_str(),-1,&measure,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX|DT_EDITCONTROL);
    SelectObject(dc,old);ReleaseDC(input,dc);
    int height=std::clamp(int(measure.bottom)+14,42,std::max(42,int(client.bottom)-490));
    if(height!=inputHeight){inputHeight=height;layout();}
    int lines=int(SendMessageW(input,EM_GETLINECOUNT,0,0));
    if(lines*23+8<=inputHeight)SendMessageW(input,EM_LINESCROLL,0,-int(SendMessageW(input,EM_GETFIRSTVISIBLELINE,0,0)));
    else SendMessageW(input,EM_SCROLLCARET,0,0);
}

// Quotation marks and their contents share bold weight, including unfinished streamed quotes.
float formattedText(Gdiplus::Graphics &g,const std::wstring &text,Gdiplus::RectF bounds,float scroll,bool paint){
    Gdiplus::Font regular(L"Segoe UI",15,Gdiplus::FontStyleRegular,Gdiplus::UnitPoint),bold(L"Segoe UI",15,Gdiplus::FontStyleBold,Gdiplus::UnitPoint);
    Gdiplus::SolidBrush ink(Gdiplus::Color(255,57,46,66));
    Gdiplus::StringFormat format(Gdiplus::StringFormat::GenericTypographic());format.SetFormatFlags(format.GetFormatFlags()|Gdiplus::StringFormatFlagsMeasureTrailingSpaces);
    float x=0,y=0,line=regular.GetHeight(&g)+5;bool quoted=false;
    for(size_t i=0;i<text.size();){
      wchar_t c=text[i];if(c==L'\r'){++i;continue;}if(c==L'\n'){x=0;y+=line;++i;continue;}
      bool quote=c==L'"'||c==L'“'||c==L'”';bool weight=quoted||quote;
      size_t end=i+1;
      if(!quote&&c!=L' ')while(end<text.size()&&text[end]!=L' '&&text[end]!=L'\n'&&text[end]!=L'\r'&&text[end]!=L'"'&&text[end]!=L'“'&&text[end]!=L'”')++end;
      auto run=text.substr(i,end-i);auto *font=weight?&bold:&regular;Gdiplus::RectF measured;
      g.MeasureString(run.c_str(),int(run.size()),font,Gdiplus::PointF(0,0),&format,&measured);
      if(x>0&&x+measured.Width>bounds.Width){x=0;y+=line;}
      if(paint)g.DrawString(run.c_str(),int(run.size()),font,Gdiplus::PointF(bounds.X+x,bounds.Y+y-scroll),&format,&ink);
      x+=measured.Width;if(quote)quoted=c==L'“'?true:c==L'”'?false:!quoted;i=end;
    }
    return y+line;
}

GLuint dialogueTexture=0;
std::string dialogueCache;
unsigned dialogueRasterizations=0;
void drawDialogueOverlay(){
    if(mainMenu||naming||!initialized)return;
    HWND hwnd=GetDlgItem(window,Dialogue);RECT r{};GetClientRect(hwnd,&r);
    if(r.right<=0||r.bottom<=0)return;
    POINT origin{};MapWindowPoints(hwnd,canvas,&origin,1);
    bool animated=clockTime()-bubbleBirth<.4f||(busy&&getText(hwnd).empty());
    std::string key=getText(hwnd)+speaking+std::to_string(r.right)+":"+std::to_string(r.bottom)+":"+std::to_string(dialogueScroll);
    if(animated)key+=":"+std::to_string(int(clockTime()*30));
    if(key!=dialogueCache||!dialogueTexture){
      Gdiplus::Bitmap bitmap(r.right,r.bottom,PixelFormat32bppARGB);
      {
        Gdiplus::Graphics g(&bitmap);g.Clear(Gdiplus::Color(0,0,0,0));
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
        COLORREF tint=accent(speaking);float lift=std::max(0.f,1.f-(clockTime()-bubbleBirth)/.28f)*8.f;
        rounded(g,4,7+lift,float(r.right-8),float(r.bottom-12),26,Gdiplus::Color(24,80,60,100));
        Gdiplus::Color fill=speaking=="hana"?Gdiplus::Color(205,219,246,235):
          speaking=="mira"?Gdiplus::Color(205,255,228,206):
          speaking=="leila"?Gdiplus::Color(205,237,219,255):
          speaking=="tessa"?Gdiplus::Color(205,215,234,255):Gdiplus::Color(205,241,234,249);
        rounded(g,2,2+lift,float(r.right-8),float(r.bottom-12),26,fill);
        Gdiplus::SolidBrush ornament(color(tint,32));
        g.FillEllipse(&ornament,float(r.right-90),12+lift,48.f,48.f);
        g.FillEllipse(&ornament,float(r.right-113),20+lift,16.f,16.f);
        g.FillEllipse(&ornament,float(r.right-32),47+lift,7.f,7.f);
        rounded(g,22,15+lift,144,31,15,color(tint));
        Gdiplus::Font font(L"Segoe UI",12,Gdiplus::FontStyleBold,Gdiplus::UnitPoint);
        Gdiplus::SolidBrush white(Gdiplus::Color(255,255,255,255));
        auto label=wide(speaking.empty()?"A moment here":name(speaking));g.DrawString(label.c_str(),-1,&font,Gdiplus::PointF(36,19+lift),&white);
        auto text=wide(getText(hwnd));Gdiplus::RectF bounds(24,59+lift,float(r.right-55),float(r.bottom-73));
        float measured=formattedText(g,text,bounds,0,false);
        int maxScroll=std::max(0,int(measured-bounds.Height));dialogueScroll=std::min(dialogueScroll,maxScroll);
        g.SetClip(bounds);formattedText(g,text,bounds,float(dialogueScroll),true);g.ResetClip();
        if(busy&&text.empty()){
          float t=clockTime();
          for(int i=0;i<3;++i){Gdiplus::SolidBrush dot(color(tint));float y=76+std::sin(t*5-i*.8f)*4;g.FillEllipse(&dot,28.f+i*19,y,9.f,9.f);}
        }
        if(maxScroll){Gdiplus::SolidBrush dot(color(tint));g.FillEllipse(&dot,float(r.right-23),float(r.bottom-25),5.f,5.f);}

      }
      Gdiplus::BitmapData data{};Gdiplus::Rect rect(0,0,r.right,r.bottom);
      if(bitmap.LockBits(&rect,Gdiplus::ImageLockModeRead,PixelFormat32bppARGB,&data)!=Gdiplus::Ok)throw std::runtime_error("Cannot render dialogue");
      if(!dialogueTexture)glGenTextures(1,&dialogueTexture);
      glBindTexture(GL_TEXTURE_2D,dialogueTexture);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
      glPixelStorei(GL_UNPACK_ALIGNMENT,4);
      glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,r.right,r.bottom,0,GL_BGRA_EXT,GL_UNSIGNED_BYTE,data.Scan0);
      bitmap.UnlockBits(&data);dialogueCache=key;++dialogueRasterizations;
    }
    glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,dialogueTexture);glColor4ub(255,255,255,255);
    glBegin(GL_QUADS);
    glTexCoord2f(0,0);glVertex2f(float(origin.x),float(origin.y));
    glTexCoord2f(1,0);glVertex2f(float(origin.x+r.right),float(origin.y));
    glTexCoord2f(1,1);glVertex2f(float(origin.x+r.right),float(origin.y+r.bottom));
    glTexCoord2f(0,1);glVertex2f(float(origin.x),float(origin.y+r.bottom));glEnd();
}
LRESULT CALLBACK BubbleProc(HWND hwnd,UINT msg,WPARAM w,LPARAM l){
    // Hidden state holder. The scene renderer owns all visible dialogue pixels.
    if(msg==WM_ERASEBKGND)return 1;
    return DefWindowProcW(hwnd,msg,w,l);
}
LRESULT CALLBACK CanvasProc(HWND hwnd,UINT msg,WPARAM w,LPARAM l) {
    if(msg==WM_MOUSEWHEEL){dialogueScroll=std::max(0,dialogueScroll-GET_WHEEL_DELTA_WPARAM(w)/WHEEL_DELTA*48);return 0;}
    if(msg==WM_ERASEBKGND)return 1;
    return DefWindowProcW(hwnd,msg,w,l);
}
// Capture the actual native controls and the OpenGL canvas in hidden QA windows.
void captureInterface(const std::filesystem::path &path) {
    renderer.draw(scene,std::chrono::duration<float>(std::chrono::steady_clock::now()-animationStart).count(),false,false,-1,-1,-1,false,nullptr,view["title"]);
    RECT r{};GetClientRect(window,&r);int width=r.right,height=r.bottom;
    HDC screen=GetDC(window),dc=CreateCompatibleDC(screen);BITMAPINFO info{};
    info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;
    info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    void *bits=nullptr;HBITMAP bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&bits,nullptr,0);
    auto old=SelectObject(dc,bitmap);FillRect(dc,&r,panelBrush);
    RECT cr{};GetClientRect(canvas,&cr);int cw=cr.right,ch=cr.bottom;
    POINT origin{};MapWindowPoints(canvas,window,&origin,1);
    std::vector<unsigned char> pixels(size_t(cw)*ch*3);
    glReadBuffer(GL_FRONT);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,cw,ch,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    auto *out=static_cast<unsigned char*>(bits);
    for(int y=0;y<ch;++y)for(int x=0;x<cw;++x){
      size_t src=(size_t(ch-1-y)*cw+x)*3,dst=(size_t(y+origin.y)*width+x+origin.x)*4;
      out[dst]=pixels[src+2];out[dst+1]=pixels[src+1];out[dst+2]=pixels[src];out[dst+3]=255;
    }
    for(int id=Send;id<=Tagline;++id){HWND child=GetDlgItem(window,id);
      if(!(GetWindowLongPtrW(child,GWL_STYLE)&WS_VISIBLE))continue;
      RECT cr{};GetWindowRect(child,&cr);MapWindowPoints(nullptr,window,reinterpret_cast<POINT*>(&cr),2);
      int saved=SaveDC(dc);SetViewportOrgEx(dc,cr.left,cr.top,nullptr);
      SendMessageW(child,WM_PRINT,WPARAM(dc),PRF_CLIENT|PRF_NONCLIENT|PRF_ERASEBKGND);RestoreDC(dc,saved);
    }
    std::ofstream file(path,std::ios::binary);file<<"P6\n"<<width<<' '<<height<<"\n255\n";
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){size_t i=(size_t(y)*width+x)*4;char rgb[]={char(out[i+2]),char(out[i+1]),char(out[i])};file.write(rgb,3);}
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(window,screen);
}
LRESULT CALLBACK WindowProc(HWND hwnd,UINT msg,WPARAM w,LPARAM l) {
    try {
      if(msg==WM_CTLCOLORSTATIC||msg==WM_CTLCOLOREDIT||msg==WM_CTLCOLORLISTBOX){
        HDC dc=reinterpret_cast<HDC>(w);SetTextColor(dc,RGB(69,56,80));SetBkColor(dc,msg==WM_CTLCOLOREDIT?RGB(255,252,255):RGB(246,241,250));return LRESULT(msg==WM_CTLCOLOREDIT?inputBrush:panelBrush);
      }
      if(msg==WM_DRAWITEM){
        auto *d=reinterpret_cast<DRAWITEMSTRUCT*>(l);
        if(d->CtlType==ODT_COMBOBOX){
          FillRect(d->hDC,&d->rcItem,panelBrush);SetBkMode(d->hDC,TRANSPARENT);
          SetTextColor(d->hDC,RGB(248,226,198));SelectObject(d->hDC,uiFont);
          if(d->itemID!=UINT(-1)){wchar_t label[256]{};
            SendMessageW(d->hwndItem,CB_GETLBTEXT,d->itemID,LPARAM(label));
            RECT r=d->rcItem;r.left+=8;DrawTextW(d->hDC,label,-1,&r,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
          }return TRUE;
        }
        bool off=(d->itemState&ODS_DISABLED)!=0,pressed=(d->itemState&ODS_SELECTED)!=0;
        FillRect(d->hDC,&d->rcItem,panelBrush);
        COLORREF tint=accent(speaking);if(d->CtlID==Speaker&&!speakers.empty())tint=accent(speakers[size_t(selections[Speaker])]);
        bool primary=d->CtlID==Send||d->CtlID==Resume||d->CtlID==Next||d->CtlID==Speaker;
        {
          Gdiplus::Graphics g(d->hDC);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
          float v=hover[d->CtlID];int shift=pressed?-20:int(v*18);
          Gdiplus::Color fill=primary?Gdiplus::Color(255,std::clamp(int(GetRValue(tint))+shift,0,255),std::clamp(int(GetGValue(tint))+shift,0,255),std::clamp(int(GetBValue(tint))+shift,0,255)):
            Gdiplus::Color(255,int(230+v*8),int(219+v*10),int(239+v*5));
          if(off)fill=Gdiplus::Color(255,231,226,235);
          rounded(g,0,0,float(d->rcItem.right),float(d->rcItem.bottom),18,fill);
        }
        SetBkMode(d->hDC,TRANSPARENT);SetTextColor(d->hDC,off?RGB(160,148,169):primary?RGB(255,255,255):RGB(83,62,103));
        SelectObject(d->hDC,uiFont);auto label=wide(getText(d->hwndItem));RECT r=d->rcItem;
        DrawTextW(d->hDC,label.c_str(),-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);

        if(d->itemState&ODS_FOCUS){InflateRect(&r,-4,-4);DrawFocusRect(d->hDC,&r);}return TRUE;
      }
      if(msg==WM_MEASUREITEM){auto *m=reinterpret_cast<MEASUREITEMSTRUCT*>(l);m->itemHeight=24;return TRUE;}
      if(msg==WM_SIZE&&initialized){layout();growInput();return 0;}
      if(msg==WM_MOUSEWHEEL&&!mainMenu&&!naming){dialogueScroll=std::max(0,dialogueScroll-GET_WHEEL_DELTA_WPARAM(w)/WHEEL_DELTA*48);return 0;}
      if(msg==WM_COMMAND&&LOWORD(w)==Input&&HIWORD(w)==EN_CHANGE){growInput();return 0;}
      if(msg==WM_GETMINMAXINFO){auto *m=reinterpret_cast<MINMAXINFO*>(l);m->ptMinTrackSize={1000,700};return 0;}
      if(msg==WM_TIMER&&initialized&&!IsIconic(hwnd)){
        float time=std::chrono::duration<float>(std::chrono::steady_clock::now()-animationStart).count();
        art->tick(scene,time);

        POINT cursor{};GetCursorPos(&cursor);HWND under=WindowFromPoint(cursor);
        for(int id=Send;id<=Tagline;++id){
          HWND child=GetDlgItem(window,id);float target=child==under?1.f:0.f;
          if(std::abs(hover[id]-target)>.02f){hover[id]+=(target-hover[id])*.25f;InvalidateRect(child,nullptr,FALSE);}
        }
        renderer.draw(scene,time,false,false,-1,-1,-1,false,nullptr,mainMenu?"Somewhere to come back to":view["title"]+"  |  Progress saved automatically");return 0;
      }
      if(msg==Progress){
        std::unique_ptr<std::string> message(reinterpret_cast<std::string*>(l));std::map<std::string,std::string> fields;
        std::istringstream input(*message);std::string line;
        while(std::getline(input,line)){auto p=line.find('=');if(p!=std::string::npos)fields[line.substr(0,p)]=unhex(line.substr(p+1));}
        speaking=fields["speaker"];view["speaker"]=speaking;sceneIntro=false;
        if(fields["stream"]=="start"){
          view["pose_"+speaking]=fields["pose_"+speaking];view["gesture_"+speaking]=fields["gesture_"+speaking];
          float now=std::chrono::duration<float>(std::chrono::steady_clock::now()-animationStart).count();
          art->direct(scene,speaking,view["pose_"+speaking],view["gesture_"+speaking],now);
          set(Dialogue,"");bubbleBirth=clockTime();dialogueScroll=0;
        }else if(fields["stream"]=="reply"){
          set(Dialogue,fields["reply"]);
          if(streamSmoke)std::ofstream(smokeCapture.parent_path()/"stream-visible.txt")<<fields["reply"];
          if(streamSmoke && ++streamSmokeUpdates==2)captureInterface(smokeCapture.parent_path()/"stream-partial.ppm");
        }
        InvalidateRect(GetDlgItem(window,Dialogue),nullptr,FALSE);return 0;
      }
      if(msg==Ready){
        std::unique_ptr<Result> result(reinterpret_cast<Result*>(l));busy=false;
        if(!result->error.empty()){set(Info,result->error);refreshControls();}
        else {
          apply(result->data);
          if(result->named&&view["error"].empty()){naming=false;mainMenu=false;sceneIntro=true;updateDialogue();refreshControls();SetFocus(GetDlgItem(window,Next));}
          if(view["needs_name"]=="True"&&smokeCapture.empty()){naming=true;creatingSave=false;layout();SetFocus(GetDlgItem(window,NameInput));}
          if(streamSmoke){
            if(!streamSmokeStarted){
              streamSmokeStarted=true;mainMenu=false;sceneIntro=false;layout();
              dispatch(L"/chat-stream","who="+hex("hana")+"\ntext="+hex("Hello Hana"));return 0;
            }
            if(streamSmokeUpdates<2||!view["error"].empty())throw std::runtime_error("Native streaming failed");
            captureInterface(smokeCapture);
            std::ofstream(smokeCapture.string()+".txt")<<streamSmokeUpdates;
            PostMessageW(hwnd,WM_CLOSE,0,0);return 0;
          }
          if(!smokeCapture.empty()){
            if(view["node"]=="S01"&&smokeNamingStage<2){
              if(smokeNamingStage==1){
                if(!IsWindowEnabled(GetDlgItem(window,Next))||!sceneIntro)throw std::runtime_error("New save left Begin scene disabled");
                SendMessageW(hwnd,WM_COMMAND,MAKEWPARAM(Next,BN_CLICKED),0);
                if(!IsWindowEnabled(GetDlgItem(window,Input)))throw std::runtime_error("New save could not start conversation");
              }
              sceneIntro=false;naming=true;refreshControls();++smokeNamingStage;
              dispatch(smokeNamingStage==1?L"/new":L"/name","name="+hex("Alex"));return 0;
            }
            if(!IsWindowEnabled(GetDlgItem(window,Next))||!sceneIntro)throw std::runtime_error("Scene entry is disabled");
            SendMessageW(hwnd,WM_COMMAND,MAKEWPARAM(Next,BN_CLICKED),0);
            if(sceneIntro||!IsWindowEnabled(GetDlgItem(window,Input))||!IsWindowEnabled(GetDlgItem(window,Send)))throw std::runtime_error("Scene cannot enter conversation");
            mainMenu=true;layout();SendMessageW(hwnd,WM_COMMAND,MAKEWPARAM(Resume,BN_CLICKED),0);
            if(mainMenu||!IsWindowEnabled(GetDlgItem(window,Input)))throw std::runtime_error("Resume disabled conversation");
            sceneIntro=true;updateDialogue();refreshControls();
            renderer.capturePath=smokeCapture;
            renderer.draw(scene,.8f,false,false,-1,-1,-1,false,nullptr,"Connected story smoke test");
            std::ofstream log(smokeCapture.string()+".txt");log<<view["node"]<<'\n'<<view["cast"]<<'\n'<<scene.characters.size()<<'\n';
            if(view["node"]=="S02"){
              captureInterface(smokeCapture.parent_path()/"interface-scene.ppm");
              mainMenu=true;layout();renderer.draw(scene,.8f,false,false,-1,-1,-1,false,nullptr,"Somewhere to come back to");
              captureInterface(smokeCapture.parent_path()/"interface-menu.ppm");
              SendMessageW(hwnd,WM_COMMAND,MAKEWPARAM(Resume,BN_CLICKED),0);
              SendMessageW(hwnd,WM_COMMAND,MAKEWPARAM(Next,BN_CLICKED),0);
              if(mainMenu||sceneIntro||!IsWindowEnabled(GetDlgItem(window,Input))||getText(GetDlgItem(window,Speaker))!="Hana")throw std::runtime_error("Menu or scene introduction blocked conversation");
              renderer.draw(scene,.8f,false,false,-1,-1,-1,false,nullptr,view["title"]);
              captureInterface(smokeCapture.parent_path()/"interface-chat.ppm");
              set(Input,"A longer thought, on several lines.\r\nI want to see the beginning while I write.\r\nAnd keep this last line visible too.");growInput();
              if(inputHeight<75||SendMessageW(GetDlgItem(window,Input),EM_GETFIRSTVISIBLELINE,0,0)!=0)throw std::runtime_error("Composer did not grow");
              set(Dialogue,"She smiles. \"I'm glad you stayed.\" Then, softly: “Tell me more.”");speaking="hana";bubbleBirth=-1;
              renderer.draw(scene,.8f,false,false,-1,-1,-1,false,nullptr,view["title"]);
              captureInterface(smokeCapture.parent_path()/"interface-formatting.ppm");set(Input,"");growInput();
              SendMessageW(hwnd,WM_COMMAND,MAKEWPARAM(NewGame,BN_CLICKED),0);
              if(!naming||!(GetWindowLongPtrW(GetDlgItem(window,NameInput),GWL_STYLE)&WS_VISIBLE))throw std::runtime_error("New-save name entry missing");
              renderer.draw(scene,.8f,false,false,-1,-1,-1,false,nullptr,view["title"]);
              captureInterface(smokeCapture.parent_path()/"interface-name.ppm");
              SendMessageW(hwnd,WM_COMMAND,MAKEWPARAM(NameCancel,BN_CLICKED),0);mainMenu=false;layout();
              for(const auto &who:speakers){
                speaking=who;sceneIntro=false;view["reply"]="I'm glad you stayed. Tell me what's on your mind.";
                set(Dialogue,view["reply"]);bubbleBirth=-1;
                captureInterface(smokeCapture.parent_path()/("bubble-"+who+".ppm"));
              }
              for(int frame=0;frame<2;++frame){
                renderer.capturePath=smokeCapture.parent_path()/("motion-"+std::to_string(frame)+".ppm");
                renderer.draw(scene,.3f+frame*.8f,false,false,-1,-1,-1,false,nullptr,"");
              }
              art->audit(scene,smokeCapture.string()+".motions.csv");
              bubbleBirth=-1;renderer.draw(scene,1.f,false,false,-1,-1,-1,false,nullptr,"");
              unsigned rasterizations=dialogueRasterizations;
              auto renderStart=std::chrono::steady_clock::now();
              for(int frame=0;frame<60;++frame)renderer.draw(scene,1.f+frame/30.f,false,false,-1,-1,-1,false,nullptr,"");
              auto renderMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-renderStart).count();
              if(dialogueRasterizations!=rasterizations)throw std::runtime_error("Static dialogue is rasterized every frame");
              std::ofstream(smokeCapture.parent_path()/"render-timing.txt")<<"60 frames: "<<renderMs<<" ms; cached dialogue rasterizations: 0\n";
              for(size_t p=0;p<storyPoses.size();++p){
                for(auto &c:scene.characters)art->direct(scene,c.id,storyPoses[p],"nod",0);
                renderer.capturePath=smokeCapture.parent_path()/(std::string("pose-")+storyPoses[p]+".ppm");
                renderer.draw(scene,.7f,false,false,-1,-1,-1,false,nullptr,std::string("Pose library: ")+storyPoses[p]);
              }
            }
            PostMessageW(hwnd,WM_CLOSE,0,0);
          }
        }
        return 0;
      }
      if(msg==WM_COMMAND&&HIWORD(w)==BN_CLICKED&&!busy){
        int id=LOWORD(w);
        if(id==Send){int i=selections[Speaker];auto text=getText(GetDlgItem(window,Input));if(i>=0&&i<int(speakers.size())&&!text.empty()){dispatch(L"/chat-stream","who="+hex(speakers[size_t(i)])+"\ntext="+hex(text));set(Input,"");}}
        if(id==Speaker)choosePerson(Speaker,speakers);
        if(id==VisitList)choosePerson(VisitList,visits);
        if(id==Menu){mainMenu=true;historyOpen=false;layout();}
        if(id==Resume){if(view["needs_name"]=="True"&&smokeCapture.empty()){naming=true;creatingSave=false;}else mainMenu=false;layout();SetFocus(GetDlgItem(window,naming?NameInput:sceneIntro?Next:Input));}
        if(id==Hints)MessageBoxW(hwnd,wide(view["hints"]).c_str(),wide("Possible paths — "+view["title"]).c_str(),MB_OK);
        if(id==NameCancel){naming=false;mainMenu=true;layout();}
        if(id==NameAccept){auto player=getText(GetDlgItem(window,NameInput));if(player.empty()){set(Info,"What should they call you?");SetFocus(GetDlgItem(window,NameInput));}else dispatch(creatingSave?L"/new":L"/name","name="+hex(player));}
        if(id==History){historyOpen=!historyOpen;layout();}
        if(id==Quit)PostMessageW(hwnd,WM_CLOSE,0,0);
        if(id==Help)MessageBoxW(hwnd,L"Speak in your own words. Choose who you address, then press Speak.\n\nConversations can open a new path. Continue takes that path; Keep talking stays in the moment. Between scenes, visit someone or start the next day.\n\nRomance grows through attention, honesty, flirting and asking. Kisses and cuddles unfold in the dialogue. A shared romance is possible when everyone wants it.\n\nYour story saves automatically, even if a response is interrupted. History shows earlier conversations. Escape opens the menu; Ctrl+Enter sends your message.",L"How to play",MB_OK);
        if(id==Next){if(sceneIntro){sceneIntro=false;updateDialogue();refreshControls();SetFocus(GetDlgItem(window,Input));}else dispatch(L"/advance");}
        if(id==KeepTalking)dispatch(L"/keep-talking");
        if(id==Leave)dispatch(L"/leave");
        if(id==Retry)dispatch(view["pending"]=="True"?L"/retry-stream":L"/state","",view["pending"]=="True");
        if(id==Visit){int i=selections[VisitList];if(i>=0&&i<int(visits.size()))dispatch(L"/visit","who="+hex(visits[size_t(i)]));}
        if(id==NewGame){naming=true;creatingSave=true;set(NameInput,"");set(Info,"Your current story will be kept as a backup when you begin.");layout();SetFocus(GetDlgItem(window,NameInput));}
        return 0;
      }
      if(msg==WM_CLOSE){if(initialized){renderer.close(canvas);initialized=false;}DestroyWindow(hwnd);return 0;}
      if(msg==WM_DESTROY){KillTimer(hwnd,1);if(initialized){renderer.close(canvas);initialized=false;}PostQuitMessage(0);return 0;}
    }catch(const std::exception &e){set(Info,e.what());busy=false;refreshControls();return 0;}
    return DefWindowProcW(hwnd,msg,w,l);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show) {
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    ULONG_PTR graphicsToken=0;Gdiplus::GdiplusStartupInput graphicsInput;Gdiplus::GdiplusStartup(&graphicsToken,&graphicsInput,nullptr);
    try{
      int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
      if(argc<3)throw std::runtime_error("Use start-game.cmd to start the game and its inference service.");
      port=std::stoi(argv[1]);token=argv[2];if(argc>=5){auto flag=std::wstring(argv[3]);if(flag==L"--smoke"||flag==L"--stream-smoke")smokeCapture=argv[4];streamSmoke=flag==L"--stream-smoke";}LocalFree(argv);
      if(port<1||port>65535)throw std::runtime_error("Invalid local bridge port");
      wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);root=std::filesystem::path(exe).parent_path().parent_path();
      gameFont=rasterFont(defaultFont());
      art=std::make_unique<StoryArt>(root.parent_path()/"Game"/"assets");
      renderer.fillViewport=true;renderer.showStatus=false;renderer.overlay=drawDialogueOverlay;
      WNDCLASSW wc{};wc.hInstance=instance;wc.hbrBackground=panelBrush;wc.lpszClassName=L"LastLightWindow";wc.lpfnWndProc=WindowProc;wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);
      wc.lpszClassName=L"LastLightCanvas";wc.lpfnWndProc=CanvasProc;wc.style=CS_OWNDC;wc.hbrBackground=nullptr;RegisterClassW(&wc);
      wc.lpszClassName=L"LastLightBubble";wc.lpfnWndProc=BubbleProc;wc.style=0;wc.hbrBackground=panelBrush;RegisterClassW(&wc);
      window=CreateWindowExW(0,L"LastLightWindow",L"The Light We Leave On",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1280,850,nullptr,nullptr,instance,nullptr);
      canvas=CreateWindowExW(0,L"LastLightCanvas",L"",WS_CHILD|WS_VISIBLE,10,10,800,600,window,nullptr,instance,nullptr);renderer.initialize(canvas);
      uiFont=CreateFontW(-19,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
      titleFont=CreateFontW(-42,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Georgia");
      auto control=[&](int id,const wchar_t *cls,const wchar_t *label,DWORD style){HWND h=CreateWindowExW(0,cls,label,WS_CHILD|WS_VISIBLE|WS_TABSTOP|(cls==std::wstring(L"BUTTON")?BS_OWNERDRAW:style),0,0,100,30,window,HMENU(INT_PTR(id)),instance,nullptr);SendMessageW(h,WM_SETFONT,WPARAM(uiFont),TRUE);return h;};
      control(Transcript,L"EDIT",L"",ES_MULTILINE|ES_READONLY|WS_VSCROLL|ES_AUTOVSCROLL);
      control(Input,L"EDIT",L"",ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN|WS_VSCROLL);SendMessageW(GetDlgItem(window,Input),EM_SETLIMITTEXT,3000,0);
      control(Speaker,L"BUTTON",L"Hana",0);control(VisitList,L"BUTTON",L"Choose company",0);
      control(Info,L"STATIC",L"Welcome back.",0);
      control(Send,L"BUTTON",L"Speak",BS_PUSHBUTTON);control(Next,L"BUTTON",L"Continue",BS_PUSHBUTTON);control(Leave,L"BUTTON",L"Leave scene",BS_PUSHBUTTON);
      control(KeepTalking,L"BUTTON",L"Keep talking",BS_PUSHBUTTON);
      control(Visit,L"BUTTON",L"Visit",BS_PUSHBUTTON);control(Retry,L"BUTTON",L"Retry response",BS_PUSHBUTTON);control(NewGame,L"BUTTON",L"New story",BS_PUSHBUTTON);
      control(Menu,L"BUTTON",L"Menu",0);control(History,L"BUTTON",L"History",0);
      control(Hints,L"BUTTON",L"?",0);
      control(NameTitle,L"STATIC",L"What should they call you?\r\nChoose a name for your story.",0);
      control(NameInput,L"EDIT",L"",ES_AUTOHSCROLL|WS_BORDER);SendMessageW(GetDlgItem(window,NameInput),EM_SETLIMITTEXT,32,0);
      SendMessageW(GetDlgItem(window,NameInput),0x1501,TRUE,LPARAM(L"Your name"));
      control(NameAccept,L"BUTTON",L"Begin my story",0);control(NameCancel,L"BUTTON",L"Back",0);
      control(Resume,L"BUTTON",L"Continue your story",0);control(Help,L"BUTTON",L"How to play",0);control(Quit,L"BUTTON",L"Quit",0);
      control(Heading,L"STATIC",L"",0);control(Dialogue,L"LastLightBubble",L"",0);
      control(Brand,L"STATIC",L"The Light\r\nWe Leave On",0);SendMessageW(GetDlgItem(window,Brand),WM_SETFONT,WPARAM(titleFont),TRUE);
      control(Tagline,L"STATIC",L"A summer of late conversations,\r\nunexpected closeness, and finding your place.",0);
      scene=Scene{};scene.fonts.push_back(gameFont);prepare(scene);renderer.upload(scene);initialized=true;if(!smokeCapture.empty())mainMenu=false;animationStart=std::chrono::steady_clock::now();layout();
      SetTimer(window,1,33,nullptr);ShowWindow(window,smokeCapture.empty()?show:SW_HIDE);UpdateWindow(window);dispatch(L"/state","",false);
      MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){if(msg.message==WM_KEYDOWN&&msg.wParam==VK_ESCAPE){if(naming){naming=false;mainMenu=true;}else mainMenu=!mainMenu;historyOpen=false;layout();continue;}
        if(msg.message==WM_KEYDOWN&&msg.wParam==VK_RETURN&&naming){SendMessageW(window,WM_COMMAND,MAKEWPARAM(NameAccept,BN_CLICKED),0);continue;}
        if(msg.message==WM_KEYDOWN&&msg.wParam==VK_RETURN&&(GetKeyState(VK_CONTROL)&0x8000)&&!mainMenu&&IsWindowEnabled(GetDlgItem(window,Send))){SendMessageW(window,WM_COMMAND,MAKEWPARAM(Send,BN_CLICKED),0);continue;}
        if(!IsDialogMessageW(window,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
      DeleteObject(uiFont);DeleteObject(titleFont);DeleteObject(panelBrush);DeleteObject(inputBrush);PlaySoundW(nullptr,nullptr,0);Gdiplus::GdiplusShutdown(graphicsToken);CoUninitialize();return 0;
    }catch(const std::exception &e){MessageBoxW(nullptr,wide(e.what()).c_str(),L"The game could not start",MB_OK|MB_ICONERROR);CoUninitialize();return 1;}
}
