#include "about_dialog.h"
#include "version.h"

#include "imgui.h"

#include <Windows.h>
#include <shellapi.h>
#include <d3d11.h>
#include <gdiplus.h>
#include <filesystem>

// Access the global DX11 device from app_window.cpp
extern ID3D11Device* g_pd3dDevice;

namespace asteria::ui {

namespace {

bool g_aboutOpen = false;
bool g_iconLoaded = false;
bool g_iconLoadAttempted = false;
ID3D11ShaderResourceView* g_iconSRV = nullptr;
int g_iconWidth = 0;
int g_iconHeight = 0;

void loadIconTexture() {
    if (g_iconLoadAttempted) return;
    g_iconLoadAttempted = true;

    if (!g_pd3dDevice) return;

    namespace fs = std::filesystem;
    fs::path iconPath = fs::current_path() / "asteria_icon.png";
    if (!fs::exists(iconPath)) return;

    // Use GDI+ to load PNG
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    if (Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr) != Gdiplus::Ok)
        return;

    {
        std::wstring wpath = iconPath.wstring();
        Gdiplus::Bitmap bitmap(wpath.c_str());
        if (bitmap.GetLastStatus() != Gdiplus::Ok) {
            Gdiplus::GdiplusShutdown(gdiplusToken);
            return;
        }

        g_iconWidth = static_cast<int>(bitmap.GetWidth());
        g_iconHeight = static_cast<int>(bitmap.GetHeight());

        Gdiplus::BitmapData bmpData;
        Gdiplus::Rect rect(0, 0, g_iconWidth, g_iconHeight);
        if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bmpData) != Gdiplus::Ok) {
            Gdiplus::GdiplusShutdown(gdiplusToken);
            return;
        }

        // GDI+ gives BGRA, DX11 wants RGBA — swap channels
        std::vector<unsigned char> rgba(static_cast<size_t>(g_iconWidth) * g_iconHeight * 4);
        for (int y = 0; y < g_iconHeight; ++y) {
            const auto* src = reinterpret_cast<const unsigned char*>(
                static_cast<const char*>(bmpData.Scan0) + y * bmpData.Stride);
            auto* dst = rgba.data() + y * g_iconWidth * 4;
            for (int x = 0; x < g_iconWidth; ++x) {
                dst[x * 4 + 0] = src[x * 4 + 2]; // R
                dst[x * 4 + 1] = src[x * 4 + 1]; // G
                dst[x * 4 + 2] = src[x * 4 + 0]; // B
                dst[x * 4 + 3] = src[x * 4 + 3]; // A
            }
        }
        bitmap.UnlockBits(&bmpData);

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = static_cast<UINT>(g_iconWidth);
        desc.Height = static_cast<UINT>(g_iconHeight);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA initData{};
        initData.pSysMem = rgba.data();
        initData.SysMemPitch = static_cast<UINT>(g_iconWidth * 4);

        ID3D11Texture2D* texture = nullptr;
        if (SUCCEEDED(g_pd3dDevice->CreateTexture2D(&desc, &initData, &texture))) {
            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
            srvDesc.Format = desc.Format;
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1;
            if (SUCCEEDED(g_pd3dDevice->CreateShaderResourceView(texture, &srvDesc, &g_iconSRV))) {
                g_iconLoaded = true;
            }
            texture->Release();
        }
    }

    Gdiplus::GdiplusShutdown(gdiplusToken);
}

}  // namespace

void openAboutDialog() {
    g_aboutOpen = true;
}

void drawAboutDialog() {
    if (!g_aboutOpen) return;

    loadIconTexture();

    ImGui::OpenPopup("About Asteria");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("About Asteria", &g_aboutOpen,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        // Icon
        if (g_iconLoaded && g_iconSRV) {
            float iconDisplaySize = 96.0f;
            float avail = ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX((avail - iconDisplaySize) * 0.5f + ImGui::GetCursorPosX());
            ImGui::Image(reinterpret_cast<ImTextureID>(g_iconSRV),
                         ImVec2(iconDisplaySize, iconDisplaySize));
        }

        // Title
        {
            const char* title = "Asteria";
            float textW = ImGui::CalcTextSize(title).x;
            float avail = ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX((avail - textW) * 0.5f + ImGui::GetCursorPosX());
            ImGui::TextUnformatted(title);
        }

        // Version
        {
            const char* ver = "Version " ASTERIA_VERSION_STRING;
            float textW = ImGui::CalcTextSize(ver).x;
            float avail = ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX((avail - textW) * 0.5f + ImGui::GetCursorPosX());
            ImGui::TextDisabled("%s", ver);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Description
        ImGui::TextWrapped("A native Windows astrology application for power astrologers. "
                           "Built with the embedded Astrolog computation engine and Swiss Ephemeris precision.");

        ImGui::Spacing();

        // Copyright & license
        ImGui::TextWrapped("%s", ASTERIA_COPYRIGHT);
        ImGui::TextWrapped("Licensed under the %s", ASTERIA_LICENSE);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Links
        if (ImGui::Button("GitHub: thetheosopher/Asteria")) {
            ShellExecuteA(nullptr, "open", ASTERIA_GITHUB_URL, nullptr, nullptr, SW_SHOWNORMAL);
        }
        if (ImGui::Button("Buy Me a Coffee")) {
            ShellExecuteA(nullptr, "open", ASTERIA_COFFEE_URL, nullptr, nullptr, SW_SHOWNORMAL);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Credits
        ImGui::TextDisabled("Third-Party Components:");
        ImGui::BulletText("Astrolog 7.80 by Walter D. Pullen (GPL v2)");
        ImGui::BulletText("Swiss Ephemeris by Astrodienst AG");
        ImGui::BulletText("Dear ImGui by Omar Cornut (MIT)");
        ImGui::BulletText("SQLite (Public Domain)");

        ImGui::Spacing();

        float buttonW = 120.0f;
        float avail = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX((avail - buttonW) * 0.5f + ImGui::GetCursorPosX());
        if (ImGui::Button("Close", ImVec2(buttonW, 0))) {
            g_aboutOpen = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

}  // namespace asteria::ui
