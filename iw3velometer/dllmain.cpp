#include "pch.h"

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

typedef HRESULT(__stdcall* endScene)(IDirect3DDevice9* pDevice);
endScene pEndScene;

typedef HRESULT(__stdcall* resetScene)(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* Param);
resetScene pResetScene;

typedef IDirect3D9* (WINAPI* FND3DC9)(UINT);
FND3DC9 Direct3DCreate9_out;

std::stringstream str;

HANDLE process;

D3DDEVICE_CREATION_PARAMETERS cparams;
RECT screenrect;

LPD3DXFONT font;

bool showMaxVelocity = true;
int fontSize = 60;
float maxVelocityX = 0;
float maxVelocityY = -110;
float velocityX = 0;
float velocityY = -50;

unsigned char maxVelocityAlpha = 255;
unsigned char maxVelocityR = 255;
unsigned char maxVelocityG = 128;
unsigned char maxVelocityB = 128;

unsigned char velocityAlpha = 255;
unsigned char velocityR = 255;
unsigned char velocityG = 255;
unsigned char velocityB = 255;

std::string selectedFont = "Arial";

int toggleKey = 0x60;
int resetKey = 0x61;

int maxvel = 0;

bool showHud = true;
bool resetDown = false;

HRESULT __stdcall hookedEndScene(IDirect3DDevice9* pDevice) {
    pDevice->GetCreationParameters(&cparams);
    GetWindowRect(cparams.hFocusWindow, &screenrect);

    RECT veloRectangle = { 0, 0, screenrect.right - screenrect.left + velocityX, screenrect.bottom - screenrect.top + velocityY };
    RECT maxRectangle = { 0, 0, screenrect.right - screenrect.left + maxVelocityX, screenrect.bottom - screenrect.top + maxVelocityY };

    if (!font)
        D3DXCreateFont(pDevice, fontSize, 0, FW_REGULAR, 1, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, selectedFont.c_str(), &font);

    float x, y;
    ReadProcessMemory(process, reinterpret_cast<PVOID>(0x79449C), &x, sizeof(x), nullptr);
    ReadProcessMemory(process, reinterpret_cast<PVOID>(0x7944A0), &y, sizeof(y), nullptr);
    int vel = (int)sqrt((x * x) + (y * y));

    if (vel > maxvel)
        maxvel = vel;

    str.str(std::string());
    str << "(" << maxvel << ")";

    if(showMaxVelocity && showHud)
        font->DrawText(NULL, str.str().c_str(), -1, &maxRectangle, DT_NOCLIP | DT_CENTER | DT_BOTTOM, D3DCOLOR_ARGB(maxVelocityAlpha, maxVelocityR, maxVelocityG, maxVelocityB));

    str.str(std::string());
    str << vel;

    if (showHud)
        font->DrawText(NULL, str.str().c_str(), -1, &veloRectangle, DT_NOCLIP | DT_CENTER | DT_BOTTOM, D3DCOLOR_ARGB(velocityAlpha, velocityR, velocityG, velocityB));

    if (GetAsyncKeyState(resetKey))
        maxvel = 0;

    bool key = GetAsyncKeyState(toggleKey);

    if (key && !resetDown)
    {
        showHud = !showHud;
        resetDown = true;
    }
    else if(key == 0)
        resetDown = false;

    return pEndScene(pDevice);
}

HRESULT __stdcall hookedResetScene(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* Param) {

    if (font)
    {
        font->Release();
        font = NULL;
    }

    return pResetScene(pDevice, Param);
}

void initHooks() {
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

    void** vTable = *reinterpret_cast<void***>(pDevice);

    pEndScene = (endScene)DetourFunction((PBYTE)vTable[42], (PBYTE)hookedEndScene);
    pResetScene = (resetScene)DetourFunction((PBYTE)vTable[16], (PBYTE)hookedResetScene);

    pDevice->Release();
    pD3D->Release();
}

bool initConfig()
{
    CSimpleIniA ini;
    ini.SetUnicode();

    SI_Error rc = ini.LoadFile("iw3velometer.ini");
    if (rc < 0)
        return false;
    
    const char* pv;

    pv = ini.GetValue("Config", "showMaxVelocity", "True");
    if (strcmp(pv, "False") == 0 )
        showMaxVelocity = false;

    pv = ini.GetValue("Config", "fontSize", "60");
    fontSize = std::stoi(pv);
    pv = ini.GetValue("Config", "font", "Arial");
    selectedFont = pv;

    pv = ini.GetValue("Config", "maxVelocityX", "0");
    maxVelocityX = std::stof(pv);
    pv = ini.GetValue("Config", "maxVelocityY", "-110");
    maxVelocityY = std::stof(pv);

    pv = ini.GetValue("Config", "VelocityY", "0");
    velocityY = std::stof(pv);
    pv = ini.GetValue("Config", "VelocityX", "-50");
    velocityX = std::stof(pv);

    pv = ini.GetValue("Config", "maxVelocityAlpha", "255");
    maxVelocityAlpha = std::stof(pv);
    pv = ini.GetValue("Config", "maxVelocityR", "255");
    maxVelocityR = std::stof(pv);
    pv = ini.GetValue("Config", "maxVelocityG", "128");
    maxVelocityG = std::stof(pv);
    pv = ini.GetValue("Config", "maxVelocityB", "128");
    maxVelocityB = std::stof(pv);

    pv = ini.GetValue("Config", "velocityAlpha", "255");
    velocityAlpha = std::stof(pv);
    pv = ini.GetValue("Config", "velocityR", "255");
    velocityR = std::stof(pv);
    pv = ini.GetValue("Config", "velocityG", "128");
    velocityG = std::stof(pv);
    pv = ini.GetValue("Config", "velocityB", "128");
    velocityB = std::stof(pv);

    pv = ini.GetValue("Config", "toggleKey", "0x61");
    toggleKey = std::stoi(pv, nullptr, 16);
    pv = ini.GetValue("Config", "resetKey", "0x60");
    resetKey = std::stoi(pv, nullptr, 16);

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
    process = GetCurrentProcess();

    if(initConfig())
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