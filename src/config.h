#pragma once
#include "nlohmann/json.hpp"
#include <fstream>

struct Color { float r = 1.f, g = 1.f, b = 1.f, a = 1.f; };

struct Config {
    bool menu = false;

    bool espBox        = true;
    bool espHealth     = true;
    bool espDistance   = true;
    bool espSnapline   = false;
    bool espHeadDot    = false;
    bool espName       = false;
    bool espWeapon     = false;
    bool espSkeleton   = false;

    bool onlyEnemies   = false;
    bool ignoreDormant = true;
    float maxDistance  = 3000.0f;

    int   boxStyle     = 0;
    float boxThickness = 1.5f;
    bool  outline      = true;
    bool  distanceFade = true;
    float smoothing    = 0.30f;

    Color colEnemy     { 1.00f, 0.25f, 0.25f, 1.0f };
    Color colTeam      { 0.30f, 0.65f, 1.00f, 1.0f };
    Color colSkeleton  { 1.00f, 1.00f, 1.00f, 0.9f };

    bool  fovCircle    = false;
    float fovRadius    = 120.0f;
    bool  watermark    = true;
    int   menuKey      = 0xDC;

    bool  bhop         = false;
    int   bhopKey      = 0x20;
    bool  autostrafe   = true;

    bool  aimbot       = false;
    int   aimKey       = 0x06;
    float aimFov       = 8.0f;
    float aimSmooth    = 0.55f;
    float aimMaxDist   = 2500.0f;

    void Save(const char* path) const {
        using nlohmann::json;
        json j{
            {"espBox", espBox}, {"espHealth", espHealth}, {"espDistance", espDistance},
            {"espSnapline", espSnapline}, {"espHeadDot", espHeadDot},
            {"espName", espName}, {"espWeapon", espWeapon}, {"espSkeleton", espSkeleton},
            {"onlyEnemies", onlyEnemies}, {"ignoreDormant", ignoreDormant},
            {"maxDistance", maxDistance}, {"boxStyle", boxStyle},
            {"boxThickness", boxThickness}, {"outline", outline},
            {"distanceFade", distanceFade}, {"smoothing", smoothing},
            {"fovCircle", fovCircle}, {"fovRadius", fovRadius},
            {"watermark", watermark}, {"menuKey", menuKey},
            {"bhop", bhop}, {"bhopKey", bhopKey}, {"autostrafe", autostrafe},
            {"aimbot", aimbot}, {"aimKey", aimKey}, {"aimFov", aimFov},
            {"aimSmooth", aimSmooth}, {"aimMaxDist", aimMaxDist},
            {"colEnemy", {colEnemy.r, colEnemy.g, colEnemy.b, colEnemy.a}},
            {"colTeam", {colTeam.r, colTeam.g, colTeam.b, colTeam.a}},
            {"colSkeleton", {colSkeleton.r, colSkeleton.g, colSkeleton.b, colSkeleton.a}}
        };
        std::ofstream f(path);
        if (f) f << j.dump(2);
    }

    void Load(const char* path) {
        std::ifstream f(path);
        if (!f) return;
        try {
            nlohmann::json j;
            f >> j;
            auto g = [&](const char* k, bool& v) { if (j.contains(k)) v = j[k].get<bool>(); };
            auto n = [&](const char* k, float& v) { if (j.contains(k)) v = j[k].get<float>(); };
            auto i = [&](const char* k, int& v) { if (j.contains(k)) v = j[k].get<int>(); };
            auto col = [&](const char* k, Color& c) {
                if (!j.contains(k)) return;
                const auto& a = j[k];
                if (a.size() == 4) { c.r = a[0]; c.g = a[1]; c.b = a[2]; c.a = a[3]; }
            };
            g("espBox", espBox); g("espHealth", espHealth); g("espDistance", espDistance);
            g("espSnapline", espSnapline); g("espHeadDot", espHeadDot);
            g("espName", espName); g("espWeapon", espWeapon); g("espSkeleton", espSkeleton);
            g("onlyEnemies", onlyEnemies); g("ignoreDormant", ignoreDormant);
            n("maxDistance", maxDistance); i("boxStyle", boxStyle);
            n("boxThickness", boxThickness); g("outline", outline);
            g("distanceFade", distanceFade); n("smoothing", smoothing);
            g("fovCircle", fovCircle); n("fovRadius", fovRadius);
            g("watermark", watermark); i("menuKey", menuKey);
            g("bhop", bhop); i("bhopKey", bhopKey); g("autostrafe", autostrafe);
            g("aimbot", aimbot); i("aimKey", aimKey); n("aimFov", aimFov);
            n("aimSmooth", aimSmooth); n("aimMaxDist", aimMaxDist);
            col("colEnemy", colEnemy); col("colTeam", colTeam); col("colSkeleton", colSkeleton);
        } catch (...) {}
    }
};

inline Config cfg;