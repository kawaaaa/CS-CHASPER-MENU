#include "overlay.h"
#include "branding.h"
#include "config.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <dwmapi.h>
#include <cstdio>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static bool m_menuGlobal = false;

struct FindCtx { DWORD pid; HWND hwnd; };

static BOOL CALLBACK EnumProc(HWND h, LPARAM lp) {
    auto* c = reinterpret_cast<FindCtx*>(lp);
    DWORD p = 0;
    GetWindowThreadProcessId(h, &p);
    if (p != c->pid || !IsWindowVisible(h)) return TRUE;
    RECT r{};
    GetClientRect(h, &r);
    if (r.right - r.left < 200) return TRUE;
    c->hwnd = h;
    return FALSE;
}

static void ForceForeground(HWND h) {
    const HWND fg = GetForegroundWindow();
    const DWORD fgThread = fg ? GetWindowThreadProcessId(fg, nullptr) : 0;
    const DWORD myThread = GetCurrentThreadId();
    const bool attach = fgThread && fgThread != myThread;
    if (attach) AttachThreadInput(fgThread, myThread, TRUE);
    BringWindowToTop(h);
    SetForegroundWindow(h);
    SetFocus(h);
    if (attach) AttachThreadInput(fgThread, myThread, FALSE);
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (ImGui_ImplWin32_WndProcHandler(h, m, w, l)) return 1;
    switch (m) {
    case WM_SETCURSOR:
        if (LOWORD(l) == HTCLIENT && m_menuGlobal) {
            SetCursor(LoadCursorW(nullptr, IDC_ARROW));
            return TRUE;
        }
        break;
    case WM_MOUSEACTIVATE:
        return MA_ACTIVATE;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static const char* KeyName(int vk) {
    static char buf[32];
    const UINT sc = MapVirtualKeyW(static_cast<UINT>(vk), MAPVK_VK_TO_VSC);
    if (GetKeyNameTextA(static_cast<LONG>(sc) << 16, buf, sizeof(buf)) > 0) return buf;
    return "?";
}

bool Overlay::Create(DWORD pid) {
    FindCtx fc{ pid, nullptr };
    EnumWindows(EnumProc, reinterpret_cast<LPARAM>(&fc));
    if (!fc.hwnd) { std::printf("[!] fereastra jocului negasita\n"); return false; }
    std::printf("[*] fereastra joc: %p\n", static_cast<void*>(fc.hwnd));
    m_game = fc.hwnd;

    WNDCLASSEXW wc{ sizeof(wc) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"kawas_menu_overlay";
    RegisterClassExW(&wc);

    POINT p{ 0, 0 };
    ClientToScreen(m_game, &p);
    RECT cr{};
    GetClientRect(m_game, &cr);
    m_pos = p;
    m_size = { cr.right, cr.bottom };

    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        wc.lpszClassName, L"kawa's menu", WS_POPUP,
        p.x, p.y, m_size.x, m_size.y,
        nullptr, nullptr, wc.hInstance, nullptr);
    if (!m_hwnd) { std::printf("[!] CreateWindowEx: %lu\n", GetLastError()); return false; }
    std::printf("[*] overlay hwnd=%p %ldx%ld\n", static_cast<void*>(m_hwnd), m_size.x, m_size.y);

    MARGINS margins{ -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(m_hwnd, &margins);
    SetLayeredWindowAttributes(m_hwnd, 0, 255, LWA_ALPHA);
    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);

    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = m_hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL lvl{};
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    if (FAILED(D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2,
            D3D11_SDK_VERSION, &sd, &m_swap, &m_dev, &lvl, &m_ctx))) {
        std::printf("[!] D3D11CreateDeviceAndSwapChain failed\n");
        return false;
    }

    ID3D11Texture2D* back{};
    m_swap->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back));
    if (back) { m_dev->CreateRenderTargetView(back, nullptr, &m_rtv); back->Release(); }
    std::printf("[*] rtv=%p\n", static_cast<void*>(m_rtv));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(m_hwnd);
    ImGui_ImplDX11_Init(m_dev, m_ctx);
    return true;
}

void Overlay::Destroy() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    ReleaseTargets();
    if (m_swap) m_swap->Release();
    if (m_ctx) m_ctx->Release();
    if (m_dev) m_dev->Release();
    if (m_hwnd) DestroyWindow(m_hwnd);
}

