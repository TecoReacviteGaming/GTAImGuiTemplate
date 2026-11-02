#pragma once

#include "ocornut/imgui_internal.h"

#include <aml-psdk/game_sa/plugin.h>
#include <aml-psdk/game_sa/engine/RsGlobal.h>
#include <aml-psdk/renderware/RwBase.h>
#include <aml-psdk/renderware/RwImage.h>
#include <aml-psdk/renderware/RwRaster.h>
#include <aml-psdk/renderware/RwRender.h>
#include <aml-psdk/gta_base/DrawVertices.h>
#include <aml-psdk/game_sa/engine/Sprite2d.h>
#include <aml-psdk/game_sa/other/Widgets.h>

#include <functional>
using ImSDKCallback = std::function<void()>;
enum eGameType : int
{
    UNKNOWN = -1,
    GTA_III,
    GTA_VC,
    GTA_SA
};

class ImSDK
{
public:
    static void CreateImGUI(ImSDKCallback buildBase, ImSDKCallback renderBase);
    static void DestroyImGui();
    static void NewCallback(ImSDKCallback newBuildBase, ImSDKCallback newRenderBase);
};
