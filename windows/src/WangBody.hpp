#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objidl.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <gdiplus.h>
#include <initializer_list>
#include "CatPhysics.hpp"

namespace wang {
namespace body {
using namespace Gdiplus;

inline Color ink() { return Color(255, 29, 27, 35); }
inline Color charcoal() { return Color(255, 74, 67, 78); }
inline Color ash() { return Color(255, 94, 84, 98); }
inline Color shadow() { return Color(255, 54, 50, 62); }
inline Color brass() { return Color(255, 185, 144, 81); }
inline Color gold() { return Color(255, 227, 195, 128); }
inline Color glint() { return Color(255, 255, 231, 167); }

inline void fill(Graphics& g, GraphicsPath& path, Color color) {
    SolidBrush brush(color);
    g.FillPath(&brush, &path);
}
inline void rim(Graphics& g, GraphicsPath& path, Color color, REAL width) {
    Pen pen(color,width);
    pen.SetLineJoin(LineJoinRound);
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);
    g.DrawPath(&pen,&path);
}
inline void shape(Graphics& g, std::initializer_list<PointF> points, Color color) {
    SolidBrush brush(color);
    g.FillPolygon(&brush,points.begin(),static_cast<INT>(points.size()));
}
inline void line(Graphics& g, std::initializer_list<PointF> points, Color color, REAL width) {
    Pen pen(color,width);
    pen.SetLineJoin(LineJoinMiter);
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);
    g.DrawLines(&pen,points.begin(),static_cast<INT>(points.size()));
}

// A swept, tapering single tail. The ring, zigzag and tip share one outline
// and rotate about the seated haunch, including during the startle animation.
inline void tail(Graphics& g, float seatX, float seatY, const CatPose& p) {
    GraphicsState frame = g.Save();
    g.TranslateTransform(seatX + 48.f, seatY - 8.f);
    g.RotateTransform(4.f*std::sin(p.tailPhase) - p.tailLiftDegrees);
    GraphicsPath path;
    path.AddBezier(-7.f,-8.f, 7.f,-9.f, 25.f,2.f, 37.f,-5.f);
    path.AddBezier(37.f,-5.f, 54.f,-14.f, 45.f,-44.f, 52.f,-60.f);
    path.AddBezier(52.f,-60.f, 59.f,-77.f, 75.f,-84.f, 86.f,-79.f);
    path.AddBezier(86.f,-79.f, 83.f,-78.f, 81.f,-76.f, 80.f,-74.f);
    path.AddBezier(80.f,-74.f, 84.f,-74.f, 86.f,-73.f, 86.f,-70.f);
    path.AddBezier(86.f,-70.f, 67.f,-69.f, 65.f,-58.f, 65.f,-46.f);
    path.AddBezier(65.f,-46.f, 66.f,-26.f, 73.f,-4.f, 51.f,9.f);
    path.AddBezier(51.f,9.f, 34.f,20.f, 9.f,15.f, -8.f,6.f);
    path.CloseFigure();
    fill(g,path,charcoal());
    GraphicsState clip = g.Save();
    g.SetClip(&path,CombineModeIntersect);
    shape(g,{{-12,8},{33,12},{48,7},{60,-5},{66,-31},{78,-12},{73,17},{39,27}},shadow());
    shape(g,{{45,-61},{56,-82},{94,-91},{93,-63},{70,-52},{52,-55}},shadow());
    GraphicsPath ring;
    ring.AddBezier(49.f,-62.f,55.f,-57.f,64.f,-54.f,70.f,-54.f);
    ring.AddLine(70.f,-54.f,69.f,-42.f);
    ring.AddBezier(69.f,-42.f,61.f,-41.f,51.f,-44.f,46.f,-48.f);
    ring.CloseFigure();
    fill(g,ring,gold()); rim(g,ring,ink(),1.4f);
    shape(g,{{47,-55},{51,-55},{56,-51},{54,-45},{46,-45}},brass());
    line(g,{{49,-37},{52,-25},{49,-20},{51,-16},{46,-11},{43,-4},{36,-1},{34,3},{26,2},{18,4}},
         gold(),2.f);
    g.Restore(clip);
    rim(g,path,ink(),2.2f);
    g.Restore(frame);
}

inline void spineMarks(Graphics& g) {
    // Angular seal-like fragments spaced down the turning ridge of the haunch.
    line(g,{{24,-74},{32,-71},{36,-72},{46,-66},{43,-62},{38,-64},{37,-61}},gold(),2.5f);
    shape(g,{{46,-65},{49,-66},{51,-61},{46,-62},{42,-58},{40,-59}},glint());
    line(g,{{49,-61},{55,-56},{52,-52},{49,-54},{46,-51},{53,-47},{56,-50},{61,-46}},
         gold(),2.1f);
    line(g,{{54,-49},{52,-45},{49,-44}},brass(),1.7f);
    line(g,{{61,-44},{63,-39},{59,-36},{57,-39},{55,-36},{57,-31},{62,-31},{64,-27}},
         gold(),2.f);
    shape(g,{{63,-28},{65,-25},{62,-21},{61,-17},{59,-21},{57,-22}},brass());
}

