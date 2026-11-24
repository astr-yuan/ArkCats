#pragma once
#include "CatPhysics.hpp"
#include "LuoArtwork.hpp"
#include "LuoEyeContours.hpp"
#include "HeiXiu.hpp"
#include <iterator>

namespace arkluo {
namespace detail {
using namespace Gdiplus;
using art::Part;

inline float unit(float v) { return std::clamp(v,0.f,1.f); }
inline float ease(float x) { float u=unit(x); return u*u*(3.f-2.f*u); }
inline float mix(float a,float b,float u) { return a+(b-a)*u; }
inline Pt blend(Pt a,Pt b,float u) { return {mix(a.x,b.x,u),mix(a.y,b.y,u)}; }
inline Pt plus(Pt p,Pt d) { return {p.x+d.x,p.y+d.y}; }
inline Pt minus(Pt a,Pt b) { return {a.x-b.x,a.y-b.y}; }
inline Pt times(Pt p,float s) { return {p.x*s,p.y*s}; }
inline float norm(Pt p) { return std::hypot(p.x,p.y); }
inline PointF pf(Pt p) { return {p.x,p.y}; }
inline Pt source(float x,float y) { return {184.f+(x-309.f)*.35f,234.f+(y-624.f)*.35f}; }
inline Color ink(float a=1.f) { return Color(BYTE(255*unit(a)),80,50,38); }
inline void rounded(Pen& pen) {
    pen.SetLineJoin(LineJoinRound);
    pen.SetLineCap(LineCapRound,LineCapRound,DashCapRound);
}

// Each point of the lower silhouette is attached to one of these three planes.
// The plane also positions the cutout textures using the identical transformation.
struct Plane {
    float a=1,b=0,c=0,d=1,x=0,y=0;
    Plane(Pt offset,float angle,Pt pivot,Pt center,Pt scale) {
        float radians=angle*.01745329252f, cs=std::cos(radians),sn=std::sin(radians);
        a=.35f*scale.x*cs; b=.35f*scale.x*sn;
        c=-.35f*scale.y*sn; d=.35f*scale.y*cs;
        Pt origin=source(0,0);
        float dx=(origin.x-center.x)*scale.x+center.x-pivot.x;
        float dy=(origin.y-center.y)*scale.y+center.y-pivot.y;
        x=offset.x+pivot.x+cs*dx-sn*dy; y=offset.y+pivot.y+sn*dx+cs*dy;
    }
    Pt operator()(float u,float v) const { return {x+a*u+c*v,y+b*u+d*v}; }
    Pt invert(Pt p) const {
        float det=a*d-b*c;
        return {((p.x-x)*d-(p.y-y)*c)/det,((p.y-y)*a-(p.x-x)*b)/det};
    }
    void apply(Graphics& g) const {
        Matrix matrix(a,b,c,d,x,y);
        g.MultiplyTransform(&matrix);
    }
};

struct Motion {
    Pt armShift{},seatShift{},chestShift{},whole{1,1};
    float lift{},yaw{},pitch{},headAngle{},sleep{},pet{},breath{},padBreath{},surprise{};
    Plane head,seat,chest,tail;
    Pt perch() const {
        Pt p=head(306,181);
        return {184+(p.x-184)*whole.x,234+(p.y-234)*whole.y+lift};
    }
    Pt unscale(Pt p) const { return {184+(p.x-184)/whole.x,234+(p.y-234-lift)/whole.y}; }
    void apply(Graphics& g) const {
        g.TranslateTransform(184,234+lift);g.ScaleTransform(whole.x,whole.y);g.TranslateTransform(-184,-234);
    }
    explicit Motion(const CatPose& p)
        :head({},0,{},{},{1,1}),seat(head),chest(head),tail(head) {
        breath=std::sin(p.time*1.53f); padBreath=std::sin(p.time*1.53f-.54f);
        yaw=std::clamp(p.headYaw.value,-1.f,1.f); pitch=std::clamp(p.headPitch.value,-1.f,1.f);
        sleep=ease((p.luoSleepBlend-.26f)/.54f);
        pet=ease(p.petHeadIntensity*2.f);
        surprise=unit(p.alertPupils*1.7f);
        float weight=0;
        for(int i=0;i<2;++i) {
            Pt tip=i?p.rightPaw.point():p.leftPaw.point();
            Pt rest=i?p.RightPawRest:p.LeftPawRest;
            float contact=ease(norm(minus(tip,rest))/18.f);
            Pt delta=minus(tip,{i?197.f:169.f,229.f});
            float reach=ease((norm(delta)-28.f)/83.f)*contact;
            armShift.x+=std::clamp(delta.x*.135f,-12.f,12.f)*reach;
            armShift.y+=std::clamp(delta.y*.095f,0.f,5.2f)*reach;
            weight+=contact;
        }
        armShift=times(armShift,1/std::max(1.f,weight));
        float load=std::clamp(1.f-p.bodySquash.value,-.045f,.11f);
        whole={1+.024f*p.sleepiness+.66f*load,1-.14f*p.sleepiness-load};
        lift=load*11.f;
        seatShift={armShift.x*.64f,armShift.y*.64f+.52f*padBreath};
        chestShift={armShift.x+.70f*yaw,armShift.y-.90f*breath};
        seat=Plane(seatShift,0,{184,234},{184,234},{1+.007f*padBreath,1+.013f*padBreath});
        chest=Plane(chestShift,armShift.x*.10f-.45f*yaw,{184,225},{184,230},{1+.013f*breath,1+.019f*breath});
        tail=Plane({seatShift.x-.42f*yaw,seatShift.y},
                   2.3f*std::sin(p.time*1.53f-.9f)-armShift.x*.18f-.57f*p.tailLiftDegrees,
                   source(207,594),{184,234},{1,1});
        float nod=2.15f*pitch+.86f*breath+2.4f*pet*std::sin(p.time*4.1f)+4.2f*p.sleepiness-1.3f*surprise;
        headAngle=2.1f*yaw+1.75f*yaw*pitch+armShift.x*.072f+pet*(1+2.1f*std::sin(p.time*3.4f));
        head=Plane({armShift.x*.89f+yaw*2.4f,armShift.y*.85f+nod},headAngle,
                   source(309,458),source(309,310),{1-.030f*yaw*yaw,1-.009f*pitch*pitch});
    }
};

inline void cubic(GraphicsPath& path,Pt& last,Pt a,Pt b,Pt end) {
    path.AddBezier(pf(last),pf(a),pf(b),pf(end));
    last=end;
}
inline void silhouette(GraphicsPath& path,const Motion& m,float tuck) {
    const Plane &h=m.head,&c=m.chest,&b=m.seat;
    Pt l=blend(h(243,473),c(243,473),.24f),r=blend(h(370,473),c(370,473),.24f),last=h(117,327);
    auto add=[&](Pt a,Pt b,Pt end) { cubic(path,last,a,b,end); };
    add(h(118,351),h(124,394),h(141,415));
    add(h(161,446),h(196,462),h(231,467));
    add(h(237,468),plus(l,{0,-.95f}),l);
    add(plus(l,{0,1.2f}),c(226,493),c(216,527));
    add(c(211,543),b(206,568),b(208,590));
    add(b(209,615),b(223,623),b(247,624));
    add(b(270,624),b(291,619),b(307,619));
    add(b(328,619),b(348,625),b(374,624-5*tuck));
    add(b(398,624-9*tuck),b(409,615-7*tuck),b(407,584));
    add(b(407,560),c(403,538),c(394,515));
    add(c(385,493),plus(r,{0,1.2f}),r);
    add(plus(r,{0,-.95f}),h(378,468),h(385,467));
    add(h(420,461),h(450,444),h(467,419));
    add(h(480,400),h(486,360),h(489,327));
}

constexpr Pt pawTrace[2][13]={
    {{246,529},{242,552},{239,590},{248,608},{254,623},{262,627},{273,627},
     {291,628},{303,623},{303,610},{305,591},{300,552},{296,537}},
    {{319,537},{316,554},{313,588},{315,607},{316,622},{323,630},{336,629},
     {352,629},{364,622},{368,601},{373,582},{374,551},{371,531}}
};
constexpr Pt rootJoint[2]={{271,530},{345,530}},padJoint[2]={{275,613},{339,614}};

struct Limb {
    int side;
    Pt start,control,tip;
    float taper,swing,width,length,opacity;
    Pt trace(Pt point) const {
        Pt j=rootJoint[side],pad=padJoint[side];
        float f=(point.y-j.y)/(pad.y-j.y),u=unit(f),v=1-u;
        float across=(point.x-mix(j.x,pad.x,f))*.35f*width*(1-taper*ease((u-.11f)/.89f));
        Pt tangent=blend(minus(control,start),minus(tip,control),u);
        float len=norm(tangent);
        tangent=len>.0001f?times(tangent,1.f/len):Pt{0,1};
        Pt at=blend(blend(start,control,u),blend(control,tip,u),u);
        float cap=(f-u)*length*(f>1?1-.8f*taper:1);
        return {at.x+across*mix(1,tangent.y,swing)+cap*tangent.x,at.y-across*tangent.x*swing+cap*tangent.y};
    }
    void path(GraphicsPath& path) const {
        PointF pts[13];
        for(int j=0;j<13;++j)pts[j]=pf(trace(pawTrace[side][j]));
        path.AddBeziers(pts,13);
    }
};
inline Limb hind(const CatPose& p,const Motion& m) {
    Pt hip=plus({210,228},m.seatShift),tip=p.hindFoot.point();
    float w=unit(p.hindFootActive.value),distance=norm(minus(tip,hip)),bend=w*std::min(1.f,distance/25);
    Pt control=plus(blend(hip,tip,.5f),{10*bend,-8*bend});
    return {1,hip,control,tip,.13f+.10f*ease((distance-20)/50),1,.78f,22,ease((w-.03f)/.19f)};
}
inline Limb front(int i,const CatPose& p,const Motion& m,const Limb& foot) {
    Pt j=rootJoint[i],pad=padJoint[i];
    Pt start=plus(source(j.x,j.y),m.chestShift),rest=plus(source(pad.x,pad.y),m.seatShift);
    Pt raw=i?p.rightPaw.point():p.leftPaw.point(),neutral=i?p.RightPawRest:p.LeftPawRest;
    float engaged=ease(norm(minus(raw,neutral))/12.f);
    Pt tip=plus(raw,times(minus(rest,neutral),1-engaged));
    Pt control=plus(blend(start,tip,.5f),{(i?4.f:-4.f)*engaged,-5*engaged});
    float span=norm(minus(tip,start)),swing=unit(norm(minus(tip,rest))/24.f);
    float rear=unit(p.hindFootActive.value);
    if(rear>.03f && engaged>.03f && span>.01f) {
        Pt d=minus(tip,start),offset=minus(foot.tip,start);
        float u=unit((offset.x*d.x+offset.y*d.y)/(span*span)),v=1-u;
        Pt n=times({d.y,-d.x},(i?1.f:-1.f)/span);
        Pt gap=minus(blend(blend(start,control,u),blend(control,tip,u),u),foot.tip);
        float separation=gap.x*n.x+gap.y*n.y;
        float overlap=ease((13-std::abs(separation))/8)*ease((u-.15f)/.18f)*ease((.975f-u)/.13f);
        control=plus(control,times(n,std::min(22.f,std::max(0.f,12-separation)/std::max(.12f,2*u*v))*overlap*rear*engaged));
    }
    return {i,start,control,tip,.235f*ease((span-norm(minus(rest,start))-5)/69),swing,1,rest.y-start.y,1};
}
inline void paintLimb(Graphics& g,const Limb& leg) {
    if(leg.opacity<=.001f)return;
    GraphicsPath path;leg.path(path);
    SolidBrush fill(Color(BYTE(255*leg.opacity),1,1,1));g.FillPath(&fill,&path);
    Pt tip=leg.tip;
    if(norm(minus(tip,leg.start))<.01f)tip.y+=.01f;
    LinearGradientBrush gradient(pf(leg.start),pf(tip),ink(.06f*leg.opacity),ink(leg.opacity));
    Color colors[]={ink(.06f*leg.opacity),ink(.73f*leg.opacity),ink(leg.opacity),ink(leg.opacity)};
    REAL stops[]={0,.12f,.28f,1};
    gradient.SetInterpolationColors(colors,stops,4);
    // The painted tip projects past the wrist. Extend the solid end colour
    // through it rather than letting the gradient restart at the round toe.
    gradient.SetWrapMode(WrapModeTileFlipX);
    Pen rim(&gradient,3.15f);rounded(rim);g.DrawPath(&rim,&path);
}

inline void around(Graphics& g,Pt center,float x,float y) {
    g.TranslateTransform(center.x,center.y);g.ScaleTransform(x,y);g.TranslateTransform(-center.x,-center.y);
}
inline void face(Graphics& g,const CatPose& p,const Motion& m) {
    auto skull=g.Save();m.head.apply(g);
    art::draw(g,Part::Head);
    float blink=std::max(p.rawBlink()*(1-m.surprise),p.sleepiness*.85f);
    float star=m.pet*(1-blink);
    Pt gaze=p.eyeGaze.point();
    for(int i=0;i<2;++i) {
        Pt center=i?Pt{402,316}:Pt{212,314};
        float farSide=std::max(0.f,(i?1:-1)*m.yaw),nearSide=std::max(0.f,(i?-1:1)*m.yaw);
        float alpha=blink>.92f?ease((1-blink)/.08f):1;
        if(alpha>.001f) {
            auto eye=g.Save();
            g.TranslateTransform(m.yaw*(8.4f+3.2f*nearSide),m.pitch*(4.8f+1.2f*nearSide));
            around(g,plus(center,{0,8}),1-.13f*farSide+.014f*nearSide,std::max(.012f,1-blink)*(1-.016f*std::abs(m.pitch)));
            art::draw(g,i?Part::EyeR:Part::EyeL,alpha);
            GraphicsPath socket;
            if(i)socket.AddBeziers(rightEyeRim,int(std::size(rightEyeRim)));
            else socket.AddBeziers(leftEyeRim,int(std::size(leftEyeRim)));
            socket.CloseFigure();
            g.SetClip(&socket,CombineModeIntersect);
            if(star<.999f) {
                auto pupil=g.Save();
                g.TranslateTransform(std::clamp(gaze.x/.88f,-1.f,1.f)*(15-8*star),std::clamp(gaze.y/.85f,-1.f,1.f)*(14-8*star));
                around(g,center,1-.13f*m.surprise,1-.13f*m.surprise);
                art::draw(g,i?Part::PupilR:Part::PupilL,alpha);
                g.Restore(pupil);
            }
            if(star>.001f)art::draw(g,i?Part::StarR:Part::StarL,star*alpha);
            g.Restore(eye);
        }
        float lid=ease((blink-.84f)/.15f);
        if(lid>0) {
            Pt a=i?Pt{331,323}:Pt{136,320},control=i?Pt{400,359}:Pt{205,357},b=i?Pt{469,322}:Pt{286,318};
            Pen pen(ink(lid),10);rounded(pen);
            g.DrawBezier(&pen,pf(a),pf(blend(a,control,2.f/3)),pf(blend(b,control,2.f/3)),pf(b));
        }
    }
    if(m.pet>.001f)art::draw(g,Part::Mouth,m.pet*.96f);
    g.Restore(skull);
}

inline void seated(Graphics& g,const CatPose& p,const Motion& m) {
    auto sitting=g.Save();m.apply(g);
    auto tail=g.Save();m.tail.apply(g);art::draw(g,Part::Tail);g.Restore(tail);
    GraphicsPath neck;
    silhouette(neck,m,ease((p.hindFootActive.value-.14f)/.30f));
    SolidBrush fill(Color(255,1,1,1));g.FillPath(&fill,&neck);
    Pen rim(ink(),3.65f);rounded(rim);g.DrawPath(&rim,&neck);
    Limb foot=hind(p,m);paintLimb(g,foot);
    for(int i=0;i<2;++i)paintLimb(g,front(i,p,m,foot));
    face(g,p,m);
    g.Restore(sitting);
}
inline void configure(Graphics& g) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetInterpolationMode(InterpolationModeHighQualityBilinear);
    g.SetPixelOffsetMode(PixelOffsetModeHalf);
}
inline void paint(Graphics& g,const CatPose& p,const Motion& m) {
    auto saved=g.Save();configure(g);
    if(m.sleep>.001f) {
        auto nap=g.Save();g.TranslateTransform(184,234+(1-m.sleep)*3);
        g.ScaleTransform(.215f*(1+.006f*m.padBreath),.215f*(1+.013f*m.breath));
        g.TranslateTransform(-458,-619);art::draw(g,Part::Sleep,m.sleep);g.Restore(nap);
    }
    if(m.sleep<=.001f)seated(g,p,m);
    else if(m.sleep<.999f) {
        // Fade the entire seated silhouette as a single image. Compositing each
        // eye or paw separately would show the construction inside the body.
        Bitmap layer(720,640,PixelFormat32bppPARGB);
        {
            Graphics off(&layer);configure(off);off.ScaleTransform(2,2);seated(off,p,m);
        }
        ColorMatrix fade={{{1,0,0,0,0},{0,1,0,0,0},{0,0,1,0,0},{0,0,0,1-m.sleep,0},{0,0,0,0,1}}};
        ImageAttributes attr;attr.SetColorMatrix(&fade);
        g.DrawImage(&layer,RectF(0,0,360,320),0,0,720,640,UnitPixel,&attr);
    }
    drawHeiXiu(g,m.perch(),m.headAngle,p.time,m.sleep,p.luoQuietAge,p.luoNoticeAge,p.luoBrushAge);
    g.Restore(saved);
}
} // namespace detail

