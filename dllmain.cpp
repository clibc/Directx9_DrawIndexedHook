#include <iostream>
#include <assert.h>
#include <Windows.h>
#include <d3d9.h>
#include <gl/GL.h>

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "opengl32.lib")

typedef long long int s64;

typedef BOOL (__stdcall * SWAPBUFFERS)(HDC);
typedef HRESULT (WINAPI * ENDSCENE)(IDirect3DDevice9*);
typedef HRESULT (WINAPI * DRAWINDEXEDPRIMITIVE)(IDirect3DDevice9* Device,
                                                D3DPRIMITIVETYPE unnamedParam1,
                                                INT BaseVertexIndex,
                                                UINT MinVertexIndex,
                                                UINT NumVertices,
                                                UINT startIndex,
                                                UINT primCount);
SWAPBUFFERS OriginalSwapBuffers;
ENDSCENE OldEndScene;
DRAWINDEXEDPRIMITIVE OldDrawIndexedPrimitive;

static void 
SpawnConsole()
{
    AllocConsole();
    FILE* Dummy;
    freopen_s(&Dummy, "CONOUT$", "w", stdout);
}

static IDirect3DDevice9* GameDevice = NULL;
static bool ConsoleSpawned = false;

BOOL __stdcall
SwapBuffersReplace(HDC Context)
{
    MessageBox(NULL, NULL, L"Rendering", MB_OK);
    return OriginalSwapBuffers(Context);
}

HRESULT WINAPI
NewDrawIndexedPrimitive(IDirect3DDevice9* Device,
                        D3DPRIMITIVETYPE unnamedParam1,
                        INT BaseVertexIndex,
                        UINT MinVertexIndex,
                        UINT NumVertices,
                        UINT startIndex,
                        UINT primCount)
{
    if(!ConsoleSpawned)
    {
        SpawnConsole();
        ConsoleSpawned = true;
    }
    else
    {
#if 0
        printf("Primitive type:%i, BaseVertexIndex:%i, MinVertexIndex:%i, NumVertices:%i, StartIndex:%i, PrimCount:%i\n",
               unnamedParam1, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
#endif
        Device->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
    }

    GameDevice = Device;
    return OldDrawIndexedPrimitive(Device, unnamedParam1, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
}

static void*
TrampolineHook(void* Source, void* Dest, int Length)
{
    assert(Length >= 5);
    DWORD OldProtection;
    VirtualProtect(Source, (size_t)Length, PAGE_EXECUTE_READWRITE, &OldProtection);
    
    BYTE* Gateway = (BYTE*)VirtualAlloc(0, (size_t)Length, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    memcpy(Gateway, Source, (size_t)Length);
    *(Gateway + Length) = 0xE9;
    int GDiff = (int)(((int)Source + Length) - ((int)Gateway + Length + 0x5));
    *(int*)(Gateway + Length + 1) = GDiff;

    BYTE* Ptr = (BYTE*)Source;
    *Ptr = 0xE9;
    int Diff = (int)((int)Dest - (int)Source - 5);
    *(int*)(Ptr + 1) = Diff;

    if(Length > 5)
    {
        memset(Ptr + 5, 0x90, Length - 5);
    }

    VirtualProtect(Source, (size_t)Length, OldProtection, NULL);

    return Gateway;
}

DWORD WINAPI
ThreadProc(HMODULE Module)
{
    SpawnConsole();
    printf("Thread Started!\n");

    IDirect3D9* D3D = Direct3DCreate9(D3D_SDK_VERSION);
    D3DPRESENT_PARAMETERS Params = {};
    Params.Windowed = true;
    Params.SwapEffect = D3DSWAPEFFECT_DISCARD;
    Params.hDeviceWindow = GetForegroundWindow();
    IDirect3DDevice9* Device;
    HRESULT Result = D3D->CreateDevice(NULL, D3DDEVTYPE_HAL, Params.hDeviceWindow, D3DCREATE_HARDWARE_VERTEXPROCESSING, &Params, &Device);
    if(!SUCCEEDED(Result))
    {
        __debugbreak();
    }
    // EndScene static addr : d3d9.dll+63130
    void** VTable = *(void***)Device;
    //OldEndScene = (ENDSCENE)TrampolineHook(VTable[42], NewEndScene, 7);
    OldDrawIndexedPrimitive = (DRAWINDEXEDPRIMITIVE)TrampolineHook(VTable[82], NewDrawIndexedPrimitive, 5);
    printf("Hooked function address : 0x%llx\n", (unsigned long long)OldDrawIndexedPrimitive);

    while(true)
    {
        if(GetAsyncKeyState(VK_NUMPAD0) & 0x8000)
        {
            printf("Exitting thread!\n");
            break;
        }
//         if(GameDevice != NULL)
//         {
//             GameDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
//         }
        if(GetAsyncKeyState(VK_NUMPAD2) & 0x8000)
        {
            GameDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESS);
        }
    }

    FreeConsole();
    FreeLibrary(Module);
    return 0;
}

BOOL APIENTRY
DllMain(HMODULE hModule,
        DWORD   ul_reason_for_call,
        LPVOID  /*lpReserved*/)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        CloseHandle(CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)ThreadProc, hModule, 0, NULL));
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}