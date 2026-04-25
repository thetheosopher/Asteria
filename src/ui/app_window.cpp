#include "app_window.h"
#include "app_context.h"

#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <tchar.h>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include "library_panel.h"
#include "chart_workspace_panel.h"
#include "transit_timeline_panel.h"
#include "compare_workspace_panel.h"
#include "settings_panel.h"
#include "ai_interpretation_panel.h"
#include "astrology_font.h"
#include "util/atlas_service.h"
#include "render/theme_presets.h"

#include <filesystem>

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {

// Direct3D globals
ID3D11Device*           g_pd3dDevice        = nullptr;
ID3D11DeviceContext*    g_pd3dDeviceContext  = nullptr;
IDXGISwapChain*         g_pSwapChain        = nullptr;
ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

bool CreateDeviceD3D(HWND hWnd) {
  DXGI_SWAP_CHAIN_DESC sd{};
  sd.BufferCount        = 2;
  sd.BufferDesc.Width   = 0;
  sd.BufferDesc.Height  = 0;
  sd.BufferDesc.Format  = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferDesc.RefreshRate.Numerator   = 60;
  sd.BufferDesc.RefreshRate.Denominator = 1;
  sd.Flags              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
  sd.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow       = hWnd;
  sd.SampleDesc.Count   = 1;
  sd.SampleDesc.Quality = 0;
  sd.Windowed           = TRUE;
  sd.SwapEffect         = DXGI_SWAP_EFFECT_DISCARD;

  const D3D_FEATURE_LEVEL featureLevels[] = {
      D3D_FEATURE_LEVEL_11_0,
      D3D_FEATURE_LEVEL_10_0,
  };
  D3D_FEATURE_LEVEL featureLevel;
  HRESULT hr = D3D11CreateDeviceAndSwapChain(
      nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
      featureLevels, 2, D3D11_SDK_VERSION,
      &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
  if (hr == DXGI_ERROR_UNSUPPORTED)
    hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
        featureLevels, 2, D3D11_SDK_VERSION,
        &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
  if (FAILED(hr)) return false;
  CreateRenderTarget();
  return true;
}

void CleanupDeviceD3D() {
  CleanupRenderTarget();
  if (g_pSwapChain)        { g_pSwapChain->Release();        g_pSwapChain = nullptr; }
  if (g_pd3dDeviceContext)  { g_pd3dDeviceContext->Release();  g_pd3dDeviceContext = nullptr; }
  if (g_pd3dDevice)         { g_pd3dDevice->Release();         g_pd3dDevice = nullptr; }
}

void CreateRenderTarget() {
  ID3D11Texture2D* pBackBuffer = nullptr;
  g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
  if (pBackBuffer) {
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
  }
}

void CleanupRenderTarget() {
  if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
    return true;

  switch (msg) {
    case WM_SIZE:
      if (wParam == SIZE_MINIMIZED) return 0;
      if (g_pd3dDevice) {
        CleanupRenderTarget();
        g_pSwapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam),
                                    DXGI_FORMAT_UNKNOWN, 0);
        CreateRenderTarget();
      }
      return 0;
    case WM_SYSCOMMAND:
      if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
      break;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(hWnd, msg, wParam, lParam);
}

}  // namespace

