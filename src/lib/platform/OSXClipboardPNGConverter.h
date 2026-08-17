/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2026 InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#pragma once

#include "platform/OSXClipboardAnyBitmapConverter.h"

namespace inputleap {

class OSXClipboardPNGConverter : public OSXClipboardAnyBitmapConverter {
public:
    OSXClipboardPNGConverter();
    virtual ~OSXClipboardPNGConverter();

    // IOSXClipboardConverter overrides
    virtual IClipboard::EFormat getFormat() const override;
    virtual CFStringRef getOSXFormat() const override;

protected:
    virtual std::string doFromIClipboard(const std::string&) const override;
    virtual std::string doToIClipboard(const std::string&) const override;
};

} // namespace inputleap
