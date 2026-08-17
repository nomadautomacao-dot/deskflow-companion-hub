/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2026 InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#pragma once

#include "platform/MSWindowsClipboard.h"

namespace inputleap {

//! Convert to/from PNG image format on Windows clipboard
class MSWindowsClipboardPNGConverter : public IMSWindowsClipboardConverter {
public:
    MSWindowsClipboardPNGConverter();
    virtual ~MSWindowsClipboardPNGConverter();

    // IMSWindowsClipboardConverter overrides
    virtual IClipboard::EFormat getFormat() const override;
    virtual UINT getWin32Format() const override;
    virtual HANDLE fromIClipboard(const std::string& data) const override;
    virtual std::string toIClipboard(HANDLE data) const override;

private:
    UINT m_pngFormat;
};

} // namespace inputleap
