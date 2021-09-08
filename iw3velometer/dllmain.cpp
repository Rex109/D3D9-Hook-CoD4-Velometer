#include "pch.h"

#include <iostream>
#include <sstream>

#include <d3d9.h>
#include <d3dx9.h>

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

std::ostringstream str;

HANDLE process;

D3DDEVICE_CREATION_PARAMETERS cparams;
RECT screenrect;

LPD3DXFONT font;

int maxvel = 0;

HRESULT __stdcall hookedEndScene(IDirect3DDevice9* pDevice) {
    pDevice->GetCreationParameters(&cparams);
    GetWindowRect(cparams.hFocusWindow, &screenrect);

    RECT veloRectangle = { 0, 0, screenrect.right - screenrect.left, screenrect.bottom - screenrect.top - 50 };
    RECT maxRectangle = { 0, 0, screenrect.right - screenrect.left, screenrect.bottom - screenrect.top - 110 };

    if (!font)
        D3DXCreateFont(pDevice, 60, 0, FW_REGULAR, 1, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial", &font);

    float x, y;
    ReadProcessMemory(process, reinterpret_cast<PVOID>(0x79449C), &x, sizeof(x), nullptr);
    ReadProcessMemory(process, reinterpret_cast<PVOID>(0x7944A0), &y, sizeof(y), nullptr);
    int vel = (int)sqrt((x * x) + (y * y));

    if (vel > maxvel)
        maxvel = vel;

    str.str(std::string());
    str << "(" << maxvel << ")";

    font->DrawText(NULL, str.str().c_str(), -1, &maxRectangle, DT_NOCLIP | DT_CENTER | DT_BOTTOM, D3DCOLOR_ARGB(255, 255, 128, 128));

    str.str(std::string());
    str << vel;

    font->DrawText(NULL, str.str().c_str(), -1, &veloRectangle, DT_NOCLIP | DT_CENTER | DT_BOTTOM, D3DCOLOR_ARGB(255, 255, 255, 255));

    if (GetAsyncKeyState(VK_NUMPAD0))
        maxvel = 0;

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
    process = GetCurrentProcess();

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

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        InitializeD3D9();
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)initHooks, NULL, 0, NULL);
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