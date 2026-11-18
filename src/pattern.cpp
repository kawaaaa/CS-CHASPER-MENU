#include "pattern.h"
#include <vector>

namespace pattern {

static bool Parse(std::string_view sig, std::vector<std::uint8_t>& bytes,
                  std::vector<std::uint8_t>& mask) {
    for (std::size_t i = 0; i < sig.size();) {
        if (sig[i] == ' ') { ++i; continue; }
        if (sig[i] == '?') {
            bytes.push_back(0); mask.push_back(0);
            while (i < sig.size() && sig[i] != ' ') ++i;
            continue;
        }
        auto hex = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            return -1;
        };
        if (i + 1 >= sig.size()) return false;
        int hi = hex(sig[i]), lo = hex(sig[i + 1]);
        if (hi < 0 || lo < 0) return false;
        bytes.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
        mask.push_back(1);
        i += 2;
    }
    return !bytes.empty();
}

std::optional<std::uintptr_t> Scan(const Memory& mem, std::uintptr_t start,
                                   std::size_t size, std::string_view signature) {
    std::vector<std::uint8_t> bytes, mask;
    if (!Parse(signature, bytes, mask)) return std::nullopt;

    std::vector<std::uint8_t> buffer(size);
    if (!mem.ReadRaw(start, buffer.data(), size)) return std::nullopt;

    const std::size_t last = size - bytes.size();
    for (std::size_t i = 0; i <= last; ++i) {
        bool hit = true;
        for (std::size_t j = 0; j < bytes.size(); ++j) {
            if (mask[j] && buffer[i + j] != bytes[j]) { hit = false; break; }
        }
        if (hit) return start + i;
    }
    return std::nullopt;
}

std::optional<std::uintptr_t> ResolveRip(const Memory& mem, std::uintptr_t match,
                                         std::size_t dispOffset, std::size_t instrLen) {
    auto disp = mem.Read<std::int32_t>(match + dispOffset);
    if (!disp) return std::nullopt;
    return match + instrLen + static_cast<std::intptr_t>(disp);
}

}