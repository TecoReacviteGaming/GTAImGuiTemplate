#include <mod/amlmod.h>
#include <mod/logger.h>
#include <ocornut/imgui_internal.h>
#include <ocornut/backend/imgui_impl_renderware.h>
#include <ocornut/imgui_osk.h>

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
RsGlobalType* RsGlobal = nullptr;

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
// ImGui rendering core
void RenderImGui()
{
    if (!g_imguiCtx) return;
    ImGuiIO& io = ImGui::GetIO();
    
    ImGui_ImplRenderWare_NewFrame(); 
    ImGui::NewFrame();

    // our keyboard, should be first at all times
    ImGuiOSK::ProcessImGuiKeyboard();

    // your render ImGui code here
    ImGui::SetNextWindowBgAlpha(0.92f);
    ImGui::SetNextWindowSizeConstraints(ImVec2(io.DisplaySize.x * 0.23f, io.DisplaySize.y * 0.47f), ImVec2(io.DisplaySize.x, io.DisplaySize.y));
    ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("ImGui Template (VC)"))
    {
        ImGui::Text("ImGUI version: %s", ImGui::GetVersion());
        ImGui::Text("ImGUI FPS: %.2f", io.Framerate);
        ImGui::BeginChild("Scrolling", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() - 20.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::TextUnformatted(logBuf.begin());
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ScrollToBottom)
        {
            ImGui::SetScrollHereY(1.00f);
            ScrollToBottom = false;
        }
        ImGui::EndChild();
        if (ImGui::Button("Push") && buf[0] != '\0')
        {
            AddGuiLog("imgui-log: %s", buf);
            memset(buf, 0, sizeof(buf));
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear")) logBuf.clear();
        ImGui::SameLine();
        ImGui::PushItemWidth(io.DisplaySize.x * 0.12f);
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
    SET_TO(RsGlobal, (RsGlobalType*)(g_pGame + BYBIT(0x714F08, 0x991C20)));
    
    // game hooks
    HOOK(TouchEvent, aml->GetSym(g_hGame, "_Z14AND_TouchEventiiii"));
    Events::initRwEvent.after += []()
    {
        if (g_imguiCtx) return;
        g_imguiCtx = ImGui::CreateContext();
        ImGui_ImplRenderWare_Init();

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
