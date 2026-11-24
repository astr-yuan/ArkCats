#pragma once
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>
#include "PngResource.hpp"
#include "FoxResourceIds.h"

namespace arkfox {
namespace detail {

// GDI+ expects premultiplied BGRA. Keep that representation through sampling,
// motion and compositing, including the pale fur at semitransparent edges.
using arkart::Pixel;
using arkart::Pixels;
using arkart::loadPng;
constexpr int Side = 560;
constexpr int EyeX = 196, EyeY = 180, EyeW = 168, EyeH = 72;
constexpr int PawX = 196, PawY = 376, PawW = 168, PawH = 152;

inline Pixel blend(Pixel a, Pixel b, unsigned fraction) {
    auto channel = [fraction](unsigned from, unsigned to) -> BYTE {
        return BYTE(((256-fraction)*from + fraction*to + 128) >> 8);
    };
    return {channel(a.b,b.b), channel(a.g,b.g), channel(a.r,b.r), channel(a.a,b.a)};
}

inline Pixel sample(const Pixels& pixels, float x, float y) {
    if (x < 0 || y < 0 || x >= Side-1 || y >= Side-1) return {};
    int ix = int(x), iy = int(y);
    auto* p = pixels.data()+iy*Side+ix;
    if (!(p[0].a | p[1].a | p[Side].a | p[Side+1].a)) return {};
    unsigned fx = unsigned((x-ix)*256), fy = unsigned((y-iy)*256);
    return blend(blend(p[0], p[1], fx), blend(p[Side], p[Side+1], fx), fy);
}

inline int distance(Pixel a, Pixel b) {
    return std::abs(a.b-b.b) + std::abs(a.g-b.g) + std::abs(a.r-b.r) + std::abs(a.a-b.a);
}

struct Artwork {
    Pixels open, lids, pawBacks, pose, rendered;
    Pixels eyeLayer;
    std::vector<BYTE> pawSide;
    // Four-tap filtering is only useful near a contour or a colour boundary.
    // Include every eye and paw variant, so their moving edges stay smooth.
    static constexpr int Tile=4, Tiles=Side/Tile;
    std::array<BYTE,Tiles*Tiles> edges{};

    void markEdges(const Pixel* pixels,int w,int h,int ox,int oy) {
        for(int y=0;y<h-1;++y) for(int x=0;x<w-1;++x) {
            const Pixel* p=pixels+y*w+x;
            if(distance(p[0],p[1])<32 && distance(p[0],p[w])<32) continue;
            int l=std::max(0,(ox+x-2)/Tile),r=std::min(Tiles-1,(ox+x+2)/Tile);
            int t=std::max(0,(oy+y-2)/Tile),b=std::min(Tiles-1,(oy+y+2)/Tile);
            for(int j=t;j<=b;++j) for(int i=l;i<=r;++i) edges[j*Tiles+i]=1;
        }
    }
    bool nearEdge(float x,float y) const {
        if(x<0 || y<0 || x>=Side || y>=Side) return false;
        return edges[(int(y)/Tile)*Tiles+int(x)/Tile]!=0;
    }

    bool load(bool red) {
        if (!loadPng(red ? IDR_FOX_RED_OPEN : IDR_FOX_ARCTIC_OPEN, Side, Side, open) ||
            !loadPng(red ? IDR_FOX_RED_LIDS : IDR_FOX_ARCTIC_LIDS, EyeW, EyeH*10, lids) ||
            !loadPng(red ? IDR_FOX_RED_PAW_BACKS : IDR_FOX_ARCTIC_PAW_BACKS, PawW, PawH*2, pawBacks)) {
            open.clear();
            return false;
        }
        // The two painted restoration patches overlap. Choose the patch which
        // actually removed fur at each texel, so both paws can reach together.
        pawSide.resize(PawW*PawH);
        for (int y=0; y<PawH; ++y) for (int x=0; x<PawW; ++x) {
            int i = y*PawW+x;
            Pixel original = open[(PawY+y)*Side+PawX+x];
            int l = distance(original,pawBacks[i]);
            int r = distance(original,pawBacks[PawW*PawH+i]);
            pawSide[i] = l==0 && r==0 ? 0 : l>=r ? 1 : 2;
        }
        pose = open;
        eyeLayer.resize(EyeW*EyeH);
        markEdges(open.data(),Side,Side,0,0);
        // The catchlight and ink move together through this range. Use the
        // silhouette filter there even when it lies beyond the original eye.
        for(int j=(EyeY-16)/Tile;j<=(EyeY+EyeH+12)/Tile;++j)
            for(int i=(EyeX-14)/Tile;i<=(EyeX+EyeW+14)/Tile;++i)
                edges[j*Tiles+i]=1;
        for(int i=0;i<2;++i) markEdges(pawBacks.data()+i*PawW*PawH,PawW,PawH,PawX,PawY);
        return true;
    }

