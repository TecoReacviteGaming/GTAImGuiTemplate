#include <mod/amlmod.h>
#include <mod/logger.h>
#include "imsdk/core_imgui.h"

MYMOD(net.ocornut.teco.imsdk, ImGui SDK, 1.0, ocornut & TecoReacvite)
BEGIN_DEPLIST()
    ADD_DEPENDENCY_VER(net.rusjj.aml, 1.4.0)
END_DEPLIST()

// OWN vars
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

// main
ON_MOD_LOAD()
{
    logger->SetTag("ImGui Template");

    // The usage is very simple... a lot
    ImSDK::CreateImGUI(
        [&]() // initial ImGui (fonts, window size, etc...)
        {
            ImGuiIO& io = ImGui::GetIO();
            ImGuiStyle& style = ImGui::GetStyle();
            style.ScaleAllSizes(g_UIScale);
            ImGui::StyleColorsDark();
            ImFontConfig config;
            config.SizePixels = roundf(13.0f * g_UIScale);
            io.Fonts->AddFontDefault(&config);
        },
        [&]() // render ImGui (where stuffs will be rendered)
        {
            ImGuiIO& io = ImGui::GetIO();
            ImGui::SetNextWindowBgAlpha(0.70f);
            ImGui::SetNextWindowSizeConstraints(ImVec2(io.DisplaySize.x * 0.23f, io.DisplaySize.y * 0.36f), ImVec2(io.DisplaySize.x, io.DisplaySize.y));
            ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);
            if (ImGui::Begin("ImGui Template"))
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
        }
    );
}
