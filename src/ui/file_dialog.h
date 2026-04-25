#pragma once

#include <optional>
#include <string>

namespace asteria::ui {

std::optional<std::string> showSaveFileDialog(
    const std::wstring& title,
    const std::string& defaultFileName,
    const wchar_t* filter,
    const wchar_t* defaultExtension);

std::string sanitizeFileName(const std::string& value);

}  // namespace asteria::ui