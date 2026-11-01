#pragma once

#include <aml-psdk/game_sa/plugin.h>
#include <aml-psdk/game_sa/engine/RsGlobal.h>
#include <aml-psdk/renderware/RwBase.h>
#include <aml-psdk/renderware/RwImage.h>
#include <aml-psdk/renderware/RwRaster.h>
#include <aml-psdk/renderware/RwRender.h>
#include <aml-psdk/gta_base/DrawVertices.h>
#include <aml-psdk/game_sa/engine/Sprite2d.h>
#include <aml-psdk/game_sa/other/Widgets.h>

#include "ocornut/imgui_internal.h"
#include <functional>

using ImSDKCallback = std::function<void()>;
void InitiateImGUI(ImSDKCallback buildBase, ImSDKCallback renderBase);
