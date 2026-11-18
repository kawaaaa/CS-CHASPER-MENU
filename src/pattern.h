#pragma once
#include "memory.h"
#include <optional>
#include <string_view>
#include <cstdint>

namespace pattern {
    // Scanează un interval de memorie după o semnătură IDA ("48 8B 05 ? ? ? ?").
    std::optional<std::uintptr_t> Scan(const Memory& mem, std::uintptr_t start,
                                       std::size_t size, std::string_view signature);

    // Rezolvă o referință RIP-relativă: match + offsetDisp -> adresă absolută.
    std::optional<std::uintptr_t> ResolveRip(const Memory& mem, std::uintptr_t match,
                                             std::size_t dispOffset, std::size_t instrLen);
}