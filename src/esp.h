#pragma once
#include "imgui.h"
#include "entity.h"
#include "vecmath.h"
#include "config.h"
#include <cmath>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <unordered_map>

namespace espd {

// compenseaza exact intarzierea netezirii; nu se regleaza din meniu
inline constexpr float kPrediction = 1.0f;

inline ImU32 Col(const Color& c, float mul = 1.0f) {
    return IM_COL32(static_cast<int>(c.r * 255.0f), static_cast<int>(c.g * 255.0f),
                    static_cast<int>(c.b * 255.0f),
                    static_cast<int>(c.a * mul * 255.0f));
}

inline void ShadowText(ImDrawList* dl, ImVec2 p, ImU32 col, const char* txt) {
    dl->AddText({ p.x + 1.0f, p.y + 1.0f }, IM_COL32(0, 0, 0, 190), txt);
    dl->AddText(p, col, txt);
}

inline void Corners(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col, float t) {
    const float w = b.x - a.x;
    const float h = b.y - a.y;
    const float lx = (std::min)(w * 0.28f, 12.0f);
    const float ly = (std::min)(h * 0.18f, 10.0f);

    dl->AddLine({ a.x, a.y }, { a.x + lx, a.y }, col, t);
    dl->AddLine({ a.x, a.y }, { a.x, a.y + ly }, col, t);
    dl->AddLine({ b.x, a.y }, { b.x - lx, a.y }, col, t);
    dl->AddLine({ b.x, a.y }, { b.x, a.y + ly }, col, t);
    dl->AddLine({ a.x, b.y }, { a.x + lx, b.y }, col, t);
    dl->AddLine({ a.x, b.y }, { a.x, b.y - ly }, col, t);
    dl->AddLine({ b.x, b.y }, { b.x - lx, b.y }, col, t);
    dl->AddLine({ b.x, b.y }, { b.x, b.y - ly }, col, t);
}

struct Smooth {
    Vec3   head{};
    Vec3   feet{};
    bool   valid{ false };
    double last{ 0.0 };
};

inline std::unordered_map<std::uintptr_t, Smooth>& Store() {
    static std::unordered_map<std::uintptr_t, Smooth> s;
    return s;
}

inline float Lambda(float smoothing) {
    return 2.0f + (std::clamp)(smoothing, 0.0f, 1.0f) * 26.0f;
}

inline void Prune(double now) {
    auto& s = Store();
    for (auto it = s.begin(); it != s.end();) {
        if (now - it->second.last > 1.2) it = s.erase(it);
        else ++it;
    }
}

inline void SmoothTo(std::uintptr_t key, Vec3 th, Vec3 tf, const Vec3& vel,
                     float dt, Vec3& oh, Vec3& of) {
    auto& st = Store()[key];
    const double now = ImGui::GetTime();

    const float lambda = Lambda(cfg.smoothing);
    const float tau = kPrediction / lambda;

    const float px = vel.x * tau;
    const float py = vel.y * tau;
    th.x += px; tf.x += px;
    th.y += py; tf.y += py;

    const float dx = th.x - st.head.x;
    const float dy = th.y - st.head.y;
    const float dz = th.z - st.head.z;
    const float d2 = dx * dx + dy * dy + dz * dz;

    const bool fresh = !st.valid || (now - st.last) > 0.6;
    if (fresh || d2 > 150.0f * 150.0f) {
        st.head = th;
        st.feet = tf;
        st.valid = true;
        st.last = now;
        oh = th;
        of = tf;
        return;
    }

    const float a = 1.0f - std::exp(-lambda * dt);
    st.head = { st.head.x + dx * a, st.head.y + dy * a, st.head.z + dz * a };
    st.feet = { st.feet.x + (tf.x - st.feet.x) * a,
                st.feet.y + (tf.y - st.feet.y) * a,
                st.feet.z + (tf.z - st.feet.z) * a };
    st.valid = true;
    st.last = now;
    oh = st.head;
    of = st.feet;
}

inline constexpr int kLinks[][2] = {
    { 5, 6 }, { 4, 5 }, { 2, 4 }, { 0, 2 },
    { 8, 5 }, { 9, 8 }, { 20, 9 },
    { 13, 5 }, { 14, 13 }, { 15, 14 },
    { 0, 22 }, { 22, 23 }, { 23, 24 },
    { 0, 25 }, { 25, 26 }, { 26, 27 }
};

} // namespace espd