void Overlay::ReleaseTargets() {
    if (m_rtv) { m_rtv->Release(); m_rtv = nullptr; }
}

void Overlay::ToggleMenu() {
    m_menu = !m_menu;
    m_menuGlobal = m_menu;
    cfg.menu = m_menu;

    LONG_PTR ex = GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE);

    if (m_menu) {
        ex &= ~(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
        SetWindowLongPtrW(m_hwnd, GWL_EXSTYLE, ex);
        SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | SWP_NOACTIVATE);

        ClipCursor(nullptr);
        SetCursor(LoadCursorW(nullptr, IDC_ARROW));
        ForceForeground(m_hwnd);
        SetFocus(m_hwnd);
        ImGui::GetIO().MouseDrawCursor = false;
    } else {
        RECT cr{};
        GetClientRect(m_game, &cr);
        POINT tl{ 0, 0 };
        ClientToScreen(m_game, &tl);
        const RECT clip{ tl.x, tl.y, tl.x + cr.right, tl.y + cr.bottom };
        ClipCursor(&clip);

        ex |= (WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
        SetWindowLongPtrW(m_hwnd, GWL_EXSTYLE, ex);
        SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | SWP_NOACTIVATE);

        ForceForeground(m_game);
        SetFocus(m_game);
    }
    std::printf("[*] meniu %s\n", m_menu ? "DESCHIS" : "INCHIS");
}

void Overlay::Reposition() {
    POINT p{ 0, 0 };
    ClientToScreen(m_game, &p);
    RECT cr{};
    GetClientRect(m_game, &cr);
    if (p.x == m_pos.x && p.y == m_pos.y &&
        cr.right == m_size.x && cr.bottom == m_size.y) return;

    m_pos = p;
    m_size = { cr.right, cr.bottom };
    SetWindowPos(m_hwnd, HWND_TOPMOST, p.x, p.y, m_size.x, m_size.y, SWP_NOACTIVATE);

    if (m_swap && m_size.x > 0 && m_size.y > 0) {
        m_ctx->OMSetRenderTargets(0, nullptr, nullptr);
        ReleaseTargets();
        m_swap->ResizeBuffers(0, m_size.x, m_size.y, DXGI_FORMAT_UNKNOWN, 0);
        ID3D11Texture2D* back{};
        m_swap->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back));
        if (back) { m_dev->CreateRenderTargetView(back, nullptr, &m_rtv); back->Release(); }
    }
}

void Overlay::Frame(const RenderCallback& cb) {
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (GetAsyncKeyState(cfg.menuKey) & 1) {
        ToggleMenu();
    }

    if (m_menu) ClipCursor(nullptr);

    Reposition();
    if (!m_rtv) {
        static int n = 0;
        if (n++ % 500 == 0) std::printf("[!] fara rtv\n");
        return;
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    auto* bg = ImGui::GetBackgroundDrawList();
    bg->AddCircleFilled({ 18.0f, 18.0f }, 6.0f, IM_COL32(0, 255, 0, 255), 16);

    if (cfg.fovCircle && !m_menu)
        bg->AddCircle({ m_size.x * 0.5f, m_size.y * 0.5f }, cfg.fovRadius,
                      IM_COL32(255, 255, 255, 80), 64, 1.2f);

    if (cfg.watermark) {
        char wm[128];
        std::snprintf(wm, sizeof(wm), "kawa's menu  |  %.0f fps  |  [%s] meniu",
                      ImGui::GetIO().Framerate, KeyName(cfg.menuKey));
        const ImVec2 sz = ImGui::CalcTextSize(wm);
        const ImVec2 pos{ m_size.x - sz.x - 14.0f, 12.0f };
        bg->AddRectFilled({ pos.x - 6.0f, pos.y - 4.0f },
                          { pos.x + sz.x + 6.0f, pos.y + sz.y + 4.0f },
                          IM_COL32(0, 0, 0, 140), 4.0f);
        bg->AddText(pos, IM_COL32(200, 200, 200, 230), wm);
    }

    cb(static_cast<float>(m_size.x), static_cast<float>(m_size.y));

    ImGui::Render();
    const float clear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_ctx->ClearRenderTargetView(m_rtv, clear);
    m_ctx->OMSetRenderTargets(1, &m_rtv, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    m_swap->Present(0, 0);
}