#include <Windows.h>
#include <timeapi.h>
#include <cstdio>
#include <array>
#include <vector>
#include <cmath>
#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <cstdint>
#include "branding.h"
#include "log.h"
#include "proc.h"
#include "memory.h"
#include "offsets.h"
#include "entity.h"
#include "overlay.h"
#include "esp.h"
#include "imgui.h"

#pragma comment(lib, "winmm.lib")

static bool g_waitMenuKey = false;
static bool g_waitBhopKey = false;
static bool g_waitAimKey = false;
static std::atomic<bool> g_run{ true };
static std::atomic<bool> g_bhopDebug{ true };

struct Snapshot {
    std::mutex mtx;
    std::vector<PlayerData> players;
    Vec3 localOrigin{};
    std::uint8_t localTeam{};
    std::array<float, 16> view{};
};

struct QAngle { float pitch{}, yaw{}; };

static bool IsElevated() {
    HANDLE tok = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) return false;
    TOKEN_ELEVATION te{};
    DWORD cb = sizeof(te);
    const bool r = GetTokenInformation(tok, TokenElevation, &te, cb, &cb) && te.TokenIsElevated;
    CloseHandle(tok);
    return r;
}

static void DrawKeyBind(const char* id, int& key, bool& waiting) {
    ImGui::PushID(id);
    if (waiting) {
        for (int vk = 8; vk < 256; ++vk) {
            if (vk >= VK_LBUTTON && vk <= VK_XBUTTON2) continue;
            if (GetAsyncKeyState(vk) & 0x8000) { key = vk; waiting = false; break; }
        }
        ImGui::Button("...", { 70, 0 });
        ImGui::PopID();
        return;
    }
    char label[48];
    const UINT sc = MapVirtualKeyW(static_cast<UINT>(key), MAPVK_VK_TO_VSC);
    char nm[32] = "?";
    if (GetKeyNameTextA(static_cast<LONG>(sc) << 16, nm, sizeof(nm)) == 0) nm[0] = '?';
    std::snprintf(label, sizeof(label), "%s", nm);
    if (ImGui::Button(label, { 70, 0 })) waiting = true;
    ImGui::PopID();
}

