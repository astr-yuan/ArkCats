#pragma once
#include "KeyMap.hpp"
#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <array>
#include <cmath>

namespace arkluo {
namespace heixiu {

inline float ease(float x) {
    x=std::clamp(x,0.f,1.f);
    return x*x*(3.f-2.f*x);
}
inline float pulse(float t,float start,float peak,float release,float end) {
    return ease((t-start)/(peak-start))*(1.f-ease((t-release)/(end-release)));
}
inline float arc(float t,float start,float end,float height) {
    const float u=std::clamp((t-start)/(end-start),0.f,1.f);
    return height*4.f*u*(1.f-u);
}
inline float hopAge(float quietAge) {
    if (quietAge<0.f) return -1.f;
    const float phase=std::fmod(quietAge,95.4f);
    for (float start : {19.6f,46.2f,65.1f,87.8f})
        if (phase>=start && phase<start+1.48f) return phase-start;
    return -1.f;
}
inline void quadratic(Gdiplus::GraphicsPath& p,Pt from,Pt control,Pt to) {
    p.AddBezier(from.x,from.y,(from.x+2.f*control.x)/3.f,(from.y+2.f*control.y)/3.f,
                (to.x+2.f*control.x)/3.f,(to.y+2.f*control.y)/3.f,to.x,to.y);
}
inline void drawBody(Gdiplus::Graphics& g,float sx,float sy) {
    using namespace Gdiplus;
    const auto state=g.Save();
    g.TranslateTransform(306.f,191.f);
    g.ScaleTransform(sx,sy);
    g.TranslateTransform(-306.f,-191.f);
    constexpr Pt segments[][2]={
        {{241,124},{258,111}},{{258,99},{264,96}},{{270,93},{279,101}},
        {{295,98},{312,100}},{{317,89},{323,91}},{{328,92},{334,104}},
        {{356,111},{366,128}},{{374,142},{371,161}},{{367,182},{351,187}},
        {{328,194},{305,191}},{{275,195},{253,186}},{{235,177},{238,149}}
    };
    Pt from{238,149};
    GraphicsPath silhouette;
    for (const auto& segment : segments) {
        quadratic(silhouette,from,segment[0],segment[1]);
        from=segment[1];
    }
    silhouette.CloseFigure();
    SolidBrush body(Color(255,8,9,10));
    Pen rim(Color(255,20,21,23),1.45f);
    rim.SetLineJoin(LineJoinRound);
    g.FillPath(&body,&silhouette);
    g.DrawPath(&rim,&silhouette);
    g.Restore(state);
}

// Intersect each fixed ellipse with the two curved lids. A single filled
// outline keeps the moving lid edge antialiased, even on a scaled window.
inline void eyeOpening(Gdiplus::Graphics& g,Gdiplus::SolidBrush& fill,
                       float rx,float ry,float closure) {
    if (closure<.001f) {
        g.FillEllipse(&fill,-rx,-ry,2.f*rx,2.f*ry);
        return;
    }
    if (closure>.998f) return;
    const float gap=1.f-closure;
    const float top=2.f-36.f*gap,bottom=2.f+29.f*gap;
    const float topCenter=(top+11.1f-42.f*gap)*.5f;
    const float bottomCenter=(bottom+11.1f+25.f*gap)*.5f;
    auto span=[&](float x) {
        const float q=x*x/(29.f*29.f);
        const float height=ry*std::sqrt(std::max(0.f,1.f-x*x/(rx*rx)));
        return Pt{std::max(-height,topCenter+(top-topCenter)*q),
                  std::min(height,bottomCenter+(bottom-bottomCenter)*q)};
    };
    float right=rx;
    if (span(right).y<span(right).x) {
        float lo=0.f,hi=rx;
        for (int i=0;i<14;++i) {
            const float mid=(lo+hi)*.5f;
            const Pt section=span(mid);
            if (section.y>=section.x) lo=mid; else hi=mid;
        }
        right=lo;
    }
    constexpr int count=40;
    std::array<Gdiplus::PointF,2*(count+1)> points;
    for (int i=0;i<=count;++i) {
        const float x=-right*std::cos(3.14159265f*i/count);
        const Pt section=span(x);
        points[i]={x,section.x};
        points[2*count+1-i]={x,section.y};
    }
    g.FillPolygon(&fill,points.data(),int(points.size()));
}
} // namespace heixiu

inline void drawHeiXiu(Gdiplus::Graphics& g,Pt perch,float headAngle,float time,
                       float sleep,float quietAge,float noticeAge,float brushAge) {
    using namespace Gdiplus;
    using namespace heixiu;
    const float age=sleep<.025f?hopAge(quietAge):-1.f;
    const float first=arc(age,.25f,.71f,4.6f),second=arc(age,.84f,1.18f,2.15f);
    const float firstPress=pulse(age,0,.162f,.185f,.25f);
    const float rebound=pulse(age,.71f,.774f,.787f,.84f);
    const float settle=pulse(age,1.18f,1.243f,1.253f,1.48f);
    const float squish=.19f*firstPress+.14f*rebound+.075f*settle;
    const float travel=noticeAge>.09f && noticeAge<.82f
        ? std::sin((noticeAge-.09f)/.73f*3.14159265f) : 0.f;
    const float land=pulse(noticeAge,.75f,.83f,.86f,1.01f);
    const float load=pulse(noticeAge,0,.065f,.09f,.19f);
    const float shiver=brushAge>=0.f && brushAge<.47f
        ? std::sin(brushAge*19.f)*std::exp(-brushAge*6.6f)*(1.f-ease((brushAge-.30f)/.17f)) : 0.f;
    const float breath=std::sin(time*2.03f-.81f);
    const float x=perch.x+(169.f-perch.x)*sleep+3.5f*travel+.55f*shiver+.07f*first-.06f*second;
    const float y=perch.y+(141.4f+std::sin(time*1.53f)-perch.y)*sleep-20.f*travel-first-second
                  +.20f*breath-3.f*shiver;
    const float angle=headAngle+(-6.f-headAngle)*sleep-10.f*travel+2.2f*land+2.3f*shiver-.25f*first+.20f*second;
    const auto previous=g.Save();
    g.TranslateTransform(x,y);
    g.RotateTransform(angle);
    g.ScaleTransform(.35f,.35f);
    g.TranslateTransform(-306.f,-181.f);
    const float sx=1.f+.16f*land+.09f*load-.035f*travel-.012f*breath+.63f*squish;
    const float sy=1.f-.22f*land-.15f*load+.07f*travel+.021f*breath-squish;
    drawBody(g,sx,sy);
    const float blink=std::max({pulse(std::fmod(time+1.35f,8.4f),3.40f,3.51f,3.57f,3.75f),
                                      sleep,.52f*land,.23f*firstPress,.30f*rebound});
    SolidBrush sclera(Color(255,255,254,248)),pupil(Color(255,7,8,9));
    const BYTE lidAlpha=BYTE(255.f*ease((blink-.82f)/.16f));
    Pen lid(Color(lidAlpha,115,119,117),2.85f);
    lid.SetLineCap(LineCapRound,LineCapRound,DashCapRound);
    for (const float eyeX : {271.f,339.f}) {
        const auto eye=g.Save();
        g.TranslateTransform(306.f+(eyeX-306.f)*sx,191.f+(146.f-191.f)*sy);
        eyeOpening(g,sclera,25.1f,29.7f,blink);
        eyeOpening(g,pupil,12.6f,18.5f,blink);
        if (lidAlpha) {
            GraphicsPath path;
            quadratic(path,{-19.4f,2.f},{0.f,11.1f},{19.4f,2.f});
            g.DrawPath(&lid,&path);
        }
        g.Restore(eye);
    }
    g.Restore(previous);
}
} // namespace arkluo
