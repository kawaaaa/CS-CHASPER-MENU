#pragma once
#include "entity.h"

struct Vec2 { float x{}, y{}; };

inline bool WorldToScreen(const float* m, const Vec3& p, float w, float h, Vec2& out) {
    const float cw = p.x * m[12] + p.y * m[13] + p.z * m[14] + m[15];
    if (cw < 0.01f) return false;

    const float cx = p.x * m[0] + p.y * m[1] + p.z * m[2] + m[3];
    const float cy = p.x * m[4] + p.y * m[5] + p.z * m[6] + m[7];

    out.x = (w * 0.5f) * (1.0f + cx / cw);
    out.y = (h * 0.5f) * (1.0f - cy / cw);
    return true;
}