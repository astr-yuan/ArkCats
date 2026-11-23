#pragma once
#include "CatPhysics.hpp"
#include "FoxSprite.hpp"

namespace arkfox {
namespace detail {

inline float smooth(float lo, float hi, float x) {
    float t=std::clamp((x-lo)/(hi-lo),0.f,1.f);
    return t*t*(3.f-2.f*t);
}
inline float pulse(float a,float b,float c,float d,float t) {
    return smooth(a,b,t)*(1-smooth(c,d,t));
}
inline float scale(bool red) { return red ? .34f : .37f; }
inline Pt world(Pt master,bool red) {
    float s=scale(red);
    return {184.f+(master.x-280.f)*s,238.f+(master.y-520.f)*s};
}
inline float ellipse(float x,float y,float cx,float cy,float rx,float ry) {
    float dx=(x-cx)/rx, dy=(y-cy)/ry;
    return 1-smooth(0,1,dx*dx+dy*dy);
}
inline Pt bend(float x,float y,Pt base,Pt tip,float radius) {
    float dx=tip.x-base.x,dy=tip.y-base.y,len2=dx*dx+dy*dy;
    float along=((x-base.x)*dx+(y-base.y)*dy)/len2;
    float off=std::abs((x-base.x)*dy-(y-base.y)*dx)/std::sqrt(len2);
    float weight=smooth(0,.83f,along)*(1-smooth(1.25f,1.60f,along))*
                 (1-smooth(radius*.62f,radius*1.10f,off));
    return {-(y-base.y)*weight,(x-base.x)*weight};
}

struct Motion {
    float breath, sleep, close, earL, earR, tail, yaw, nod, tilt, lean, squash, tuck;
};

inline Motion motion(const CatPose& p,bool red) {
    float phase=std::fmod(p.time,10.f);
    float breath=std::sin(p.time*1.25663706f);
    bool occupied=p.leftPawOnMouse || p.leftPawTyping || p.rightPawTyping;
    float sleep=std::clamp(p.sleepiness,0.f,1.f);
    float idle=occupied ? 0.f : (1-sleep)*(1-p.alertPupils);
    float close=std::max(p.effectiveEyeClosure(),smooth(.28f,.86f,sleep));
    if (red) close=std::max(close,idle*std::max(pulse(1.33f,1.62f,1.70f,2.12f,phase),
                                                .74f*pulse(3.76f,4.11f,4.16f,4.55f,phase)));
    close=std::clamp(close*(1-std::clamp(p.alertPupils*1.75f,0.f,1.f)),0.f,1.f);
    float fold=std::clamp(p.earFold.value,-.5f,1.f);
    float twitch=std::clamp(p.earTwitch.value,-1.f,1.f);
    float left=.050f*idle*pulse(2.24f,2.83f,2.97f,3.6f,phase) -.085f*fold+.07f*twitch;
    float right=-.046f*idle*pulse(7.73f,8.31f,8.42f,9.15f,phase)+.085f*fold+.06f*twitch;
    return {breath,sleep,close,left,right,.022f*breath+p.tailLiftDegrees*.007f,
            p.headYaw.value*13.f, (p.headOffsetY.value-5.f*sleep)*1.45f+p.headPitch.value*3.f,
            p.headTilt.value*.42f,p.bodyShiftX.value*1.2f+p.bodyTilt.value*10.f,
            std::clamp(p.bodySquash.value-1.f,-.20f,.20f),smooth(.06f,.62f,sleep)};
}

// Source-space displacement on a 16-pixel lattice keeps connected joints and
// chest continuous. Open-eye frames sample the untouched master face.
struct Node {
    float head,chest,leftPaw,rightPaw,tail,breathing;
    Pt earL,earR,tailBend;
};
constexpr int Cells=35, Nodes=Cells+1;
using Mesh=std::array<Pt,Nodes*Nodes>;
using Rig=std::array<Node,Nodes*Nodes>;

inline Rig makeRig(bool red) {
    Rig rig{};
    for (int j=0; j<Nodes; ++j) for(int i=0; i<Nodes; ++i) {
        float x=float(i*16),y=float(j*16);
        Node& n=rig[j*Nodes+i];
        if (red) {
            n.head=ellipse(x,y,280,174,222,178);
            n.chest=ellipse(x,y,279,341,125,132)*(1-smooth(365,395,y));
            n.earL=bend(x,y,{210,153},{178,35},64);
            n.earR=bend(x,y,{349,153},{378,34},64);
            n.tailBend=bend(x,y,{128,416},{225,489},44);
            float tailLimit=1-smooth(208,224,x);
            n.tailBend.x*=tailLimit; n.tailBend.y*=tailLimit;
            continue;
        }
        n.head=smooth(96,124,x)*(1-smooth(436,464,x))*(1-smooth(309,384,y));
        n.chest=smooth(300,329,y)*(1-smooth(397,436,y));
        float paws=smooth(381,426,y)*(1-smooth(437,459,y));
        n.leftPaw=paws*(1-smooth(12,30,std::abs(x-244)));
        n.rightPaw=paws*(1-smooth(12,30,std::abs(x-309)));
        n.tail=smooth(418,435,y)*(1-smooth(417,440,y))*smooth(195,241,x);
        n.breathing=n.chest*(1-smooth(47,135,std::abs(x-280)));
        n.earL=bend(x,y,{196,162},{186,85},45);
        n.earR=bend(x,y,{366,162},{371,85},45);
    }
    return rig;
}

inline Mesh deform(bool red,const Motion& f) {
    static const Rig redRig=makeRig(true), whiteRig=makeRig(false);
    const Rig& rig=red ? redRig : whiteRig;
    Mesh mesh{};
    for(int j=0; j<Nodes; ++j) for(int i=0; i<Nodes; ++i) {
        int k=j*Nodes+i;
        const Node& n=rig[k];
        float x=float(i*16)-280.f, y=float(j*16)-200.f;
        float dx=n.head*(f.yaw-f.tilt*y)+f.lean*(n.head+n.chest*.35f);
        float dy=n.head*(f.nod+f.tilt*x)+(y-320.f)*f.squash*(n.head+n.chest*.45f);
        if (red) {
            dx+=n.earL.x*f.earL+n.earR.x*f.earR+n.tailBend.x*f.tail+n.chest*x*.012f*f.breath;
            dy+=n.earL.y*f.earL+n.earR.y*f.earR+n.tailBend.y*f.tail+
                n.head*(3.3f*f.sleep-1.8f*f.breath)-n.chest*1.55f*f.breath;
        } else {
            dx+=1.2f*f.tuck*(n.leftPaw-n.rightPaw)+.0045f*x*n.breathing*f.breath+
                .38f*f.breath*n.tail+.38f*(n.earL.x*f.earL+n.earR.x*f.earR);
            dy+=n.head*(12.f*f.sleep+.85f*f.breath)+n.chest*(2.4f*f.sleep-.65f*f.breath)
                -4.1f*(n.leftPaw+n.rightPaw)*f.tuck+n.tail*(-1.9f*f.sleep+.4f*f.breath)
                +.38f*(n.earL.y*f.earL+n.earR.y*f.earR);
        }
        mesh[k]={dx,dy};
    }
    return mesh;
}

inline Pt interpolate(Pt a,Pt b,float t) { return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t}; }
inline Pt displacement(const Mesh& mesh,float x,float y) {
    float gx=std::clamp(x/16.f,0.f,Cells-.001f),gy=std::clamp(y/16.f,0.f,Cells-.001f);
    int i=int(gx),j=int(gy);
    auto row=mesh.data()+j*Nodes+i;
    return interpolate(interpolate(row[0],row[1],gx-i),interpolate(row[Nodes],row[Nodes+1],gx-i),gy-j);
}

inline void paintSprite(Gdiplus::Graphics& g,Artwork& art,const Pixels& pose,const Mesh& mesh,bool red) {
    using namespace Gdiplus;
    Matrix matrix; REAL transform[6]; g.GetTransform(&matrix); matrix.GetElements(transform);
    float deviceScale=std::max(std::hypot(transform[0],transform[1]),std::hypot(transform[2],transform[3]));
    Pt origin=world({0,0},red);
    const float extent=Side*scale(red);
    bool deviceAligned=transform[0]>0 && transform[3]>0 &&
                       std::abs(transform[1])<.00001f && std::abs(transform[2])<.00001f;
    float deviceX=origin.x*transform[0]+transform[4], deviceY=origin.y*transform[3]+transform[5];
    int left=int(std::floor(deviceX)),top=int(std::floor(deviceY));
    int deviceW=std::max(1,int(std::ceil(deviceX+extent*transform[0]))-left);
    int deviceH=std::max(1,int(std::ceil(deviceY+extent*transform[3]))-top);
    int fallback=std::clamp(int(std::ceil(extent*deviceScale)),1,Side);
    int width=deviceAligned ? std::min(deviceW,Side) : fallback;
    int height=deviceAligned ? std::min(deviceH,Side) : fallback;
    float stepX=deviceAligned ? deviceW/(width*transform[0]*scale(red)) : float(Side)/width;
    float stepY=deviceAligned ? deviceH/(height*transform[3]*scale(red)) : float(Side)/height;
    // Integrate the source footprint when a monitor pixel covers several
    // artwork pixels. This keeps the fine rim and ear tips antialiased.
    float tapX=.25f*std::max(0.f,stepX-1.f),tapY=.25f*std::max(0.f,stepY-1.f);
    float x0=(deviceAligned ? (left-deviceX)/(transform[0]*scale(red)) : 0.f)+.5f*stepX;
    float y0=(deviceAligned ? (top-deviceY)/(transform[3]*scale(red)) : 0.f)+.5f*stepY;

    // Compute the same continuous model on the desktop's actual pixel grid.
    // The fractional origin stays in the sampler even while the pet jumps.
    // GDI+ can now composite the finished bitmap without rescaling it.
    std::array<int,Cells+1> edges{};
    for(int i=1;i<Cells;++i)
        edges[i]=std::clamp(int(std::ceil((i*16.f-x0)/stepX)),0,width);
    edges[Cells]=width;
    art.rendered.resize(width*height);
    for(int y=0; y<height; ++y) {
        float py=y0+y*stepY;
        float gridY=std::clamp(py/16.f,0.f,Cells-.0001f);
        int j=int(gridY);float fy=gridY-j;
        const Pt* row=mesh.data()+j*Nodes;
        Pt a=interpolate(row[0],row[Nodes],fy);
        for(int i=0; i<Cells; ++i) {
            Pt b=interpolate(row[i+1],row[Nodes+i+1],fy);
            float gx=x0+stepX*edges[i]-16.f*i;
            float dx=(b.x-a.x)/16.f,dy=(b.y-a.y)/16.f;
            float u=16.f*i+gx-a.x-gx*dx-.5f,v=py-a.y-gx*dy-.5f;
            float du=stepX*(1-dx),dv=-stepX*dy;
            Pixel* dest=art.rendered.data()+y*width;
            if (std::max(stepX,stepY)>1.2f) {
                for(int k=edges[i];k<edges[i+1];++k,u+=du,v+=dv) {
                    if(art.nearEdge(u,v))
                        dest[k]=blend(blend(sample(pose,u-tapX,v-tapY),sample(pose,u+tapX,v-tapY),128),
                                      blend(sample(pose,u-tapX,v+tapY),sample(pose,u+tapX,v+tapY),128),128);
                    else dest[k]=sample(pose,u,v);
                }
            } else {
                for(int k=edges[i];k<edges[i+1];++k,u+=du,v+=dv) dest[k]=sample(pose,u,v);
            }
            a=b;
        }
    }
    Bitmap sprite(width,height,width*4,PixelFormat32bppPARGB,reinterpret_cast<BYTE*>(art.rendered.data()));
    if (deviceAligned) {
        auto state=g.Save();g.ResetTransform();
        g.DrawImage(&sprite,left,top,deviceW,deviceH);
        g.Restore(state);
    } else {
        g.DrawImage(&sprite,RectF(origin.x,origin.y,extent,extent),0.f,0.f,float(width),float(height),UnitPixel);
    }
}

struct Limb {
    Pt shoulder, middle, paw;
    float width, tip, weight;
};

inline Limb foreleg(bool red,bool left,const CatPose& p) {
    Pt joint=left ? p.leftPaw.point() : p.rightPaw.point();
    Pt rest=left ? p.LeftPawRest : p.RightPawRest;
    float dist=std::hypot(joint.x-rest.x,joint.y-rest.y);
    float weight=smooth(.8f,15.f,dist);
    Pt paw=world(red ? Pt{left ? 260.f:309.f,506.f} : Pt{left ? 245.f:309.f,423.f},red);
    paw=interpolate(paw,rest,weight);
    paw.x+=joint.x-rest.x; paw.y+=joint.y-rest.y;
    Pt shoulder=world(red ? Pt{left ? 246.f:322.f,390.f} : Pt{left ? 226.f:332.f,364.f},red);
    Pt middle=interpolate(shoulder,paw,.51f);
    middle.x+=(left ? -7.f : 7.f)*weight;
    middle.y-=4.f*weight;
    return {shoulder,middle,paw,red?7.1f:8.2f,red?6.5f:7.4f,weight};
}

inline Limb hindLeg(const CatPose& p,bool red) {
    Pt hip=world(red?Pt{356,437}:Pt{384,423},red);
    Pt paw=p.hindFoot.point(), mid=interpolate(hip,paw,.5f);
    mid.x+=10;mid.y-=7;
    return {hip,mid,paw,8.f,6.3f,std::clamp(p.hindFootActive.value,0.f,1.f)};
}

inline Pt around(Pt center,Pt normal,float radius) {
    return {center.x+normal.x*radius,center.y+normal.y*radius};
}
inline Pt unitNormal(Pt from,Pt to) {
    float dx=to.x-from.x,dy=to.y-from.y,len=std::max(.01f,std::hypot(dx,dy));
    return {-dy/len,dx/len};
}
inline Gdiplus::PointF point(Pt p) { return {p.x,p.y}; }

inline void paintLimb(Gdiplus::Graphics& g,const Limb& limb,bool red) {
    using namespace Gdiplus;
    if(limb.weight<=.006f) return;
    Pt a=limb.shoulder,b=limb.middle,c=limb.paw;
    Pt n0=unitNormal(a,b),n1=unitNormal(b,c);
    Pt t0={n0.y,-n0.x},t1={n1.y,-n1.x};
    float w=limb.width,r=limb.tip;
    Pt al=around(a,n0,w),ar=around(a,n0,-w),bl=around(b,n0,w),br=around(b,n0,-w);
    Pt cl=around(c,n1,r),cr=around(c,n1,-r);
    GraphicsPath path;
    path.AddBezier(point(al),point(bl),point(around(cl,t1,-r)),point(cl));
    path.AddBezier(point(cl),point(around(cl,t1,1.3f*r)),point(around(cr,t1,1.3f*r)),point(cr));
    path.AddBezier(point(cr),point(around(cr,t1,-r)),point(br),point(ar));
    path.AddBezier(point(ar),point(around(ar,t0,-.7f*w)),point(around(al,t0,-.7f*w)),point(al));
    path.CloseFigure();
    BYTE alpha=BYTE(255*limb.weight+.5f);
    Color colors[] = {red?Color(alpha,225,112,51):Color(alpha,248,239,231),
                      red?Color(alpha,226,111,49):Color(alpha,247,238,229),
                      red?Color(alpha,77,47,36):Color(alpha,244,234,223),
                      red?Color(alpha,70,43,33):Color(alpha,238,225,213)};
    REAL positions[]={0.f,.34f,.365f,1.f};
    // Extend past both round caps, so GDI+ never restarts the gradient at a toe.
    LinearGradientBrush brush(point(around(a,t0,-w)),point(around(c,t1,2.f*r)),colors[0],colors[3]);
    brush.SetInterpolationColors(colors,positions,4);
    g.FillPath(&brush,&path);
    if(!red) {
        // The snow fox carries the same very fine cool edge on an exposed paw.
        Pen outline(Color(BYTE(alpha*.74f),117,125,139),.74f);
        g.DrawBezier(&outline,point(al),point(bl),point(around(cl,t1,-r)),point(cl));
        g.DrawBezier(&outline,point(cl),point(around(cl,t1,1.3f*r)),point(around(cr,t1,1.3f*r)),point(cr));
    }
}

inline bool onLimb(Pt q,const Limb& limb) {
    if(limb.weight<.3f) return false;
    Pt last=limb.shoulder;
    for(int i=1;i<=8;++i) {
        float t=i/8.f;
        Pt here=interpolate(interpolate(limb.shoulder,limb.middle,t),interpolate(limb.middle,limb.paw,t),t);
        float dx=here.x-last.x,dy=here.y-last.y;
        float f=std::clamp(((q.x-last.x)*dx+(q.y-last.y)*dy)/std::max(.001f,dx*dx+dy*dy),0.f,1.f);
        if(std::hypot(q.x-last.x-f*dx,q.y-last.y-f*dy)<limb.tip) return true;
        last=here;
    }
    return false;
}

} // namespace detail

