#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <cstring>
#include <vector>

namespace arkart {

// Store premultiplied BGRA returned by GDI+, including antialiased edges.
struct Pixel { BYTE b, g, r, a; };
using Pixels = std::vector<Pixel>;
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

} // namespace arkart
