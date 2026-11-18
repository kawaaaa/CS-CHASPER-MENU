#pragma once
#include "proc.h"
#include <cstdint>
#include <cstddef>

class Memory {
public:
    explicit Memory(const Process& proc) : m_proc(proc) {}
    HANDLE handle() const { return m_proc.handle(); }
    bool ReadRaw(std::uintptr_t address, void* buffer, std::size_t size) const {
        SIZE_T read = 0;
        return ReadProcessMemory(m_proc.handle(), reinterpret_cast<LPCVOID>(address),
                                 buffer, size, &read) && read == size;
    }

    template <typename T>
    T Read(std::uintptr_t address) const {
        T out{};
        ReadRaw(address, &out, sizeof(T));
        return out;
    }

    template <typename T>
    bool Write(std::uintptr_t address, const T& value) const {
        SIZE_T written = 0;
        return WriteProcessMemory(m_proc.handle(), reinterpret_cast<LPVOID>(address),
                                  &value, sizeof(T), &written) && written == sizeof(T);
    }

    bool Valid(std::uintptr_t address) const {
        if (!address || address < 0x10000) return false;
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQueryEx(m_proc.handle(), reinterpret_cast<LPCVOID>(address),
                            &mbi, sizeof(mbi))) return false;
        return mbi.State == MEM_COMMIT
            && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD));
    }

private:
    const Process& m_proc;
};