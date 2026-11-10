#include "story_art.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace comic {
const std::array<const char *,12> storyPoses={"neutral","wave","explain","folded","shy","laugh","worried","firm","seated","seated_relaxed","affectionate","cuddle"};
const std::array<const char *,10> storyGestures={"breathe","nod","shake_head","tilt","emphasize","chuckle","bashful","reassure","sigh","glance"};
namespace {
constexpr float pi=3.14159265359f;
Image cell(const Image &strip,int column) {
    int left=int(strip.width)*column/4,right=int(strip.width)*(column+1)/4;
    int x0=right,y0=int(strip.height),x1=left,y1=0;
    for(int y=0;y<int(strip.height);++y)for(int x=left;x<right;++x)
      if(strip.rgba[(size_t(y)*strip.width+x)*4+3]>12){x0=std::min(x0,x);x1=std::max(x1,x);y0=std::min(y0,y);y1=std::max(y1,y);}
    if(x1-x0<40||y1-y0<100)throw std::runtime_error("Pose strip contains an empty or unreadable cell");
    // Texture extraction only: retain original colors and alpha, plus an eight-pixel deformation gutter.
    Image out;out.width=uint32_t(x1-x0+17);out.height=uint32_t(y1-y0+17);out.rgba.resize(size_t(out.width)*out.height*4,0);
    for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){size_t src=(size_t(y)*strip.width+x)*4,dst=(size_t(y-y0+8)*out.width+x-x0+8)*4;std::copy_n(&strip.rgba[src],4,&out.rgba[dst]);}
    return out;
}
void rotate(std::vector<Vec2> &points,const Layer &l,int root,int pivot,float degrees) {
    float a=degrees*pi/180,cs=std::cos(a),sn=std::sin(a);auto center=points[size_t(pivot)];
    for(size_t j=0;j<points.size();++j){int ancestor=int(j);while(ancestor>=0&&ancestor!=root)ancestor=l.joints[size_t(ancestor)].parent;
      if(ancestor==root){auto v=points[j]-center;points[j]=center+Vec2{v.x*cs-v.y*sn,v.x*sn+v.y*cs};}}
}
Layer rig(const Image &im,int pose) {
    std::array<Vec2,21> p={{{.50f,.49f},{.50f,.31f},{.50f,.205f},{.50f,.115f},{.50f,.035f},
      {.34f,.235f},{.27f,.38f},{.22f,.51f},{.22f,.55f},
      {.66f,.235f},{.73f,.38f},{.78f,.51f},{.78f,.55f},
      {.43f,.53f},{.41f,.735f},{.42f,.91f},{.43f,.975f},
      {.57f,.53f},{.60f,.735f},{.63f,.91f},{.66f,.975f}}};
    // Pose-specific arm landmarks; modest rotations keep flattened painted limbs intact.
    if(pose==1){p[6]={.20f,.345f};p[7]={.14f,.205f};p[8]={.14f,.16f};}
    if(pose==2){p[6]={.25f,.37f};p[7]={.14f,.30f};p[8]={.08f,.28f};p[10]={.71f,.36f};p[11]={.57f,.31f};p[12]={.55f,.28f};}
    if(pose==3){p[6]={.25f,.36f};p[7]={.62f,.34f};p[8]={.69f,.32f};p[10]={.75f,.36f};p[11]={.37f,.35f};p[12]={.31f,.32f};}
    if(pose==4){p[6]={.31f,.39f};p[7]={.47f,.51f};p[8]={.49f,.53f};p[10]={.69f,.39f};p[11]={.52f,.51f};p[12]={.51f,.53f};}
    if(pose==5||pose==6){p[6]={.30f,.36f};p[7]={.43f,pose==5?.20f:.29f};p[8]={.48f,pose==5?.18f:.27f};}
    if(pose==7){p[6]={.17f,.35f};p[7]={.36f,.45f};p[8]={.39f,.45f};p[10]={.83f,.35f};p[11]={.64f,.45f};p[12]={.61f,.45f};}
    bool seated=pose==8||pose==9||pose==11;
    if(seated){p[0]={.51f,.54f};p[1]={.51f,.34f};p[13]={.40f,.57f};p[17]={.61f,.57f};p[14]={.30f,.70f};p[18]={.52f,.71f};p[15]={.28f,.91f};p[19]={.60f,.91f};p[16]={.23f,.97f};p[20]={.67f,.97f};p[7]={.43f,.56f};p[8]={.46f,.58f};p[11]={.57f,.56f};p[12]={.54f,.58f};}
    if(pose==10){p[6]={.22f,.36f};p[7]={.16f,.43f};p[8]={.13f,.46f};}
    static const int parents[]={-1,0,1,2,3,1,5,6,7,1,9,10,11,0,13,14,15,0,17,18,19};
    static const char *names[]={"pelvis","chest","neck","head","crown","left_shoulder","left_elbow","left_wrist","left_hand","right_shoulder","right_elbow","right_wrist","right_hand","left_hip","left_knee","left_ankle","left_toe","right_hip","right_knee","right_ankle","right_toe"};
    Layer l;l.id=storyPoses[size_t(pose)];l.grid=28;l.visible=pose==0;
    for(int j=0;j<21;++j)l.joints.push_back({names[j],parents[j],{p[j].x*im.width,p[j].y*im.height}});
    std::vector<Vec2> bind;for(auto &j:l.joints)bind.push_back(j.bind);
    for(int motion=0;motion<10;++motion){
      Clip c;c.name=storyGestures[size_t(motion)];c.duration=motion==0?7.2f:motion==8?6.4f:4.8f;c.loop=true;
      for(int frame=0;frame<=32;++frame){
        float u=frame/32.f,s=std::sin(2*pi*u),envelope=std::sin(pi*u);auto points=bind;
        float unit=float(im.height)/700;
        if(motion==0){for(size_t j=1;j<13;++j)points[j].y-=1.5f*unit*envelope*envelope;rotate(points,l,3,2,.45f*s);}
        if(motion==1){rotate(points,l,3,2,3.0f*s*envelope);for(int j=3;j<=4;++j)points[size_t(j)].y+=1.2f*unit*envelope;}
        if(motion==2)rotate(points,l,3,2,2.8f*std::sin(4*pi*u)*envelope);
        if(motion==3)rotate(points,l,3,2,4.f*envelope*envelope);
        if(motion==4){rotate(points,l,5,5,(pose==1?4.f:1.4f)*s*envelope);rotate(points,l,7,6,(pose==1?6.f:2.2f)*s*envelope);rotate(points,l,3,2,.7f*s);}
        if(motion==5){for(int j=1;j<13;++j)points[size_t(j)].y-=2.2f*unit*std::sin(6*pi*u)*envelope;rotate(points,l,3,2,-1.8f*envelope);}
        if(motion==6){rotate(points,l,3,2,-3.2f*envelope);rotate(points,l,1,0,1.f*envelope);}
        if(motion==7){rotate(points,l,1,0,1.5f*envelope);rotate(points,l,3,2,-1.f*envelope);}
        if(motion==8){for(int j=1;j<13;++j)points[size_t(j)].y-=3.f*unit*std::sin(pi*u);rotate(points,l,3,2,1.2f*s);}
        if(motion==9){rotate(points,l,3,2,-2.f*envelope);for(int j=3;j<=4;++j)points[size_t(j)].x+=1.7f*unit*envelope;}
        // Visible, continuous movement rather than a one-shot that disappears while reading.
        for(size_t j=0;j<points.size();++j)points[j]=bind[j]+(points[j]-bind[j])*2.4f;
        if(frame==0||frame==32)points=bind;
        c.keys.push_back({c.duration*u,std::move(points)});
      }
      l.clips.push_back(std::move(c));
    }
    l.scale=(seated?350.f:460.f)/im.height;
    l.offset={-float(im.width)*l.scale/2,-float(im.height)*l.scale};
    buildMesh(l,im);return l;
}
}
Scene &StoryArt::character(const std::string &id) {
    auto found=characters.find(id);if(found!=characters.end())return found->second;
    Scene s;Character c;c.id=id;c.position={480,605};c.text="";
    for(int part=0;part<3;++part){
      auto path=assets/"characters"/(id+"-"+std::to_string(part)+".png");
      if(!std::filesystem::exists(path))throw std::runtime_error("Missing character artwork: "+path.string());
      auto strip=importImage(path,4096);
      if(strip.width<strip.height*2)throw std::runtime_error("Expected a horizontal four-pose strip: "+path.string());
      for(int col=0;col<4;++col){auto image=cell(strip,col);auto layer=rig(image,part*4+col);layer.image=uint32_t(s.images.size());s.images.push_back(std::move(image));c.layers.push_back(std::move(layer));}
    }
    s.characters.push_back(std::move(c));return characters.emplace(id,std::move(s)).first->second;
}
Scene StoryArt::make(const std::string &background,const std::vector<std::string> &cast,Font font) {
    if(cast.empty()||cast.size()>4)throw std::runtime_error("A story scene needs one to four characters");
    auto bg=backgrounds.find(background);
    if(bg==backgrounds.end()){
      auto path=assets/"backgrounds"/(background+".png");
      if(!std::filesystem::exists(path))throw std::runtime_error("Missing illustrated background: "+path.string());
      bg=backgrounds.emplace(background,importImage(path,2048)).first;
    }
    Scene s;s.width=1280;s.height=640;s.title="The Light We Leave On";s.fonts.push_back(std::move(font));
    s.images.push_back(bg->second);s.backgrounds.push_back({0,{0,-106},{1280,853}});playback.clear();
    for(size_t i=0;i<cast.size();++i){auto &library=character(cast[i]);auto c=library.characters[0];
      uint32_t base=uint32_t(s.images.size());s.images.insert(s.images.end(),library.images.begin(),library.images.end());
      c.position={1280.f*(float(i)+.5f)/float(cast.size()),630};
      for(auto &l:c.layers){
        l.image+=base;float zoom=cast.size()==4?1.05f:1.16f;
        if(background=="car"){zoom=1.9f;c.position={800,820};}
        else if(background=="cinema"){zoom=1.6f;c.position.y=780;}
        l.scale*=zoom;l.offset=l.offset*zoom;
      }
      c.bubble.width=145;c.bubble.offset={-72,-48};s.characters.push_back(std::move(c));
    }
    prepare(s);return s;
}
void StoryArt::direct(Scene &s,const std::string &who,const std::string &pose,const std::string &gesture,float now) {
    for(auto &c:s.characters)if(c.id==who){
      size_t selected=0;for(size_t i=0;i<c.layers.size();++i)if(c.layers[i].id==pose)selected=i;
      for(size_t i=0;i<c.layers.size();++i)c.layers[i].visible=i==selected;
      auto &l=c.layers[selected];l.active=0;
      for(size_t i=0;i<l.clips.size();++i)if(l.clips[i].name==gesture)l.active=uint32_t(i);
      l.timeOrigin=now;playback[who]={now,now+8.f+float(selected%4),0};
    }
}
void StoryArt::tick(Scene &s,float now) {
    // Selection is persisted by the director: n equally likely clips plus one current-clip entry.
    // All clips loop; sampling is driven by the renderer's independent layer clock.
    (void)s;(void)now;
}
void StoryArt::audit(const Scene &s,const std::filesystem::path &report) const {
    std::ofstream f(report);f<<"character,pose,motion,joints,frames,max_displacement\n";
    for(auto &c:s.characters){if(c.layers.size()!=12)throw std::runtime_error("Incomplete character pose library");
      for(auto l:c.layers){if(l.clips.size()!=10)throw std::runtime_error("Incomplete character motion library");
        for(size_t clip=0;clip<l.clips.size();++clip){l.active=uint32_t(clip);float maximum=0;
          for(int frame=0;frame<=32;++frame){auto points=sample(l,l.clips[clip].duration*frame/32.f,0);deform(l,points);
            for(auto v:l.deformed)if(!std::isfinite(v.x)||!std::isfinite(v.y))throw std::runtime_error("Invalid animated mesh");
            for(size_t j=0;j<points.size();++j){auto d=points[j]-l.joints[j].bind;maximum=std::max(maximum,std::sqrt(d.x*d.x+d.y*d.y));}
          }
          if(maximum<=.01f)throw std::runtime_error("Motion contains no animation");
          f<<c.id<<','<<l.id<<','<<l.clips[clip].name<<','<<l.joints.size()<<','<<l.clips[clip].keys.size()<<','<<maximum<<'\n';
        }
      }
    }
    if(!f)throw std::runtime_error("Could not save animation audit");
}
}
