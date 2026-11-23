#pragma once
#include <objidl.h>
#include <gdiplus.h>
#include "CatPhysics.hpp"

namespace wang {

// The grid lies on the same desk plane as the keyboard. Perspective affects
// both the grid and the stone centers, so every stone lands on an intersection.
inline Gdiplus::PointF intersection(float column, float row) {
    float depth = row / 4.f;
    return {47.f - 3.f * depth + column * (12.5f + 1.5f * depth), 195.f + row * 8.f};
}

inline void drawGoStones(Gdiplus::Graphics& g, const CatPhysics& cat) {
    using namespace Gdiplus;
    using Move = CatPhysics::GoMove;
    if (!cat.go.visible() || !cat.go.turns) return;
    float age = cat.go.age;
    float opacity = cat.go.opacity();
    auto color = [opacity](BYTE alpha, BYTE red, BYTE green, BYTE blue) {
        return Color(static_cast<BYTE>(alpha * opacity), red, green, blue);
    };
    PointF surface[] = {{41,190}, {103,190}, {109,232}, {36,232}};
    PointF front[] = {{36,232}, {109,232}, {108,236}, {37,236}};
    SolidBrush shadow(color(38,32,26,33)), edge(color(255,128,88,56));
    g.FillEllipse(&shadow, 37.f, 229.f, 73.f, 9.f);
    g.FillPolygon(&edge, front, 4);
    LinearGradientBrush wood(PointF(65,190),PointF(66,233),color(255,207,171,115),color(255,238,206,149));
    Pen border(color(255,75,56,47),1.35f), rules(color(192,99,70,48),0.9f);
    g.FillPolygon(&wood,surface,4);
    g.DrawPolygon(&border,surface,4);
    for (int i=0; i<5; ++i) {
        g.DrawLine(&rules,intersection(0.f,float(i)),intersection(4.f,float(i)));
        g.DrawLine(&rules,intersection(float(i),0.f),intersection(float(i),4.f));
    }
    PointF center = intersection(2,2);
    SolidBrush hoshi(color(245,83,54,39));
    g.FillEllipse(&hoshi,center.X-1.4f,center.Y-0.9f,2.8f,1.8f);

    auto stone = [&](PointF p, bool white, float radius) {
        float ry = radius*0.76f;
        g.FillEllipse(&shadow, p.X-radius+0.8f, p.Y-ry+1.9f, 2*radius, 2*ry);
        Color top = white ? color(255,255,251,237) : color(255,91,90,103);
        Color base = white ? color(255,199,195,188) : color(255,36,34,42);
        LinearGradientBrush ceramic(PointF(p.X,p.Y-ry),PointF(p.X,p.Y+ry),top,base);
        Pen rim(white ? color(255,109,100,89) : color(255,27,25,31),1.05f);
        g.FillEllipse(&ceramic,p.X-radius,p.Y-ry,2*radius,2*ry);
        g.DrawEllipse(&rim,p.X-radius,p.Y-ry,2*radius,2*ry);
        SolidBrush glint(white ? color(230,255,255,252) : color(210,192,190,190));
        g.FillEllipse(&glint,p.X-radius*0.55f,p.Y-ry*0.57f,radius*0.66f,ry*0.35f);
    };
    // A small opening pattern, beginning with two stones already on the board.
    constexpr int moves[Move::sequenceLength+2][2] = {
        {1,2},{3,2},{2,1},{1,1},{3,1},{2,2},{1,3},{3,3},{0,2},{4,2}
    };
    int active = cat.go.turns + 1;
    for (int k=0; k<active; ++k)
        stone(intersection(float(moves[k][0]),float(moves[k][1])),k%2==1,4.9f);

    PointF target = intersection(float(moves[active][0]),float(moves[active][1]));
    float t = std::clamp((age-Move::release)/(Move::landing-Move::release),0.f,1.f);
    if (t >= 1.f && age < Move::landing+0.27f) {
        float pulse = (age - Move::landing) / 0.27f;
        Pen ripple(color(static_cast<BYTE>((1.f-pulse)*170),251,237,192),1.3f);
        float r = 5.f + pulse*5.f;
        g.DrawEllipse(&ripple,target.X-r,target.Y-r*0.60f,r*2,r*1.2f);
    }
    Pt launch = cat.go.launch;
    PointF p(launch.x+(target.X-launch.x)*t,
             launch.y-5.f+(target.Y-launch.y+5.f)*t-112.f*t*(1.f-t));
    stone(p,active%2==1,4.9f+std::sin(t*3.141593f)*0.9f);
}

} // namespace wang
