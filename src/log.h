#pragma once
#include <cstdio>
#include <Windows.h>

namespace log_ {
inline void SetColor(WORD c) {
    static HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(h, c);
}
inline void Info(const char* fmt, ...) {
    SetColor(0x0B);
    std::printf("[*] ");
    SetColor(0x07);
    va_list a; va_start(a, fmt); std::vprintf(fmt, a); va_end(a);
    std::printf("\n");
}
inline void Ok(const char* fmt, ...) {
    SetColor(0x0A);
    std::printf("[+] ");
    SetColor(0x07);
    va_list a; va_start(a, fmt); std::vprintf(fmt, a); va_end(a);
    std::printf("\n");
}
inline void Err(const char* fmt, ...) {
    SetColor(0x0C);
    std::printf("[-] ");
    SetColor(0x07);
    va_list a; va_start(a, fmt); std::vprintf(fmt, a); va_end(a);
    std::printf("\n");
}
}