#pragma once

#include <ocornut/imgui.h>

struct RsGlobalType
{
    const char *appName;
    int32_t width;
    int32_t height;
    int32_t maximumWidth;
    int32_t maximumHeight;
};
extern RsGlobalType* RsGlobal;

bool ImGui_ImplRenderWare_Init();
void ImGui_ImplRenderWare_ShutDown();
void ImGui_ImplRenderWare_NewFrame();
void ImGui_ImplRenderWare_RenderDrawData(ImDrawData* draw_data);
bool ImGui_ImplRenderWare_CreateDeviceObjects();
