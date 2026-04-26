#include "file_dialog.h"

#include <Windows.h>
#include <commdlg.h>

#include <algorithm>
#include <vector>

namespace asteria::ui {

namespace {

std::wstring utf8ToWide(const std::string& value) {
  if (value.empty()) return {};
  const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
  if (size <= 0) return {};

  std::wstring out(static_cast<size_t>(size - 1), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, out.data(), size);
  return out;
}

std::string wideToUtf8(const std::wstring& value) {
  if (value.empty()) return {};
  const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
  if (size <= 0) return {};

  std::string out(static_cast<size_t>(size - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, out.data(), size, nullptr, nullptr);
  return out;
}

}  // namespace

std::string sanitizeFileName(const std::string& value) {
  std::string out = value;
  std::replace_if(out.begin(), out.end(),
                  [](char ch) {
                    return ch == '<' || ch == '>' || ch == ':' || ch == '"' ||
                           ch == '/' || ch == '\\' || ch == '|' || ch == '?' || ch == '*';
                  },
                  '_');
  return out.empty() ? std::string("asteria") : out;
}

std::optional<std::string> showSaveFileDialog(
    const std::wstring& title,
    const std::string& defaultFileName,
    const wchar_t* filter,
    const wchar_t* defaultExtension) {
  std::wstring defaultName = utf8ToWide(defaultFileName);
  std::vector<wchar_t> fileBuffer(4096, L'\0');
  if (!defaultName.empty()) {
    defaultName.copy(fileBuffer.data(), (std::min)(defaultName.size(), fileBuffer.size() - 1));
  }

  OPENFILENAMEW ofn{};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = GetActiveWindow();
  ofn.lpstrFile = fileBuffer.data();
  ofn.nMaxFile = static_cast<DWORD>(fileBuffer.size());
  ofn.lpstrFilter = filter;
  ofn.nFilterIndex = 1;
  ofn.lpstrTitle = title.c_str();
  ofn.lpstrDefExt = defaultExtension;
  ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

  if (!GetSaveFileNameW(&ofn)) {
    return std::nullopt;
  }

  return wideToUtf8(fileBuffer.data());
}

std::optional<std::string> showOpenFileDialog(
    const std::wstring& title,
    const wchar_t* filter) {
  std::vector<wchar_t> fileBuffer(4096, L'\0');

  OPENFILENAMEW ofn{};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = GetActiveWindow();
  ofn.lpstrFile = fileBuffer.data();
  ofn.nMaxFile = static_cast<DWORD>(fileBuffer.size());
  ofn.lpstrFilter = filter;
  ofn.nFilterIndex = 1;
  ofn.lpstrTitle = title.c_str();
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

  if (!GetOpenFileNameW(&ofn)) {
    return std::nullopt;
  }

  return wideToUtf8(fileBuffer.data());
}

}  // namespace asteria::ui