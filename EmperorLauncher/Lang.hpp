#pragma once
// Launcher UI language (English / German).
//
// Usage: T(L"English text", L"Deutscher Text") returns the string for the active language.
// German umlauts are written as \u escapes (\u00e4 \u00f6 \u00fc \u00c4 \u00d6 \u00dc \u00df) so
// the source stays plain ASCII and compiles the same regardless of the compiler's code page.
#include <windows.h>
#include <string>

enum class Lang { EN, DE };

inline Lang gLang = Lang::EN;

inline const wchar_t* T(const wchar_t* en, const wchar_t* de)
{
  return gLang == Lang::DE ? de : en;
}

// "de" / "en" as stored in the settings; anything else (empty = never chosen) follows Windows.
inline Lang languageFromSetting(const std::string& value)
{
  if (value == "de")
    return Lang::DE;
  if (value == "en")
    return Lang::EN;
  return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_GERMAN ? Lang::DE : Lang::EN;
}

inline const char* languageToSetting(Lang lang)
{
  return lang == Lang::DE ? "de" : "en";
}

inline std::wstring toWide(const std::string& s)
{
  return std::wstring(s.begin(), s.end()); // paths here are ASCII-only (checked at startup)
}