namespace asteria::ui {

namespace {

int parseSettingIndex(const asteria::ui::AppContext& ctx, const char* key, int fallback, int maxExclusive) {
  int value = fallback;
  try { value = std::stoi(ctx.getSetting(key, std::to_string(fallback))); }
  catch (...) { value = fallback; }
  if (value < 0 || value >= maxExclusive) return fallback;
  return value;
}

ImVec4 colorVec(const asteria::render::Color& color, float alpha = 1.0f) {
  return ImVec4(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, alpha);
}

ImVec4 mixColor(const ImVec4& a, const ImVec4& b, float weightToB, float alpha = 1.0f) {
  return ImVec4(a.x + (b.x - a.x) * weightToB,
                a.y + (b.y - a.y) * weightToB,
                a.z + (b.z - a.z) * weightToB,
                alpha);
}

float luminance(const ImVec4& color) {
  return color.x * 0.2126f + color.y * 0.7152f + color.z * 0.0722f;
}

void applyApplicationTheme(const asteria::render::ThemePreset& theme) {
  ImGuiStyle& style = ImGui::GetStyle();
  ImVec4* colors = style.Colors;

  const ImVec4 background = colorVec(theme.background);
  const ImVec4 primary = colorVec(theme.primary);
  const ImVec4 secondary = colorVec(theme.secondary);
  const ImVec4 accent = colorVec(theme.accent);
  const ImVec4 text = colorVec(theme.text);
  const bool isDark = luminance(background) < 0.45f;
  const ImVec4 contrast = isDark ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : ImVec4(0.0f, 0.0f, 0.0f, 1.0f);

  colors[ImGuiCol_Text] = text;
  colors[ImGuiCol_TextDisabled] = mixColor(text, background, 0.45f, 1.0f);
  colors[ImGuiCol_WindowBg] = background;
  colors[ImGuiCol_ChildBg] = mixColor(background, contrast, isDark ? 0.05f : 0.03f, 1.0f);
  colors[ImGuiCol_PopupBg] = mixColor(background, contrast, isDark ? 0.08f : 0.05f, 0.98f);
  colors[ImGuiCol_Border] = colorVec(theme.secondary, 0.55f);
  colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
  colors[ImGuiCol_FrameBg] = mixColor(background, contrast, isDark ? 0.10f : 0.08f, 1.0f);
  colors[ImGuiCol_FrameBgHovered] = mixColor(accent, background, 0.72f, 1.0f);
  colors[ImGuiCol_FrameBgActive] = mixColor(accent, background, 0.58f, 1.0f);
  colors[ImGuiCol_TitleBg] = mixColor(background, primary, isDark ? 0.28f : 0.14f, 1.0f);
  colors[ImGuiCol_TitleBgActive] = mixColor(accent, background, 0.58f, 1.0f);
  colors[ImGuiCol_TitleBgCollapsed] = colors[ImGuiCol_TitleBg];
  colors[ImGuiCol_MenuBarBg] = mixColor(background, primary, isDark ? 0.18f : 0.08f, 1.0f);
  colors[ImGuiCol_ScrollbarBg] = mixColor(background, contrast, isDark ? 0.04f : 0.02f, 1.0f);
  colors[ImGuiCol_ScrollbarGrab] = mixColor(secondary, background, 0.35f, 1.0f);
  colors[ImGuiCol_ScrollbarGrabHovered] = mixColor(accent, background, 0.45f, 1.0f);
  colors[ImGuiCol_ScrollbarGrabActive] = accent;
  colors[ImGuiCol_CheckMark] = accent;
  colors[ImGuiCol_SliderGrab] = accent;
  colors[ImGuiCol_SliderGrabActive] = mixColor(accent, contrast, isDark ? 0.18f : 0.12f, 1.0f);
  colors[ImGuiCol_Button] = mixColor(accent, background, 0.55f, 1.0f);
  colors[ImGuiCol_ButtonHovered] = mixColor(accent, contrast, isDark ? 0.10f : 0.06f, 1.0f);
  colors[ImGuiCol_ButtonActive] = accent;
  colors[ImGuiCol_Header] = mixColor(primary, background, 0.52f, 1.0f);
  colors[ImGuiCol_HeaderHovered] = mixColor(accent, background, 0.52f, 1.0f);
  colors[ImGuiCol_HeaderActive] = accent;
  colors[ImGuiCol_Separator] = colorVec(theme.secondary, 0.60f);
  colors[ImGuiCol_SeparatorHovered] = accent;
  colors[ImGuiCol_SeparatorActive] = accent;
  colors[ImGuiCol_ResizeGrip] = colorVec(theme.secondary, 0.45f);
  colors[ImGuiCol_ResizeGripHovered] = colorVec(theme.accent, 0.75f);
  colors[ImGuiCol_ResizeGripActive] = accent;
  colors[ImGuiCol_Tab] = mixColor(primary, background, 0.62f, 1.0f);
  colors[ImGuiCol_TabHovered] = mixColor(accent, background, 0.55f, 1.0f);
  colors[ImGuiCol_TabSelected] = mixColor(accent, background, 0.42f, 1.0f);
  colors[ImGuiCol_TabDimmed] = mixColor(primary, background, 0.78f, 1.0f);
  colors[ImGuiCol_TabDimmedSelected] = mixColor(accent, background, 0.62f, 1.0f);
}

bool ollamaEnabled(const AppContext& ctx) {
  return ctx.getSetting("ollama_enabled", "0") == "1";
}

void applyDefaultDockLayout(ImGuiID dockspaceId,
                            const ImVec2& size,
                            bool showAiInterpretation,
                            LibraryPanel& libraryPanel,
                            ChartWorkspacePanel& chartPanel,
                            TransitTimelinePanel& transitTimelinePanel,
                            CompareWorkspacePanel& comparePanel,
                            AiInterpretationPanel& aiPanel) {
  libraryPanel.open = true;
  chartPanel.open = true;
  transitTimelinePanel.open = true;
  comparePanel.open = true;
  aiPanel.open = showAiInterpretation;

  ImGui::DockBuilderRemoveNode(dockspaceId);
  ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(dockspaceId, size);

  ImGuiID leftDockId = 0;
  ImGuiID rightDockId = 0;
  ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Left, 0.25f, &leftDockId, &rightDockId);

