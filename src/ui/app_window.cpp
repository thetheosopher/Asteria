#include "app_window.h"
#include "app_context.h"

#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <tchar.h>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include "library_panel.h"
#include "chart_workspace_panel.h"
#include "compare_workspace_panel.h"
#include "settings_panel.h"

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

int runApplication(data::SQLiteDatabase& database, engine::IChartEngine& engine) {
  // Create shared application context
  AppContext ctx(database, engine);

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

  // Load fonts: default + Segoe UI Symbol for astrological glyphs
  {
    // Primary UI font (Segoe UI, or fall back to built-in)
    const char* segoeUI = "C:\\Windows\\Fonts\\segoeui.ttf";
    if (GetFileAttributesA(segoeUI) != INVALID_FILE_ATTRIBUTES) {
      io.Fonts->AddFontFromFileTTF(segoeUI, 16.0f);
    } else {
      io.Fonts->AddFontDefault();
    }

    // Merge Segoe UI Symbol for astrological Unicode glyphs
    const char* segoeSymbol = "C:\\Windows\\Fonts\\seguisym.ttf";
    if (GetFileAttributesA(segoeSymbol) != INVALID_FILE_ATTRIBUTES) {
      ImFontConfig mergeConfig;
      mergeConfig.MergeMode = true;
      mergeConfig.PixelSnapH = true;
      // Zodiac signs U+2648..U+2653, misc symbols U+2600..U+26FF, geometric U+25A0..U+25FF
      static const ImWchar glyphRanges[] = {
        0x2500, 0x26FF,  // Box drawing through Misc Symbols (planets, signs, aspects)
        0x2700, 0x27BF,  // Dingbats
        0x00B0, 0x00B0,  // Degree sign °
        0,
      };
      io.Fonts->AddFontFromFileTTF(segoeSymbol, 16.0f, &mergeConfig, glyphRanges);
    }

    // Larger font for chart glyph rendering
    const char* glyphFontPath = (GetFileAttributesA(segoeSymbol) != INVALID_FILE_ATTRIBUTES)
        ? segoeSymbol : segoeUI;
    if (GetFileAttributesA(glyphFontPath) != INVALID_FILE_ATTRIBUTES) {
      // This will be font index 1 in the font atlas
      ImFontConfig cfg;
      cfg.SizePixels = 22.0f;
      io.Fonts->AddFontFromFileTTF(glyphFontPath, 22.0f, &cfg);
      // Merge symbol glyphs into the large font too
      if (GetFileAttributesA(segoeSymbol) != INVALID_FILE_ATTRIBUTES) {
        ImFontConfig mergeLarge;
        mergeLarge.MergeMode = true;
        mergeLarge.PixelSnapH = true;
        static const ImWchar glyphRangesLarge[] = {
          0x2500, 0x26FF,
          0x2700, 0x27BF,
          0x00B0, 0x00B0,
          0,
        };
        io.Fonts->AddFontFromFileTTF(segoeSymbol, 22.0f, &mergeLarge, glyphRangesLarge);
      }
    }

    io.Fonts->Build();
  }

  // Workspace panels
  LibraryPanel libraryPanel(ctx);
  ChartWorkspacePanel chartPanel(ctx);
  CompareWorkspacePanel comparePanel(ctx);
  SettingsPanel settingsPanel(ctx);

  const ImVec4 clearColor(0.10f, 0.10f, 0.12f, 1.00f);

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
          ImGui::MenuItem("Library",   nullptr, &libraryPanel.open);
          ImGui::MenuItem("Chart",     nullptr, &chartPanel.open);
          ImGui::MenuItem("Compare",   nullptr, &comparePanel.open);
          ImGui::MenuItem("Settings",  nullptr, &settingsPanel.open);
          ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
      }

      ImGuiID dockspaceId = ImGui::GetID("AsteriaDockspace");
      ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
      ImGui::End();
    }

    // Draw workspace panels
    libraryPanel.draw();
    chartPanel.setSelectedPerson(libraryPanel.selectedPersonId());
    chartPanel.draw();
    comparePanel.draw();
    settingsPanel.draw();

    // Render
    ImGui::Render();
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
