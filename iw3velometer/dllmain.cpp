#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx9.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_stdlib.h"

#include <iostream>
#include <sstream>

#include <d3d9.h>
#include <d3dx9.h>

#include "simpleini.h"
#include <string.h>

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

#include "detours.h"
#pragma comment(lib, "detours.lib")

IMGUI_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

typedef HRESULT(__stdcall* endScene)(IDirect3DDevice9* pDevice);
endScene pEndScene;

typedef HRESULT(__stdcall* resetScene)(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* Param);
resetScene pResetScene;

WNDPROC oWndProc;

typedef IDirect3D9* (WINAPI* FND3DC9)(UINT);
FND3DC9 Direct3DCreate9_out;

std::stringstream str;

std::string INIPath;

D3DDEVICE_CREATION_PARAMETERS cparams;
RECT screenrect;

LPD3DXFONT font;

TCHAR appName[MAX_PATH];

HWND gameWindow;

struct iw3velometerConfig_s {

    bool showMaxVelocity = true;
    int fontSize = 60;

    std::string selectedFont = "Arial";

    int maxVelocityPos[2] = { 0, -110 };
    int velocityPos[2] = { 0, -50 };

    float maxVelocityColor[4] = { 1.0f, 0.5f, 0.5f, 1.0f };
    float velocityColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    int toggleKey = 0x60;
    int resetKey = 0x61;
    int guiKey = 0x4D;

    bool resetOnDeath = true;
};

iw3velometerConfig_s iw3velometerConfig;

int *keyConfig = nullptr;

int prevFontSize = iw3velometerConfig.fontSize;
std::string prevFont = iw3velometerConfig.selectedFont;

int maxvel = 0;

bool showGui = false;
bool showHud = true;
bool HudDown = false;
bool GuiDown = false;
bool isAlive = false;
bool imguiinit = false;

bool setConfig()
{
    CSimpleIniA ini;
    ini.SetUnicode();

    SI_Error rc = ini.LoadFile(INIPath.c_str());
    if (rc < 0)
        return false;

    char str[10];

    ini.SetValue("Config", "showMaxVelocity", iw3velometerConfig.showMaxVelocity ? "True" : "False");

    sprintf_s(str, "%d", iw3velometerConfig.fontSize);
    ini.SetValue("Config", "fontSize", str);

    sprintf_s(str, "%d", iw3velometerConfig.maxVelocityPos[0]);
    ini.SetValue("Config", "maxVelocityX", str);
    sprintf_s(str, "%d", iw3velometerConfig.maxVelocityPos[1]);
    ini.SetValue("Config", "maxVelocityY", str);

    sprintf_s(str, "%d", iw3velometerConfig.velocityPos[0]);
    ini.SetValue("Config", "VelocityX", str);
    sprintf_s(str, "%d", iw3velometerConfig.velocityPos[1]);
    ini.SetValue("Config", "VelocityY", str);

    sprintf_s(str, "%.1f", iw3velometerConfig.maxVelocityColor[3]);
    ini.SetValue("Config", "maxVelocityAlpha", str);
    sprintf_s(str, "%.1f", iw3velometerConfig.maxVelocityColor[0]);
    ini.SetValue("Config", "maxVelocityR", str);
    sprintf_s(str, "%.1f", iw3velometerConfig.maxVelocityColor[1]);
    ini.SetValue("Config", "maxVelocityG", str);
    sprintf_s(str, "%.1f", iw3velometerConfig.maxVelocityColor[2]);
    ini.SetValue("Config", "maxVelocityB", str);

    sprintf_s(str, "%.1f", iw3velometerConfig.velocityColor[3]);
    ini.SetValue("Config", "velocityAlpha", str);
    sprintf_s(str, "%.1f", iw3velometerConfig.velocityColor[0]);
    ini.SetValue("Config", "velocityR", str);
    sprintf_s(str, "%.1f", iw3velometerConfig.velocityColor[1]);
    ini.SetValue("Config", "velocityG", str);
    sprintf_s(str, "%.1f", iw3velometerConfig.velocityColor[2]);
    ini.SetValue("Config", "velocityB", str);

    sprintf_s(str, "0x%x", iw3velometerConfig.toggleKey);
    ini.SetValue("Config", "toggleKey", str);

    sprintf_s(str, "0x%x", iw3velometerConfig.resetKey);
    ini.SetValue("Config", "resetKey", str);

    sprintf_s(str, "0x%x", iw3velometerConfig.guiKey);
    ini.SetValue("Config", "guiKey", str);

    ini.SetValue("Config", "resetOnDeath", iw3velometerConfig.resetOnDeath ? "True" : "False");

    ini.SaveFile(INIPath.c_str());

    return true;
}

