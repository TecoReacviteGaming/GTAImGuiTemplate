#include <mod/amlmod.h>
#include <mod/logger.h>
#include "ocornut/imgui_internal.h"
#include "ocornut/imgui_impl_renderware.h"

// AML PSDK
#include <aml-psdk/game_vc/plugin.h>
#include <aml-psdk/game_vc/Events.h>
#include <aml-psdk/game_vc/camera/Camera.h>
#include <aml-psdk/game_vc/base/Timer.h>

// macro to exclude ImGui rendering in-menu
//#define IMGUI_EXCLUDE_MENU
// macro to exclude ImGui rendering in-game
//#define IMGUI_EXCLUDE_GAME

MYMOD(net.ocornut.teco.imGuiTemplate, VC ImGui Template, 1.0, ocornut & TecoReacvite)
BEGIN_DEPLIST()
    ADD_DEPENDENCY_VER(net.rusjj.aml, 1.4.0)
END_DEPLIST()

// OWN vars
uintptr_t g_pGame = 0;
void* g_hGame = nullptr;
RsGlobalType* RsGlobal;

char buf[256] = "";
ImGuiTextBuffer logBuf;
bool ScrollToBottom = false;
float g_UIScale = 2.5f;

void AddGuiLog(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    logBuf.appendfv(fmt, args);
    logBuf.appendfv("\n", args);
    va_end(args);
    ScrollToBottom = true;
}

// ImGUI vars
ImGuiContext* g_imguiCtx = NULL;
ImVec2 displaySize;
ImVec2 zeroVec(0,0);
static float flScaleX, flScaleY;
static int nDisplayX, nDisplayY;
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
    if (!g_imguiCtx || trackNum < 0 || trackNum >= 10)
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
void (*OS_KeyboardRequest)(bool);
bool (*OS_KeyboardIsSoftVisible)();
DECL_HOOKv(KeyboardEvent, bool pushed, int keyNum, int ctrl_or_shift, int alwaysZero)
{
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantTextInput)
    {
        KeyboardEvent(pushed, keyNum, ctrl_or_shift, alwaysZero);
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
    if (!g_imguiCtx) return;
    ImGuiIO& io = ImGui::GetIO();
    
    ImGui_ImplRenderWare_NewFrame(); 
    ImGui::NewFrame();

    // your render ImGui code here
    ImGui::SetNextWindowBgAlpha(0.70f);
    ImGui::SetNextWindowSizeConstraints(ImVec2(io.DisplaySize.x * 0.23f, io.DisplaySize.y * 0.36f), ImVec2(io.DisplaySize.x, io.DisplaySize.y));
    ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("VC ImGui Template"))
    {
        ImGui::Text("ImGUI FPS: %.2f", io.Framerate);
        ImGui::BeginChild("Scrolling", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), true, ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::TextUnformatted(logBuf.begin());
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ScrollToBottom)
        {
            ImGui::SetScrollHereY(1.00f);
            ScrollToBottom = false;
        }
        ImGui::EndChild();
        if (ImGui::Button("Send") && buf[0] != '\0')
        {
            AddGuiLog("user: %s", buf);
            memset(buf, 0, sizeof(buf));
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear")) logBuf.clear();
        ImGui::SameLine();
        ImGui::PushItemWidth(io.DisplaySize.x * 0.15f);
        ImGui::InputText("", buf, IM_ARRAYSIZE(buf));
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImGui::Text("%d/%d max", (int)std::strlen(buf), (int)sizeof(buf));
    }
    ImGui::End();
    // ended here
    
    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplRenderWare_RenderDrawData(ImGui::GetDrawData());

    if (g_nClearMousePos > 0 && --g_nClearMousePos == 0) io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
    if (io.WantTextInput != g_bLastTextInput)
    {
        OS_KeyboardRequest((io.WantTextInput && !OS_KeyboardIsSoftVisible()) ? 1 : 0);
        g_bLastTextInput = io.WantTextInput;
    }
}

// main
ON_MOD_LOAD()
{
    logger->SetTag("ImGui Template (VC)");
    g_pGame = aml->GetLib("libGTAVC.so");
    g_hGame = aml->GetLibHandle("libGTAVC.so");
    if (!g_pGame || !g_hGame)
    {
        logger->Error("Unsupported game, expected GTA VC game!");
        return;
    }

    // game vars
    SET_TO(NVtoKK, aml->GetSym(g_hGame, "NVtoKK"));
    SET_TO(KKtoChar, aml->GetSym(g_hGame, "KKtoChar"));
    SET_TO(KKtoShiftedChar, aml->GetSym(g_hGame, "KKtoShiftedChar"));
    SET_TO(RsGlobal, (RsGlobalType*)aml->GetSym(g_hGame, "RsGlobal"));
    SET_TO(OS_KeyboardRequest, aml->GetSym(g_hGame, "_Z18OS_KeyboardRequesti"));
    SET_TO(OS_KeyboardIsSoftVisible, aml->GetSym(g_hGame, "_Z24OS_KeyboardIsSoftVisiblev"));

    // game hooks
    HOOK(TouchEvent, aml->GetSym(g_hGame, "_Z14AND_TouchEventiiii"));
    HOOK(KeyboardEvent, aml->GetSym(g_hGame, "_Z17AND_KeyboardEventbiib"));
    Events::initRwEvent.after += []()
    {
        if (g_imguiCtx) return;
        g_imguiCtx = ImGui::CreateContext();
        ImGui_ImplRenderWare_Init();
        nDisplayX = RsGlobal->maximumWidth;
        nDisplayY = RsGlobal->maximumHeight;
        flScaleX = nDisplayY * 0.00052083333f;
        flScaleY = nDisplayY * 0.00092592592f;
        displaySize.x = nDisplayX;
        displaySize.y = nDisplayY;

        // your initial ImGui code here
        ImGuiIO& io = ImGui::GetIO();
        ImGuiStyle& style = ImGui::GetStyle();
        style.ScaleAllSizes(g_UIScale);
        ImGui::StyleColorsDark();
        ImFontConfig config;
        config.SizePixels = roundf(13.0f * g_UIScale);
        io.Fonts->AddFontDefault(&config);
        // ended here
    };
#ifndef IMGUI_EXCLUDE_GAME
    Events::drawingEvent.after += []()
    {
        g_bAllowGuiTouchOnMenu = false;
        RenderImGui();
    };
#endif
#ifndef IMGUI_EXCLUDE_MENU
    Events::menuDrawingEvent.after += [](CMenuManager* menu)
    {
        g_bAllowGuiTouchOnMenu = true;
        RenderImGui();
    };
#endif
    Events::shutdownRwEvent.before += []()
    {
        if (!g_imguiCtx) return;
        ImGui_ImplRenderWare_ShutDown();
        ImGui::DestroyContext(g_imguiCtx);
        g_imguiCtx = NULL;
    };
}
