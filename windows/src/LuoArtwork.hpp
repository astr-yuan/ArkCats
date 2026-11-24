#pragma once
#include "PngResource.hpp"
#include "LuoResourceIds.h"

namespace arkluo {
namespace art {

enum class Part { Head, Tail, EyeL, EyeR, PupilL, PupilR, StarL, StarR, Mouth, Sleep };

struct Region {
    int atlasX, atlasY, originalX, originalY, width, height;
};

// Each layer retains its source registration, including the separate sleeping
// pose. Transparent gutters in the atlas keep adjacent pieces out of the filter.
inline constexpr Region regions[] = {
    {3,489,108,159,392,304}, {391,799,118,497,97,115},
    {401,489,119,204,184,234}, {591,489,314,207,172,230},
    {277,799,158,240,108,147}, {164,799,349,242,107,149},
    {769,489,127,212,168,218}, {3,799,322,215,155,214},
    {494,799,284,430,54,38}, {3,3,129,150,665,480}
};

inline constexpr int atlasWidth=1024, atlasHeight=1016;
inline arkart::Pixels& atlas() {
    static arkart::Pixels pixels=[] {
        arkart::Pixels decoded;
        arkart::loadPng(IDR_LUO_CAT_ART,atlasWidth,atlasHeight,decoded);
        return decoded;
    }();
    return pixels;
}

inline bool contains(Part part,float sourceX,float sourceY) {
    const Region& r=regions[static_cast<unsigned>(part)];
    const float x=sourceX-r.originalX,y=sourceY-r.originalY;
    if (x<0 || y<0 || x>=r.width || y>=r.height) return false;
    const auto& pixels=atlas();
    return !pixels.empty() && pixels[(r.atlasY+int(y))*atlasWidth+r.atlasX+int(x)].a>32;
}

inline void draw(Gdiplus::Graphics& g, Part part, float opacity=1.f) {
    using namespace Gdiplus;
    if (opacity<=.001f) return;
    auto& pixels=atlas();
    if (pixels.empty()) return;

    const Region& r=regions[static_cast<unsigned>(part)];
    // This wrapper borrows decoded memory. It is destroyed while GDI+ is active.
    auto* start=reinterpret_cast<BYTE*>(pixels.data()+r.atlasY*atlasWidth+r.atlasX);
    Bitmap sprite(r.width,r.height,atlasWidth*sizeof(arkart::Pixel),PixelFormat32bppPARGB,start);
    RectF bounds(float(r.originalX),float(r.originalY),float(r.width),float(r.height));
    if (opacity>=.999f) {
        g.DrawImage(&sprite,bounds,0.f,0.f,float(r.width),float(r.height),UnitPixel);
        return;
    }
    ColorMatrix alpha={{{1,0,0,0,0},{0,1,0,0,0},{0,0,1,0,0},{0,0,0,opacity,0},{0,0,0,0,1}}};
    ImageAttributes attrs;
    attrs.SetColorMatrix(&alpha);
    g.DrawImage(&sprite,bounds,0.f,0.f,float(r.width),float(r.height),UnitPixel,&attrs);
}

} // namespace art
} // namespace arkluo