LRESULT __stdcall MessageHandler(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (Msg == WM_KEYDOWN && keyConfig != nullptr)
    {
        if (keyConfig == &iw3velometerConfig.toggleKey)
            HudDown = true;
        else if (keyConfig == &iw3velometerConfig.guiKey)
            GuiDown = true;

        *keyConfig = wParam;
        keyConfig = nullptr;
    }

    if(showGui)
        ImGui_ImplWin32_WndProcHandler(hWnd, Msg, wParam, lParam);

    return CallWindowProc(oWndProc, hWnd, Msg, wParam, lParam);
}

HRESULT __stdcall hookedEndScene(IDirect3DDevice9* pDevice) {
    pDevice->GetCreationParameters(&cparams);
    GetWindowRect(cparams.hFocusWindow, &screenrect);

    RECT veloRectangle = { 0, 0, screenrect.right - screenrect.left + iw3velometerConfig.velocityPos[0], screenrect.bottom - screenrect.top + iw3velometerConfig.velocityPos[1]};
    RECT maxRectangle = { 0, 0, screenrect.right - screenrect.left + iw3velometerConfig.maxVelocityPos[0], screenrect.bottom - screenrect.top + iw3velometerConfig.maxVelocityPos[1] };

    if (!imguiinit)
    {
        oWndProc = (WNDPROC)SetWindowLongPtr(GetForegroundWindow(), GWL_WNDPROC, (LONG_PTR)MessageHandler);
        ImGui::CreateContext();
        ImGui::GetIO().ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
        ImGui::StyleColorsDark();
        ImGui_ImplWin32_Init(GetForegroundWindow());
        ImGui_ImplDX9_Init(pDevice);
        imguiinit = true;
    }

    if (font && (iw3velometerConfig.fontSize != prevFontSize || iw3velometerConfig.selectedFont != prevFont))
    {
        font->Release();
        font = NULL;

        prevFont = iw3velometerConfig.selectedFont;
        prevFontSize = iw3velometerConfig.fontSize;
    }


    if (!font)
        D3DXCreateFont(pDevice, iw3velometerConfig.fontSize, 0, FW_REGULAR, 1, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, iw3velometerConfig.selectedFont.c_str(), &font);
        

    bool* isAliveRead{};
    float* x{}, * y{};

    if (strstr(appName, "iw3mp.exe"))
    {
        x = reinterpret_cast<float*>(0x79449C);
        y = reinterpret_cast<float*>(0x7944A0);
        isAliveRead = reinterpret_cast<bool*>(0x8C9CD7);
    }
    else
    {
        x = reinterpret_cast<float*>(0x714BD0);
        y = reinterpret_cast<float*>(0x714BD4);
        isAliveRead = reinterpret_cast<bool*>(0xC8149D);
    }

    int vel = (int)sqrt((*x * *x) + (*y * *y));

    if (vel > maxvel)
        maxvel = vel;

    str.str(std::string());
    str << "(" << maxvel << ")";

    if(iw3velometerConfig.showMaxVelocity && showHud)
        font->DrawText(NULL, str.str().c_str(), -1, &maxRectangle, DT_NOCLIP | DT_CENTER | DT_BOTTOM, D3DCOLOR_ARGB((int)(255 * iw3velometerConfig.maxVelocityColor[3]), (int)(255 * iw3velometerConfig.maxVelocityColor[0]), (int)(255 * iw3velometerConfig.maxVelocityColor[1]), (int)(255 * iw3velometerConfig.maxVelocityColor[2])));

    str.str(std::string());
    str << vel;

    if (showHud)
        font->DrawText(NULL, str.str().c_str(), -1, &veloRectangle, DT_NOCLIP | DT_CENTER | DT_BOTTOM, D3DCOLOR_ARGB((int)(255 * iw3velometerConfig.velocityColor[3]), (int)(255 * iw3velometerConfig.velocityColor[0]), (int)(255 * iw3velometerConfig.velocityColor[1]), (int)(255 * iw3velometerConfig.velocityColor[2])));

    if (GetAsyncKeyState(iw3velometerConfig.resetKey) || (iw3velometerConfig.resetOnDeath && isAlive != *isAliveRead))
    {
        maxvel = 0;
        isAlive = *isAliveRead;
    }

    bool key = GetAsyncKeyState(iw3velometerConfig.toggleKey);

    if (key && !HudDown)
    {
        showHud = !showHud;
        HudDown = true;
    }
    else if(key == 0)
        HudDown = false;

    key = GetAsyncKeyState(iw3velometerConfig.guiKey);

    if (key && !GuiDown)
    {
        showGui = !showGui;
        ImGui::GetIO().MouseDrawCursor = showGui;
        GuiDown = true;
    }
    else if (key == 0)
        GuiDown = false;

    
    if (showGui)
    {
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ImGui::SetNextWindowSize(ImVec2(500, 500), ImGuiCond_FirstUseEver);
        ImGui::Begin("IW3Velometer", &showGui);
        ImGui::Checkbox("Show max velocity", &iw3velometerConfig.showMaxVelocity);
        ImGui::DragInt("Font size", &iw3velometerConfig.fontSize);
        ImGui::InputText("Font", &iw3velometerConfig.selectedFont);
        ImGui::DragInt2("Max velocity position", iw3velometerConfig.maxVelocityPos);
        ImGui::DragInt2("Velocity position", iw3velometerConfig.velocityPos);
        ImGui::ColorEdit4("Max velocity color", iw3velometerConfig.maxVelocityColor);
        ImGui::ColorEdit4("Velocity color", iw3velometerConfig.velocityColor);

        char buffer[20];

        if(keyConfig != &iw3velometerConfig.toggleKey)
            sprintf_s(buffer, "0x%x", iw3velometerConfig.toggleKey);
        else
            sprintf_s(buffer, "Press any key");

        if (ImGui::Button(buffer, ImVec2(150, 20)))
            keyConfig = &iw3velometerConfig.toggleKey;
            

        ImGui::SameLine();
        ImGui::Text("Toggle key");

        if (keyConfig != &iw3velometerConfig.resetKey)
            sprintf_s(buffer, "0x%x", iw3velometerConfig.resetKey);
        else
            sprintf_s(buffer, "Press any key");

        if (ImGui::Button(buffer, ImVec2(150, 20)))
            keyConfig = &iw3velometerConfig.resetKey;

        ImGui::SameLine();
        ImGui::Text("Reset key");

        if (keyConfig != &iw3velometerConfig.guiKey)
            sprintf_s(buffer, "0x%x", iw3velometerConfig.guiKey);
        else
            sprintf_s(buffer, "Press any key");

        if (ImGui::Button(buffer, ImVec2(150, 20)))
            keyConfig = &iw3velometerConfig.guiKey;

        ImGui::SameLine();
        ImGui::Text("Toggle gui key");

        ImGui::Checkbox("Reset on death", &iw3velometerConfig.resetOnDeath);

        ImGui::NewLine();
        ImGui::NewLine();

        if (ImGui::Button("Save configuration", ImVec2(150, 20)))
            setConfig();

        ImGui::End();

        ImGui::EndFrame();
        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    }

    return pEndScene(pDevice);
}


