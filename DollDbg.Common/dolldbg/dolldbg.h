#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>

#include <locale>
#include <string>
#include <vector>
#include <cstdint>
#include <codecvt>
#include <sstream>

namespace DollDbg
{
    using buffer_t = std::vector<uint8_t>;

    using u8string_t = std::string;
    using u8string_convert_t = std::wstring_convert<std::codecvt_utf8_utf16<std::wstring::value_type>, std::wstring::value_type>;
    using string_convert_t = std::wstring_convert<std::codecvt<std::wstring::value_type, std::string::value_type, mbstate_t>, std::wstring::value_type>;

    // NOTE: Programming style tip: Use UNICODE and TEXT(), not _UNICODE, _TEXT() or _T()
    // According to Raymond Chen, the former affects Windows headers while the latter affects CRT headers
    // https://devblogs.microsoft.com/oldnewthing/20040212-00/?p=40643
#ifdef UNICODE
    using string_t = std::wstring;
    using sstream = std::wstringstream;
    
    inline string_t string_from(const std::wstring& str)
        { return str; }
    inline string_t string_from(const std::string& str)
        { return string_convert_t().from_bytes(str); }
    inline string_t string_from(const u8string_t& str, nullptr_t) // HACK: Only Protocol.GetString uses this anyways
        { return u8string_convert_t().from_bytes(str); }
    inline std::wstring string_to_wcs(const string_t& str)
        { return str; }
    inline std::string string_to_mbcs(const string_t& str)
        { return string_convert_t().to_bytes(str); }
    inline u8string_t string_to_u8s(const string_t& str)
        { return u8string_convert_t().to_bytes(str); }
#else
    using string_t = std::string;
    using sstream = std::stringstream;

    inline string_t string_from(const std::wstring& str)
        { return string_convert_t().to_bytes(str); }
    inline string_t string_from(const std::string& str)
        { return str; }
    inline string_t string_from(const u8string_t& str)
        { return string_convert_t().to_bytes(u8string_convert_t().from_bytes(str)); }
    inline std::wstring string_to_wcs(const string_t& str)
        { return string_convert_t().from_bytes(str); }
    inline std::string string_to_mbcs(const string_t& str)
        { return str; }
    inline u8string_t string_to_u8s(const string_t& str)
        { return u8string_convert_t().to_bytes(string_convert_t().from_bytes(str)); }
#endif
}