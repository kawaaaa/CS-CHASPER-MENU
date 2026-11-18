#include "proc.h"
#include <vector>

bool Process::Attach(std::wstring_view exeName) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe{ sizeof(pe) };
    bool found = false;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (exeName == pe.szExeFile) { m_pid = pe.th32ProcessID; found = true; break; }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    if (!found) return false;

    m_handle = OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION,
        FALSE, m_pid);
    return m_handle != nullptr;
}

void Process::Detach() {
    if (m_handle) { CloseHandle(m_handle); m_handle = nullptr; }
    m_pid = 0;
}

std::uintptr_t Process::ModuleBase(std::wstring_view moduleName) const {
    if (!m_handle) return 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, m_pid);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    MODULEENTRY32W me{ sizeof(me) };
    std::uintptr_t base = 0;
    if (Module32FirstW(snap, &me)) {
        do {
            if (moduleName == me.szModule) { base = reinterpret_cast<std::uintptr_t>(me.modBaseAddr); break; }
        } while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);
    return base;
}

std::size_t Process::ModuleSize(std::wstring_view moduleName) const {
    std::uintptr_t base = ModuleBase(moduleName);
    if (!base || !m_handle) return 0;

    HMODULE mods[1024]{};
    DWORD needed = 0;
    if (!EnumProcessModulesEx(m_handle, mods, sizeof(mods), &needed, LIST_MODULES_ALL)) return 0;

    const DWORD count = needed / sizeof(HMODULE);
    for (DWORD i = 0; i < count; ++i) {
        if (reinterpret_cast<std::uintptr_t>(mods[i]) != base) continue;
        MODULEINFO mi{};
        if (GetModuleInformation(m_handle, mods[i], &mi, sizeof(mi)))
            return mi.SizeOfImage;
    }
    return 0;
}