HRESULT __stdcall hookedResetScene(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* Param) {

    if (font)
    {
        font->Release();
        font = NULL;
    }

    if (imguiinit)
    {
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplDX9_Init(pDevice);
    }

    return pResetScene(pDevice, Param);
}

void initHooks()
{
    IDirect3D9* pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!pD3D)
        return;

    D3DPRESENT_PARAMETERS d3dparams = { 0 };
    d3dparams.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dparams.hDeviceWindow = GetForegroundWindow();
    d3dparams.Windowed = true;

    IDirect3DDevice9* pDevice = nullptr;

    HRESULT result = pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3dparams.hDeviceWindow, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dparams, &pDevice);
    if (FAILED(result) || !pDevice) {
        pD3D->Release();
        return;
    }

    auto creationparams = D3DDEVICE_CREATION_PARAMETERS{ };
    pDevice->GetCreationParameters(&creationparams);

    void** vTable = *reinterpret_cast<void***>(pDevice);

    pEndScene = (endScene)DetourFunction((PBYTE)vTable[42], (PBYTE)hookedEndScene);
    pResetScene = (resetScene)DetourFunction((PBYTE)vTable[16], (PBYTE)hookedResetScene);

    pDevice->Release();
    pD3D->Release();
}

bool getConfig()
{
    CSimpleIniA ini;
    ini.SetUnicode();

    TCHAR Path[MAX_PATH] = { 0 };
    GetModuleFileName(NULL, Path, MAX_PATH);
    std::wstring::size_type pos = std::string(Path).find_last_of("\\/");
    INIPath = std::string(Path).substr(0, pos).append("\\iw3velometer.ini");

    SI_Error rc = ini.LoadFile(INIPath.c_str());
    if (rc < 0)
        return false;
    
    const char* pv;

    pv = ini.GetValue("Config", "showMaxVelocity", "True");
    if (strcmp(pv, "False") == 0 )
        iw3velometerConfig.showMaxVelocity = false;

    pv = ini.GetValue("Config", "fontSize", "60");
    iw3velometerConfig.fontSize = std::stoi(pv);
    pv = ini.GetValue("Config", "font", "Arial");
    iw3velometerConfig.selectedFont = pv;

    pv = ini.GetValue("Config", "maxVelocityX", "0");
    iw3velometerConfig.maxVelocityPos[0] = std::stof(pv);
    pv = ini.GetValue("Config", "maxVelocityY", "-110");
    iw3velometerConfig.maxVelocityPos[1] = std::stof(pv);

    pv = ini.GetValue("Config", "VelocityX", "0");
    iw3velometerConfig.velocityPos[0] = std::stof(pv);
    pv = ini.GetValue("Config", "VelocityY", "-50");
    iw3velometerConfig.velocityPos[1] = std::stof(pv);

    pv = ini.GetValue("Config", "maxVelocityAlpha", "1.0");
    iw3velometerConfig.maxVelocityColor[3] = std::stof(pv);
    pv = ini.GetValue("Config", "maxVelocityR", "1.0");
    iw3velometerConfig.maxVelocityColor[0] = std::stof(pv);
    pv = ini.GetValue("Config", "maxVelocityG", "0.5");
    iw3velometerConfig.maxVelocityColor[1] = std::stof(pv);
    pv = ini.GetValue("Config", "maxVelocityB", "0.5");
    iw3velometerConfig.maxVelocityColor[2] = std::stof(pv);

    pv = ini.GetValue("Config", "velocityAlpha", "1.0");
    iw3velometerConfig.velocityColor[3] = std::stof(pv);
    pv = ini.GetValue("Config", "velocityR", "1.0");
    iw3velometerConfig.velocityColor[0] = std::stof(pv);
    pv = ini.GetValue("Config", "velocityG", "1.0");
    iw3velometerConfig.velocityColor[1] = std::stof(pv);
    pv = ini.GetValue("Config", "velocityB", "1.0");
    iw3velometerConfig.velocityColor[2] = std::stof(pv);

    pv = ini.GetValue("Config", "toggleKey", "0x61");
    iw3velometerConfig.toggleKey = std::stoi(pv, nullptr, 16);
    pv = ini.GetValue("Config", "resetKey", "0x60");
    iw3velometerConfig.resetKey = std::stoi(pv, nullptr, 16);
    pv = ini.GetValue("Config", "guiKey", "0x4D");
    iw3velometerConfig.guiKey = std::stoi(pv, nullptr, 16);

    pv = ini.GetValue("Config", "resetOnDeath", "True");
    if (strcmp(pv, "False") == 0)
        iw3velometerConfig.resetOnDeath = false;

    return true;
}

