#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <string>
#include <cstdint>

class Process {
public:
    bool Attach(std::wstring_view exeName);
    void Detach();

    std::uintptr_t ModuleBase(std::wstring_view moduleName) const;
    std::size_t    ModuleSize(std::wstring_view moduleName) const;

    HANDLE  handle() const { return m_handle; }
    DWORD   pid()    const { return m_pid; }
    bool    valid()  const { return m_handle != nullptr; }

private:
    HANDLE m_handle{};
    DWORD  m_pid{};
};