  ImGui::DockBuilderDockWindow("Library", leftDockId);
  ImGui::DockBuilderDockWindow("Chart Workspace", rightDockId);
  ImGui::DockBuilderDockWindow("Transit Timeline", rightDockId);
  ImGui::DockBuilderDockWindow("Compare Workspace", rightDockId);
  ImGui::DockBuilderDockWindow("AI Interpretation", rightDockId);

  ImGui::DockBuilderFinish(dockspaceId);
}

}  // namespace

int runApplication(data::SQLiteDatabase& database, engine::IChartEngine& engine,
                   util::AtlasService& atlas) {
  // Create shared application context
  AppContext ctx(database, engine);
  ctx.atlasService = std::move(atlas);

  // Register window class
  WNDCLASSEXW wc{};
  wc.cbSize        = sizeof(WNDCLASSEXW);
  wc.style         = CS_CLASSDC;
  wc.lpfnWndProc   = WndProc;
  wc.hInstance      = GetModuleHandleW(nullptr);
  wc.lpszClassName  = L"AsteriaWindow";
  RegisterClassExW(&wc);

  HWND hwnd = CreateWindowExW(
      0, wc.lpszClassName, L"Asteria",
      WS_OVERLAPPEDWINDOW,
      100, 100, 1600, 1000,
      nullptr, nullptr, wc.hInstance, nullptr);

  if (!CreateDeviceD3D(hwnd)) {
    CleanupDeviceD3D();
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 1;
  }

  ShowWindow(hwnd, SW_SHOWDEFAULT);
  UpdateWindow(hwnd);

  // ImGui setup
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  ImGui::StyleColorsDark();
  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowRounding   = 4.0f;
  style.FrameRounding    = 3.0f;
  style.GrabRounding     = 3.0f;
  style.TabRounding      = 4.0f;
  style.FramePadding     = ImVec2(8, 4);
  style.ItemSpacing      = ImVec2(8, 6);

  ImGui_ImplWin32_Init(hwnd);
  ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

  // Load fonts: primary UI font plus Astro.ttf for app-wide astrology glyphs
  // when it is bundled next to the executable.
  {
    astrology_font::load(io, std::filesystem::current_path());
  }

  // Workspace panels
  LibraryPanel libraryPanel(ctx);
  ChartWorkspacePanel chartPanel(ctx);
  TransitTimelinePanel transitTimelinePanel(ctx);
  CompareWorkspacePanel comparePanel(ctx);
  SettingsPanel settingsPanel(ctx);
  AiInterpretationPanel aiPanel(ctx);
  chartPanel.setAiPanel(&aiPanel);
  comparePanel.setAiPanel(&aiPanel);

  bool resetToDefaultLayout = true;
  int appliedThemeIndex = -1;

  // Main loop
  bool running = true;
  while (running) {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
      if (msg.message == WM_QUIT) running = false;
    }
    if (!running) break;

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    settingsPanel.ensureLoaded();
    const int currentThemeIndex = parseSettingIndex(ctx, "default_theme", 0, 4);
    if (currentThemeIndex != appliedThemeIndex) {
      applyApplicationTheme(render::themePresetByIndex(currentThemeIndex));
      appliedThemeIndex = currentThemeIndex;
    }

    // Full-window dockspace
    {
      const ImGuiViewport* viewport = ImGui::GetMainViewport();
      ImGui::SetNextWindowPos(viewport->WorkPos);
      ImGui::SetNextWindowSize(viewport->WorkSize);
      ImGui::SetNextWindowViewport(viewport->ID);
      ImGuiWindowFlags hostFlags =
          ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
          ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
          ImGuiWindowFlags_NoBringToFrontOnFocus |
          ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoDocking |
          ImGuiWindowFlags_MenuBar;
      ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
      ImGui::Begin("DockSpaceHost", nullptr, hostFlags);
      ImGui::PopStyleVar(3);

      // Menu bar
      if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
          if (ImGui::MenuItem("Exit", "Alt+F4")) running = false;
          ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
          if (ImGui::BeginMenu("Panels")) {
            ImGui::MenuItem("Library",   nullptr, &libraryPanel.open);
            ImGui::MenuItem("Chart",     nullptr, &chartPanel.open);
            ImGui::MenuItem("Transit Timeline", nullptr, &transitTimelinePanel.open);
            ImGui::MenuItem("Compare",   nullptr, &comparePanel.open);
            ImGui::MenuItem("AI Interpretation", nullptr, &aiPanel.open);
            ImGui::EndMenu();
          }

          settingsPanel.drawViewMenuContents();

          ImGui::Separator();
          if (ImGui::MenuItem("Restore Default Layout")) {
            resetToDefaultLayout = true;
          }
          ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
      }

      ImGuiID dockspaceId = ImGui::GetID("AsteriaDockspace");
      ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
      if (resetToDefaultLayout) {
        applyDefaultDockLayout(dockspaceId,
                               viewport->WorkSize,
                               settingsPanel.isOllamaEnabled(),
                               libraryPanel,
                               chartPanel,
                               transitTimelinePanel,
                               comparePanel,
                               aiPanel);
        resetToDefaultLayout = false;
      }
      ImGui::End();
    }

    // Draw workspace panels
    libraryPanel.draw();
    chartPanel.setSelectedPerson(libraryPanel.selectedPersonId());
    transitTimelinePanel.setSelectedPerson(libraryPanel.selectedPersonId());
    chartPanel.draw();
    transitTimelinePanel.draw();
    comparePanel.draw();
    aiPanel.draw();

    // Render
    ImGui::Render();
    const ImVec4 clearColor = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
    const float clearColorArray[4] = {
        clearColor.x * clearColor.w,
        clearColor.y * clearColor.w,
        clearColor.z * clearColor.w,
        clearColor.w};
    g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
    g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clearColorArray);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_pSwapChain->Present(1, 0);  // VSync
  }

  // Cleanup
  ImGui_ImplDX11_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();

  CleanupDeviceD3D();
  DestroyWindow(hwnd);
  UnregisterClassW(wc.lpszClassName, wc.hInstance);
  return 0;
}

}  // namespace asteria::ui