inline void draw(Gdiplus::Graphics& g,const CatPhysics& simulation,CatCoatTheme coat) {
    using namespace detail;
    bool red=coat==CatCoatTheme::RedFox;
    Artwork& art=artwork(red);
    if(art.open.empty()) return;
    const CatPose& pose=simulation.displayPose();
    Motion movement=motion(pose,red);
    Limb left=foreleg(red,true,pose),right=foreleg(red,false,pose);
    Mesh mesh=deform(red,movement);
    paintLimb(g,hindLeg(pose,red),red);
    paintSprite(g,art,art.update(movement.close,left.weight,right.weight),mesh,red);
    paintLimb(g,left,red);
    paintLimb(g,right,red);
}

inline int hitTest(Pt canvas,const CatPhysics& simulation) {
    using namespace detail;
    bool red=simulation.coat()==CatCoatTheme::RedFox;
    auto& art=artwork(red);
    if(art.open.empty()) return 0;
    const CatPose& p=simulation.displayPose();
    canvas.y+=p.jumpHeight;
    float u=280.f+(canvas.x-184.f)/scale(red),v=520.f+(canvas.y-238.f)/scale(red);
    if (u>=0 && v>=0 && u<Side && v<Side) {
        Mesh mesh=deform(red,motion(p,red));
        Pt d=displacement(mesh,u,v);
        u-=d.x;v-=d.y;
        if (sample(art.open,u,v).a>36) return v<304.f ? 1 : 2;
    }
    return onLimb(canvas,foreleg(red,true,p)) || onLimb(canvas,foreleg(red,false,p)) ||
           onLimb(canvas,hindLeg(p,red)) ? 2 : 0;
}

