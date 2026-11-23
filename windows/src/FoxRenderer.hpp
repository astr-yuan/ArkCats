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

inline Pt interpolate(Pt a,Pt b,float t) { return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t}; }
inline Pt add(Pt a,Pt b) { return {a.x+b.x,a.y+b.y}; }
inline Pt sub(Pt a,Pt b) { return {a.x-b.x,a.y-b.y}; }
inline Pt mul(Pt a,float s) { return {a.x*s,a.y*s}; }
inline float length(Pt a) { return std::hypot(a.x,a.y); }
inline Pt shoulderSocket(bool red,int side) {
    return red ? Pt{side ? 328.f:233.f,384.f} : Pt{side ? 333.f:223.f,369.f};
}
inline Pt restingPad(bool red,int side) {
    return red ? Pt{side ? 309.f:260.f,503.f} : Pt{side ? 309.f:245.f,423.f};
}
inline float upperLength(bool red) { return red?26.f:28.f; }
inline float lowerLength(bool red) { return red?32.f:34.f; }
struct Motion {
    float breath{},sleep{},close{},earL{},earR{},tail{},yaw{},pitch{},nod{},tilt{},squash{},tuck{};
    Pt weight{},origin{},scapula[2]{};
    float contact[2]{};
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
    Motion m;
    m.breath=breath; m.sleep=sleep; m.close=close; m.earL=left; m.earR=right;
    m.tail=.022f*breath+p.tailLiftDegrees*.007f;
    m.weight=p.foxWeightShift.point();
    m.origin=mul(m.weight,.70f); // move the seat; the chest steps further toward the target
    m.yaw=std::clamp(p.headYaw.value,-1.f,1.f);m.pitch=std::clamp(p.headPitch.value,-1.f,1.f);
    m.tilt=std::clamp(p.headTilt.value*.62f+p.bodyTilt.value*.16f+m.weight.x*.0006f,-.22f,.22f);
    m.nod=(p.headOffsetY.value-5.f*sleep)*1.15f+m.pitch*3.6f;
    m.squash=std::clamp(p.bodySquash.value-1.f,-.20f,.20f);
    m.tuck=smooth(.06f,.62f,sleep);
    for(int i=0;i<2;++i) {
        Pt target=i?p.rightPaw.point():p.leftPaw.point();
        Pt rest=i?p.RightPawRest:p.LeftPawRest;
        float engagement=i?p.foxRightContact.value:p.foxLeftContact.value;
        // Notification hover and retraction are still driven by the displayed joints.
        m.contact[i]=std::clamp(std::max(engagement,smooth(1.2f,18.f,length(sub(target,rest)))),0.f,1.f);
        Pt shoulder=add(world(shoulderSocket(red,i),red),m.weight);
        Pt delta=sub(target,shoulder);
        float span=std::max(1.f,length(delta));
        float ergonomic=upperLength(red)+lowerLength(red)-6.f;
        float slide=std::clamp(span-ergonomic,0.f,red?20.f:23.f)*m.contact[i];
        m.scapula[i]=mul(delta,slide/span);
    }
    return m;
}

