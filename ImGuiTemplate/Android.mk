LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_CPP_EXTENSION := .cpp .cc
ifeq ($(TARGET_ARCH_ABI), armeabi-v7a)
    LOCAL_MODULE := ImGuiTemplate
else
    LOCAL_MODULE := ImGuiTemplate64
endif
LOCAL_SRC_FILES := main.cpp mod/logger.cpp
LOCAL_SRC_FILES += $(wildcard $(LOCAL_PATH)/imsdk/ocornut/*.cpp) imsdk/core_imgui.cpp
LOCAL_CXXFLAGS += -O2 -DNDEBUG -std=c++17 -DIMGUI_EXCLUDE_MENU
LOCAL_LDLIBS += -llog
include $(BUILD_SHARED_LIBRARY)