inline void torso(Graphics& g, float seatX, float seatY, const CatPose& p) {
    GraphicsState frame = g.Save();
    g.TranslateTransform(seatX,seatY);
    g.RotateTransform(p.bodyTilt.value*57.29578f);
    g.ScaleTransform(1.f+(1.f-p.bodySquash.value)*.35f,p.bodySquash.value);
    GraphicsPath outline;
    outline.AddBezier(-34.f,-72.f,-26.f,-90.f,4.f,-95.f,20.f,-80.f);
    outline.AddBezier(20.f,-80.f,29.f,-73.f,44.f,-73.f,55.f,-62.f);
    outline.AddBezier(55.f,-62.f,66.f,-52.f,72.f,-38.f,70.f,-18.f);
    outline.AddBezier(70.f,-18.f,70.f,-3.f,63.f,8.f,50.f,9.f);
    outline.AddBezier(50.f,9.f,33.f,10.f,25.f,8.f,15.f,8.f);
    outline.AddBezier(15.f,8.f,-1.f,9.f,-25.f,10.f,-34.f,2.f);
    outline.AddBezier(-34.f,2.f,-46.f,-8.f,-48.f,-34.f,-43.f,-51.f);
    outline.AddBezier(-43.f,-51.f,-42.f,-60.f,-38.f,-65.f,-34.f,-72.f);
    outline.CloseFigure();
    fill(g,outline,charcoal());
    GraphicsState clip = g.Save();
    g.SetClip(&outline,CombineModeIntersect);
    GraphicsPath flank;
    flank.AddBezier(7.f,-72.f,39.f,-70.f,59.f,-62.f,62.f,-37.f);
    flank.AddBezier(62.f,-37.f,65.f,-20.f,55.f,-7.f,37.f,-2.f);
    flank.AddBezier(37.f,-2.f,48.f,-25.f,24.f,-32.f,16.f,-46.f);
    flank.AddBezier(16.f,-46.f,12.f,-56.f,4.f,-67.f,7.f,-72.f);
    flank.CloseFigure();
    fill(g,flank,ash());
    GraphicsPath belly;
    belly.AddBezier(-47.f,-37.f,-43.f,-19.f,-27.f,-9.f,-10.f,-11.f);
    belly.AddBezier(-10.f,-11.f,21.f,-10.f,25.f,-4.f,39.f,-8.f);
    belly.AddBezier(39.f,-8.f,54.f,-14.f,60.f,-7.f,70.f,-35.f);
    belly.AddLine(70.f,-35.f,74.f,18.f);
    belly.AddLine(74.f,18.f,-48.f,17.f);
    belly.CloseFigure();
    fill(g,belly,shadow());
    spineMarks(g);
    g.Restore(clip);
    rim(g,outline,ink(),2.4f);
    // The folded hind leg breaks the lower contour into a recognizable paw.
    GraphicsPath heel;
    heel.AddBezier(53.f,-32.f,47.f,-40.f,33.f,-38.f,29.f,-26.f);
    heel.AddBezier(29.f,-26.f,25.f,-16.f,29.f,-11.f,23.f,-5.f);
    heel.AddBezier(23.f,-5.f,20.f,-1.f,16.f,1.f,16.f,6.f);
    heel.AddBezier(16.f,6.f,21.f,10.f,44.f,10.f,52.f,6.f);
    rim(g,heel,shadow(),1.65f);
    line(g,{{24,5},{23,8}},shadow(),1.25f);
    line(g,{{29,6},{28,8}},shadow(),1.25f);
    g.Restore(frame);
}

struct ArmCurve {
    Pt start,control,end;
    PointF at(float t,float side) const {
        float u=1.f-t;
        float tx=u*(control.x-start.x)+t*(end.x-control.x);
        float ty=u*(control.y-start.y)+t*(end.y-control.y);
        float norm=std::max(0.01f,std::hypot(tx,ty));
        return {u*u*start.x+2.f*u*t*control.x+t*t*end.x-side*ty/norm,
                u*u*start.y+2.f*u*t*control.y+t*t*end.y+side*tx/norm};
    }
};

