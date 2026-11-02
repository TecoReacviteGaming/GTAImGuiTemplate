#include <mod/amlmod.h>
#include <mod/logger.h>

#include "core_imgui.h"
#include "ocornut/imgui_impl_renderware.h"

// AML PSDK
#include "aml-psdk/game_sa/Events.h"
#include "aml-psdk/game_sa/entity/Camera.h"
#include "aml-psdk/game_sa/base/Timer.h"
#include "aml-psdk/game_sa/engine/OS.h"

// ImGUI core code
namespace
{
    uintptr_t g_pGame = 0;
    void* g_hGame = nullptr;
    static eGameType g_eGameType = UNKNOWN;
    
    // ImGUI vars
    ImGuiContext* g_imguiCtx = NULL;
    bool g_bImGuiInitialized = false;
    ImVec2 displaySize;
    ImVec2 zeroVec(0,0);
    static float flScaleX, flScaleY;
    static int nDisplayX, nDisplayY;
    static ImSDKCallback g_buildCall = nullptr;
    static ImSDKCallback g_renderCall = nullptr;

    // ImGui core functions
    void InitUI()
    {
        if (g_bImGuiInitialized || g_imguiCtx) return;
        g_imguiCtx = ImGui::CreateContext();
        ImGui_ImplRenderWare_Init();
        nDisplayX = RsGlobal.maximumWidth;
        nDisplayY = RsGlobal.maximumHeight;
        flScaleX = nDisplayY * 0.00052083333f;
        flScaleY = nDisplayY * 0.00092592592f;
        displaySize.x = nDisplayX;
        displaySize.y = nDisplayY;
        g_buildCall();
        g_bImGuiInitialized = true;
    }
    void RenderUI()
    {
        if (!g_bImGuiInitialized || !g_imguiCtx) return;
        ImGui_ImplRenderWare_NewFrame(); 
        ImGui::NewFrame();
        g_renderCall();
        ImGui::EndFrame();
        ImGui::Render();
        ImGui_ImplRenderWare_RenderDrawData(ImGui::GetDrawData());
    }
    void ShutdownUI()
    {
        if (!g_bImGuiInitialized || !g_imguiCtx) return;
        ImGui_ImplRenderWare_ShutDown();
        ImGui::DestroyContext(g_imguiCtx);
        g_imguiCtx = NULL;
        g_bImGuiInitialized = false;
    }
    bool IsTouchedOverUI(float x, float y)
    {
        if (!g_imguiCtx) return false;
        for (int i = g_imguiCtx->Windows.Size - 1; i >= 0; --i)
        {
            ImGuiWindow* win = g_imguiCtx->Windows[i];
            if (!win || !win->Active || win->Hidden) continue;
            if (win->Flags & (ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMouseInputs)) continue;
            const ImVec2 p = win->Pos;
            const ImVec2 s = win->Size;
            if (x >= p.x && x < p.x + s.x && y >= p.y && y < p.y + s.y) return true;
        }
        return false;
    }

    // touch system
    static char g_nClearMousePos = 0;
    static bool g_bIgnoredFingers[10] = {false};
    static bool g_bFingerOnUI[10] = {false};
    static uint8_t g_bFingerAsMouse = 0xFF;
    static bool g_bAllowGuiTouchOnMenu = false;
    DECL_HOOKv(TouchEvent, int actionType, int trackNum, int x, int y)
    {
        if (!g_bImGuiInitialized || trackNum < 0 || trackNum >= 10)
        {
            TouchEvent(actionType, trackNum, x, y);
            return;
        }
        ImGuiIO& io = ImGui::GetIO();
    
        switch (actionType)
        {
            case 1: // touch release
            {
                if (g_bFingerAsMouse == trackNum)
                {
                    g_nClearMousePos = 3;
                    io.AddMouseButtonEvent(0, false);
                    g_bFingerAsMouse = 0xFF;
                }
                break;
            }
            
            case 2: // touch push
            {
                const bool bCanUseUI = (CTimer::IsPaused() == g_bAllowGuiTouchOnMenu && TheCamera.GetScreenFadeStatus() != 2);
                g_bFingerOnUI[trackNum] = bCanUseUI && (io.WantTextInput || IsTouchedOverUI((float)x, (float)y));
                if (g_bFingerOnUI[trackNum] && g_bFingerAsMouse == 0xFF)
                {
                    io.AddMousePosEvent((float)x, (float)y);
                    io.AddMouseButtonEvent(0, true);
                    g_bFingerAsMouse = trackNum;
                }
                break;
            }
    
            case 3: // touch move
            {
                if (g_bFingerAsMouse == trackNum)
                    io.AddMousePosEvent((float)x, (float)y);
                break;
            }
        }
        
        const bool swallow = g_bFingerOnUI[trackNum];
        if (actionType == 1) g_bFingerOnUI[trackNum] = false;
        if (!swallow) TouchEvent(actionType, trackNum, x, y);
    }
    