inline void draw(Gdiplus::Graphics& g,const CatPhysics& simulation) {
    const CatPose& p=simulation.displayPose();
    detail::paint(g,p,detail::Motion(p));
}

inline int hitTest(Pt canvas,const CatPhysics& simulation) {
    using namespace detail;
    const CatPose& p=simulation.displayPose();Motion m(p);
    canvas.y+=p.jumpHeight;
    Pt perch=blend(m.perch(),{169,141.4f+m.breath},m.sleep);
    float age=m.sleep<.025f?heixiu::hopAge(p.luoQuietAge):-1.f;
    float travel=p.luoNoticeAge>.09f && p.luoNoticeAge<.82f?std::sin((p.luoNoticeAge-.09f)*3.14159265f/.73f):0.f;
    perch.x+=3.5f*travel;
    perch.y-=heixiu::arc(age,.25f,.71f,4.6f)+heixiu::arc(age,.84f,1.18f,2.15f)+20.f*travel;
    if(std::hypot((canvas.x-perch.x)/24.f,(canvas.y-perch.y+13)/19.f)<=1)return 1;
    if(m.sleep>.52f) {
        float u=458+(canvas.x-184)/(.215f*(1+.006f*m.padBreath));
        float v=619+(canvas.y-234-(1-m.sleep)*3)/(.215f*(1+.013f*m.breath));
        return art::contains(Part::Sleep,u,v)?(u<620 && v<526?1:2):0;
    }
    Pt q=m.unscale(canvas),s=m.head.invert(q);
    if(art::contains(Part::Head,s.x,s.y))return 1;
    Limb foot=hind(p,m);
    for(const Limb& l:{front(0,p,m,foot),front(1,p,m,foot),foot}) {
        if(l.opacity<.08f)continue;
        Gdiplus::GraphicsPath path;l.path(path);
        if(path.IsVisible(q.x,q.y))return 2;
    }
    Gdiplus::GraphicsPath path;silhouette(path,m,0);
    if(path.IsVisible(q.x,q.y))return s.y<472?1:2;
    Pt tail=m.tail.invert(q);
    return art::contains(Part::Tail,tail.x,tail.y)?2:0;
}

inline bool drawPortrait(Gdiplus::Graphics& g,const Gdiplus::RectF& box) {
    CatPose p;
    auto saved=g.Save();
    float scale=std::min(box.Width/145.f,box.Height/138.f);
    g.TranslateTransform(box.X+(box.Width-145*scale)*.5f,box.Y+(box.Height-138*scale)*.5f);
    g.ScaleTransform(scale,scale);g.TranslateTransform(-111.f,-47.f);
    Gdiplus::GraphicsPath crop;
    crop.AddLine(111.f,47.f,256.f,47.f);
    crop.AddLine(256.f,47.f,256.f,160.f);
    crop.AddBezier(256.f,160.f,225.f,173.f,207.f,182.f,184.f,182.f);
    crop.AddBezier(184.f,182.f,161.f,182.f,143.f,173.f,111.f,160.f);
    crop.CloseFigure();
    g.SetClip(&crop,Gdiplus::CombineModeIntersect);
    detail::paint(g,p,detail::Motion(p));
    g.Restore(saved);
    return true;
}
} // namespace arkluo
