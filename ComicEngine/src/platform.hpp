#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include "core.hpp"
#include <windows.h>

namespace comic {
std::string utf8(const std::wstring &s);
std::wstring wide(const std::string &s);
Image importImage(const std::filesystem::path &path, uint32_t maxDimension = 4096);
Font rasterFont(const LOGFONTW &descriptor);
LOGFONTW defaultFont();
std::filesystem::path fileDialog(HWND owner, bool save, const wchar_t *filter, const wchar_t *ext);
} // namespace comic
