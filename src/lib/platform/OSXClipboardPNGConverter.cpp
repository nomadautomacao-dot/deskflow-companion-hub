/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2026 InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "platform/OSXClipboardPNGConverter.h"
#include "base/Log.h"

namespace inputleap {

OSXClipboardPNGConverter::OSXClipboardPNGConverter()
{
}

OSXClipboardPNGConverter::~OSXClipboardPNGConverter()
{
}

IClipboard::EFormat OSXClipboardPNGConverter::getFormat() const
{
    return IClipboard::kPNG;
}

CFStringRef OSXClipboardPNGConverter::getOSXFormat() const
{
    return CFSTR("public.png");
}

std::string OSXClipboardPNGConverter::doFromIClipboard(const std::string& data) const
{
    // Return raw PNG data directly to macOS pasteboard
    return data;
}

std::string OSXClipboardPNGConverter::doToIClipboard(const std::string& data) const
{
    // Return raw PNG data directly from macOS pasteboard
    return data;
}

} // namespace inputleap
