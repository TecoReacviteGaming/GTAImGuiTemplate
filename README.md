# ImSDK
ImGui SDK mainly made for AML plugin!

Original ImGui repository: https://github.com/ocornut/imgui

**ocornut** is the author of ImGui and I'm only making this SDK for easy integration in AML plugin.

## ✨ Features
- Functional keyboard for ImGui text input.
- Touching ImGui will only moves the imGui, not everything, so game camera movement or game controls is unaffected.
- Supported for both 32bit and 64bit game (GTA SA for now).

## 📜 Requirements
- [AML-PSDK](<https://github.com/AndroidModLoader/aml-psdk>)
- [SAUtils](<https://github.com/AndroidModLoader/SAUtils>)

## ⚙️ Integration
- Download this repository and put the "imsdk" folder (from "ImGuiTemplate" folder) in your project.
- Now put this line in your "Android.mk" file.
```makefile
LOCAL_SRC_FILES += $(wildcard $(LOCAL_PATH)/imsdk/ocornut/*.cpp) imsdk/core_imgui.cpp
```
- After you done these steps, you will now have a fully functional ImGui when you compile it.

these are optional, you can exclude ImGui rendering by defining one of these macros in "Android.mk" file.
```makefile
-- Add this macro to exclude ImGui rendering in-menu
LOCAL_CXXFLAGS += -DIMGUI_EXCLUDE_MENU

-- Add this macro to exclude ImGui rendering in-game
LOCAL_CXXFLAGS += -DIMGUI_EXCLUDE_GAME
```
or more straightforward you simply use this repository instead which will include the ImGui template and can compile right-away after you have all necessary files that the project needs.

This ImGUI can be updated by taking the latest source code from Dear ImGui repository and put all them in "ocornut" folder incase when ImSDK is outdated.