static void DrawMenu() {
    ImGui::SetNextWindowSize({ 420, 430 }, ImGuiCond_FirstUseEver);
    ImGui::Begin("kawa's menu", nullptr, ImGuiWindowFlags_NoCollapse);
    ImGui::TextDisabled("kawa's menu  |  CS2  |  %.0f fps", ImGui::GetIO().Framerate);
    ImGui::Separator();

    if (ImGui::BeginTabBar("tabs")) {
        if (ImGui::BeginTabItem("ESP")) {
            ImGui::Checkbox("Cutie", &cfg.espBox);
            ImGui::Checkbox("Bara viata", &cfg.espHealth);
            ImGui::Checkbox("Distanta", &cfg.espDistance);
            ImGui::Checkbox("Snapline", &cfg.espSnapline);
            ImGui::Checkbox("Punct cap", &cfg.espHeadDot);
            ImGui::Checkbox("Info HP", &cfg.espName);
            ImGui::Checkbox("Schelet", &cfg.espSkeleton);
            ImGui::Separator();
            ImGui::Checkbox("Doar inamici", &cfg.onlyEnemies);
            ImGui::Checkbox("Ignora dormant", &cfg.ignoreDormant);
            ImGui::SliderFloat("Distanta max", &cfg.maxDistance, 500.0f, 6000.0f, "%.0f");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Vizual")) {
            const char* styles[] = { "Cutie", "Colturi", "Umplut" };
            ImGui::Combo("Stil", &cfg.boxStyle, styles, 3);
            ImGui::SliderFloat("Grosime", &cfg.boxThickness, 0.5f, 4.0f, "%.1f");
            ImGui::Checkbox("Contur", &cfg.outline);
            ImGui::Checkbox("Fade cu distanta", &cfg.distanceFade);
            ImGui::SliderFloat("Netezire", &cfg.smoothing, 0.0f, 1.0f, "%.2f");
            ImGui::Separator();
            ImGui::ColorEdit4("Inamic", &cfg.colEnemy.r, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4("Coleg", &cfg.colTeam.r, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4("Schelet", &cfg.colSkeleton.r, ImGuiColorEditFlags_NoInputs);
            ImGui::Separator();
            ImGui::Checkbox("Cerc FOV", &cfg.fovCircle);
            ImGui::SliderFloat("Raza FOV", &cfg.fovRadius, 40.0f, 400.0f, "%.0f");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Movement")) {
            ImGui::Checkbox("Bhop", &cfg.bhop);
            ImGui::Text("Tasta:");
            ImGui::SameLine();
            DrawKeyBind("bhopkey", cfg.bhopKey, g_waitBhopKey);
            ImGui::Checkbox("Auto strafe", &cfg.autostrafe);
            ImGui::TextDisabled("autostrafe merge doar in aer, cu bhop activ");
            bool dbg = g_bhopDebug.load();
            if (ImGui::Checkbox("Debug in consola", &dbg)) g_bhopDebug.store(dbg);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Aim")) {
            ImGui::Checkbox("Aimbot", &cfg.aimbot);
            ImGui::Text("Tasta aim:");
            ImGui::SameLine();
            DrawKeyBind("aimkey", cfg.aimKey, g_waitAimKey);
            ImGui::SliderFloat("FOV aim", &cfg.aimFov, 1.0f, 40.0f, "%.1f");
            ImGui::SliderFloat("Netezire aim", &cfg.aimSmooth, 0.0f, 0.95f, "%.2f");
            ImGui::SliderFloat("Distanta max aim", &cfg.aimMaxDist, 300.0f, 5000.0f, "%.0f");
            ImGui::TextDisabled("tine tasta apasata; mouse-ul sa stea");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Setari")) {
            ImGui::Text("Tasta meniu:");
            ImGui::SameLine();
            DrawKeyBind("menukey", cfg.menuKey, g_waitMenuKey);
            ImGui::Separator();
            ImGui::Checkbox("Watermark", &cfg.watermark);
            ImGui::Separator();
            if (ImGui::Button("Salveaza config")) cfg.Save("kawas.json");
            ImGui::SameLine();
            if (ImGui::Button("Incarca config")) cfg.Load("kawas.json");
            ImGui::SameLine();
            if (ImGui::Button("Reset")) cfg = Config{};
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

// Fir separat: doar citiri. Tine un cache ca sa nu clipeasca ESP-ul.
static void DataThread(const std::uintptr_t base, const Offsets& off,
                       Memory& mem, Snapshot& snap) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    EntityFinder finder(mem);
    std::unordered_map<std::uintptr_t, PlayerData> known;
    known.reserve(64);
    ULONGLONG emptySince = 0;
    ULONGLONG badSince = 0;

    while (g_run) {
        const auto local = mem.Read<std::uintptr_t>(base + off.dwLocalPlayerPawn);

        if (!mem.Valid(local)) {
            finder.Reset();
            emptySince = 0;
            if (badSince == 0) badSince = GetTickCount64();
            else if (GetTickCount64() - badSince > 300) {
                std::lock_guard<std::mutex> lk(snap.mtx);
                known.clear();
                snap.players.clear();
            }
            Sleep(15);
            continue;
        }
        badSince = 0;

        if (!finder.Located()) {
            finder.Locate(local, off);
            if (!finder.Located()) { Sleep(30); continue; }
        }

        auto fresh = finder.Collect(local, off);

        const auto node = mem.Read<std::uintptr_t>(local + off.m_pGameSceneNode);
        const Vec3 localOrigin = mem.Valid(node)
                               ? mem.Read<Vec3>(node + off.m_vecAbsOrigin) : Vec3{};
        const auto localTeam = mem.Read<std::uint8_t>(local + off.m_iTeamNum);
        const auto view = mem.Read<std::array<float, 16>>(base + off.dwViewMatrix);

        const double now = static_cast<double>(GetTickCount64());

        for (auto& p : fresh) {
            p.lastSeen = now;
            known[p.pawn] = p;
        }

        for (auto it = known.begin(); it != known.end();) {
            if (now - it->second.lastSeen > 600.0) it = known.erase(it);
            else ++it;
        }

        std::vector<PlayerData> players;
        players.reserve(known.size());
        for (auto& kv : known) players.push_back(kv.second);

        for (auto& p : players) {
            const float dx = p.origin.x - localOrigin.x;
            const float dy = p.origin.y - localOrigin.y;
            const float dz = p.origin.z - localOrigin.z;
            p.dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        }

        if (players.empty()) {
            if (emptySince == 0) emptySince = GetTickCount64();
            else if (GetTickCount64() - emptySince > 1200) { finder.Reset(); emptySince = 0; }
        } else {
            emptySince = 0;
        }

        {
            std::lock_guard<std::mutex> lk(snap.mtx);
            snap.players.swap(players);
            snap.localOrigin = localOrigin;
            snap.localTeam = localTeam;
            snap.view = view;
        }

        Sleep(cfg.menu ? 55 : 8);
    }
}

// Bhop + autostrafe. Autostrafe tine A/D si roteste yaw-ul ca viteza sa
// ramana perpendiculara pe privire, cat timp esti in aer.
static void BhopThread(const std::uintptr_t base, const Offsets& off, Memory& mem) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    const auto jumpAddr  = base + off.dwForceJump;
    const auto leftAddr  = base + off.dwForceLeft;
    const auto rightAddr = base + off.dwForceRight;
    const auto anglesAddr = base + off.dwViewAngles;

    bool pressed = false;
    bool spam = false;
    bool strafing = false;
    int zeroFlags = 0;
    int logged = 0;
    std::uintptr_t local = 0;

    while (g_run) {
        const bool key = cfg.bhop && (GetAsyncKeyState(cfg.bhopKey) & 0x8000);

        if (!key) {
            if (pressed) { mem.Write<std::int32_t>(jumpAddr, 256); pressed = false; }
            if (strafing) {
                mem.Write<std::int32_t>(leftAddr, 256);
                mem.Write<std::int32_t>(rightAddr, 256);
                strafing = false;
            }
            zeroFlags = 0;
            local = 0;
            logged = 0;
            Sleep(5);
            continue;
        }

        if (spam) {
            mem.Write<std::int32_t>(jumpAddr, 65537);
            Sleep(10);
            mem.Write<std::int32_t>(jumpAddr, 256);
            Sleep(10);
            continue;
        }

        if (!mem.Valid(local))
            local = mem.Read<std::uintptr_t>(base + off.dwLocalPlayerPawn);
        if (!mem.Valid(local)) { Sleep(5); continue; }

        const auto flags = mem.Read<std::uint32_t>(local + off.m_fFlags);

        if (g_bhopDebug && logged < 3) {
            ++logged;
            log_::Ok("bhop debug: jumpAddr=0x%llX val=0x%X flags=0x%X",
                     jumpAddr, mem.Read<std::uint32_t>(jumpAddr), flags);
        }

        if (flags == 0) {
            if (++zeroFlags > 250) {
                spam = true;
                log_::Err("m_fFlags nu raspunde - bhop trecut pe spam");
            }
        } else {
            zeroFlags = 0;
        }

        const bool onGround = (flags & 1u) != 0;

        if (onGround || flags == 0) {
            if (!pressed) {
                if (!mem.Write<std::int32_t>(jumpAddr, 65537))
                    log_::Err("bhop: WriteProcessMemory a esuat");
                pressed = true;
            }
        } else if (pressed) {
            mem.Write<std::int32_t>(jumpAddr, 256);
            pressed = false;
        }

        // autostrafe
        const bool aiming = cfg.aimbot && (GetAsyncKeyState(cfg.aimKey) & 0x8000);
        if (cfg.autostrafe && !onGround && flags != 0 && !aiming) {
            const Vec3 vel = mem.Read<Vec3>(local + off.m_vecAbsVelocity);
            const float speed = std::sqrt(vel.x * vel.x + vel.y * vel.y);

            if (speed > 40.0f) {
                const QAngle cur = mem.Read<QAngle>(anglesAddr);
                const float velYaw = std::atan2(vel.y, vel.x) * 57.2957795f;
                float diff = velYaw - cur.yaw;
                while (diff > 180.0f) diff -= 360.0f;
                while (diff < -180.0f) diff += 360.0f;

                constexpr float kStep = 0.55f;
                QAngle out = cur;
                if (diff > 0.0f) {
                    out.yaw = cur.yaw + kStep;
                    mem.Write<std::int32_t>(rightAddr, 65537);
                    mem.Write<std::int32_t>(leftAddr, 256);
                } else {
                    out.yaw = cur.yaw - kStep;
                    mem.Write<std::int32_t>(leftAddr, 65537);
                    mem.Write<std::int32_t>(rightAddr, 256);
                }
                while (out.yaw > 180.0f) out.yaw -= 360.0f;
                while (out.yaw < -180.0f) out.yaw += 360.0f;
                mem.Write<QAngle>(anglesAddr, out);
                strafing = true;
            }
        } else if (strafing) {
            mem.Write<std::int32_t>(leftAddr, 256);
            mem.Write<std::int32_t>(rightAddr, 256);
            strafing = false;
        }

        Sleep(2);
    }

    if (pressed) mem.Write<std::int32_t>(jumpAddr, 256);
}

static void AimThread(const std::uintptr_t base, const Offsets& off,
                      Memory& mem, Snapshot& snap) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    const auto anglesAddr = base + off.dwViewAngles;
    std::vector<PlayerData> list;
    list.reserve(64);

    while (g_run) {
        if (!cfg.aimbot || !(GetAsyncKeyState(cfg.aimKey) & 0x8000)) { Sleep(4); continue; }

        Vec3 eye{};
        std::uint8_t team = 0;
        {
            std::lock_guard<std::mutex> lk(snap.mtx);
            list = snap.players;
            eye = snap.localOrigin;
            team = snap.localTeam;
        }
        eye.z += 64.0f;

        const QAngle cur = mem.Read<QAngle>(anglesAddr);
        if (!std::isfinite(cur.pitch) || !std::isfinite(cur.yaw)) { Sleep(4); continue; }

        float bestFov = cfg.aimFov;
        Vec3 bestTarget{};
        bool found = false;

        for (const auto& p : list) {
            if (p.team == team || !p.hasBones || p.health < 1) continue;
            if (p.dist > cfg.aimMaxDist) continue;

            const Vec3& t = p.bones[6];
            const float dx = t.x - eye.x;
            const float dy = t.y - eye.y;
            const float dz = t.z - eye.z;
            const float hyp = std::sqrt(dx * dx + dy * dy);

            const float pitch = std::atan2(-dz, hyp) * 57.2957795f;
            const float yaw = std::atan2(dy, dx) * 57.2957795f;

            float dYaw = yaw - cur.yaw;
            while (dYaw > 180.0f) dYaw -= 360.0f;
            while (dYaw < -180.0f) dYaw += 360.0f;
            const float dPitch = pitch - cur.pitch;

            const float fov = std::sqrt(dPitch * dPitch + dYaw * dYaw);
            if (fov < bestFov) {
                bestFov = fov;
                bestTarget = t;
                found = true;
            }
        }

        if (!found) { Sleep(2); continue; }

        const float dx = bestTarget.x - eye.x;
        const float dy = bestTarget.y - eye.y;
        const float dz = bestTarget.z - eye.z;
        const float hyp = std::sqrt(dx * dx + dy * dy);

        const float pitch = std::atan2(-dz, hyp) * 57.2957795f;
        const float yaw = std::atan2(dy, dx) * 57.2957795f;

        float dYaw = yaw - cur.yaw;
        while (dYaw > 180.0f) dYaw -= 360.0f;
        while (dYaw < -180.0f) dYaw += 360.0f;

        const float k = 1.0f - (std::clamp)(cfg.aimSmooth, 0.0f, 0.95f);
        QAngle out;
        out.pitch = (std::clamp)(cur.pitch + (pitch - cur.pitch) * k, -89.0f, 89.0f);
        out.yaw = cur.yaw + dYaw * k;
        while (out.yaw > 180.0f) out.yaw -= 360.0f;
        while (out.yaw < -180.0f) out.yaw += 360.0f;

        mem.Write<QAngle>(anglesAddr, out);
        Sleep(1);
    }
}

int main() {
    SetConsoleTitleW(kawa::kName);
    timeBeginPeriod(1);

    if (!IsElevated()) {
        log_::Err("NU ruleaza ca Administrator - inchide si porneste elevat");
        std::getchar();
        return 1;
    }

    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    cfg.Load("kawas.json");

    Process proc;
    if (!proc.Attach(L"cs2.exe")) { log_::Err("fara proces"); std::getchar(); return 1; }
    const auto base = proc.ModuleBase(L"client.dll");
    if (!base) { log_::Err("fara client.dll"); std::getchar(); return 1; }

    Offsets off;
    if (!off.Load("offsets.json")) { std::getchar(); return 1; }
    Memory mem(proc);

    Overlay overlay;
    if (!overlay.Create(proc.pid())) { log_::Err("overlay failed"); std::getchar(); return 1; }
    log_::Ok("overlay pornit");

    Snapshot snap;
    snap.view.fill(0.0f);
    std::thread worker(DataThread, base, std::cref(off), std::ref(mem), std::ref(snap));
    std::thread bhop(BhopThread, base, std::cref(off), std::ref(mem));
    std::thread aim(AimThread, base, std::cref(off), std::ref(mem), std::ref(snap));

    std::vector<PlayerData> local;
    local.reserve(64);
    Vec3 origin{};
    std::uint8_t team = 0;
    std::array<float, 16> view{};
    view.fill(0.0f);

    for (;;) {
        {
            std::lock_guard<std::mutex> lk(snap.mtx);
            local = snap.players;
            origin = snap.localOrigin;
            team = snap.localTeam;
            view = snap.view;
        }

        overlay.Frame([&](float w, float h) {
            if (cfg.menu || !local.empty())
                DrawESP(ImGui::GetBackgroundDrawList(), local, origin, team,
                        view.data(), w, h);
            if (cfg.menu) DrawMenu();
        });

        Sleep(0);
    }

    g_run = false;
    aim.join();
    bhop.join();
    worker.join();
    timeEndPeriod(1);
}