#pragma once

namespace kawa {
    inline constexpr const wchar_t* kName     = L"kawa's menu";
    inline constexpr const char*    kNameUtf8 = "kawa's menu";
    inline constexpr const char*    kVersion  = "0.1.0";
    inline constexpr const char*    kAuthor   = "licenta";

    // Virtual-key pentru deschiderea meniului: VK_OEM_5 = tasta "\"
    inline constexpr int kMenuKey = VK_OEM_5;
}