inline bool drawPortrait(Gdiplus::Graphics& g,CatCoatTheme coat,const Gdiplus::RectF& box) {
    using namespace Gdiplus;
    bool red=coat==CatCoatTheme::RedFox;
    auto& art=detail::artwork(red);
    if(art.open.empty()) return false;
    Bitmap portrait(detail::Side,detail::Side,detail::Side*4,PixelFormat32bppPARGB,
                    reinterpret_cast<BYTE*>(art.open.data()));
    // Crop the entire pair of ears and the muzzle from the registered face.
    float w=red?318.f:328.f, h=red?282.f:230.f;
    float s=std::min(box.Width/w,box.Height/h);
    RectF dest(box.X+(box.Width-w*s)*.5f,box.Y+(box.Height-h*s)*.5f,w*s,h*s);
    auto previous=g.Save();
    GraphicsPath clip;
    // The cheeks meet the chest in the master image. A curved lower boundary
    // prevents two clipped columns of coat from showing below the chin.
    clip.AddRectangle(RectF(dest.X,dest.Y,dest.Width,dest.Height*.66f));
    clip.AddEllipse(RectF(dest.X,dest.Y+dest.Height*.26f,dest.Width,dest.Height*.74f));
    clip.SetFillMode(FillModeWinding);
    g.SetClip(&clip,CombineModeIntersect);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    auto status=g.DrawImage(&portrait,dest,280.f-w*.5f,red?26.f:77.f,w,h,UnitPixel);
    g.Restore(previous);
    return status==Ok;
}

} // namespace arkfox
