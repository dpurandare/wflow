#pragma once

#include <string>

namespace wflow {

class TextInjector {
public:
    bool InsertText(const std::wstring& text, bool allowClipboardFallback = false) const;

private:
    bool InsertUnicode(const std::wstring& text) const;
    bool InsertViaClipboardPaste(const std::wstring& text) const;
};

}  // namespace wflow
