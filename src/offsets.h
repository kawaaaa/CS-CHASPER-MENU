#pragma once
#include <cstdint>
#include <fstream>
#include <string>
#include "log.h"
#include <nlohmann/json.hpp>

// Dump a2x/cs2-dumper 2026-11-12. Se schimba la patch:
// buttons.hpp pentru jump/left/right, offsets.hpp pentru dwViewAngles,
// client_dll.hpp la C_BaseEntity pentru restul.
struct Offsets {
    static constexpr std::uintptr_t kForceJump  = 0x2225E10;
    static constexpr std::uintptr_t kForceLeft  = 0x2225C60;
    static constexpr std::uintptr_t kForceRight = 0x2225CF0;
    static constexpr std::uintptr_t kFlags      = 0x3F4;
    static constexpr std::uintptr_t kVelocity   = 0x3F8;
    static constexpr std::uintptr_t kViewAngles = 0x256A4B8;
    static constexpr std::uintptr_t kModelBones = 0x1C0;

    std::uintptr_t dwLocalPlayerPawn{};
    std::uintptr_t dwEntityList{};
    std::uintptr_t dwGameEntitySystem{};
    std::uintptr_t dwViewMatrix{};
    std::uintptr_t dwViewAngles{};
    std::uintptr_t dwForceJump{};
    std::uintptr_t dwForceLeft{};
    std::uintptr_t dwForceRight{};
    std::uintptr_t m_iHealth{};
    std::uintptr_t m_iTeamNum{};
    std::uintptr_t m_lifeState{};
    std::uintptr_t m_fFlags{};
    std::uintptr_t m_vecAbsVelocity{};
    std::uintptr_t m_vecAbsOrigin{};
    std::uintptr_t m_pGameSceneNode{};
    std::uintptr_t m_pEntity{};
    std::uintptr_t m_bDormant{};
    std::uintptr_t m_modelBones{};

    bool Load(const char* path) {
        std::ifstream f(path);
        if (!f) { log_::Err("offsets.json lipseste langa exe"); return false; }
        try {
            nlohmann::json j; f >> j;
            auto get = [&](const char* k, std::uintptr_t def = 0) -> std::uintptr_t {
                if (!j.contains(k)) return def;
                const auto v = j[k].get<std::uintptr_t>();
                return v ? v : def;
            };
            dwLocalPlayerPawn = get("dwLocalPlayerPawn");
            dwEntityList      = get("dwEntityList");
            dwGameEntitySystem= get("dwGameEntitySystem");
            dwViewMatrix      = get("dwViewMatrix");
            m_iHealth         = get("m_iHealth");
            m_iTeamNum        = get("m_iTeamNum");
            m_lifeState       = get("m_lifeState");
            m_vecAbsOrigin    = get("m_vecAbsOrigin");
            m_pGameSceneNode  = get("m_pGameSceneNode");
            m_pEntity         = get("m_pEntity");
            m_bDormant        = get("m_bDormant", 0x103);
        } catch (const std::exception& e) {
            log_::Err("offsets.json invalid: %s", e.what());
            return false;
        }

        dwForceJump      = kForceJump;
        dwForceLeft      = kForceLeft;
        dwForceRight     = kForceRight;
        m_fFlags         = kFlags;
        m_vecAbsVelocity = kVelocity;
        dwViewAngles     = kViewAngles;
        m_modelBones     = kModelBones;

        log_::Ok("offsets: localPawn=0x%llX entityList=0x%llX health=0x%llX",
                 dwLocalPlayerPawn, dwEntityList, m_iHealth);
        log_::Ok("bhop: jump=0x%llX left=0x%llX right=0x%llX flags=0x%llX",
                 dwForceJump, dwForceLeft, dwForceRight, m_fFlags);

        return dwLocalPlayerPawn && m_iHealth;
    }
};