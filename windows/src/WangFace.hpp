#pragma once
#include "CatPhysics.hpp"
#include <objidl.h>
#include <gdiplus.h>

namespace wang {
namespace face_detail {
using namespace Gdiplus;

inline Color color(unsigned rgb) {
    return Color(255, (rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255);
}

inline void fill(Graphics& g, GraphicsPath& path, unsigned pigment) {
    SolidBrush brush(color(pigment));
    g.FillPath(&brush, &path);
}

inline void outline(Graphics& g, GraphicsPath& path, unsigned pigment, float width) {
    Pen pen(color(pigment), width);
    pen.SetLineJoin(LineJoinRound);
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);
    g.DrawPath(&pen, &path);
}

// Points on the mask bow toward the viewer. Its midline and two eye sockets
// follow the same surface even when one cheek turns away from the cursor.
struct Surface {
    float cy, sy, cp, sp;
    explicit Surface(const CatPose& cat) {
        float yaw = cat.headYaw.value * .52f;
        float pitch = cat.headPitch.value * .32f;
        cy = std::cos(yaw); sy = std::sin(yaw);
        cp = std::cos(pitch); sp = std::sin(pitch);
    }
    PointF at(float x, float y, float depth) const {
        float z = depth * std::clamp(1.f - x*x / 2600.f - y*y / 3900.f, 0.f, 1.f);
        return {x*cy + z*sy, y*cp + (z*cy - x*sy)*sp};
    }
};

// Draw one half by reflecting its local coordinates. Curves are defined in the
// mask plane, then projected before GDI+ flattens them for this frame's scale.
struct Half {
    const Surface& surface;
    float side, depth;
    PointF p(float x, float y) const { return surface.at(x*side, y, depth); }
    void curve(GraphicsPath& path, float x, float y, float x1, float y1,
               float x2, float y2, float endX, float endY) const {
        path.AddBezier(p(x,y), p(x1,y1), p(x2,y2), p(endX,endY));
    }
    void line(GraphicsPath& path, float x, float y, float endX, float endY) const {
        path.AddLine(p(x,y), p(endX,endY));
    }
};

inline void mane(Graphics& g, const Surface& surface) {
    for (float side : {-1.f,1.f}) {
        Half h{surface,side,-3.f};
        GraphicsPath shape;
        h.curve(shape,-2,5, 11,7, 29,3, 38,-1);
        h.curve(shape,38,-1, 43,-1, 46,-5, 48,-9);
        h.curve(shape,48,-9, 50,0, 51,11, 47,19);
        h.line(shape,47,19, 52,16);
        h.curve(shape,52,16, 50,25, 46,31, 39,34);
        h.line(shape,39,34, 44,34);
        h.curve(shape,44,34, 40,41, 29,44, 21,42);
        h.line(shape,21,42, 24,46);
        h.curve(shape,24,46, 15,45, 9,42, 4,40);
        h.line(shape,4,40, -2,41);
        shape.CloseFigure();
        fill(g,shape,0x28272C);
        GraphicsPath sweep;
        h.curve(sweep,47,19, 44,24, 37,28, 28,29);
        h.curve(sweep,28,29, 34,30, 37,29, 39,29);
        h.curve(sweep,39,29, 36,34, 28,37, 18,37);
        h.curve(sweep,18,37, 22,41, 28,41, 33,40);
        h.curve(sweep,33,40, 29,44, 18,44, 11,40);
        h.curve(sweep,11,40, 8,37, 9,30, 12,26);
        sweep.CloseFigure();
        fill(g,sweep,0x39373F);
    }
    // Short, overlapping throat tufts bridge the two broad cheek fans.
    Half h{surface,1.f,0.f};
    GraphicsPath throat;
    h.curve(throat,-20,29, -12,32, -3,31, 7,28);
    h.curve(throat,7,28, 13,27, 16,27, 19,26);
    h.curve(throat,19,26, 17,32, 13,36, 9,38);
    h.line(throat,9,38, 11,34);
    h.curve(throat,11,34, 6,37, 0,40, -5,40);
    h.line(throat,-5,40, -3,37);
    h.curve(throat,-3,37, -9,38, -15,36, -20,29);
    throat.CloseFigure();
    fill(g,throat,0x4B4750);
}

inline void skull(Graphics& g, const Surface& surface) {
    // The folded pinnae sit behind the two forward-projecting mask plates.
    // A rear skull surface stays visible between them as the head turns.
    Half h{surface,1.f,3.f};
    GraphicsPath base;
    h.curve(base,-39,-5, -40,-20, -33,-33, -23,-34);
    h.curve(base,-23,-34, -12,-32, -6,-29, 0,-30);
    h.curve(base,0,-30, 7,-29, 15,-32, 23,-34);
    h.curve(base,23,-34, 34,-32, 40,-19, 40,-4);
    h.curve(base,40,-4, 39,18, 29,27, 0,29);
    h.curve(base,0,29, -27,27, -39,19, -39,-5);
    base.CloseFigure();
    fill(g,base,0x302F35);
}

inline void ear(Graphics& g, const Half& h, const CatPose& cat) {
    // Rotate the whole folded ear about its root. Gold and dark folds travel together.
    GraphicsState state = g.Save();
    PointF root = h.p(28,-24);
    g.TranslateTransform(root.X,root.Y);
    g.RotateTransform(h.side * (cat.earFold.value * 14.f + cat.earTwitch.value * 2.f));
    g.TranslateTransform(-root.X,-root.Y);
    GraphicsPath rim;
    h.curve(rim,18,-29, 17,-35, 19,-39, 21,-42);
    h.curve(rim,21,-42, 26,-45, 34,-43, 39,-38);
    h.curve(rim,39,-38, 44,-33, 47,-24, 44,-18);
    h.curve(rim,44,-18, 43,-15, 39,-14, 36,-15);
    h.line(rim,36,-15, 21,-21);
    rim.CloseFigure();
    fill(g,rim,0x57535D);
    outline(g,rim,0x242328,1.8f);
    GraphicsPath fold;
    h.curve(fold,19,-32, 23,-36, 29,-38, 33,-38);
    h.curve(fold,33,-38, 37,-37, 39,-30, 40,-25);
    h.line(fold,40,-25, 36,-18);
    h.line(fold,36,-18, 25,-21);
    h.curve(fold,25,-21, 20,-22, 17,-26, 19,-32);
    fold.CloseFigure();
    fill(g,fold,0x38363F);
    GraphicsPath gold;
    h.curve(gold,33,-35, 34,-31, 36,-28, 39,-27);
    h.curve(gold,39,-27, 38,-24, 37,-21, 35,-18);
    h.line(gold,35,-18, 27,-20);
    h.curve(gold,27,-20, 29,-23, 28,-25, 25,-27);
    h.curve(gold,25,-27, 29,-28, 31,-31, 33,-35);
    gold.CloseFigure();
    fill(g,gold,0xE7BF74);
    GraphicsPath gleam;
    h.curve(gleam,33,-33, 33,-28, 35,-27, 37,-26);
    h.line(gleam,37,-26, 34,-22);
    h.curve(gleam,34,-22, 35,-26, 32,-27, 30,-28);
    gleam.CloseFigure();
    fill(g,gleam,0xF7DA9B);
    g.Restore(state);
}

inline void maskPanel(Graphics& g, const Half& h) {
    // A separate gold lower plate shows in the cheek cutouts and below each lobe.
    GraphicsPath lower;
    h.curve(lower,0,21, 9,25, 17,20, 22,19);
    h.curve(lower,22,19, 28,18, 35,14, 38,12);
    h.curve(lower,38,12, 38,18, 34,22, 29,26);
    h.line(lower,29,26, 25,23);
    h.curve(lower,25,23, 23,21, 22,22, 20,25);
    h.line(lower,20,25, 12,34);
    h.line(lower,12,34, 9,31.8f);
    h.curve(lower,9,31.8f, 6,32.5f, 3,33, -.5f,32.5f);
    lower.CloseFigure();
    fill(g,lower,0xD8AF68);
    outline(g,lower,0x242328,1.1f);
    GraphicsPath lip;
    h.curve(lip,35,17, 33,21, 30,23, 29,23.5f);
    h.line(lip,29,23.5f, 26,21);
    h.curve(lip,26,21, 30,22, 33,18, 35,17);
    lip.CloseFigure();
    fill(g,lip,0xFAE5AF);

    GraphicsPath panel;
    h.curve(panel,0,-31, 6,-32, 12,-34, 19,-30);
    h.line(panel,19,-30, 16,-25);
    h.curve(panel,16,-25, 18,-20, 22,-18, 27,-19);
    h.curve(panel,27,-19, 29,-19, 32,-20, 34,-20);
    h.curve(panel,34,-20, 38,-16, 40,-10, 40,-4);
    h.curve(panel,40,-4, 40,2, 39,11, 38,17);
    h.curve(panel,38,17, 36,19.5f, 32,20, 29,19);
    // The concave cut is open to the dark ruff, as on the physical mask.
    h.line(panel,29,19, 31,13);
    h.curve(panel,31,13, 26,12, 21,15, 18,19);
    h.line(panel,18,19, 23,21);
    h.curve(panel,23,21, 21,26, 18,28, 14,29);
    h.curve(panel,14,29, 10,30.5f, 3,30, 0,27);
    h.curve(panel,0,27, -.3f,14, .4f,-18, 0,-31);
    panel.CloseFigure();
    bool dark = h.side < 0;
    LinearGradientBrush plane(PointF(0,-34),PointF(0,30),
        color(dark ? 0x514C55 : 0xFDFBF2),color(dark ? 0x615A62 : 0xEAE6DB));
    plane.SetWrapMode(WrapModeTileFlipXY);
    g.FillPath(&plane,&panel);
    outline(g,panel,0x27262A,1.45f);
}

inline void eye(Graphics& g, const Half& h, const CatPose& cat) {
    const bool darkSide = h.side < 0;
    float alert = std::clamp(std::max(cat.huntIntensity.value,cat.alertPupils),0.f,1.f);
    float closure = cat.effectiveEyeClosure();
    if (cat.isHappyClosedEyes()) closure = 1.f;
    if (cat.isBlissfulSquint()) closure = std::max(closure,.83f);
    GraphicsState state = g.Save();
    PointF origin = h.p(20,-1);
    g.TranslateTransform(origin.X,origin.Y);
    float yaw = cat.headYaw.value*.52f + h.side*.33f;
    float width = std::clamp(std::cos(yaw)/std::cos(.33f),.66f,1.06f);
    g.ScaleTransform(h.side*width,1.f);
    if (closure > .86f) {
        GraphicsPath lid;
        lid.AddBezier(-10.f,1.6f,-6.f,4.6f,3.f,3.6f,10.f,-1.0f);
        outline(g,lid,darkSide ? 0xDAC398 : 0x312C2A,1.8f);
        g.Restore(state);
        return;
    }
    // The cut-metal sockets keep their pointed inner corners as the lids close.
    g.ScaleTransform(1.f,std::max(.09f,1.f-closure));
    GraphicsPath socket;
    socket.AddBezier(-10.f,8.f, -10.f,2.f, -9.5f,-4.8f, -4.6f,-7.7f);
    socket.AddBezier(-4.6f,-7.7f, .4f,-11.f, 6.f,-10.4f, 10.8f,-6.0f);
    socket.AddLine(10.8f,-6.f, 9.3f,-4.7f);
    socket.AddBezier(9.3f,-4.7f, 9.5f,1.4f, 6.f,6.5f, 1.3f,7.3f);
    socket.AddBezier(1.3f,7.3f, -3.f,8.2f, -6.2f,5.f, -10.f,8.f);
    socket.CloseFigure();
    LinearGradientBrush rim(PointF(0,-9),PointF(0,7),color(0xA57F56),color(0xEACB93));
    rim.SetWrapMode(WrapModeTileFlipXY);
    g.FillPath(&rim,&socket);
    outline(g,socket,0x2B2727,1.25f);
    GraphicsState clipped = g.Save();
    g.SetClip(&socket,CombineModeIntersect);
    Pt gaze = cat.eyeGaze.point();
    float x = h.side*gaze.x*2.0f, y = -1.4f+gaze.y*1.25f;
    float r = (darkSide ? 6.9f : 6.3f) + alert*1.5f;
    SolidBrush disk(color(darkSide ? 0xFFFDF4 : 0x26262C));
    g.FillEllipse(&disk,x-r,y-r,r*2,r*2);
    float pupil = (darkSide ? 1.35f : 1.60f) + alert*(darkSide ? 1.6f : .9f);
    pupil *= std::clamp(cat.pupilScale.value,.9f,1.2f);
    SolidBrush pupilBrush(color(darkSide ? 0x242126 : 0xFFFCF2));
    g.FillEllipse(&pupilBrush,x-pupil,y-pupil,2*pupil,2*pupil);
    g.Restore(clipped);
    g.Restore(state);
}
} // namespace face_detail

inline void drawHeadAndMane(Gdiplus::Graphics& g, Pt headCenter, const CatPose& cat) {
    using namespace face_detail;
    Gdiplus::GraphicsState state = g.Save();
    g.TranslateTransform(headCenter.x,headCenter.y);
    g.RotateTransform(cat.headTilt.value*57.2957795f);
    Surface surface(cat);
    mane(g,surface);
    skull(g,surface);
    for (float side : {-1.f,1.f}) ear(g,{surface,side,-2.f},cat);
    for (float side : {-1.f,1.f}) maskPanel(g,{surface,side,21.f});
    for (float side : {-1.f,1.f}) eye(g,{surface,side,21.f},cat);
    g.Restore(state);
}
} // namespace wang
