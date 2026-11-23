#pragma once
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>
#include "FoxResourceIds.h"

namespace arkfox {
namespace detail {

// GDI+ expects premultiplied BGRA. Keep that representation through sampling,
// motion and compositing, including the pale fur at semitransparent edges.
struct Pixel { BYTE b, g, r, a; };
using Pixels = std::vector<Pixel>;
constexpr int Side = 560;
constexpr int EyeX = 196, EyeY = 180, EyeW = 168, EyeH = 72;
constexpr int PawX = 196, PawY = 376, PawW = 168, PawH = 152;

inline bool loadPng(int id, int w, int h, Pixels& pixels) {
    HMODULE module = GetModuleHandleW(nullptr);
    HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10));
    if (!resource) return false;
    DWORD size = SizeofResource(module, resource);
    const void* bytes = LockResource(LoadResource(module, resource));
    if (!bytes || !size) return false;
    HGLOBAL storage = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!storage) return false;
    void* destination = GlobalLock(storage);
    if (!destination) { GlobalFree(storage); return false; }
    std::memcpy(destination, bytes, size);
    GlobalUnlock(storage);
    IStream* stream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(storage, TRUE, &stream))) {
        GlobalFree(storage); return false;
    }
    bool ok = false;
    {
        Gdiplus::Bitmap image(stream);
        Gdiplus::Rect bounds(0, 0, w, h);
        Gdiplus::BitmapData data{};
        if (image.GetLastStatus() == Gdiplus::Ok && image.GetWidth() == UINT(w) &&
            image.GetHeight() == UINT(h) &&
            image.LockBits(&bounds, Gdiplus::ImageLockModeRead, PixelFormat32bppPARGB, &data) == Gdiplus::Ok) {
            pixels.resize(w*h);
            auto* start = static_cast<const BYTE*>(data.Scan0);
            for (int y = 0; y < h; ++y)
                std::memcpy(pixels.data()+y*w, start+y*data.Stride, w*sizeof(Pixel));
            image.UnlockBits(&data);
            ok = true;
        }
    }
    stream->Release();
    return ok;
}

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
            !loadPng(red ? IDR_FOX_RED_LIDS : IDR_FOX_ARCTIC_LIDS, EyeW, EyeH*9, lids) ||
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
        markEdges(open.data(),Side,Side,0,0);
        for(int i=0;i<9;++i) markEdges(lids.data()+i*EyeW*EyeH,EyeW,EyeH,EyeX,EyeY);
        for(int i=0;i<2;++i) markEdges(pawBacks.data()+i*PawW*PawH,PawW,PawH,PawX,PawY);
        return true;
    }

    const Pixels& update(float closure, float left, float right) {
        if (closure<=0 && left<=0 && right<=0) return open;
        float frame = std::clamp(closure,0.f,1.f)*8.f;
        int lo = int(frame), hi = std::min(lo+1,8);
        unsigned fraction = unsigned((frame-lo)*256);
        for (int y=0; y<EyeH; ++y) {
            auto* dest = pose.data()+(y+EyeY)*Side+EyeX;
            if (closure<=0) {
                std::copy_n(open.data()+(y+EyeY)*Side+EyeX,EyeW,dest);
                continue;
            }
            const Pixel* a=lids.data()+(lo*EyeH+y)*EyeW;
            const Pixel* b=lids.data()+(hi*EyeH+y)*EyeW;
            for (int x=0; x<EyeW; ++x) dest[x]=blend(a[x],b[x],fraction);
        }
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