inline void limb(Graphics& g, Pt joint, Pt paw, float bend, bool decorated) {
    ArmCurve arm{joint,{(joint.x+paw.x)*.5f+bend,(joint.y+paw.y)*.5f-3.f},paw};
    constexpr int steps=12;
    std::array<PointF,2*(steps+1)> edge;
    for (int i=0;i<=steps;++i) {
        float t=static_cast<float>(i)/steps;
        float width=(decorated ? 10.4f:8.5f) - 4.3f*t + 1.3f*t*t*t*t;
        edge[i]=arm.at(t,width);
        edge[edge.size()-i-1]=arm.at(t,-width);
    }
    GraphicsPath silhouette;
    silhouette.AddLines(edge.data(),steps+1);
    silhouette.AddBezier(edge[steps],arm.at(1.075f,7.f),arm.at(1.075f,-7.f),edge[steps+1]);
    silhouette.AddLines(edge.data()+steps+1,steps+1);
    silhouette.AddBezier(edge.back(),arm.at(-.10f,-8.f),arm.at(-.10f,8.f),edge.front());
    silhouette.CloseFigure();
    fill(g,silhouette,decorated ? charcoal() : shadow());
    // The shoulder merges into the chest. Only the outer foreleg and paw
    // have a full ink edge, avoiding a cuff at the top of a reaching leg.
    GraphicsPath pawEdge;
    pawEdge.AddLines(edge.data()+2,steps-1);
    pawEdge.AddBezier(edge[steps],arm.at(1.075f,7.f),arm.at(1.075f,-7.f),edge[steps+1]);
    pawEdge.AddLines(edge.data()+steps+1,steps-1);
    rim(g,pawEdge,ink(),1.85f);
    if (decorated) {
        // The lightning branches follow the foreleg's centreline even at full reach.
        shape(g,{arm.at(.08f,-6),arm.at(.19f,-4),arm.at(.24f,1),arm.at(.33f,1),
                 arm.at(.41f,5),arm.at(.46f,2),arm.at(.56f,3),arm.at(.64f,-1),
                 arm.at(.75f,-3),arm.at(.56f,.3f),arm.at(.51f,-.5f),arm.at(.43f,2),
                 arm.at(.34f,-1),arm.at(.27f,-2),arm.at(.20f,-6)},gold());
        shape(g,{arm.at(.24f,-2),arm.at(.20f,2),arm.at(.17f,6),arm.at(.25f,4),arm.at(.28f,1)},gold());
        shape(g,{arm.at(.52f,1),arm.at(.55f,5),arm.at(.61f,6),arm.at(.57f,2)},brass());
    }
    line(g,{arm.at(.90f,-3),arm.at(.98f,-3.7f)},ink(),1.1f);
    line(g,{arm.at(.93f,1),arm.at(.995f,.6f)},ink(),1.1f);
}

inline void paws(Graphics& g, float seatX, float seatY, const CatPose& p) {
    float shoulderY=seatY-48.f*p.bodySquash.value;
    if (p.hindFootActive.value>.03f)
        limb(g,{seatX+40.f,seatY-11.f},p.hindFoot.point(),11.f,false);
    limb(g,{seatX-24.f,shoulderY-22.f*p.bodyTilt.value},p.leftPaw.point(),-6.f,true);
    limb(g,{seatX+21.f,shoulderY+18.f*p.bodyTilt.value},p.rightPaw.point(),4.f,true);
}

inline void pendant(Graphics& g, Pt head, float seatX, const CatPose& p) {
    GraphicsState frame=g.Save();
    float junctionX=seatX+(head.x-seatX)*.5f;
    g.TranslateTransform(junctionX,head.y+38.f);
    g.RotateTransform(-p.bodyTilt.value*20.f+p.headYaw.value*2.f);
    // Separate Y bail, large pinched four-corner plaque, smaller charm and bar.
    line(g,{{-7,0},{0,5},{7,-1}},ink(),4.8f);
    line(g,{{-7,0},{0,5},{7,-1}},gold(),2.6f);
    line(g,{{0,5},{0,10}},ink(),4.7f);
    line(g,{{0,5},{0,10}},gold(),2.6f);
    GraphicsPath star;
    star.AddBezier(-10.f,9.f,-6.f,11.f,-2.f,11.f,0.f,11.5f);
    star.AddBezier(0.f,11.5f,3.f,11.f,6.f,10.f,10.f,8.f);
    star.AddBezier(10.f,8.f,7.f,13.f,6.f,14.f,5.5f,17.f);
    star.AddBezier(5.5f,17.f,6.f,20.f,8.f,23.f,10.f,26.f);
    star.AddBezier(10.f,26.f,5.f,24.f,3.f,23.f,0.f,22.5f);
    star.AddBezier(0.f,22.5f,-3.f,23.f,-6.f,25.f,-10.f,27.f);
    star.AddBezier(-10.f,27.f,-7.f,23.f,-6.f,21.f,-5.5f,18.f);
    star.AddBezier(-5.5f,18.f,-6.f,15.f,-8.f,12.f,-10.f,9.f);
    star.CloseFigure();
    fill(g,star,gold()); rim(g,star,ink(),1.7f);
    shape(g,{{-9,10},{0,13},{0,17},{-2,17}},glint());
    shape(g,{{0,17},{5,17},{8,24},{.2f,21.5f},{-8,25}},brass());
    shape(g,{{-5,26},{.5f,27},{5,24},{3,29},{5,35},{0,33},{-4,35},{-3,30}},ink());
    shape(g,{{-4,27},{.5f,28.1f},{3.7f,26},{2.2f,29.5f},{3.5f,33.2f},{0,31.9f},{-3,33.2f},{-2.0f,30}},gold());
    line(g,{{.2f,34},{.6f,49}},ink(),3.7f);
    line(g,{{.2f,34},{.6f,49}},gold(),2.0f);
    g.Restore(frame);
}
} // namespace body
} // namespace wang