inline void DrawESP(ImDrawList* dl, std::vector<PlayerData>& players,
                    const Vec3& localOrigin, std::uint8_t localTeam,
                    const float* view, float w, float h) {
    std::sort(players.begin(), players.end(),
              [](const PlayerData& a, const PlayerData& b) { return a.dist > b.dist; });

    const float dt = (std::clamp)(ImGui::GetIO().DeltaTime, 1.0f / 1000.0f, 0.05f);
    espd::Prune(ImGui::GetTime());

    for (auto& p : players) {
        const bool sameTeam = p.team == localTeam;
        if (cfg.onlyEnemies && sameTeam) continue;
        if (p.dist > cfg.maxDistance) continue;

        Vec3 headTarget = p.origin;
        headTarget.z += 72.0f;

        Vec3 head, feet;
        espd::SmoothTo(p.pawn, headTarget, p.origin, p.vel, dt, head, feet);

        Vec2 sh, sf;
        const bool okHead = WorldToScreen(view, head, w, h, sh);
        const bool okFeet = WorldToScreen(view, feet, w, h, sf);
        if (!okHead || !okFeet) continue;

        float bh = sf.y - sh.y;
        if (bh < 3.0f || bh > h * 2.0f) {
            if (!WorldToScreen(view, headTarget, w, h, sh)) continue;
            if (!WorldToScreen(view, p.origin, w, h, sf)) continue;
            bh = sf.y - sh.y;
            if (bh < 3.0f || bh > h * 2.0f) continue;
        }

        const float bw = bh * 0.45f;
        const float fade = cfg.distanceFade
            ? (std::clamp)(1.0f - p.dist / cfg.maxDistance * 0.85f, 0.35f, 1.0f)
            : 1.0f;

        const Color& base = sameTeam ? cfg.colTeam : cfg.colEnemy;
        const ImU32 col = espd::Col(base, fade);
        const ImVec2 tl{ sh.x - bw * 0.5f, sh.y };
        const ImVec2 br{ sh.x + bw * 0.5f, sf.y };

        if (cfg.espSnapline) {
            const ImU32 sc = espd::Col(base, fade * 0.6f);
            dl->AddLine({ w * 0.5f, h }, { sh.x, sf.y }, sc, 1.0f);
        }

        if (cfg.espSkeleton && p.hasBones) {
            const ImU32 scol = espd::Col(cfg.colSkeleton, fade);
            for (const auto& link : espd::kLinks) {
                Vec2 a, b;
                if (!WorldToScreen(view, p.bones[link[0]], w, h, a)) continue;
                if (!WorldToScreen(view, p.bones[link[1]], w, h, b)) continue;
                dl->AddLine({ a.x, a.y }, { b.x, b.y }, scol, 1.2f);
            }
        }

        if (cfg.espBox) {
            if (cfg.outline)
                dl->AddRect({ tl.x - 1.0f, tl.y - 1.0f }, { br.x + 1.0f, br.y + 1.0f },
                            IM_COL32(0, 0, 0, static_cast<int>(190 * fade)),
                            0.0f, 0, cfg.boxThickness + 1.0f);

            switch (cfg.boxStyle) {
            case 1:
                espd::Corners(dl, tl, br, col, cfg.boxThickness + 0.5f);
                break;
            case 2:
                dl->AddRectFilled(tl, br, espd::Col(base, fade * 0.22f));
                dl->AddRect(tl, br, col, 0.0f, 0, cfg.boxThickness);
                break;
            default:
                dl->AddRect(tl, br, col, 0.0f, 0, cfg.boxThickness);
                break;
            }
        }

        if (cfg.espHeadDot)
            dl->AddCircleFilled({ sh.x, sh.y - 4.0f }, 2.5f, col, 10);

        if (cfg.espHealth) {
            const float hh = bh * (static_cast<float>(p.health) / 100.0f);
            const ImU32 hc = p.health > 60 ? IM_COL32(70, 220, 70, static_cast<int>(230 * fade))
                           : p.health > 30 ? IM_COL32(230, 200, 60, static_cast<int>(230 * fade))
                                           : IM_COL32(230, 70, 70, static_cast<int>(230 * fade));
            dl->AddRectFilled({ tl.x - 5.0f, sf.y - bh - 1.0f }, { tl.x - 2.0f, sf.y + 1.0f },
                              IM_COL32(0, 0, 0, static_cast<int>(170 * fade)));
            dl->AddRectFilled({ tl.x - 4.5f, sf.y - hh }, { tl.x - 2.5f, sf.y }, hc);
        }

        if (cfg.espDistance) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.0fm", p.dist / 100.0f);
            espd::ShadowText(dl, { sh.x - 10.0f, sh.y - 15.0f },
                             IM_COL32(235, 235, 235, static_cast<int>(235 * fade)), buf);
        }

        if (cfg.espName) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "HP %d", p.health);
            espd::ShadowText(dl, { sh.x - 16.0f, sf.y + 3.0f },
                             IM_COL32(200, 200, 200, static_cast<int>(200 * fade)), buf);
        }
    }
}