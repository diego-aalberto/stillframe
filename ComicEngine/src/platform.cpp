#include "platform.hpp"
#include <algorithm>
#include <commdlg.h>
#include <stdexcept>
#include <wincodec.h>

namespace comic {
std::string utf8(const std::wstring &s) {
    if (s.empty())
        return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0, nullptr, nullptr);
    std::string r(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), int(s.size()), r.data(), n, nullptr, nullptr);
    return r;
}
std::wstring wide(const std::string &s) {
    if (s.empty())
        return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0);
    std::wstring r(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), r.data(), n);
    return r;
}
template <class T> struct Com {
    T *p = nullptr;
    ~Com() {
        if (p)
            p->Release();
    }
    T **out() {
        return &p;
    }
    T *operator->() {
        return p;
    }
};
void hr(HRESULT r) {
    if (FAILED(r))
        throw std::runtime_error("Windows could not decode this image. Use a PNG, JPEG, BMP, or CIMG file.");
}
Image importImage(const std::filesystem::path &p, uint32_t maxDimension) {
    if (maxDimension == 0 || maxDimension > 4096)
        throw std::runtime_error("Invalid import size limit");
    if (p.extension() == L".cimg")
        return loadImage(p);
    Com<IWICImagingFactory> factory;
    hr(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.out())));
    Com<IWICBitmapDecoder> decoder;
    hr(factory->CreateDecoderFromFilename(p.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad,
                                          decoder.out()));
    Com<IWICBitmapFrameDecode> frame;
    hr(decoder->GetFrame(0, frame.out()));
    Image i;
    hr(frame->GetSize(&i.width, &i.height));
    if (!i.width || !i.height || i.width > 4096 || i.height > 4096)
        throw std::runtime_error("Imported images must be at most 4096 x 4096 pixels.");
    Com<IWICFormatConverter> converter;
    hr(factory->CreateFormatConverter(converter.out()));
    Com<IWICFormatConverter> premultiplied;
    Com<IWICBitmapScaler> scaler;
    IWICBitmapSource *source = frame.p;
    if (std::max(i.width, i.height) > maxDimension) {
        float scale = float(maxDimension) / std::max(i.width, i.height);
        i.width = std::max(1u, uint32_t(i.width * scale + .5f));
        i.height = std::max(1u, uint32_t(i.height * scale + .5f));
        hr(factory->CreateFormatConverter(premultiplied.out()));
        hr(premultiplied->Initialize(frame.p, GUID_WICPixelFormat32bppPRGBA, WICBitmapDitherTypeNone, nullptr,
                                     0, WICBitmapPaletteTypeCustom));
        hr(factory->CreateBitmapScaler(scaler.out()));
        hr(scaler->Initialize(premultiplied.p, i.width, i.height, WICBitmapInterpolationModeFant));
        source = scaler.p;
    }
    hr(converter->Initialize(source, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0,
                             WICBitmapPaletteTypeCustom));
    i.rgba.resize(size_t(i.width) * i.height * 4);
    hr(converter->CopyPixels(nullptr, i.width * 4, UINT(i.rgba.size()), i.rgba.data()));
    return i;
}
LOGFONTW defaultFont() {
    LOGFONTW f{};
    f.lfHeight = -24;
    f.lfWeight = FW_MEDIUM;
    f.lfQuality = ANTIALIASED_QUALITY;
    wcscpy_s(f.lfFaceName, L"Segoe UI");
    return f;
}
Font rasterFont(const LOGFONTW &desc) {
    LOGFONTW lf = desc;
    lf.lfHeight = -24;
    lf.lfWidth = 0;
    lf.lfQuality = ANTIALIASED_QUALITY;
    lf.lfEscapement = lf.lfOrientation = 0;
    HDC dc = CreateCompatibleDC(nullptr);
    HFONT h = CreateFontIndirectW(&lf);
    if (!dc || !h) {
        if (dc)
            DeleteDC(dc);
        if (h)
            DeleteObject(h);
        throw std::runtime_error("Could not create font");
    }
    auto oldFont = SelectObject(dc, h);
    TEXTMETRICW metrics{};
    GetTextMetricsW(dc, &metrics);
    Font f;
    f.name = utf8(lf.lfFaceName);
    LONG maxAdvance = 0;
    for (int c = 32; c <= 255; ++c) {
        wchar_t ch = wchar_t(c);
        SIZE size{};
        GetTextExtentPoint32W(dc, &ch, 1, &size);
        maxAdvance = std::max(maxAdvance, size.cx);
    }
    f.cellWidth = float(std::max(32L, maxAdvance + 4));
    f.cellHeight = float(std::max(32L, LONG(metrics.tmHeight + 4)));
    f.lineHeight = float(metrics.tmHeight + 4);
    f.atlas.width = uint32_t(f.cellWidth) * 16;
    f.atlas.height = uint32_t(f.cellHeight) * 14;
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = LONG(f.atlas.width);
    bi.bmiHeader.biHeight = -LONG(f.atlas.height);
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void *bits = nullptr;
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp) {
        SelectObject(dc, oldFont);
        DeleteObject(h);
        DeleteDC(dc);
        throw std::runtime_error("Could not create font atlas");
    }
    auto oldBmp = SelectObject(dc, bmp);
    PatBlt(dc, 0, 0, int(f.atlas.width), int(f.atlas.height), BLACKNESS);
    SetTextColor(dc, RGB(255, 255, 255));
    SetBkMode(dc, TRANSPARENT);
    for (int c = 32; c <= 255; c++) {
        wchar_t ch = wchar_t(c);
        SIZE size{};
        GetTextExtentPoint32W(dc, &ch, 1, &size);
        f.glyphs[c - 32].advance = std::min(float(size.cx), f.cellWidth);
        TextOutW(dc, ((c - 32) % 16) * int(f.cellWidth), ((c - 32) / 16) * int(f.cellHeight), &ch, 1);
    }
    GdiFlush();
    f.atlas.rgba.resize(size_t(f.atlas.width) * f.atlas.height * 4);
    auto *src = static_cast<uint8_t *>(bits);
    for (size_t i = 0; i < f.atlas.rgba.size(); i += 4) {
        f.atlas.rgba[i] = f.atlas.rgba[i + 1] = f.atlas.rgba[i + 2] = 255;
        f.atlas.rgba[i + 3] = src[i];
    }
    SelectObject(dc, oldBmp);
    SelectObject(dc, oldFont);
    DeleteObject(bmp);
    DeleteObject(h);
    DeleteDC(dc);
    return f;
}
std::filesystem::path fileDialog(HWND owner, bool save, const wchar_t *filter, const wchar_t *ext) {
    wchar_t path[32768]{};
    OPENFILENAMEW of{};
    of.lStructSize = sizeof(of);
    of.hwndOwner = owner;
    of.lpstrFilter = filter;
    of.lpstrFile = path;
    of.nMaxFile = 32768;
    of.lpstrDefExt = ext;
    of.Flags =
        OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    if (save ? GetSaveFileNameW(&of) : GetOpenFileNameW(&of))
        return path;
    return {};
}
} // namespace comic