    // keyboard system
    int* NVtoKK;
    char *KKtoChar, *KKtoShiftedChar;
    DECL_HOOKv(KeyboardEvent, bool pushed, int keyNum, int ctrl_or_shift, int alwaysZero)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantTextInput)
        {
            AND_KeyboardEvent(pushed, keyNum, ctrl_or_shift, alwaysZero);
            return;
        }
    
        switch (keyNum)
        {
            case 3:
                io.AddKeyEvent(ImGuiKey_Enter, pushed);
                break;
            
            case 4:
                io.AddKeyEvent(ImGuiKey_Backspace, pushed);
                break;
    
            default:
                if (pushed && keyNum >= 0)
                {
                    char sym = 0;
                    if (ctrl_or_shift <= 0) sym = KKtoChar[NVtoKK[keyNum]];
                    else sym = KKtoShiftedChar[NVtoKK[keyNum]];
                    if (sym != 0) io.AddInputCharacter(sym);
                }
                break;
        }
    }
    
    // ImGui rendering core
    static bool g_bLastTextInput = false;
    static std::string g_lastTextPasted;
    void RenderImGui()
    {
        RenderUI();
        ImGuiIO& io = ImGui::GetIO();
        if (g_nClearMousePos > 0 && --g_nClearMousePos == 0) io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
        if (io.WantTextInput != g_bLastTextInput)
        {
            OS_KeyboardRequest((io.WantTextInput && !OS_KeyboardIsSoftVisible()) ? 1 : 0);
            g_bLastTextInput = io.WantTextInput;
        }
    }
}

// ImSDK class functions
void ImSDK::CreateImGUI(ImSDKCallback buildBase, ImSDKCallback renderBase)
{
    g_pGame = aml->GetLib("libGTASA.so");
    g_hGame = aml->GetLibHandle("libGTASA.so");
    g_eGameType = GTA_SA;
    if (!g_pGame || !g_hGame)
    {
        logger->Error("Unsupported game, expected GTA game!");
        return;
    }

    // ImSDK callbacks
    g_buildCall = std::move(buildBase);
    g_renderCall = std::move(renderBase);
    if (!g_buildCall || !g_renderCall)
    {
        logger->Error("The build or render callback is invalid.");
        return;
    }

    // game vars
    SET_TO(NVtoKK, aml->GetSym(g_hGame, "NVtoKK"));
    SET_TO(KKtoChar, aml->GetSym(g_hGame, "KKtoChar"));
    SET_TO(KKtoShiftedChar, aml->GetSym(g_hGame, "KKtoShiftedChar"));
    
    // game hooks
    HOOK(TouchEvent, AND_TouchEvent);
    HOOK(KeyboardEvent, AND_KeyboardEvent);

    if (g_eGameType == GTA_SA)
    {
        Events::initRwEvent.after += InitUI;
#if !defined(IMGUI_EXCLUDE_GAME)
        Events::drawingEvent.after += []()
        {
            g_bAllowGuiTouchOnMenu = false;
            RenderImGui();
        };
#elif !defined(IMGUI_EXCLUDE_MENU)
        Events::menuDrawingEvent.after += []()
        {
            g_bAllowGuiTouchOnMenu = true;
            RenderImGui();
        };
#endif
        Events::shutdownRwEvent.before += ShutdownUI;
    }
}

void ImSDK::DestroyImGui()
{
    if (!g_bImGuiInitialized) return;
    ShutdownUI();
    g_bImGuiInitialized = false;
}

void ImSDK::NewCallback(ImSDKCallback newBuildBase, ImSDKCallback newRenderBase)
{
    if (newBuildBase)
    {
        g_buildCall = std::move(newBuildBase);
        g_buildCall();
    }
    
    if (newRenderBase)
        g_renderCall = std::move(newRenderBase);
}
