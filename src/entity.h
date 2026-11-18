#pragma once
#include <cstdint>
#include <cstring>
#include <array>
#include <vector>
#include <unordered_set>
#include <algorithm>
#include <cmath>
#include "memory.h"
#include "offsets.h"

struct Vec3 { float x{}, y{}, z{}; };

inline constexpr int kBoneCount = 30;

struct PlayerData {
    std::uintptr_t pawn{};
    Vec3 origin{};
    Vec3 vel{};
    Vec3 aimPoint{};
    std::array<Vec3, kBoneCount> bones{};
    bool hasBones{};
    int health{};
    std::uint8_t team{};
    float dist{};
    double lastSeen{};
};

inline bool PlausiblePtr(std::uintptr_t p) {
    return p > 0x10000 && p < 0x7FFFFFFEFFFFull && (p & 0x7) == 0;
}

inline float BoneDist(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

class EntityFinder {
public:
    explicit EntityFinder(const Memory& mem) : m_mem(mem) {}

    void Reset() { m_anchor = 0; }
    bool Located() const { return m_anchor != 0; }

    bool Locate(std::uintptr_t localPawn, const Offsets& off) {
        Reset();
        int best = 0;

        constexpr std::size_t CHUNK = 0x400000;
        std::vector<std::uint8_t> buf(CHUNK);
        std::uintptr_t addr = 0x10000;
        MEMORY_BASIC_INFORMATION mbi{};

        while (VirtualQueryEx(m_mem.handle(), reinterpret_cast<LPCVOID>(addr),
                              &mbi, sizeof(mbi))) {
            const auto rbase = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
            const auto rsize = static_cast<std::size_t>(mbi.RegionSize);
            addr = rbase + rsize;

            if (mbi.State != MEM_COMMIT || (mbi.Protect & PAGE_GUARD)) continue;
            if (!(mbi.Protect & (PAGE_READWRITE | PAGE_READONLY |
                                 PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_READ))) continue;

            for (std::size_t o = 0; o < rsize; o += CHUNK) {
                const std::size_t toRead = (std::min)(CHUNK, rsize - o);
                if (!m_mem.ReadRaw(rbase + o, buf.data(), toRead)) continue;

                for (std::size_t i = 0; i + 8 <= toRead; i += 8) {
                    std::uintptr_t val{};
                    std::memcpy(&val, buf.data() + i, 8);
                    if (val != localPawn) continue;

                    const auto hit = rbase + o + i;
                    const int score = Score(hit, localPawn, off);
                    if (score > best) { best = score; m_anchor = hit; }
                    if (best >= 6) return true;
                }
            }
        }
        return m_anchor != 0;
    }

    std::vector<PlayerData> Collect(std::uintptr_t localPawn, const Offsets& off) const {
        std::vector<PlayerData> out;
        out.reserve(64);
        if (!m_anchor) return out;

        constexpr int SPAN = 256;
        constexpr std::size_t STRIDE = 0x18;
        constexpr std::size_t COUNT = SPAN * 2 + 1;
        constexpr std::size_t BYTES = COUNT * STRIDE;

        m_buf.resize(BYTES);
        if (!m_mem.ReadRaw(m_anchor - SPAN * STRIDE, m_buf.data(), BYTES)) return out;

        m_seen.clear();
        m_seen.reserve(64);

        for (std::size_t i = 0; i < COUNT; ++i) {
            std::uintptr_t pawn{};
            std::memcpy(&pawn, m_buf.data() + i * STRIDE, 8);
            if (!PlausiblePtr(pawn) || pawn == localPawn) continue;
            if (!m_seen.insert(pawn).second) continue;

            const auto node = m_mem.Read<std::uintptr_t>(pawn + off.m_pGameSceneNode);
            if (!PlausiblePtr(node)) continue;

            const int hp = m_mem.Read<int>(pawn + off.m_iHealth);
            if (hp < 1 || hp > 100) continue;

            if (m_mem.Read<std::uint8_t>(node + off.m_bDormant) != 0) continue;

            const auto team = m_mem.Read<std::uint8_t>(pawn + off.m_iTeamNum);
            if (team != 2 && team != 3) continue;

            PlayerData p;
            p.pawn   = pawn;
            p.health = hp;
            p.team   = team;
            p.origin = m_mem.Read<Vec3>(node + off.m_vecAbsOrigin);
            p.vel    = m_mem.Read<Vec3>(pawn + off.m_vecAbsVelocity);

            // punct de rezerva pentru aim, daca oasele nu sunt valide
            p.aimPoint = p.origin;
            p.aimPoint.z += 64.0f;

            const auto boneArr = m_mem.Read<std::uintptr_t>(node + off.m_modelBones);
            if (PlausiblePtr(boneArr)) {
                // intrare de 0x20: Vec3 pozitie la +0x0, restul padding
                std::array<std::uint8_t, kBoneCount * 0x20> bones{};
                if (m_mem.ReadRaw(boneArr, bones.data(), bones.size())) {
                    bool any = false;
                    for (int b = 0; b < kBoneCount; ++b) {
                        std::memcpy(&p.bones[b], bones.data() + b * 0x20, sizeof(Vec3));
                        if (BoneDist(p.bones[b], p.origin) < 160.0f) any = true;
                    }
                    // capul trebuie sa fie aproape de origine, altfel e gunoi
                    if (BoneDist(p.bones[6], p.origin) < 160.0f) {
                        p.hasBones = any;
                        p.aimPoint = p.bones[6];
                    }
                }
            }

            out.push_back(p);
        }
        return out;
    }

private:
    int Score(std::uintptr_t anchor, std::uintptr_t localPawn, const Offsets& off) const {
        constexpr int SPAN = 256;
        constexpr std::size_t STRIDE = 0x18;
        constexpr std::size_t COUNT = SPAN * 2 + 1;
        constexpr std::size_t BYTES = COUNT * STRIDE;

        m_scoreBuf.resize(BYTES);
        if (!m_mem.ReadRaw(anchor - SPAN * STRIDE, m_scoreBuf.data(), BYTES)) return 0;

        int n = 0;
        for (std::size_t i = 0; i < COUNT; i += 2) {
            std::uintptr_t pawn{};
            std::memcpy(&pawn, m_scoreBuf.data() + i * STRIDE, 8);
            if (!PlausiblePtr(pawn) || pawn == localPawn) continue;
            const int hp = m_mem.Read<int>(pawn + off.m_iHealth);
            if (hp < 1 || hp > 100) continue;
            const auto t = m_mem.Read<std::uint8_t>(pawn + off.m_iTeamNum);
            if (t == 2 || t == 3) ++n;
        }
        return n;
    }

    const Memory& m_mem;
    std::uintptr_t m_anchor{};
    mutable std::vector<std::uint8_t> m_buf;
    mutable std::vector<std::uint8_t> m_scoreBuf;
    mutable std::unordered_set<std::uintptr_t> m_seen;
};