// Source-space displacement on a 16-pixel lattice keeps connected joints and
// chest continuous. Open-eye frames sample the untouched master face.
struct Node {
    float head,chest,leftPaw,rightPaw,tail,breathing,front,load,shoulder[2];
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
        n.head=1-smooth(298,371,y);
        n.front=ellipse(x,y,280,232,145,94);
        float hips=smooth(380,488,y);
        n.load=.90f+.10f*smooth(286,352,y)-.31f*hips;
        for(int s=0;s<2;++s) {
            Pt joint=shoulderSocket(red,s);
            // The scapula glides beneath the coat. The centerline of the bib
            // stays on the rib cage when the two shoulders move apart.
            float side=s ? smooth(284,322,x) : 1-smooth(238,276,x);
            n.shoulder[s]=.26f*side*ellipse(x,y,joint.x,joint.y,98,102)*(1-n.head);
        }
        if (red) {
            n.chest=ellipse(x,y,279,341,125,132)*(1-smooth(365,395,y))*(1-n.head);
            n.earL=bend(x,y,{210,153},{178,35},64);
            n.earR=bend(x,y,{349,153},{378,34},64);
            n.tailBend=bend(x,y,{128,416},{225,489},44);
            float tailLimit=1-smooth(208,224,x);
            n.tailBend.x*=tailLimit; n.tailBend.y*=tailLimit;
            continue;
        }
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
    float cs=std::cos(f.tilt),sn=std::sin(f.tilt),s=scale(red);
    for(int j=0; j<Nodes; ++j) for(int i=0; i<Nodes; ++i) {
        int k=j*Nodes+i;
        const Node& n=rig[k];
        float x=float(i*16)-280.f, y=float(j*16)-216.f;
        // Rotate the entire skull, ear bases and cheek silhouette as one solid
        // form. Only the snout/eye plane moves a little farther toward a gaze.
        float dx=n.head*((cs-1)*x-sn*y+8.f*f.yaw)+n.front*6.3f*f.yaw;
        float dy=n.head*(sn*x+(cs-1)*y+f.nod-57.f*f.squash)+n.front*1.4f*f.pitch;
        dx+=((n.load-.70f)*f.weight.x+n.shoulder[0]*f.scapula[0].x+n.shoulder[1]*f.scapula[1].x)/s;
        dy+=((n.load-.70f)*f.weight.y+n.shoulder[0]*f.scapula[0].y+n.shoulder[1]*f.scapula[1].y)/s;
        dx-=x*f.squash*.16f*n.chest;
        dy+=(y-235.f)*f.squash*.42f*n.chest;
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

inline Pt displacement(const Mesh& mesh,float x,float y) {
    float gx=std::clamp(x/16.f,0.f,Cells-.001f),gy=std::clamp(y/16.f,0.f,Cells-.001f);
    int i=int(gx),j=int(gy);
    auto row=mesh.data()+j*Nodes+i;
    return interpolate(interpolate(row[0],row[1],gx-i),interpolate(row[Nodes],row[Nodes+1],gx-i),gy-j);
}

// Canvas-to-source sampling needs the inverse warp; looking up a forward
// vector at the destination shortens a large shoulder reach and detaches the arm.
inline Mesh invert(const Mesh& forward) {
    Mesh back{};
    for(int j=0;j<Nodes;++j) for(int i=0;i<Nodes;++i) {
        Pt dest{16.f*i,16.f*j}, offset=forward[j*Nodes+i];
        for(int iteration=0;iteration<5;++iteration) {
            Pt next=displacement(forward,dest.x-offset.x,dest.y-offset.y);
            offset=interpolate(offset,next,.68f);
        }
        back[j*Nodes+i]=offset;
    }
    return back;
}
inline Pt anchored(Pt source,bool red,Pt origin,const Mesh& forward) {
    return add(world(add(source,displacement(forward,source.x,source.y)),red),origin);
}

inline void paintSprite(Gdiplus::Graphics& g,Artwork& art,const Pixels& pose,const Mesh& mesh,bool red,Pt shift) {
    using namespace Gdiplus;
    Matrix matrix; REAL transform[6]; g.GetTransform(&matrix); matrix.GetElements(transform);
    float deviceScale=std::max(std::hypot(transform[0],transform[1]),std::hypot(transform[2],transform[3]));
    Pt origin=add(world({0,0},red),shift);
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
    Pt shoulder, elbow, wrist, paw;
    float width, tip, weight;
};

inline Limb solveLimb(Pt shoulder,Pt goal,bool red,bool left,float mix,float upper,float lower) {
    Pt delta=sub(goal,shoulder);
    float d=std::max(.001f,length(delta));
    // The paw can fold toward the chest, but neither forearm can lengthen.
    Pt direction=mul(delta,1/d);
    d=std::min(d,upper+lower-1.3f);
    Pt paw=add(shoulder,mul(direction,d));
    float a=std::min(upper,.60f*d+3.5f),b=std::min(lower,.64f*d+4.5f);
    float elbowAlong=std::clamp((d*d+a*a-b*b)/(2*d),0.f,d);
    float outward=std::sqrt(std::max(0.f,a*a-elbowAlong*elbowAlong));
    Pt normal=left ? Pt{-direction.y,direction.x} : Pt{direction.y,-direction.x};
    Pt elbow=add(add(shoulder,mul(direction,elbowAlong)),mul(normal,outward));
    Pt hand=sub(paw,elbow); float lengthToPad=std::max(.001f,length(hand));
    Pt wrist=sub(paw,mul(hand,std::min(red?7.f:8.f,lengthToPad*.45f)/lengthToPad));
    return {shoulder,elbow,wrist,paw,red?7.0f:8.0f,red?5.5f:6.4f,mix};
}

inline Limb foreleg(bool red,int side,const CatPose& p,const Motion& m,const Mesh& forward) {
    Pt shoulder=anchored(shoulderSocket(red,side),red,m.origin,forward);
    shoulder=add(shoulder,mul(m.scapula[side],.75f));
    Pt tucked=anchored(restingPad(red,side),red,m.origin,forward);
    Pt target=side ? p.rightPaw.point() : p.leftPaw.point();
    float grip=m.contact[side];
    return solveLimb(shoulder,interpolate(tucked,target,grip),red,!side,grip,upperLength(red),lowerLength(red));
}

inline Limb hindLeg(const CatPose& p,bool red,const Motion& m,const Mesh& forward) {
    Pt hip=anchored(red?Pt{356,437}:Pt{384,423},red,m.origin,forward);
    float weight=std::clamp(p.hindFootActive.value,0.f,1.f);
    return solveLimb(hip,p.hindFoot.point(),red,false,weight,29.f,33.f);
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
    Pt a=limb.shoulder,e=limb.elbow,w=limb.wrist,c=limb.paw;
    Pt n0=unitNormal(a,e),n1=unitNormal(e,w),n2=unitNormal(w,c);
    Pt midNormal=add(n0,n1);midNormal=mul(midNormal,1.f/std::max(.01f,length(midNormal)));
    Pt t0{n0.y,-n0.x},t2{n2.y,-n2.x};
    float rootWidth=limb.width,elbowWidth=.84f*rootWidth,wristWidth=.69f*limb.tip,r=limb.tip;
    struct SidePoints { Pt root,elbow,wrist,paw; };
    auto side=[&](float sign) -> SidePoints {
        return {around(a,n0,sign*rootWidth),around(e,midNormal,sign*elbowWidth),
                around(w,n1,sign*wristWidth),around(c,n2,sign*r)};
    };
    SidePoints l=side(1),h=side(-1);
    // Round the elbow, taper at the wrist and widen around the pad. Draw both
    // flanks from these joints so the outline also follows a bent forearm.
    Pt le0=interpolate(l.root,l.elbow,.81f),le1=interpolate(l.elbow,l.wrist,.24f);
    Pt he0=interpolate(h.root,h.elbow,.81f),he1=interpolate(h.elbow,h.wrist,.24f);
    auto lower=[&](GraphicsPath& path) {
        path.AddBezier(point(le1),point(interpolate(le1,l.wrist,.50f)),point(l.wrist),point(l.wrist));
        path.AddBezier(point(l.wrist),point(around(l.wrist,t2,2.4f)),point(around(l.paw,t2,-2.8f)),point(l.paw));
        path.AddBezier(point(l.paw),point(around(l.paw,t2,1.28f*r)),point(around(h.paw,t2,1.28f*r)),point(h.paw));
    };
    GraphicsPath path;
    path.AddLine(point(l.root),point(le0));
    path.AddBezier(point(le0),point(l.elbow),point(l.elbow),point(le1));
    lower(path);
    path.AddBezier(point(h.paw),point(around(h.paw,t2,-2.8f)),point(around(h.wrist,t2,2.4f)),point(h.wrist));
    path.AddLine(point(h.wrist),point(he1));
    path.AddBezier(point(he1),point(h.elbow),point(h.elbow),point(he0));
    path.AddLine(point(he0),point(h.root));
    path.AddBezier(point(h.root),point(around(h.root,t0,-.82f*rootWidth)),
                   point(around(l.root,t0,-.82f*rootWidth)),point(l.root));
    path.CloseFigure();
    BYTE alpha=BYTE(255*limb.weight+.5f);
    Color colors[] = {red?Color(alpha,225,112,51):Color(alpha,248,239,231),
                      red?Color(alpha,226,111,49):Color(alpha,247,238,229),
                      red?Color(alpha,77,47,36):Color(alpha,244,234,223),
                      red?Color(alpha,70,43,33):Color(alpha,238,225,213)};
    REAL positions[]={0.f,.34f,.365f,1.f};
    // Extend past both round caps, so GDI+ never restarts the gradient at a toe.
    LinearGradientBrush brush(point(around(a,t0,-2.f*rootWidth)),point(around(c,t2,2.5f*r)),colors[0],colors[3]);
    brush.SetInterpolationColors(colors,positions,4);
    g.FillPath(&brush,&path);
    if(!red) {
        // The snow fox carries the same very fine cool edge on an exposed paw.
        Pen outline(Color(BYTE(alpha*.74f),117,125,139),.74f);
        GraphicsPath exposed; lower(exposed);g.DrawPath(&outline,&exposed);
    }
}

inline bool onLimb(Pt q,const Limb& limb) {
    if(limb.weight<.3f) return false;
    const Pt joints[]={limb.shoulder,limb.elbow,limb.wrist,limb.paw};
    for(int i=0;i<3;++i) {
        Pt line=sub(joints[i+1],joints[i]),v=sub(q,joints[i]);
        float t=std::clamp((v.x*line.x+v.y*line.y)/std::max(.001f,line.x*line.x+line.y*line.y),0.f,1.f);
        if(length(sub(v,mul(line,t)))<(i==0?limb.width:limb.tip)) return true;
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
    Mesh forward=deform(red,movement);
    Limb left=foreleg(red,0,pose,movement,forward),right=foreleg(red,1,pose,movement,forward);
    Pt gaze=pose.eyeGaze.point();
    paintLimb(g,hindLeg(pose,red,movement,forward),red);
    paintSprite(g,art,art.update(movement.close,left.weight,right.weight,gaze.x,gaze.y),
                invert(forward),red,movement.origin);
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
    Motion m=motion(p,red);
    Mesh forward=deform(red,m);
    if (onLimb(canvas,foreleg(red,0,p,m,forward)) || onLimb(canvas,foreleg(red,1,p,m,forward))) return 2;
    float u=280.f+(canvas.x-m.origin.x-184.f)/scale(red),v=520.f+(canvas.y-m.origin.y-238.f)/scale(red);
    if (u>=0 && v>=0 && u<Side && v<Side) {
        Pt d=displacement(invert(forward),u,v);
        u-=d.x;v-=d.y;
        float alpha=sample(art.open,u,v).a;
        if(u>=PawX && u<PawX+PawW && v>=PawY && v<PawY+PawH) {
            int k=(int(v)-PawY)*PawW+int(u)-PawX;
            int side=art.pawSide[k];
            if(side) alpha+=(art.pawBacks[(side-1)*PawW*PawH+k].a-alpha)*m.contact[side-1];
        }
        if (alpha>36) return v<304.f ? 1 : 2;
    }
    return onLimb(canvas,hindLeg(p,red,m,forward)) ? 2 : 0;
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
