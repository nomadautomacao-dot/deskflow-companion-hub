/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2026 InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "platform/MSWindowsClipboardPNGConverter.h"
#include "base/Log.h"

namespace inputleap {

MSWindowsClipboardPNGConverter::MSWindowsClipboardPNGConverter() :
    m_pngFormat(RegisterClipboardFormatA("PNG"))
{
    LOG_DEBUG("registered Windows PNG clipboard format: %u", m_pngFormat);
}

MSWindowsClipboardPNGConverter::~MSWindowsClipboardPNGConverter()
{
}

IClipboard::EFormat MSWindowsClipboardPNGConverter::getFormat() const
{
    return IClipboard::kPNG;
}

UINT MSWindowsClipboardPNGConverter::getWin32Format() const
{
    return m_pngFormat;
}

HANDLE MSWindowsClipboardPNGConverter::fromIClipboard(const std::string& data) const
{
    if (data.empty()) {
        return nullptr;
    }

    HGLOBAL gData = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, data.size());
    if (gData != nullptr) {
        char* dst = static_cast<char*>(GlobalLock(gData));
        if (dst != nullptr) {
            memcpy(dst, data.data(), data.size());
            GlobalUnlock(gData);
        }
        else {
            GlobalFree(gData);
            gData = nullptr;
        }
    }

    return gData;
}

std::string MSWindowsClipboardPNGConverter::toIClipboard(HANDLE data) const
{
    if (data == nullptr) {
        return {};
    }

    LPVOID src = GlobalLock(data);
    if (src == nullptr) {
        return {};
    }

    SIZE_T srcSize = GlobalSize(data);
    std::string image(static_cast<const char*>(src), srcSize);
    GlobalUnlock(data);

    LOG_DEBUG("extracted PNG image from Windows clipboard, size=%zu", image.size());
    return image;
}

} // namespace inputleap
