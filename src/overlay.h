#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <functional>

using RenderCallback = std::function<void(float w, float h)>;

class Overlay {
public:
    bool Create(DWORD pid);
    void Destroy();
    void Frame(const RenderCallback& cb);
    void ToggleMenu();
    bool menuOpen() const { return m_menu; }
    HWND gameHwnd() const { return m_game; }

private:
    void Reposition();
    void ReleaseTargets();

    HWND m_game{};
    HWND m_hwnd{};
    ID3D11Device* m_dev{};
    ID3D11DeviceContext* m_ctx{};
    IDXGISwapChain* m_swap{};
    ID3D11RenderTargetView* m_rtv{};
    POINT m_pos{};
    POINT m_size{};
    bool m_menu{ false };
};