bool InitializeD3D9()
{
    TCHAR szDllPath[MAX_PATH] = { 0 };

    GetSystemDirectory(szDllPath, MAX_PATH);

    lstrcat(szDllPath, "\\d3d9.dll");
    HMODULE hDll = LoadLibrary(szDllPath);

    if (hDll == NULL)
    {
        return FALSE;
    }

    Direct3DCreate9_out = (FND3DC9)GetProcAddress(hDll, "Direct3DCreate9");
    if (Direct3DCreate9_out == NULL)
    {
        FreeLibrary(hDll);
        return FALSE;
    }
}

DWORD WINAPI init() {
    if(getConfig())
        initHooks();
    else
        MessageBox(NULL, "Can't load config, IW3Velometer has been disabled.", "IW3Velometer", MB_OK | MB_ICONERROR );

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        GetModuleFileName(NULL, appName, MAX_PATH);

        if (strstr(appName, "iw3mp.exe") == NULL && strstr(appName, "iw3sp.exe") == NULL)
        {
            MessageBox(NULL, "IW3Velometer cannot be loaded by this application", "IW3Velometer", MB_OK | MB_ICONERROR);
            break;
        }

        InitializeD3D9();
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)init, NULL, 0, NULL);
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

IDirect3D9* WINAPI Direct3DCreate9(UINT SDKVersion) {
    return Direct3DCreate9_out(SDKVersion);
}