    void paintEyes(float closure, float gazeX, float gazeY) {
        // In a full blink the eyelids stay anchored to the resting face.
        float visible=1.f-std::clamp(closure,0.f,1.f);
        float follow=visible*(2.f-visible);
        float dx=std::clamp(gazeX,-1.f,1.f)*14.5f*follow;
        float dy=std::clamp(gazeY,-1.f,1.f)*(gazeY<0 ? 17.f : 13.f)*follow;
        constexpr int padX=16,padY=18;
        for(int y=EyeY-padY;y<EyeY+EyeH+padY;++y) {
            const Pixel* neutral=open.data()+y*Side+EyeX-padX;
            std::copy_n(neutral,EyeW+padX*2,pose.data()+y*Side+EyeX-padX);
        }
        if (closure<=0 && std::abs(dx)<.001f && std::abs(dy)<.001f) return;
        float frame = std::clamp(closure,0.f,1.f)*8.f;
        int lo = int(frame), hi = std::min(lo+1,8);
        unsigned fraction = unsigned((frame-lo)*256);
        const Pixel* a=lids.data()+lo*EyeW*EyeH;
        const Pixel* b=lids.data()+hi*EyeW*EyeH;
        const Pixel* ink=a;
        if(fraction>0) {
            for(int i=0;i<EyeW*EyeH;++i) eyeLayer[i]=blend(a[i],b[i],fraction);
            ink=eyeLayer.data();
        }
        // The last tile contains the original coat with just the irises
        // removed. It keeps the nose and the red fox's cheek boundary fixed.
        const Pixel* face=lids.data()+9*EyeW*EyeH;
        for(int y=0;y<EyeH;++y)
            std::copy_n(face+y*EyeW,EyeW,pose.data()+(EyeY+y)*Side+EyeX);
        int x0=int(std::floor(dx)), y0=int(std::floor(dy));
        unsigned fx=unsigned((dx-x0)*256), fy=unsigned((dy-y0)*256);
        for(int y=1;y<EyeH;++y) {
            Pixel* dst=pose.data()+(EyeY+y+y0)*Side+EyeX+x0;
            const Pixel* row=ink+y*EyeW;
            for(int x=1;x<EyeW;++x) {
                const Pixel* p=row+x;
                if(!(p[0].a|p[-1].a|p[-EyeW].a|p[-EyeW-1].a)) continue;
                Pixel fg=blend(blend(p[0],p[-1],fx),blend(p[-EyeW],p[-EyeW-1],fx),fy);
                Pixel bg=dst[x];
                unsigned remain=255-fg.a;
                dst[x]={BYTE(fg.b+(bg.b*remain+127)/255),BYTE(fg.g+(bg.g*remain+127)/255),
                        BYTE(fg.r+(bg.r*remain+127)/255),BYTE(fg.a+(bg.a*remain+127)/255)};
            }
        }
    }

    const Pixels& update(float closure, float left, float right,float gazeX=0,float gazeY=0) {
        if (closure<=0 && left<=0 && right<=0 && gazeX==0 && gazeY==0) return open;
        paintEyes(closure,gazeX,gazeY);
        const unsigned strength[] = {0, unsigned(left*256), unsigned(right*256)};
        for (int y=0; y<PawH; ++y) for (int x=0; x<PawW; ++x) {
            int i=y*PawW+x, dest=(y+PawY)*Side+PawX+x;
            unsigned side=pawSide[i];
            if (!side) continue;
            pose[dest]=blend(open[dest],pawBacks[(side-1)*PawH*PawW+i],strength[side]);
        }
        return pose;
    }
};

inline Artwork& artwork(bool red) {
    // No GDI+ object survives the draw call or GdiplusShutdown.
    static Artwork actors[2];
    static bool loaded[2]{};
    int i=red ? 0 : 1;
    if (!loaded[i]) {
        loaded[i]=actors[i].load(red);
    }
    return actors[i];
}

} // namespace detail
} // namespace arkfox
